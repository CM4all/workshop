// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "StickyManager.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/Publisher.hxx"
#include "lib/avahi/ServiceConfig.hxx"
#include "lib/avahi/Weight.hxx"
#include "system/Arch.hxx"
#include "net/InetAddress.hxx"
#include "net/rh/Node.hxx"
#include "util/FNVHash.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <cmath> // for std::log()

using std::string_view_literals::operator""sv;

struct StickyManager::Node final : RendezvousHashing::Node {
	bool is_our_own;

	void Update(const InetAddress &address, double weight, bool _is_our_own) noexcept {
		RendezvousHashing::Node::Update(address, Arch::NONE, weight);
		is_our_own = _is_our_own;
	}

	using RendezvousHashing::Node::CalculateRendezvousScore;
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

void
StickyManager::OnAvahiNewObject(const std::string &key,
				const InetAddress &address,
				AvahiStringList *txt,
				Flags flags) noexcept
{
	const auto weight = Avahi::GetWeightFromTxt(txt);

	auto [it, inserted] = nodes.try_emplace(key);
	it->second.Update(address, weight, flags.is_our_own);
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
