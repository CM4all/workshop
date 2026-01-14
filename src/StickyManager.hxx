// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "lib/avahi/ExplorerListener.hxx"
#include "lib/avahi/Service.hxx"
#include "util/BindMethod.hxx"

#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace Avahi {
class Client;
class ErrorHandler;
class Publisher;
struct ServiceConfig;
class ServiceExplorer;
}

/**
 * This class manages job stickiness (option "sticky").  It publishes
 * itself on the local network using Zeroconf and maintains a list of
 * other Workshop nodes.  The "local" check uses Rendezvous Hashing to
 * see if this node is the top-most one.
 */
class StickyManager final : Avahi::ServiceExplorerListener {
	static constexpr uint_least16_t DUMMY_PORT = 1234;

	Avahi::Publisher &publisher;

	Avahi::Service service;
	std::unique_ptr<Avahi::ServiceExplorer> explorer;

	using ChangedCallback = BoundMethod<void() noexcept>;
	const ChangedCallback changed_callback;

	struct Node;
	std::map<std::string, Node, std::less<>> nodes;

	bool is_published = false;

public:
	StickyManager(Avahi::Client &avahi_client,
		   Avahi::Publisher &_publisher,
		   Avahi::ErrorHandler &error_handler,
		   const Avahi::ServiceConfig &config,
		   ChangedCallback _changed_callback) noexcept;
	~StickyManager() noexcept;

	void BeginShutdown() noexcept;

	void Enable() noexcept;
	void Disable() noexcept;

	/**
	 * Check on which node the specified sticky id is supposed to
	 * be executed.
	 *
	 * @return the node name where the specified sticky id should
	 * be executed and a bool describing whether it is the local
	 * host
	 */
	[[gnu::pure]]
	std::pair<std::string_view, bool> IsLocal(std::string_view id) const noexcept;

private:
	/* virtual methods from class AvahiServiceExplorerListener */
	void OnAvahiNewObject(const std::string &key,
			      const InetAddress &address,
			      AvahiStringList *txt,
			      Flags flags) noexcept override;
	void OnAvahiRemoveObject(const std::string &key) noexcept override;
	void OnAvahiAllForNow() noexcept override;
};
