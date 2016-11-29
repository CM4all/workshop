/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PARTITION_HXX
#define WORKSHOP_PARTITION_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "util/BindMethod.hxx"

struct Config;
class Instance;

class Partition final : ExitListener {
    Instance &instance;

    Queue queue;
    Workplace workplace;

    BoundMethod<void()> idle_callback;

public:
    Partition(Instance &instance, const Config &config,
              BoundMethod<void()> _idle_callback);

    bool IsIdle() const {
        return workplace.IsEmpty();
    }

    void Start() {
        queue.Connect();
    }

    void Close() {
        queue.Close();
    }

    void BeginShutdown() {
        queue.Disable();
    }

    void UpdateFilter();
    void UpdateLibraryAndFilter(bool force);

private:
    bool StartJob(Job &&job);
    void OnJob(Job &&job);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
