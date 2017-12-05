/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PARTITION_HXX
#define WORKSHOP_PARTITION_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "io/Logger.hxx"
#include "util/BindMethod.hxx"

struct Config;
struct WorkshopPartitionConfig;
class Instance;
class MultiLibrary;

class WorkshopPartition final : ExitListener {
    const Logger logger;

    Instance &instance;
    MultiLibrary &library;

    WorkshopQueue queue;
    WorkshopWorkplace workplace;

    BoundMethod<void()> idle_callback;

    const size_t max_log;

public:
    WorkshopPartition(Instance &instance,
                      MultiLibrary &_library,
                      SpawnService &_spawn_service,
                      const Config &root_config,
                      const WorkshopPartitionConfig &config,
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
