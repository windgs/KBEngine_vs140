#include "loadairwalls_threadtasks.h"
#include "navigation/AirWall.h"

namespace KBEngine {
    bool KBEngine::LoadAirWallsTask::process()
    {
        AirWall::AirWall::getSingleton().loadAllAirWallFromPath(resPath_);
        return false;
    }

    thread::TPTask::TPTaskState KBEngine::LoadAirWallsTask::presentMainThread()
    {
        return thread::TPTask::TPTASK_STATE_COMPLETED;
    }
}