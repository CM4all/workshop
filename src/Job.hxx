/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_JOB_HXX
#define WORKSHOP_JOB_HXX

#include <string>
#include <list>

class Queue;

struct Job {
    Queue &queue;

    std::string id, plan_name, syslog_server;

    std::list<std::string> args;

    Job(Queue &_queue):queue(_queue) {}

    Job(Queue &_queue, const char *_id, const char *_plan_name)
        :queue(_queue), id(_id), plan_name(_plan_name) {
    }

    /**
     * Update the "progress" value of the job.
     *
     * @param job the job
     * @param progress a percent value (0 .. 100)
     * @param timeout the timeout for the next feedback (an interval
     * string that is understood by PostgreSQL)
     * @return 1 on success, 0 if the job was not found, -1 on error
     */
    int SetProgress(unsigned progress, const char *timeout);

    /**
     * Mark the job as "done".
     */
    bool SetDone(int status);
};

#endif
