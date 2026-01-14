// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "StickyManager.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/Publisher.hxx"
#include "lib/avahi/ServiceConfig.hxx"
#include "net/InetAddress.hxx"
#include "util/FNVHash.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <cmath> // for std::log()

#include <stdlib.h> // for strtod()

using std::string_view_literals::operator""sv;

/**
 * The hash algorithm we use for Rendezvous Hashing.  FNV1a is fast
 * and has just the right properties for a good distribution among all
 * nodes.
 *
 * DJB is inferior when the node addresses are too similar (which is
 * often the case when all nodes are on the same local network) and
 * when the sticky_source is too short (e.g. when database serial
 * numbers are used) due to its small prime (33).
 */
using RendezvousHashAlgorithm = FNV1aAlgorithm<FNVTraits<uint32_t>>;

/**
 * Convert a quasi-random unsigned 64 bit integer to a
 * double-precision float in the range 0..1, preserving as many bits
 * as possible.  The returned value has no arithmetic meaning; the
 * goal of this function is only to convert a hash value to a floating
 * point value.
 */
template<std::unsigned_integral I>
static constexpr double
UintToDouble(const I i) noexcept
{
	constexpr unsigned SRC_BITS = std::numeric_limits<I>::digits;

	/* the mantissa has 53 bits, and this is how many bits we can
	   preserve in the conversion */
	constexpr unsigned DEST_BITS = std::numeric_limits<double>::digits;

	if constexpr (DEST_BITS < SRC_BITS) {
		/* discard upper bits that don't fit into the mantissa */
		constexpr uint_least64_t mask = (~I{}) >> (SRC_BITS - DEST_BITS);
		constexpr double max = I{1} << DEST_BITS;

		return (i & mask) / max;
	} else {
		/* don't discard anything */
		static_assert(std::numeric_limits<uintmax_t>::digits > std::numeric_limits<I>::digits);
		constexpr double max = std::uintmax_t{1} << SRC_BITS;

		return i / max;
	}
}

struct StickyManager::Node {
	/**
	 * The weight of this node (received in a Zeroconf TXT
	 * record).  We store the negative value because this
	 * eliminates one minus operator from the method
	 * CalculateRendezvousScore().
	 */
	double negative_weight;

	/**
	 * The precalculated hash of #address for Rendezvous
	 * Hashing.
	 */
	uint32_t address_hash;

	bool is_our_own;

	explicit Node(const InetAddress &address, double weight, bool _is_our_own) noexcept
		:negative_weight(-weight),
		 address_hash(RendezvousHashAlgorithm::BinaryHash(address.GetSteadyPart())),
		 is_our_own(_is_our_own) {}

	void Update(const InetAddress &address, double weight, bool _is_our_own) noexcept {
		negative_weight = -weight;
		address_hash = RendezvousHashAlgorithm::BinaryHash(address.GetSteadyPart());
		is_our_own = _is_our_own;
	}

	double CalculateRendezvousScore(std::span<const std::byte> sticky_source) const noexcept {
		const auto rendezvous_hash = RendezvousHashAlgorithm::BinaryHash(sticky_source, address_hash);
		return negative_weight / std::log(UintToDouble(rendezvous_hash));
	}
};

StickyManager::StickyManager(Avahi::Client &avahi_client,
		       Avahi::Publisher &_publisher,
		       Avahi::ErrorHandler &error_handler,
		       const Avahi::ServiceConfig &config,
		       ChangedCallback _changed_callback) noexcept
	:publisher(_publisher),
	 service(config, nullptr, IPv4Address{DUMMY_PORT}, false, true),
	 explorer(new Avahi::ServiceExplorer(avahi_client, *this,
					     service.interface, service.protocol,
					     config.service.c_str(),
					     config.domain.empty() ? nullptr : config.domain.c_str(),
					     error_handler)),
	 changed_callback(_changed_callback)
{
	assert(config.IsEnabled());
}

StickyManager::~StickyManager() noexcept
{
	assert(!is_published);
}

void
StickyManager::BeginShutdown() noexcept
{
	Disable();
	explorer.reset();
}

void
StickyManager::Enable() noexcept
{
	if (!is_published) {
		publisher.AddService(service);
		is_published = true;
	}
}

void
StickyManager::Disable() noexcept
{
	if (is_published) {
		is_published = false;
		publisher.RemoveService(service);
	}
}

std::pair<std::string_view, bool>
StickyManager::IsLocal(std::string_view id) const noexcept
{
	const auto sticky_source = AsBytes(id);

	auto best = nodes.begin();
	if (best == nodes.end()) [[unlikely]]
		// should never happen
		return {"localhost"sv, true};

	const auto second = std::next(best);
	if (second == nodes.end())
		// it's only us - skip the score calculation
		return {second->first, true};

	double best_score = best->second.CalculateRendezvousScore(sticky_source);

	for (auto i = second; i != nodes.end(); ++i) {
		double score = i->second.CalculateRendezvousScore(sticky_source);
		if (score > best_score)
			best = i;
	}

	return {best->first, best->second.is_our_own};
}

[[gnu::pure]]
static double
GetWeightFromTxt(AvahiStringList *txt) noexcept
{
	constexpr std::string_view prefix = "weight="sv;
	txt = avahi_string_list_find(txt, "weight");
	if (txt == nullptr)
		/* there's no "weight" record */
		return 1.0;

	const char *s = reinterpret_cast<const char *>(txt->text) + prefix.size();
	char *endptr;
	double value = strtod(s, &endptr);
	if (endptr == s || *endptr != '\0' || value <= 0 || value > 1e6)
		/* parser failed: fall back to default value */
		return 1.0;

	return value;
}

void
StickyManager::OnAvahiNewObject(const std::string &key,
			     const InetAddress &address,
			     AvahiStringList *txt,
			     Flags flags) noexcept
{
	const auto weight = GetWeightFromTxt(txt);

	auto [it, inserted] = nodes.try_emplace(key, address, weight, flags.is_our_own);
	if (!inserted) {
		/* update existing member */
		it->second.Update(address, weight, flags.is_our_own);
	}
}

void
StickyManager::OnAvahiRemoveObject(const std::string &key) noexcept
{
	nodes.erase(key);
}

void
StickyManager::OnAvahiAllForNow() noexcept
{
	changed_callback();
}
