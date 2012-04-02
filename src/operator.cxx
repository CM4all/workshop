/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "operator.hxx"
#include "workplace.hxx"
#include "plan.hxx"
#include "job.hxx"

extern "C" {
#include "syslog.h"
}

#include <glib.h>

#include <map>
#include <string>

#include <assert.h>
#include <unistd.h>
#include <string.h>

Operator::~Operator()
{
    if (stdout_fd >= 0) {
        event_del(&stdout_event);
        close(stdout_fd);
    }

    if (stderr_fd >= 0) {
        event_del(&stderr_event);
        close(stderr_fd);
    }

    if (syslog != NULL)
        syslog_close(&syslog);
}

void
free_operator(struct Operator **operator_r)
{
    assert(operator_r != NULL);
    assert(*operator_r != NULL);

    struct Operator *o = *operator_r;
    *operator_r = NULL;

    delete o;
}

static int splice_string(char **pp, size_t start, size_t end,
                         const char *replacement) {
    char *p = *pp, *n;
    size_t end_length = strlen(p + end);
    size_t replacement_length = strlen(replacement);

    assert(end >= start);
    assert(replacement != NULL);

    n = (char*)malloc(start + replacement_length + end_length + 1);
    if (n == NULL)
        return errno;

    memcpy(n, p, start);
    memcpy(n + start, replacement, replacement_length);
    memcpy(n + start + replacement_length, p + end, end_length);
    n[start + replacement_length + end_length] = 0;

    free(p);
    *pp = n;
    return 0;
}

typedef std::map<std::string, std::string> StringMap;

static int
expand_vars(char **pp, const StringMap &vars)
{
    int ret;
    char *v = *pp, *p = v, *dollar, *end;

    while (1) {
        dollar = strchr(p, '$');
        if (dollar == NULL)
            break;

        if (dollar[1] == '{') {
            end = strchr(dollar + 2, '}');
            if (end == NULL)
                break;

            const std::string key(dollar + 2, end);
            auto i = vars.find(key);
            const char *expanded = i != vars.end()
                ? i->second.c_str()
                : "";

            ret = splice_string(pp, dollar - v, end + 1 - v, expanded);
            if (ret != 0)
                return ret;

            p = *pp + (p - v) + strlen(expanded);
            v = *pp;
        } else {
            ++p;
        }
    }

    return 0;
}

int
expand_operator_vars(const struct Operator *o,
                     struct strarray *argv)
{
    StringMap vars;
    vars.insert(std::make_pair("0", argv->values[0]));
    vars.insert(std::make_pair("NODE", o->workplace->node_name));
    vars.insert(std::make_pair("JOB", o->job->id));
    vars.insert(std::make_pair("PLAN", o->job->plan_name));

    for (unsigned i = 1; i < argv->num; ++i) {
        assert(argv->values[i] != NULL);
        int ret = expand_vars(&argv->values[i], vars);
        if (ret != 0)
            return ret;
    }

    return 0;
}
