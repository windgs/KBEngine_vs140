#ifndef __AIR_WALL_H__
#define __AIR_WALL_H__
#include "common/singleton.h"
#include "common/common.h"
#include "MatrixMap.h"

namespace AirWall
{
class AirWall : public KBEngine::Singleton<AirWall>
{
public:
    AirWall();
    virtual ~AirWall();
    bool loadAllAirWallFromPath(const std::string & resPath);
    bool isLoad();
    MatrixMap::MatrixMapPtr getMapById(const std::string &ID);
private:
    KBEUnordered_map<std::string, MatrixMap::MatrixMapPtr> walls_;
    bool isLoad_;
};
}
#endif
