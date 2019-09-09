#ifndef __MATRIX_MAP_H__
#define __MATRIX_MAP_H__

#include <vector>
#include <string>
#include <memory>

namespace MatrixMap
{
	static const int NEED_BITS_COUNT = 1;
	typedef uint8_t MapDataVal;
	typedef std::pair<int, int> Position;

	enum NavigationCost {
		NAV_COST_UNKNOW = 0,
		NAV_COST_PASS =1,
		NAV_COST_BLOCK = 9
	};

	class MatrixMap
	{
	public:
		MatrixMap(const std::string& path);
        MatrixMap(int x, int y, int width, int height, std::vector<int>& maps);
        MatrixMap(int x, int y, int width, int height, uint8_t *map);
		MatrixMap(const MatrixMap& map);
		MatrixMap(const std::vector<MatrixMap*> & maps, std::vector<Position>& posList, std::vector<int>& rotations);
		~MatrixMap();

		uint16_t width;
		uint16_t height;
		std::string identifier;
		std::pair<int, int> originPos;
		size_t mapDataSize;
		MapDataVal *mapData;

		int getMapNavigationCost(int x, int z);
        bool isPosInAbs(int x, int z) const;
		size_t calcMapDataSize() const;
		size_t setMapData(int x, int z, MapDataVal val);
        size_t resetMapData(int x1, int z1, MapDataVal val);
		bool isValidPos(int x, int z) const;
        static inline int getIndex(int x, int y, int width, int height) 
        {
            return (height - y - 1) * width + x;
        };
		void setOriginPos(int x, int z) { originPos = std::make_pair(x, z); }

	private:
		Position getRotatePos(uint16_t x, uint16_t z, int rotation);
	};

    typedef std::shared_ptr<MatrixMap> MatrixMapPtr;
};

#endif

