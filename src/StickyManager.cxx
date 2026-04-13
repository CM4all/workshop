// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "StickyManager.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/Publisher.hxx"
#include "lib/avahi/ServiceConfig.hxx"
#include "net/rh/Node.hxx"
#include "util/SpanCast.hxx"

#include <cassert>

using std::string_view_literals::operator""sv;

struct StickyManager::Node final : RendezvousHashing::Node {
	const std::string host_name;

	explicit Node(const std::string_view _host_name) noexcept
		:host_name(_host_name) {}
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
		return {best->second.host_name, true};

	double best_score = best->second.CalculateRendezvousScore(sticky_source);

	for (auto i = second; i != nodes.end(); ++i) {
		double score = i->second.CalculateRendezvousScore(sticky_source);
		if (score > best_score)
			best = i;
	}

	return {best->second.host_name, best->second.GetFlags().is_our_own};
}

void
StickyManager::OnAvahiNewObject(const std::string &key,
				const char *host_name,
				const InetAddress &address,
				AvahiStringList *txt,
				Avahi::ObjectFlags flags) noexcept
{
	auto [it, inserted] = nodes.try_emplace(key, host_name);
	it->second.Update(address, txt, flags);
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
