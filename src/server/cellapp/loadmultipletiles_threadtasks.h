#ifndef KBE_LOAD_MULTIPLE_TILES_THREADTASKS_H
#define KBE_LOAD_MULTIPLE_TILES_THREADTASKS_H

#include "common/common.h"
#include "thread/threadtask.h"
#include "helper/debug_helper.h"

namespace KBEngine {

	class LoadMultiTilesTask : public thread::TPTask
	{
	public:
		LoadMultiTilesTask(SPACE_ID spaceID, std::vector<std::string>& paths, std::vector<int>& rotations, std::vector< std::pair<int, int> >& positions):
			resPaths_(paths),
			rotations_(rotations),
			positions_(positions),
			spaceID_(spaceID)
		{
		}

		virtual ~LoadMultiTilesTask() {}
		virtual bool process();
		virtual thread::TPTask::TPTaskState presentMainThread();

	protected:
		std::vector<std::string> resPaths_;
		std::vector<int> rotations_;
		std::vector< std::pair<int, int> > positions_;
		SPACE_ID spaceID_;
	};


}

#endif // KBE_LOAD_MULTIPLE_TILES_THREADTASKS_H
