// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Config.hxx"
#include "spawn/ConfigParser.hxx"
#include "debug.h"
#include "pg/Interval.hxx"
#include "system/Error.hxx"
#include "io/config/FileLineParser.hxx"
#include "io/config/ConfigParser.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/Parser.hxx"
#include "net/control/Protocol.hxx"
#include "net/log/Protocol.hxx"
#include "uri/EmailAddress.hxx" // for VerifyEmailAddress()
#include "util/StringAPI.hxx"
#include "util/StringParser.hxx"
#include "config.h"

#include <string.h>
#include <unistd.h> // for gethostname()

Config::Config()
{
	if (!debug_mode)
		spawn.spawner_uid_gid.Lookup("cm4all-workshop-spawn");

	if (debug_mode)
		spawn.default_uid_gid.LoadEffective();

#ifdef HAVE_LIBSYSTEMD
	spawn.systemd_scope = "workshop-spawn.scope";
	spawn.systemd_scope_description = "The cm4all-workshop child process spawner";
	spawn.systemd_slice = "system-cm4all.slice";
#endif
}

void
Config::Check()
{
	if (node_name.empty()) {
		char name[256];
		if (gethostname(name, sizeof(name)) < 0)
			throw MakeErrno("gethostname() failed");

		node_name = name;
	}

	if (!cron_partitions.empty() && !spawn.allow_any_uid_gid && !debug_mode) {
		if (spawn.allowed_uids.empty())
			throw std::runtime_error("No 'allow_user' in 'spawn' section");

		if (spawn.allowed_gids.empty())
			throw std::runtime_error("No 'allow_group' in 'spawn' section");
	}

	if (debug_mode)
		/* accept gid=0 (keep current gid) from translation server if
		   Workshop was started as unprivileged user */
		spawn.allowed_gids.insert(0);

	if (partitions.empty() && cron_partitions.empty())
		throw std::runtime_error("No 'workshop' or 'cron' section");

	for (const auto &i : partitions)
		i.Check();

	for (const auto &i : cron_partitions)
		i.Check();
}

class WorkshopConfigParser final : public NestedConfigParser {
	Config &config;

	class Partition final : public ConfigParser {
		Config &parent;
		WorkshopPartitionConfig config;

	public:
		explicit Partition(Config &_parent):parent(_parent) {}

	protected:
		/* virtual methods from class ConfigParser */
		void ParseLine(FileLineParser &line) override;
		void Finish() override;
	};

	class CronPartition final : public ConfigParser {
		Config &parent;
		CronPartitionConfig config;

	public:
		explicit CronPartition(Config &_parent,
				       std::string &&name)
			:parent(_parent),
			 config(std::move(name)) {}

	protected:
		/* virtual methods from class ConfigParser */
		void ParseLine(FileLineParser &line) override;
		void Finish() override;
	};

	class Control final : public ConfigParser {
		WorkshopConfigParser &parent;
		Config::ControlListener config;

	public:
		explicit Control(WorkshopConfigParser &_parent)
			:parent(_parent) {}

	protected:
		/* virtual methods from class ConfigParser */
		void ParseLine(FileLineParser &line) override;
		void Finish() override;
	};

public:
	explicit WorkshopConfigParser(Config &_config)
		:config(_config) {}

protected:
	/* virtual methods from class NestedConfigParser */
	void ParseLine2(FileLineParser &line) override;

private:
	void CreateControl(FileLineParser &line);
};

void
WorkshopConfigParser::Partition::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "database") == 0) {
		config.database = line.ExpectValueAndEnd();
	} else if (strcmp(word, "database_schema") == 0) {
		config.database_schema = line.ExpectValueAndEnd();
	} else if (StringIsEqual(word, "translation_server")) {
		config.translation_socket.SetLocal(line.ExpectValueAndEnd());
	} else if (StringIsEqual(word, "tag")) {
		config.tag = line.ExpectValueAndEnd();
	} else if (strcmp(word, "max_log") == 0) {
		config.max_log = ParseSize(line.ExpectValueAndEnd());
	} else if (strcmp(word, "journal") == 0) {
		config.enable_journal = line.NextBool();
		line.ExpectEnd();
	} else
		throw LineParser::Error("Unknown option");
}

void
WorkshopConfigParser::Partition::Finish()
{
	config.Check();
	parent.partitions.emplace_front(std::move(config));

	ConfigParser::Finish();
}

static AllocatedSocketAddress
ResolveStreamConnect(const char *host, int default_port)
{
	if (*host == '/' || *host == '@') {
		AllocatedSocketAddress result;
		result.SetLocal(host);
		return result;
	} else {
		static constexpr auto hints =
			MakeAddrInfo(AI_ADDRCONFIG, AF_UNSPEC, SOCK_STREAM);

		return AllocatedSocketAddress(Resolve(host, default_port,
						      &hints).GetBest());
	}
}

void
WorkshopConfigParser::CronPartition::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "database") == 0) {
		config.database = line.ExpectValueAndEnd();
	} else if (strcmp(word, "database_schema") == 0) {
		config.database_schema = line.ExpectValueAndEnd();
	} else if (strcmp(word, "translation_server") == 0) {
		config.translation_socket.SetLocal(line.ExpectValueAndEnd());
	} else if (strcmp(word, "qmqp_server") == 0) {
		config.qmqp_server = ResolveStreamConnect(line.ExpectValueAndEnd(),
							  628);
	} else if (StringIsEqual(word, "default_email_sender")) {
		config.default_email_sender = line.ExpectValueAndEnd();
		if (!VerifyEmailAddress(config.default_email_sender))
			throw LineParser::Error{"Bad email address"};
	} else if (strcmp(word, "pond_server") == 0) {
		config.pond_server = ResolveStreamConnect(line.ExpectValueAndEnd(),
							  Net::Log::DEFAULT_PORT);
	} else if (strcmp(word, "tag") == 0) {
		config.tag = line.ExpectValueAndEnd();
	} else if (StringIsEqual(word, "default_timeout")) {
		const auto default_timeout = Pg::ParseIntervalS(line.ExpectValueAndEnd());
		if (default_timeout.count() <= 0)
			throw LineParser::Error("Bad timeout");

		config.default_timeout = default_timeout;
	} else if (StringIsEqual(word, "use_qrelay")) {
		config.use_qrelay = line.NextBool();
		line.ExpectEnd();
	} else
		throw LineParser::Error("Unknown option");
}

void
WorkshopConfigParser::CronPartition::Finish()
{
	config.Check();
	parent.cron_partitions.emplace_front(std::move(config));

	ConfigParser::Finish();
}

void
WorkshopConfigParser::Control::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "bind") == 0) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 BengControl::DEFAULT_PORT,
							 true);
	} else if (strcmp(word, "multicast_group") == 0) {
		config.multicast_group = ParseSocketAddress(line.ExpectValueAndEnd(),
							    0, false);
	} else if (strcmp(word, "interface") == 0) {
		config.interface = line.ExpectValueAndEnd();
	} else
		throw LineParser::Error("Unknown option");
}

void
WorkshopConfigParser::Control::Finish()
{
	if (config.bind_address.IsNull())
		throw LineParser::Error("Bind address is missing");

	config.Fixup();

	parent.config.control_listen.emplace_front(std::move(config));

	ConfigParser::Finish();
}

inline void
WorkshopConfigParser::CreateControl(FileLineParser &line)
{
	line.ExpectSymbolAndEol('{');
	SetChild(std::make_unique<Control>(*this));
}

void
WorkshopConfigParser::ParseLine2(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "workshop") == 0) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Partition>(config));
	} else if (strcmp(word, "cron") == 0) {
		std::string name;

		if (line.front() == '"')
			name = line.ExpectValue();

		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<CronPartition>(config, std::move(name)));
	} else if (strcmp(word, "node_name") == 0) {
		config.node_name = line.ExpectValueAndEnd();
	} else if (strcmp(word, "concurrency") == 0) {
		config.concurrency = ParsePositiveLong(line.ExpectValueAndEnd(),
						       256);
	} else if (strcmp(word, "spawn") == 0) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<SpawnConfigParser>(config.spawn));
	} else if (strcmp(word, "control") == 0) {
		CreateControl(line);
	} else
		throw LineParser::Error("Unknown option");
}

void
LoadConfigFile(Config &config, const char *path)
{
	WorkshopConfigParser parser(config);
	VariableConfigParser v_parser(parser);
	CommentConfigParser parser2(v_parser);
	IncludeConfigParser parser3(path, parser2);

	ParseConfigFile(path, parser3);
}
