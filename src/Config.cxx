/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"
#include "debug.h"
#include "system/Error.hxx"
#include "io/LineParser.hxx"
#include "io/ConfigParser.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "util/StringParser.hxx"
#include "util/RuntimeError.hxx"

#include <string.h>
#include <pwd.h>
#include <grp.h>

Config::Config()
{
    if (debug_mode)
        spawn.default_uid_gid.LoadEffective();
}

void
Config::Check()
{
    if (!debug_mode && user.IsEmpty())
        throw std::runtime_error("no user name specified (-u)");

    if (node_name.empty()) {
        char name[256];
        if (gethostname(name, sizeof(name)) < 0)
            throw MakeErrno("gethostname() failed");

        node_name = name;
    }

    if (!spawn.allow_any_uid_gid && !debug_mode) {
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

class SpawnConfigParser final : public ConfigParser {
    SpawnConfig &config;

public:
    explicit SpawnConfigParser(SpawnConfig &_config):config(_config) {}

protected:
    /* virtual methods from class ConfigParser */
    void ParseLine(LineParser &line) override;
};

static uid_t
ParseUser(const char *name)
{
    char *endptr;
    unsigned long i = strtoul(name, &endptr, 10);
    if (endptr > name && *endptr == 0)
        return i;

    const auto *pw = getpwnam(name);
    if (pw == nullptr)
        throw FormatRuntimeError("No such user: %s", name);

    return pw->pw_uid;
}

static uid_t
ParseGroup(const char *name)
{
    char *endptr;
    unsigned long i = strtoul(name, &endptr, 10);
    if (endptr > name && *endptr == 0)
        return i;

    const auto *gr = getgrnam(name);
    if (gr == nullptr)
        throw FormatRuntimeError("No such group: %s", name);

    return gr->gr_gid;
}

void
SpawnConfigParser::ParseLine(LineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "allow_user") == 0) {
        config.allowed_uids.insert(ParseUser(line.ExpectValueAndEnd()));
    } else if (strcmp(word, "allow_group") == 0) {
        config.allowed_gids.insert(ParseGroup(line.ExpectValueAndEnd()));
    } else
        throw LineParser::Error("Unknown option");
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
        void ParseLine(LineParser &line) override;
        void Finish() override;
    };

    class CronPartition final : public ConfigParser {
        Config &parent;
        CronPartitionConfig config;

    public:
        explicit CronPartition(Config &_parent):parent(_parent) {}

    protected:
        /* virtual methods from class ConfigParser */
        void ParseLine(LineParser &line) override;
        void Finish() override;
    };

public:
    explicit WorkshopConfigParser(Config &_config)
        :config(_config) {}

protected:
    /* virtual methods from class NestedConfigParser */
    void ParseLine2(LineParser &line) override;
};

void
WorkshopConfigParser::Partition::ParseLine(LineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "database") == 0) {
        config.database = line.ExpectValueAndEnd();
    } else if (strcmp(word, "database_schema") == 0) {
        config.database_schema = line.ExpectValueAndEnd();
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
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG;
        hints.ai_socktype = SOCK_STREAM;

        return AllocatedSocketAddress(Resolve(host, default_port,
                                              &hints).front());
    }
}

void
WorkshopConfigParser::CronPartition::ParseLine(LineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "database") == 0) {
        config.database = line.ExpectValueAndEnd();
    } else if (strcmp(word, "database_schema") == 0) {
        config.database_schema = line.ExpectValueAndEnd();
    } else if (strcmp(word, "translation_server") == 0) {
        config.translation_socket = line.ExpectValueAndEnd();
    } else if (strcmp(word, "qmqp_server") == 0) {
        config.qmqp_server = ResolveStreamConnect(line.ExpectValueAndEnd(),
                                                  628);
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
WorkshopConfigParser::ParseLine2(LineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "workshop") == 0) {
        line.ExpectSymbolAndEol('{');
        SetChild(std::make_unique<Partition>(config));
    } else if (strcmp(word, "cron") == 0) {
        line.ExpectSymbolAndEol('{');
        SetChild(std::make_unique<CronPartition>(config));
    } else if (strcmp(word, "node_name") == 0) {
        config.node_name = line.ExpectValueAndEnd();
    } else if (strcmp(word, "concurrency") == 0) {
        config.concurrency = ParsePositiveLong(line.ExpectValueAndEnd(),
                                               256);
    } else if (strcmp(word, "spawn") == 0) {
        line.ExpectSymbolAndEol('{');
        SetChild(std::make_unique<SpawnConfigParser>(config.spawn));
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
