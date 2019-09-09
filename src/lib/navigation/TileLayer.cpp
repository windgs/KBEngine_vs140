#include "TileLayer.h"
#include "helper/debug_helper.h"
#include <stdio.h>
using KBEngine::DebugHelper;

namespace TileLayer
{

    std::string Layer::getIdentifier(int x, int y)
    {
		std::ostringstream oss;
		oss << x << "+" << y;
        return oss.str();
    }

    bool Layer::addMatrixMap(int x, int y, int width, int height, std::vector<int>& maps)
    {
        std::string id = getIdentifier(x, y);
        if (mLayer.find(id) != mLayer.end())
        {
            ERROR_MSG(fmt::format("Layer::addMatrixMap failed! the x:{}, y:{} has existed.", x, y));
            return false;
        }

        auto mapPtr = std::make_shared<MatrixMap::MatrixMap>(x, y, width, height, maps);
        mLayer.insert(std::make_pair(id, mapPtr));
        return true;
    }

    bool Layer::addMatrixMapWithMap(MatrixMap::MatrixMapPtr mapPtr)
    {
		if (!mapPtr)
		{
			ERROR_MSG(fmt::format("Layer::addMatrixMapWithMap failed! mapPtr==NULL."));
			return false;
		}
        std::string id = getIdentifier(mapPtr->originPos.first, mapPtr->originPos.second);
        if (mLayer.find(id) != mLayer.end())
        {
            ERROR_MSG(fmt::format("Layer::addMatrixMapWithMap failed! the originPos.first:{}, originPos.second:{} has existed.", mapPtr->originPos.first, mapPtr->originPos.second));
            return false;
        }

        mLayer.insert(std::make_pair(id, mapPtr));
        return true;
    }

    bool Layer::removeMatrixMap(int x, int y)
    {
        std::string id = getIdentifier(x, y);
        auto iter = mLayer.find(id);
        if (iter == mLayer.end())
        {
            ERROR_MSG(fmt::format("Layer::removeMatrixMap failed that x:{}, y:{} not existed!", x, y));
            return false;
        }
        mLayer.erase(iter);
        return true;
    }

    MatrixMap::MatrixMapPtr Layer::getMatrixMap(int x, int y) const
    {
        for (auto mapIter : mLayer)
        {
            if (mapIter.second->isPosInAbs(x, y))
            {
                return mapIter.second;
            }
        }
        return NULL;
    }
}