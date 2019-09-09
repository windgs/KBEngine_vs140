// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#ifndef KBE_NAVIGATETILEHANDLE_H
#define KBE_NAVIGATETILEHANDLE_H

#include "navigation/navigation_handle.h"

#include "stlastar.h"
#include "MatrixMap.h"

namespace KBEngine{
	typedef std::map<std::string, MatrixMap::MatrixMap* > MATRIX_MAP_CACHE;

class NavTileHandle : public NavigationHandle
{
public:
	static NavTileHandle* pCurrNavTileHandle;
	static int currentLayer;

	static void setMapLayer(int layer)
	{ 
		currentLayer = layer; 
	}

	class MapSearchNode
	{
	public:
		int x;	 // the (x,y) positions of the node
		int y;	
		

		MapSearchNode() { x = y = 0; }
		MapSearchNode(int px, int py) {x = px; y = py; }

		float GoalDistanceEstimate( MapSearchNode &nodeGoal );
		bool IsGoal( MapSearchNode &nodeGoal );
		bool GetSuccessors( AStarSearch<MapSearchNode> *astarsearch, MapSearchNode *parent_node, bool doAdd=true );
		float GetCost( MapSearchNode &successor );
		bool IsSameState( MapSearchNode &rhs );

		void PrintNodeInfo(); 
	};
	
	static MapSearchNode nodeGoal, nodeStart;
	static AStarSearch<NavTileHandle::MapSearchNode> astarsearch;
	static MATRIX_MAP_CACHE matrixMapCache;

public:
	NavTileHandle(bool dir);
	NavTileHandle(const KBEngine::NavTileHandle & navTileHandle);

	virtual ~NavTileHandle();

	int findStraightPath(CoordinateNode* pFromNode, int layer, const Position3D& start, const Position3D& end, std::vector<Position3D>& paths);

	virtual int findRandomPointAroundCircle(CoordinateNode* pFromNode, int layer, const Position3D& centerPos,
		std::vector<Position3D>& points, uint32 max_points, float maxRadius);

	int raycast(int layer, const Position3D& start, const Position3D& end, std::vector<Position3D>& hitPointVec);

	virtual NavigationHandle::NAV_TYPE type() const{ return NAV_TILE; }

	static NavigationHandle* create(std::string resPath, const std::map< int, std::string >& params);
	static NavigationHandle* createFromMergingMultiplePath(std::vector<std::string>& paths, std::vector<int>& rotations, std::vector< MatrixMap::Position >& positions);
	static NavTileHandle* _create(const std::string& res);
	static MatrixMap::MatrixMap* _loadMapData(const std::string& path);
	virtual bool setMapTileNavCost(int layerIndex, int x, int y, MatrixMap::MapDataVal cost);
	virtual bool setMapTileOrigin(int x, int y);
    virtual bool setLayerOne(TileLayer::Layer *pLayer)
    {
        pLayerOne = pLayer;
        return true;
    };
	
	virtual int getMapNavCost(int x, int y) const;

	void bresenhamLine(const MapSearchNode& p0, const MapSearchNode& p1, std::vector<MapSearchNode>& results);
	void bresenhamLine(int x0, int y0, int x1, int y1, std::vector<MapSearchNode>& results);

	bool operator()(int x, int y) const
	{
		int res = getMapNavCost(x, y);
		return res< MatrixMap::NAV_COST_BLOCK;
	}

public:
	MatrixMap::MatrixMap *pMatrixMap;
    TileLayer::Layer *pLayerOne;
};

}
#endif // KBE_NAVIGATETILEHANDLE_H

