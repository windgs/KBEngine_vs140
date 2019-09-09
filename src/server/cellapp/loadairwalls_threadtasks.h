#ifndef KBE_LOAD_AIR_WALLS_THREAD_TASKS_H
#define KBE_LOAD_AIR_WALLS_THREAD_TASKS_H

#include "common/common.h"
#include "thread/threadtask.h"
#include "helper/debug_helper.h"

namespace KBEngine {
    class LoadAirWallsTask : public thread::TPTask
    {
    public:
        LoadAirWallsTask(std::string path)
            :resPath_(path)
        {}
        virtual ~LoadAirWallsTask() {}
        virtual bool process();
        virtual thread::TPTask::TPTaskState presentMainThread();
    private:
        std::string resPath_;
    };
}
#endif