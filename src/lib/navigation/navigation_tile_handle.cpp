// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#include "navigation_tile_handle.h"	
#include "navigation/navigation.h"
#include "resmgr/resmgr.h"
#include "thread/threadguard.h"
#include "math/math.h"
#include "JPS.h"

namespace KBEngine{	
NavTileHandle* NavTileHandle::pCurrNavTileHandle = NULL;
int NavTileHandle::currentLayer = 0;
NavTileHandle::MapSearchNode NavTileHandle::nodeGoal;
NavTileHandle::MapSearchNode NavTileHandle::nodeStart;
AStarSearch<NavTileHandle::MapSearchNode> NavTileHandle::astarsearch;
MATRIX_MAP_CACHE NavTileHandle::matrixMapCache;
CoordinateNode* pSearchFromNode = NULL;

#define DEBUG_LISTS 0
#define DEBUG_LIST_LENGTHS_ONLY 0

// Returns a random number [0..1)
/*static float frand()
{
//	return ((float)(rand() & 0xffff)/(float)0xffff);
	return (float)rand()/(float)RAND_MAX;
}*/


//-------------------------------------------------------------------------------------
NavTileHandle::NavTileHandle(bool dir) :
    NavigationHandle(),
    pMatrixMap(0),
    pLayerOne(NULL)
{
}

//-------------------------------------------------------------------------------------
NavTileHandle::NavTileHandle(const KBEngine::NavTileHandle & navTileHandle):
NavigationHandle(),
pMatrixMap(0)
{
	pMatrixMap = new MatrixMap::MatrixMap(*navTileHandle.pMatrixMap);
}

//-------------------------------------------------------------------------------------
NavTileHandle::~NavTileHandle()
{
	DEBUG_MSG(fmt::format("NavTileHandle::~NavTileHandle({1:p}, pTilemap={2:p}): ({0}) is destroyed!\n", 
		resPath, (void*)this, (void*)pMatrixMap));
	
	SAFE_RELEASE(pMatrixMap);
}

//-------------------------------------------------------------------------------------
int NavTileHandle::findStraightPath(CoordinateNode* pFromNode, int layer, const Position3D& start, const Position3D& end, std::vector<Position3D>& paths)
{
	setMapLayer(layer);
	pCurrNavTileHandle = this;
	
	pSearchFromNode = pFromNode;
	// Create a start state
	nodeStart.x = int(floor(start.x));
	nodeStart.y = int(floor(start.z));

	// Define the goal state
	nodeGoal.x = int(floor(end.x));
	nodeGoal.y = int(floor(end.z));

	JPS::PathVector pathVector;
	const MatrixMap::Position& originPos = pMatrixMap->originPos;

	bool found = JPS::findPath(pathVector, *this, nodeStart.x, nodeStart.y, nodeGoal.x, nodeGoal.y, originPos.first, originPos.second, pMatrixMap->width, pMatrixMap->height);
	pSearchFromNode = NULL;
	if (found)
	{
		JPS::PathVector::size_type vlen = pathVector.size();
		float fx = 0.0, fy = 0.0;
		for (JPS::PathVector::size_type i = 0;i < vlen; i++)
		{
			fx = pathVector[i].x;
			fy = pathVector[i].y;
			
			if (i == vlen - 1)
				paths.push_back(end);
			else
				paths.push_back(Position3D(fx+0.5, 0, fy+0.5));
		}
		if (vlen == 0)
			paths.push_back(end);
		return 0;
	}

	return 0;
}

//-------------------------------------------------------------------------------------
void swap(int& a, int& b) 
{
	int c = a;
	a = b;
	b = c;
}

//-------------------------------------------------------------------------------------
void NavTileHandle::bresenhamLine(const MapSearchNode& p0, const MapSearchNode& p1, std::vector<MapSearchNode>& results)
{
	bresenhamLine(p0.x, p0.y, p1.x, p1.y, results);
}

//-------------------------------------------------------------------------------------
void NavTileHandle::bresenhamLine(int x0, int y0, int x1, int y1, std::vector<MapSearchNode>& results)
{
	// Optimization: it would be preferable to calculate in
	// advance the size of "result" and to use a fixed-size array
	// instead of a list.

	bool steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap(x0, y0);
		swap(x1, y1);
	}
	if (x0 > x1) {
		swap(x0, x1);
		swap(y0, y1);
	}

	int deltax = x1 - x0;
	int deltay = abs(y1 - y0);
	int error = 0;
	int ystep;
	int y = y0;

	if (y0 < y1) ystep = 1; 
		else ystep = -1;

	for (int x = x0; x <= x1; x++) 
	{
		if (steep) 
			results.push_back(MapSearchNode(y, x));
		else 
			results.push_back(MapSearchNode(x, y));

		error += deltay;
		if (2 * error >= deltax) {
			y += ystep;
			error -= deltax;
		}
	}
}

bool isReverse(int x0, int y0, int x1, int y1)
{
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        return y0 > y1;
    }
    else
    {
        return x0 > x1;
    }
}

//-------------------------------------------------------------------------------------
int NavTileHandle::raycast(int layer, const Position3D& start, const Position3D& end, std::vector<Position3D>& hitPointVec)
{
	setMapLayer(layer);
	pCurrNavTileHandle = this;

	// Create a start state
	MapSearchNode nodeStart;
	nodeStart.x = int(floor(start.x));
	nodeStart.y = int(floor(start.z));

	// Define the goal state
	MapSearchNode nodeEnd;
	nodeEnd.x = int(floor(end.x));
	nodeEnd.y = int(floor(end.z));

	std::vector<MapSearchNode> vec;
	bresenhamLine(nodeStart, nodeEnd, vec);

    if (isReverse(nodeStart.x, nodeStart.y, nodeEnd.x, nodeEnd.y))
    {
        if (vec.size() > 0)
        {
            vec.erase(vec.end() - 1);
        }

        std::vector<MapSearchNode>::reverse_iterator iter = vec.rbegin();
        for (; iter != vec.rend(); iter++)
        {
            if (getMapNavCost((*iter).x, (*iter).y) == MatrixMap::NAV_COST_BLOCK)
                break;

            hitPointVec.push_back(Position3D(float((*iter).x), start.y, float((*iter).y)));
        }
    }
    else
    {
        if (vec.size() > 0)
        {
            vec.erase(vec.begin());
        }

        std::vector<MapSearchNode>::iterator iter = vec.begin();
        for (; iter != vec.end(); iter++)
        {
            if (getMapNavCost((*iter).x, (*iter).y) == MatrixMap::NAV_COST_BLOCK)
                break;

            hitPointVec.push_back(Position3D(float((*iter).x), start.y, float((*iter).y)));
        }
    }

	return 1;
}

//-------------------------------------------------------------------------------------
int NavTileHandle::findRandomPointAroundCircle(CoordinateNode* pFromNode, int layer, const Position3D& centerPos,
	std::vector<Position3D>& points, uint32 max_points, float maxRadius)
{
	setMapLayer(layer);
	pCurrNavTileHandle = this;

	int edgeLen = static_cast<int>(maxRadius*0.707);
	Position3D leftBottom = Position3D(int(centerPos.x-edgeLen), 0, int(centerPos.z-edgeLen));
	Position3D centerInt = Position3D(int(centerPos.x), 0, int(centerPos.z));
	std::vector<Position3D> posList;
	for (int i = 0; i < edgeLen*2+1;i++)
		for (int j = 0;j < edgeLen * 2 + 1;j++)
		{
			Position3D pos = Position3D(leftBottom.x + j, 0, leftBottom.z + i);
			Position3D diff = pos - centerInt;
			if (getMapNavCost(pos.x, pos.z) < MatrixMap::NAV_COST_BLOCK && fabs(diff.x)>=1 && fabs(diff.z)>=1 && !pFromNode->hasEntity(pos.x, pos.z))
				posList.push_back(pos);
		}

	uint32 posCnt = posList.size();
	if (posCnt <= max_points)
	{
		for (uint32 i = 0;i < posCnt;i++)
			points.push_back(posList[i]);
	}
	else
	{
		for (uint32 i = 0;i < max_points;i++)
		{
			int idx = rand() % posCnt;
			points.push_back(posList[idx]);
		}
	}
	return (int)points.size();
}

//-------------------------------------------------------------------------------------
NavigationHandle* NavTileHandle::create(std::string resPath, const std::map< int, std::string >& params)
{
	if(resPath == "")
		return NULL;
	
	std::string path;
	
	if(params.size() == 0)
	{
		path = resPath;
		path = Resmgr::getSingleton().matchPath(path);
		wchar_t* wpath = strutil::char2wchar(path.c_str());
		std::wstring wspath = wpath;
		free(wpath);
			
		std::vector<std::wstring> results;
		Resmgr::getSingleton().listPathRes(wspath, L"tmx", results);

		if(results.size() == 0)
		{
			ERROR_MSG(fmt::format("NavTileHandle::create: path({}) not found tmx.!\n", 
				Resmgr::getSingleton().matchRes(path)));

			return NULL;
		}
					
		char* cpath = strutil::wchar2char(results[0].c_str());
		path = cpath;
		free(cpath);
	}
	else
	{
		path = Resmgr::getSingleton().matchRes(params.begin()->second);
	}
	
	return _create(path);
}

MatrixMap::MatrixMap* NavTileHandle::_loadMapData(const std::string& path)
{
	MatrixMap::MatrixMap *map = new MatrixMap::MatrixMap(path);
	DEBUG_MSG(fmt::format("NavTileHandle::create: ({})\n", path));
	DEBUG_MSG(fmt::format("\t==> map Width : {}\n", map->width));
	DEBUG_MSG(fmt::format("\t==> map Height : {}\n", map->height));
	DEBUG_MSG(fmt::format("\t==> map size : {} \n", map->mapDataSize));
	DEBUG_MSG(fmt::format("\t==> map origin : ({},{}) \n", map->originPos.first, map->originPos.second));
	return map;
}

//-------------------------------------------------------------------------------------
NavTileHandle* NavTileHandle::_create(const std::string& res)
{
	MatrixMap::MatrixMap* map = _loadMapData(res);
	
	NavTileHandle* pNavTileHandle = new NavTileHandle(true);
	pNavTileHandle->pMatrixMap = map;
	return pNavTileHandle;
}

NavigationHandle* NavTileHandle::createFromMergingMultiplePath(std::vector<std::string>& paths, std::vector<int>& rotations, std::vector< MatrixMap::Position >& positions)
{
	std::vector<std::string>::size_type len = paths.size();
	if (rotations.size() != len || positions.size() != len)
	{
		ERROR_MSG("NavTileHandle::createFromMergingMultiplePath: paths/rotations/positions error!");
		return NULL;
	}

	for (std::vector<std::string>::const_iterator it = paths.begin();it != paths.end();it++)
	{
		if (matrixMapCache.find(*it) == matrixMapCache.end())
		{
			std::string resPath = Resmgr::getSingleton().matchRes(*it);
			if(resPath==*it)
			{
				ERROR_MSG(fmt::format("NavTileHandle::createFromMergingMultiplePath: path {} {} error!", it->c_str(), resPath.c_str()));
				return NULL;
			}
			MatrixMap::MatrixMap* map = _loadMapData(resPath);
			if (map)
				matrixMapCache[*it] = map;
			else
			{ 
				ERROR_MSG(fmt::format("NavTileHandle::createFromMergingMultiplePath2: path {} {} error!", it->c_str(), resPath.c_str()));
				return NULL;
			}
				
		}
	}
	
	std::vector<MatrixMap::MatrixMap*> maps;
	std::string joinedPath = "";
	for (std::vector<std::string>::const_iterator it = paths.begin();it != paths.end();it++)
	{
		maps.push_back(matrixMapCache[*it]);
		joinedPath += *it;
	}

	DEBUG_MSG(fmt::format("begin to merge tmx maps: {}!!\n", joinedPath.c_str()));

	MatrixMap::MatrixMap* map = new MatrixMap::MatrixMap(maps, positions, rotations);

	DEBUG_MSG(fmt::format("\t==> map Width : {}\n", map->width));
	DEBUG_MSG(fmt::format("\t==> map Height : {}\n", map->height));
	DEBUG_MSG(fmt::format("\t==> data size : {}\n", map->mapDataSize));
	DEBUG_MSG(fmt::format("\t==> map origin : ({},{}) \n", map->originPos.first, map->originPos.second));

	NavTileHandle* pNavTileHandle = new NavTileHandle(true);
	pNavTileHandle->pMatrixMap = map;
	return pNavTileHandle;
}

bool NavTileHandle::setMapTileNavCost(int layerIndex, int x, int y, MatrixMap::MapDataVal cost)
{
	return pMatrixMap->setMapData(x, y, cost);
}

bool NavTileHandle::setMapTileOrigin(int x, int y)
{
	pMatrixMap->originPos.first = x;
	pMatrixMap->originPos.second = y;
	return true;	
}

//-------------------------------------------------------------------------------------
int NavTileHandle::getMapNavCost(int x, int y) const
{
	if (pSearchFromNode)
	{
		int fromx = static_cast<int>(floor(pSearchFromNode->x()));
		int fromz = static_cast<int>(floor(pSearchFromNode->z()));
		if(abs(fromx-x)<=1 && abs(fromz-y)<=1 && pSearchFromNode->hasEntity(x, y))
			return MatrixMap::NAV_COST_BLOCK;
	}

    if (pLayerOne)
    {
        MatrixMap::MatrixMapPtr mapPtr = pLayerOne->getMatrixMap(x, y);
        if (mapPtr)
        {
            int cost = mapPtr->getMapNavigationCost(x, y);
            if (cost == MatrixMap::NAV_COST_UNKNOW)
                return MatrixMap::NAV_COST_BLOCK;
            else
                return cost;
        }
    }

	int cost = pMatrixMap->getMapNavigationCost(x, y);
	if (cost == MatrixMap::NAV_COST_UNKNOW)
		return MatrixMap::NAV_COST_BLOCK;
	else
		return cost;
}

//-------------------------------------------------------------------------------------
bool NavTileHandle::MapSearchNode::IsSameState(MapSearchNode &rhs)
{
	// same state in a maze search is simply when (x,y) are the same
	if( (x == rhs.x) &&
		(y == rhs.y) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

//-------------------------------------------------------------------------------------
void NavTileHandle::MapSearchNode::PrintNodeInfo()
{
	char str[100];
	sprintf( str, "NavTileHandle::MapSearchNode::printNodeInfo(): Node position : (%d,%d)\n", 
		x, y);
	
	DEBUG_MSG(str);
}

//-------------------------------------------------------------------------------------
// Here's the heuristic function that estimates the distance from a Node
// to the Goal. 

float NavTileHandle::MapSearchNode::GoalDistanceEstimate(MapSearchNode &nodeGoal)
{
	float xd = abs(float(((float)x - (float)nodeGoal.x)));
	float yd = abs(float(((float)y - (float)nodeGoal.y)));

	return xd + yd;
}

//-------------------------------------------------------------------------------------
bool NavTileHandle::MapSearchNode::IsGoal(MapSearchNode &nodeGoal)
{

	if( (x == nodeGoal.x) &&
		(y == nodeGoal.y) )
	{
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------
// This generates the successors to the given Node. It uses a helper function called
// AddSuccessor to give the successors to the AStar class. The A* specific initialisation
// is done for each node internally, so here you just set the state information that
// is specific to the application
bool NavTileHandle::MapSearchNode::GetSuccessors(AStarSearch<MapSearchNode> *astarsearch, MapSearchNode *parent_node, bool doAdd)
{
	int parent_x = -1; 
	int parent_y = -1; 

	if( parent_node )
	{
		parent_x = parent_node->x;
		parent_y = parent_node->y;
	}
	//FILE *fo = fopen("searchLog.txt", "at+");
	//fprintf(fo, "\n\nzt: search start %d (%d,%d)  (%d,%d)\n", NavTileHandle::pCurrNavTileHandle->direction8(), parent_x, parent_y, x, y);
	MapSearchNode NewNode;
	bool found = false;
	int offsetX = abs(x - nodeStart.x);
	int offsetY = abs(y - nodeStart.y);
	bool isFar = !(offsetX <= 1 && offsetY <= 1);
	// push each possible move except allowing the search to go backwards

	if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x-1, y ) < MatrixMap::NAV_COST_BLOCK)
		&& (!parent_node || !((parent_x == x-1) && (parent_y == y)))
		&& (isFar||!pSearchFromNode->hasEntity(x - 1, y))
	  ) 
	{
		if (doAdd)
		{
			NewNode = MapSearchNode(x - 1, y);
			astarsearch->AddSuccessor(NewNode);
		}
		found = true;
		//fprintf(fo, "zt: search add1 (%d,%d)\n", x - 1, y);
	}	

	if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x, y-1 ) < MatrixMap::NAV_COST_BLOCK)
		&& (!parent_node || !((parent_x == x) && (parent_y == y-1)))
		&& (isFar || !pSearchFromNode->hasEntity(x, y - 1))
	  ) 
	{
		if (doAdd)
		{
			NewNode = MapSearchNode(x, y - 1);
			astarsearch->AddSuccessor(NewNode);
		}
		found = true;
		//fprintf(fo, "zt: search add2 (%d,%d)\n", x, y - 1);
	}	

	if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x+1, y ) < MatrixMap::NAV_COST_BLOCK)
		&& (!parent_node || !((parent_x == x+1) && (parent_y == y)))
		&& (isFar || !pSearchFromNode->hasEntity(x + 1, y))
	  ) 
	{
		if (doAdd)
		{
			NewNode = MapSearchNode(x + 1, y);
			astarsearch->AddSuccessor(NewNode);
		}
		found = true;
		//fprintf(fo, "zt: search add3 (%d,%d)\n", x + 1, y);
	}
		
	if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x, y+1 ) < MatrixMap::NAV_COST_BLOCK)
		&& (!parent_node || !((parent_x == x) && (parent_y == y+1)))
		&& (isFar || !pSearchFromNode->hasEntity(x, y + 1))
		)
	{
		if (doAdd)
		{
			NewNode = MapSearchNode(x, y + 1);
			astarsearch->AddSuccessor(NewNode);
		}
		found = true;
		//fprintf(fo, "zt: search add4 (%d,%d)\n", x, y + 1);
	}	
	//fclose(fo);
	// 如果是8方向移动
	if(true)
	{
		if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x + 1, y + 1 ) < MatrixMap::NAV_COST_BLOCK)
			&& (!parent_node || !((parent_x == x + 1) && (parent_y == y + 1)))
			&& (isFar || !pSearchFromNode->hasEntity(x + 1, y + 1))
		  ) 
		{
			if (doAdd)
			{
				NewNode = MapSearchNode(x + 1, y + 1);
				astarsearch->AddSuccessor(NewNode);
			}
			found = true;
		}	

		if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x + 1, y-1 ) < MatrixMap::NAV_COST_BLOCK)
			&& (!parent_node || !((parent_x == x + 1) && (parent_y == y-1)))
			&& (isFar || !pSearchFromNode->hasEntity(x + 1, y - 1))
		  ) 
		{
			if (doAdd)
			{
				NewNode = MapSearchNode(x + 1, y - 1);
				astarsearch->AddSuccessor(NewNode);
			}
			found = true;
		}	

		if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x - 1, y + 1) < MatrixMap::NAV_COST_BLOCK)
			&& (!parent_node || !((parent_x == x - 1) && (parent_y == y + 1)))
			&& (isFar || !pSearchFromNode->hasEntity(x - 1, y + 1))
		  ) 
		{
			if (doAdd)
			{
				NewNode = MapSearchNode(x - 1, y + 1);
				astarsearch->AddSuccessor(NewNode);
			}
			found = true;
		}	

		if( (NavTileHandle::pCurrNavTileHandle->getMapNavCost( x - 1, y - 1 ) < MatrixMap::NAV_COST_BLOCK)
			&& (!parent_node || !((parent_x == x - 1) && (parent_y == y - 1)))
			&& (isFar || !pSearchFromNode->hasEntity(x - 1, y - 1))
			)
		{
			if (doAdd)
			{
				NewNode = MapSearchNode(x - 1, y - 1);
				astarsearch->AddSuccessor(NewNode);
			}
			found = true;
		}	
	}
	if(doAdd)
		return true;
	return found;
}

//-------------------------------------------------------------------------------------
// given this node, what does it cost to move to successor. In the case
// of our map the answer is the map terrain value at this node since that is 
// conceptually where we're moving
float NavTileHandle::MapSearchNode::GetCost( MapSearchNode &successor )
{
	/*
		一个tile寻路的性价比
		每个tile都可以定义从0~5的性价比值， 值越大性价比越低
		比如： 前方虽然能够通过但是前方是泥巴路， 行走起来非常费力， 
		或者是前方为高速公路， 行走非常快。
	*/
	
	/*
		计算代价：
		通常用公式表示为：f = g + h.
		g就是从起点到当前点的代价.
		h是当前点到终点的估计代价，是通过估价函数计算出来的.

		对于一个不再边上的节点，他周围会有8个节点，可以看成他到周围8个点的代价都是1。
		精确点，到上下左右4个点的代价是1，到左上左下右上右下的1.414就是“根号2”，这个值就是前面说的g.
		2.8  2.4  2  2.4  2.8
		2.4  1.4  1  1.4  2.4
		2    1    0    1    2
		2.4  1.4  1  1.4  2.4
		2.8  2.4  2  2.4  2.8
	*/

	return (float) NavTileHandle::pCurrNavTileHandle->getMapNavCost( x, y );

}
//-------------------------------------------------------------------------------------
}

