#include "AirWall.h"
#include "helper/debug_helper.h"
#include "resmgr/resmgr.h"

using KBEngine::DebugHelper;
using KBEngine::Singleton;

KBE_SINGLETON_INIT(AirWall::AirWall);

namespace AirWall
{
AirWall::AirWall()
    :walls_(),
	isLoad_(false)
{
}

AirWall::~AirWall()
{
    walls_.clear();
}


bool AirWall::isLoad()
{
    return isLoad_;
}

bool AirWall::loadAllAirWallFromPath(const std::string &resPath)
{
    DEBUG_MSG(fmt::format("{}: load res from path:{}\n", __FUNCTION__, resPath));
    if (isLoad_)
    {
        ERROR_MSG(fmt::format("AirWall::loadAllAirWallFromPath:all file has loaded!"));
        return false;
    }

    std::string path = KBEngine::Resmgr::getSingleton().matchPath(resPath);
    wchar_t* wpath = KBEngine::strutil::char2wchar(path.c_str());
    std::wstring wspath = wpath;
    free(wpath);

    std::vector<std::wstring> results;
    KBEngine::Resmgr::getSingleton().listPathRes(wspath, L"tmx", results);

    if (results.size() == 0)
    {
        ERROR_MSG(fmt::format("AirWall::loadAllAirWallFromPath: path({}) not found tmx.!\n",
            KBEngine::Resmgr::getSingleton().matchRes(path)));

        return false;
    }

    for (auto iter : results)
    {
        char *cpath = KBEngine::strutil::wchar2char(iter.c_str());
        path = cpath;
        free(cpath);
        auto mapPtr = std::make_shared<MatrixMap::MatrixMap>(path);
        
        int lastSlash = path.find_last_of("/");
        std::string key;
        if (lastSlash > 0)
        {
            key = path.substr(lastSlash + 1);
        }
        else
        {
            key = path;
        }

        DEBUG_MSG(fmt::format("{}: load air wall file:{}\n", __FUNCTION__, key));
        walls_.insert(std::make_pair(key, mapPtr));
    }

    isLoad_ = true;
    return true;
}

MatrixMap::MatrixMapPtr AirWall::getMapById(const std::string & ID)
{
    if (!isLoad_)
    {
        ERROR_MSG(fmt::format("{}: the res has not loaded!", __FUNCTION__));
        return NULL;
    }

    auto ret = walls_.find(ID);
    if (ret == walls_.end())
    {
        ERROR_MSG(fmt::format("AirWall::getMapById failed that ID({}) not found!\n", ID));
        return NULL;
    }

    return ret->second;
}
}