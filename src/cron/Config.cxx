/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"
#include "debug.h"
#include "system/Error.hxx"
#include "io/LineParser.hxx"
#include "io/ConfigParser.hxx"
#include "util/StringParser.hxx"

#include <string.h>
#include <unistd.h>

CronConfig::CronConfig()
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
CronConfig::Check()
{
    if (!debug_mode && !daemon_user_defined(&user))
        throw std::runtime_error("no user name specified (-u)");

    if (node_name.empty()) {
        char name[256];
        if (gethostname(name, sizeof(name)) < 0)
            throw MakeErrno("gethostname() failed");

        node_name = name;
    }

    if (database.empty())
        throw std::runtime_error("Missing 'database' setting");

    if (translation_socket.empty())
        throw std::runtime_error("Missing 'translation_server' setting");
}

class CronConfigParser final : public NestedConfigParser {
    CronConfig &config;

public:
    explicit CronConfigParser(CronConfig &_config)
        :config(_config) {}

protected:
    /* virtual methods from class NestedConfigParser */
    void ParseLine2(LineParser &line) override;
};

void
CronConfigParser::ParseLine2(LineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "database") == 0) {
        config.database = line.ExpectValueAndEnd();
    } else if (strcmp(word, "translation_server") == 0) {
        config.translation_socket = line.ExpectValueAndEnd();
    } else if (strcmp(word, "concurrency") == 0) {
        config.concurrency = ParsePositiveLong(line.ExpectValueAndEnd(),
                                               256);
    } else
        throw LineParser::Error("Unknown option");
}

void
LoadConfigFile(CronConfig &config, const char *path)
{
    CronConfigParser parser(config);
    VariableConfigParser v_parser(parser);
    CommentConfigParser parser2(v_parser);
    IncludeConfigParser parser3(path, parser2);

    ParseConfigFile(path, parser3);

}
