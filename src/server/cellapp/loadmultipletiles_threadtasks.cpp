#include "space.h"	
#include "spacememorys.h"	
#include "loadmultipletiles_threadtasks.h"
#include "server/serverconfig.h"
#include "common/deadline.h"
#include "navigation/navigation.h"

namespace KBEngine {

	//-------------------------------------------------------------------------------------
	bool LoadMultiTilesTask::process()
	{
		Navigation::getSingleton().loadMultipleTilesMap(resPaths_, rotations_, positions_);
		return false;
	}

	//-------------------------------------------------------------------------------------
	thread::TPTask::TPTaskState LoadMultiTilesTask::presentMainThread()
	{
		std::string joinedPath = "";
		for (std::vector<std::string>::iterator it = resPaths_.begin();it != resPaths_.end(); it++)
			joinedPath += (*it+";");
		for (std::vector<int>::iterator it = rotations_.begin();it != rotations_.end(); it++)
			joinedPath += (" " + std::to_string(*it));
		for (std::vector< std::pair<int, int> >::iterator it = positions_.begin();it != positions_.end(); it++)
			joinedPath += (" " + std::to_string(it->first) + " " + std::to_string(it->second));

		NavigationHandlePtr pNavigationHandle = Navigation::getSingleton().findNavigation(joinedPath);

		SpaceMemory* pSpace = SpaceMemorys::findSpace(spaceID_);
		if (pSpace == NULL || !pSpace->isGood())
		{
			ERROR_MSG(fmt::format("LoadNavmeshTask::presentMainThread(): not found space({})\n",
				spaceID_));
		}
		else
		{
			pSpace->onLoadedSpaceGeometryMapping(pNavigationHandle);
		}

		return thread::TPTask::TPTASK_STATE_COMPLETED;
	}

	//-------------------------------------------------------------------------------------
}
