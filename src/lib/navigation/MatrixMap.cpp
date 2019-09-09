#include "MatrixMap.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdio.h>


namespace MatrixMap
{
	MatrixMap::MatrixMap(const std::string& fullPath)
	{
		printf("MatrixMap::MatrixMap: map = 【%s】\n", fullPath.c_str());
		std::string fileDir = "";
		int lastSlash = fullPath.find_last_of("/");

		// Get the directory of the file using substring.
		if (lastSlash > 0)
		{
			fileDir = fullPath.substr(0, lastSlash + 1);
		}
		else
		{
			fileDir = "";
		}

		std::ifstream dataFile(fullPath);
		std::string line="";

// 		std::getline(dataFile, line);
// 		std::istringstream ixy(line);
// 		int x = 0, y = 0;
// 		ixy >> x >> y;
// 		originPos.first = x;
// 		originPos.second = y;
			
		std::getline(dataFile, line);
		std::istringstream iis(line);
		iis >> width >> height;
		if (width > 0 && height > 0)
		{
			mapDataSize = calcMapDataSize();
			mapData = new MapDataVal[mapDataSize];
			memset(mapData, NAV_COST_UNKNOW, mapDataSize);
			identifier = fullPath;
		}
		else
		{
			printf("MatrixMap::MatrixMap: map data size error, %s %d %d\n", fullPath.c_str(), width, height);
			return;
		}

		for(int z=height-1; std::getline(dataFile, line)&&z>=0; z--)
		{
			uint16_t lineLen = uint16_t(line.size());
			
			int x = 0;
			const char* lineData = line.c_str();
			const static char sepratorChar = ' ';
			for (uint16_t i = 0;i < lineLen; i++)
			{
				char c = lineData[i];
				if (c == sepratorChar || c=='\r'||c=='\n')
					continue;
				if (c >= '0' && c <= '9')
				{
					setMapData(x++, z, c - '0');
				}
				else
				{
					printf("MatrixMap::MatrixMap: map data val error: %s %d %d |%c| %d\n", fullPath.c_str(), x, z, c, int(c));
					return;
				}
					
			}
		}
		dataFile.close();
	}

    MatrixMap::MatrixMap(int x, int y, int width, int height, std::vector<int>& maps)
        :width(width)
        ,height(height)
        ,originPos(std::make_pair(0, 0))
    {
        mapDataSize = calcMapDataSize();
        mapData = new MapDataVal[mapDataSize];
        memset(mapData, NAV_COST_UNKNOW, mapDataSize);
        for (int i = 0; i < width; i++)
        {
            for (int j = 0; j < height; j++)
            {
                setMapData(i, j, maps[getIndex(i, j, width, height)]);
            }
        }

		std::ostringstream oss;
		oss << x << "+" << y;
        identifier = oss.str();

        originPos.first = x;
        originPos.second = y;
    }

    MatrixMap::MatrixMap(int x, int y, int width, int height, uint8_t *map)
        :width(width)
        , height(height)
        , originPos(std::make_pair(x, y))
    {
        mapDataSize = calcMapDataSize();
        mapData = new MapDataVal[mapDataSize];
        memcpy(mapData, map, mapDataSize);

		std::ostringstream oss;
		oss << x << "+" << y;
		identifier = oss.str();
    }

	MatrixMap::MatrixMap(const MatrixMap& map)
		:width(map.width)
		,height(map.height)
		,identifier(map.identifier)
		,originPos(map.originPos)
		, mapDataSize(map.mapDataSize)
	{
		mapData = new MapDataVal[mapDataSize];
		memcpy(mapData, map.mapData, mapDataSize);
		printf("MatrixMap::MatrixMap copy constructor: %s\n", map.identifier.c_str());
	}

	MatrixMap::MatrixMap(const std::vector<MatrixMap*> & maps, std::vector<Position>& posList, std::vector<int>& rotations)
		:width(0)
		,height(0)
		,identifier()
	{
		size_t nMaps = maps.size();
		if (nMaps != posList.size() || nMaps != rotations.size() || nMaps == 0)
		{
			printf("contruct maps error: %lu %lu %lu\n", nMaps, posList.size(), rotations.size());
			return;
		}
		originPos = posList[0];
		for (std::vector<Position>::iterator it = posList.begin(); it != posList.end(); it++)
		{
			originPos.first = std::min(originPos.first, static_cast<int>(it->first));
			originPos.second = std::min(originPos.second, static_cast<int>(it->second));
		}

		for (size_t i = 0;i < nMaps; i++)
		{
			width += maps[i]->width;
			height += maps[i]->height;
		}

		mapDataSize = calcMapDataSize();
		mapData = new MapDataVal[mapDataSize];
		memset(mapData, NAV_COST_UNKNOW, mapDataSize);

		for (size_t i = 0;i < nMaps;i++)
		{
			identifier += maps[i]->identifier + "," + "("+std::to_string(posList[i].first)+","+std::to_string(posList[i].second)+"),"+std::to_string(rotations[i])+";";
			for (int z = 0;z < maps[i]->height;z++)
			{
				for (int x = 0;x < maps[i]->width;x++)
				{
					Position rotatedPos = maps[i]->getRotatePos(x, z, rotations[i]);
					setMapData(rotatedPos.first + posList[i].first, rotatedPos.second + posList[i].second, maps[i]->getMapNavigationCost(x, z));
				}
			}
		}
	}

	MatrixMap::~MatrixMap()
	{
		delete [] mapData;
	}

	//以矩阵左下角为原点，向右向上分别为x，z的正方向，把矩阵顺时针旋转rotation欧拉角后，矩阵中各个坐标转换为旋转后的坐标
	Position MatrixMap::getRotatePos(uint16_t x, uint16_t z, int rotation)
	{
		uint16_t x1, z1;
		switch (rotation)
		{
		case 0:
			return std::make_pair(x, z);
		case 90:
			//x'=x cos(theta) + y sin(theta)
			//z'=y cos(theta) – x sin(theta)+width-1
			//对于一个格子，用左下角坐标表示，旋转90度后，左下变左上，所以z需要减1
			x1 = z;
			z1 = -x + width-1;
			return std::make_pair(x1, z1);
		case 180:
			//x'=x cos(theta) + y sin(theta)+width-1
			//z'=y cos(theta) – x sin(theta)+height-1
			x1 = -x + width-1;
			z1 = -z + height-1;
			return std::make_pair(x1, z1);
		case 270:
			//x'=x cos(theta) + y sin(theta)+height-1
			//z'=y cos(theta) – x sin(theta)
			x1 = -z + height-1;
			z1 = x;
			return std::make_pair(x1, z1);
		default:
			break;
		}
		return std::make_pair(0, 0);
	}

	size_t MatrixMap::calcMapDataSize() const
	{
		return static_cast<size_t>(ceil(width*height*NEED_BITS_COUNT / (8.0 * sizeof(MapDataVal))));
	}

	size_t MatrixMap::setMapData(int x1, int z1, MapDataVal val)
	{
		int x = x1 - originPos.first;
		int z = z1 - originPos.second;
		int bitCnt = (x + z*width)*NEED_BITS_COUNT;
		int index = bitCnt / (8*sizeof(MapDataVal));
		int offset = bitCnt % (8*sizeof(MapDataVal));
		if (index >=int(mapDataSize))
		{
			printf("setMapData invalid data index: (%d,%d), (%d,%d), %d %d %d\n", x, z, originPos.first, originPos.second, bitCnt, index, offset);
			return 0;
		}
		mapData[index] |= (val << offset);
		return index;
	}

    size_t MatrixMap::resetMapData(int x1, int z1, MapDataVal val)
    {
        int x = x1 - originPos.first;
        int z = z1 - originPos.second;
        int bitCnt = (x + z * width)*NEED_BITS_COUNT;
        int index = bitCnt / (8 * sizeof(MapDataVal));
        int offset = bitCnt % (8 * sizeof(MapDataVal));
        if (index >= int(mapDataSize))
        {
            printf("resetMapData invalid data index: (%d,%d), (%d,%d), %d %d %d\n", x, z, originPos.first, originPos.second, bitCnt, index, offset);
            return 0;
        }
        mapData[index] &= (~(val << offset));
        return index;
    }    

	int MatrixMap::getMapNavigationCost(int x1, int z1)
	{
		static size_t mapDataBits = 8 * sizeof(MapDataVal);
		int x = x1 - originPos.first;
		int z = z1 - originPos.second;
		if (!isValidPos(x, z))
			return NAV_COST_BLOCK;

		int bitCnt = (x + z * width)*NEED_BITS_COUNT;
		int index = bitCnt / mapDataBits;
		int offset = bitCnt % mapDataBits;

		if (index >= int(mapDataSize))
		{
			printf("getMapNavigationCost invalid data index: (%d,%d), (%d,%d), %d %d %d\n", x, z, originPos.first, originPos.second, bitCnt, index, offset);
			return 0;
		}

		MapDataVal val = (1 << NEED_BITS_COUNT) - 1;
		val &= (mapData[index]>>offset);
		
		return static_cast<int>(val);
	}

    bool MatrixMap::isPosInAbs(int x, int z) const
    {
        x -= originPos.first;
        z -= originPos.second;
        return (x >= 0 && x < width && z >= 0 && z < height);
    }

	bool MatrixMap::isValidPos(int x, int z) const
	{
		return (x >= 0 && x < width && z >= 0 && z < height);
	}
};
