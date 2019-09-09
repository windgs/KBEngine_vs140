// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#include "navigation.h"
#include "resmgr/resmgr.h"
#include "thread/threadguard.h"

#include "navigation_tile_handle.h"
#include "navigation_mesh_handle.h"

namespace KBEngine{

KBE_SINGLETON_INIT(Navigation);

//-------------------------------------------------------------------------------------
Navigation::Navigation():
navhandles_(),
mutex_()
{
}

//-------------------------------------------------------------------------------------
Navigation::~Navigation()
{
	finalise();
}

//-------------------------------------------------------------------------------------
void Navigation::finalise()
{
	KBEngine::thread::ThreadGuard tg(&mutex_);
	navhandles_.clear();
}

//-------------------------------------------------------------------------------------
bool Navigation::removeNavigation(std::string resPath)
{
	KBEngine::thread::ThreadGuard tg(&mutex_); 
	KBEUnordered_map<std::string, NavigationHandlePtr>::iterator iter = navhandles_.find(resPath);
	if(navhandles_.find(resPath) != navhandles_.end())
	{
		iter->second->decRef();
		navhandles_.erase(iter);

		DEBUG_MSG(fmt::format("Navigation::removeNavigation: ({}) is destroyed!\n", resPath));
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------
NavigationHandlePtr Navigation::findNavigation(std::string resPath)
{
	KBEngine::thread::ThreadGuard tg(&mutex_); 
	KBEUnordered_map<std::string, NavigationHandlePtr>::iterator iter = navhandles_.find(resPath);
	if(navhandles_.find(resPath) != navhandles_.end())
	{
		if(iter->second == NULL)
			return NULL;

		/*if(iter->second->type() == NavigationHandle::NAV_MESH)
		{
			return iter->second;
		}
		else if (iter->second->type() == NavigationHandle::NAV_TILE)
		{
			// 由于tile需要做碰撞， 每一个space都需要一份新的数据， 我们这里采用拷贝的方式来增加构造速度
			NavTileHandle* pNavTileHandle = new NavTileHandle(*(KBEngine::NavTileHandle*)iter->second.get());
			DEBUG_MSG(fmt::format("Navigation::findNavigation: copy NavTileHandle({:p})!\n", (void*)pNavTileHandle));
			return NavigationHandlePtr(pNavTileHandle);
		}*/

		return iter->second;
	}

	return NULL;
}

//-------------------------------------------------------------------------------------
bool Navigation::hasNavigation(std::string resPath)
{
	KBEngine::thread::ThreadGuard tg(&mutex_); 
	return navhandles_.find(resPath) != navhandles_.end();
}

//-------------------------------------------------------------------------------------
NavigationHandlePtr Navigation::loadNavigation(std::string resPath, const std::map< int, std::string >& params)
{
	KBEngine::thread::ThreadGuard tg(&mutex_); 
	if(resPath == "")
		return NULL;
	
	KBEUnordered_map<std::string, NavigationHandlePtr>::iterator iter = navhandles_.find(resPath);
	if(iter != navhandles_.end())
	{
		return iter->second;
	}

	NavigationHandle* pNavigationHandle_ = NULL;

	std::string path = resPath;
	path = Resmgr::getSingleton().matchPath(path);
	if(path.size() == 0)
		return NULL;
		
	wchar_t* wpath = strutil::char2wchar(path.c_str());
	std::wstring wspath = wpath;
	free(wpath);

	std::vector<std::wstring> results;
	Resmgr::getSingleton().listPathRes(wspath, L"tmx", results);
	
	if(results.size() > 0)
	{
		pNavigationHandle_ = NavTileHandle::create(resPath, params);
	}
	else 	
	{
		results.clear();
		Resmgr::getSingleton().listPathRes(wspath, L"navmesh", results);

		if(results.size() == 0)
		{
			return NULL;
		}

		pNavigationHandle_ = NavMeshHandle::create(resPath, params);
	}


	navhandles_[resPath] = NavigationHandlePtr(pNavigationHandle_);
	return pNavigationHandle_;
}

NavigationHandlePtr Navigation::loadMultipleTilesMap(std::vector<std::string> resPaths, std::vector<int>& rotations, std::vector< std::pair<int, int> >& positions)
{
	KBEngine::thread::ThreadGuard tg(&mutex_);
	if (resPaths.empty())
		return NULL;
	std::string joinedPath = "";
	for (std::vector<std::string>::iterator it = resPaths.begin();it != resPaths.end(); it++)
		joinedPath += (*it+ ";");
	for (std::vector<int>::const_iterator it = rotations.begin();it != rotations.end(); it++)
		joinedPath += (" " + std::to_string(*it));
	for (std::vector< std::pair<int, int> >::const_iterator it = positions.begin();it != positions.end(); it++)
		joinedPath += (" " + std::to_string(it->first) + " " + std::to_string(it->second));

	DEBUG_MSG(fmt::format("Navigation::loadMultipleTilesMap: {}\n", joinedPath.c_str()));

	KBEUnordered_map<std::string, NavigationHandlePtr>::iterator iter = navhandles_.find(joinedPath);
	if (iter != navhandles_.end())
	{
		//return iter->second;
	}

	NavigationHandle* pNavigationHandle_ = NavTileHandle::createFromMergingMultiplePath(resPaths, rotations, positions);
	if (!pNavigationHandle_)
		return NULL;

	navhandles_[joinedPath] = NavigationHandlePtr(pNavigationHandle_);
	return pNavigationHandle_;
}

//-------------------------------------------------------------------------------------		
}
