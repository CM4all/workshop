/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"
#include "debug.h"
#include "io/LineParser.hxx"
#include "io/ConfigParser.hxx"
#include "util/StringParser.hxx"

#include <string.h>

Config::Config()
{
    memset(&user, 0, sizeof(user));

    if (debug_mode) {
        spawn.default_uid_gid.LoadEffective();
    } else {
        spawn.default_uid_gid.uid = 65534;
        spawn.default_uid_gid.gid = 65534;
    }
}

void
Config::Check()
{
    if (!debug_mode && !daemon_user_defined(&user))
        throw std::runtime_error("no user name specified (-u)");

    if (node_name.empty())
        throw std::runtime_error("no node name specified");

    if (database.empty())
        throw std::runtime_error("no WORKSHOP_DATABASE environment variable");
}

class WorkshopConfigParser final : public NestedConfigParser {
    Config &config;

public:
    explicit WorkshopConfigParser(Config &_config)
        :config(_config) {}

protected:
    /* virtual methods from class NestedConfigParser */
    void ParseLine2(LineParser &line) override;
};

void
WorkshopConfigParser::ParseLine2(LineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "database") == 0) {
        config.database = line.ExpectValueAndEnd();
    } else if (strcmp(word, "node_name") == 0) {
        config.node_name = line.ExpectValueAndEnd();
    } else if (strcmp(word, "concurrency") == 0) {
        config.concurrency = ParsePositiveLong(line.ExpectValueAndEnd(),
                                               256);
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
