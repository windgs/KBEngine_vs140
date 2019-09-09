#ifndef __TILE_LAYER_H__
#define __TILE_LAYER_H__
#include <map>
#include "MatrixMap.h"
namespace TileLayer
{
    class Layer
    {
    private:
        std::map<std::string, MatrixMap::MatrixMapPtr> mLayer;

    public:
        Layer() :mLayer() {};
        std::string getIdentifier(int x, int y);
        bool addMatrixMap(int x, int y, int width, int height, std::vector<int> &maps);
        bool addMatrixMapWithMap(MatrixMap::MatrixMapPtr mapPtr);
        bool removeMatrixMap(int x, int y);
        MatrixMap::MatrixMapPtr getMatrixMap(int x, int y) const;
    };
}
#endif