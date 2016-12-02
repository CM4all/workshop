/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PARTITION_HXX
#define WORKSHOP_PARTITION_HXX

#include "Config.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "util/BindMethod.hxx"

class Instance;

class WorkshopPartition final : ExitListener {
    Instance &instance;

    WorkshopQueue queue;
    WorkshopWorkplace workplace;

    BoundMethod<void()> idle_callback;

public:
    WorkshopPartition(Instance &instance, SpawnService &_spawn_service,
                      const Config &root_config,
                      const Config::Partition &config,
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
    bool StartJob(WorkshopJob &&job);
    void OnJob(WorkshopJob &&job);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
