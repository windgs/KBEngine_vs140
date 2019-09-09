//-----------------------------------------------------------------------------
// TmxMap.cpp
//
// Copyright (c) 2010-2014, Tamir Atias
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL TAMIR ATIAS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Tamir Atias
//-----------------------------------------------------------------------------
#include <tinyxml.h>
#include <stdio.h>
#include <cmath>

#include "TmxMap.h"
#include "TmxTileset.h"
#include "TmxLayer.h"
#include "TmxObjectGroup.h"
#include "TmxImageLayer.h"

#ifdef USE_SDL2_LOAD
#include <SDL.h>
#endif

using std::vector;
using std::string;

namespace Tmx 
{
	Map::Map() 
		: file_name()
		, file_path()
		, version(0.0)
		, orientation(TMX_MO_ORTHOGONAL)
		, width(0)
		, height(0)
		, tile_width(0)
		, tile_height(0)
		, layers()
		, object_groups()
		, tilesets() 
		, topLeft(0,0)
		, has_error(false)
		, error_code(0)
		, error_text()
	{}

	Map::Map(const Map &_map)
		: file_name(_map.file_name)
		, file_path(_map.file_path)
		, version(_map.version)
		, orientation(_map.orientation)
		, width(_map.width)
		, height(_map.height)
		, tile_width(_map.tile_width)
		, tile_height(_map.tile_height)
		, layers()
		, object_groups()
		, tilesets() 
		, topLeft(_map.topLeft)
		, has_error(_map.has_error)
		, error_code(_map.error_code)
		, error_text(_map.error_text)
	{
		std::vector< Tmx::Layer* >::const_iterator iter = _map.layers.begin();
		for(; iter != _map.layers.end(); iter++)
		{
			layers.push_back(new Tmx::Layer(*(*iter)));
		}

		std::vector< Tmx::Tileset* >::const_iterator iter1 = _map.tilesets.begin();
		for(; iter1 != _map.tilesets.end(); iter1++)
		{
			tilesets.push_back(new Tmx::Tileset(*(*iter1)));
		}

	}

	Map::Map(const std::vector<Map*>& maps, std::vector<int>& rotations, std::vector< std::pair<int, int> >& inPositions)
		: file_name()
		, file_path()
		, orientation(TMX_MO_ORTHOGONAL)
		, width(0)
		, height(0)
		, tile_width(0)
		, tile_height(0)
		, layers()
		, object_groups()
		, tilesets()
		, topLeft(0, 0)
		, has_error(false)
		, error_code(0)
		, error_text()
	{
		int len = maps.size();
		if (len <= 0 || int(rotations.size()) != len || int(inPositions.size()) != len)
		{
			printf("error: invalid map args: %d\n", len);
			for (int i = 0;i < len;i++)
			{
				const Map* subMap = maps[i];
				printf("error: invalid map args1: %s\n", subMap->file_name.c_str());
			}
			return;
		}
		std::vector< std::pair<int, int> > positions;
		for (int j = 0;j < len;j++)
		{
			int inX = inPositions[j].first;
			int inY = inPositions[j].second;
			positions.push_back(std::make_pair(inX, -inY));
		}
			
		topLeft.first = positions[0].first;
		topLeft.second = positions[0].second;

		printf("topLeft: %d %d %d %d\n", topLeft.first, topLeft.second, inPositions[0].first, inPositions[0].second);

		for (int i = 0;i < len;i++)
		{
			const Map* subMap = maps[i];
			width += subMap->width;
			height += subMap->height;

			if (positions[i].first < topLeft.first)
				topLeft.first = positions[i].first;
			if (positions[i].second < topLeft.second)
				topLeft.second = positions[i].second;
		}
		printf("topLeft1: %d %d\n", topLeft.first, topLeft.second);
		width = int(ceil(width / 100.0)) * 100;
		height = int(ceil(height / 100.0)) * 100;
		for (int i = 0;i < len;i++)
		{
			const Map* subMap = maps[i];

			if (orientation != 0 && subMap->orientation != orientation)
			{
				has_error = true;
				error_text = "inconsistent map orientation!!!";
				break;
			}
			if (tile_width != 0 && subMap->tile_width != tile_width)
			{
				has_error = true;
				error_text = "inconsistent map tile_width!!!";
				break;
			}
			if (tile_height != 0 && subMap->tile_height != tile_height)
			{
				has_error = true;
				error_text = "inconsistent map tile_height!!!";
				break;
			}
			if (subMap->layers.size() > 1)
			{
				has_error = true;
				error_text = "multiple layers merging does not support now!!!";
				break;
			}
			if (subMap->object_groups.size() > 0 || subMap->image_layers.size() > 0)
			{
				has_error = true;
				error_text = "object_groups/image_layers merging does not support now!!!";
				break;
			}

			file_name = file_name + "|" + subMap->file_name;
			file_path = file_path + "|" + subMap->file_path;
			version = subMap->version;
			orientation = subMap->orientation;
			tile_width = subMap->tile_width;
			tile_height = subMap->tile_height;

			const std::map<std::string, std::string>& propList = subMap->properties.GetList();
			for (std::map<std::string, std::string>::const_iterator pit = propList.begin();pit != propList.end();pit++)
				properties.SetProperty(pit->first, pit->second);

			int tilesetIndexOffset = tilesets.size();
			if (tilesets.empty())
			{
				for (std::vector< Tmx::Tileset* >::size_type j = 0;j < subMap->tilesets.size();j++)
				{
					Tmx::Tileset* tileset = new Tmx::Tileset(*(subMap->tilesets[j]));
					tilesets.push_back(tileset);
				}
			}
			else
			{
				for (std::vector< Tmx::Tileset* >::size_type j = 0;j < subMap->tilesets.size();j++)
				{
					Tmx::Tileset* tileset = new Tmx::Tileset(*(subMap->tilesets[j]));
					const Tmx::Tileset* tilesetAtBack = tilesets.back();
					tileset->SetFirstGid(tilesetAtBack->GetFirstGid() + tilesetAtBack->GetTilesCnt());
					tilesets.push_back(tileset);
				}
			}

			if (layers.empty())
			{
				Tmx::Layer* layer = new Tmx::Layer(this, "mergedLayer", width, height, 1.0, true, \
					0, Tmx::PropertySet(), Tmx::TMX_ENCODING_CSV, Tmx::TMX_COMPRESSION_NONE);
				layers.push_back(layer);
			}

			Tmx::Layer* mainLayer = GetLayer(0);
			for (std::vector< Tmx::Layer* >::size_type j = 0;j < subMap->layers.size();j++)
			{
				Tmx::Layer* layer = subMap->layers[j];
				for (int x = 0;x < layer->GetWidth();x++)
					for (int y = 0;y < layer->GetHeight();y++)
					{
						int x1 = x, y1 = y;
						int rotatedOriginX = 0, rotatedOriginY = 0;
						const Tmx::MapTile& originTile = layer->GetTile(x, y);
						if (originTile.tilesetId < 0)
							continue;

						Tmx::MapTile subTile = Tmx::MapTile(originTile);
						//x1 = x * cosB + y * sinB
						//y1 = -x * sinB + y * cosB
						if (rotations[i] == 270)
						{
							rotatedOriginX = 0;
							rotatedOriginY = -layer->GetWidth() + 1;
							x1 = y;
							y1 = -x;
						}
						else if (rotations[i] == 180)
						{
							rotatedOriginX = -layer->GetWidth() + 1;
							rotatedOriginY = -layer->GetHeight() + 1;
							x1 = -x;
							y1 = -y;
						}
						else if (rotations[i] == 90)
						{
							rotatedOriginX = -layer->GetHeight() + 1;
							rotatedOriginY = 0;
							x1 = -y;
							y1 = x;
						}
						x1 = x1 - rotatedOriginX + positions[i].first-topLeft.first;
						y1 = y1 - rotatedOriginY + positions[i].second- topLeft.second;

						subTile.tilesetId += tilesetIndexOffset;
						mainLayer->SetTile(x1, y1, subTile);
					}
			}
		}
	}

	Map::~Map() 
	{
		// Iterate through all of the object groups and delete each of them.
		vector< ObjectGroup* >::iterator ogIter;
		for (ogIter = object_groups.begin(); ogIter != object_groups.end(); ++ogIter) 
		{
			ObjectGroup *objectGroup = (*ogIter);
			
			if (objectGroup)
			{
				delete objectGroup;
				objectGroup = NULL;
			}
		}

		// Iterate through all of the layers and delete each of them.
		vector< Layer* >::iterator lIter;
		for (lIter = layers.begin(); lIter != layers.end(); ++lIter) 
		{
			Layer *layer = (*lIter);

			if (layer) 
			{
				delete layer;
				layer = NULL;
			}
		}

		// Iterate through all of the layers and delete each of them.
		vector< ImageLayer* >::iterator ilIter;
		for (ilIter = image_layers.begin(); ilIter != image_layers.end(); ++ilIter) 
		{
			ImageLayer *layer = (*ilIter);

			if (layer) 
			{
				delete layer;
				layer = NULL;
			}
		}

		// Iterate through all of the tilesets and delete each of them.
		vector< Tileset* >::iterator tsIter;
		for (tsIter = tilesets.begin(); tsIter != tilesets.end(); ++tsIter) 
		{
			Tileset *tileset = (*tsIter);
			
			if (tileset) 
			{
				delete tileset;
				tileset = NULL;
			}
		}
	}

	bool Map::setMapTileCost(int layerIndex, int _x, int _y, int cost)
	{
		int x = _x - topLeft.first;
		int y = -_y - topLeft.second;
		printf("enter ----setMapTileCost %d %d %d %d %d\n", x, y, GetWidth(), GetHeight(), int(layers.size()));
		if (cost < 0)
			return false;
		
		if (layerIndex >= int(layers.size()))
			return false;

		if (x < 0 ||
			x >= GetWidth() ||
			y < 0 ||
			y >= GetHeight()
			)
		{	
			return false;
		}
		Tmx::Layer* layer = GetLayer(layerIndex);
		const Tmx::MapTile& mapTile = layer->GetTile(x, y);
		printf("enter ----mapTile %d\n", mapTile.tilesetId);
		if (mapTile.tilesetId < 0)
			return false;

		Tmx::Tileset* tileset = GetMutableTileset(mapTile.tilesetId);
		return tileset->setTilesetTileProperty(int(mapTile.id), "cost", std::to_string(cost));
	}

	bool Map::setLeftTop(int x, int y)
	{
		topLeft.first = x;
		topLeft.second = -y;
		return true;
	}

	void Map::ParseFile(const string &fileName) 
	{
		file_name = fileName;

		int lastSlash = fileName.find_last_of("/");

		// Get the directory of the file using substring.
		if (lastSlash > 0) 
		{
			file_path = fileName.substr(0, lastSlash + 1);
		} 
		else 
		{
			file_path = "";
		}

		char* fileText;
		int fileSize;

		// Open the file for reading.
#ifdef USE_SDL2_LOAD
		SDL_RWops * file = SDL_RWFromFile (fileName.c_str(), "rb");
#else
		FILE *file = fopen(fileName.c_str(), "rb");
#endif

		// Check if the file could not be opened.
		if (!file) 
		{
			has_error = true;
			error_code = TMX_COULDNT_OPEN;
			error_text = "Could not open the file.";
			return;
		}
	
		// Find out the file size.	
#ifdef USE_SDL2_LOAD
		fileSize = file->size(file);
#else
		fseek(file, 0, SEEK_END);
		fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);
#endif
		
		// Check if the file size is valid.
		if (fileSize <= 0)
		{
			has_error = true;
			error_code = TMX_INVALID_FILE_SIZE;
			error_text = "The size of the file is invalid.";
			return;
		}

		// Allocate memory for the file and read it into the memory.
		fileText = new char[fileSize + 1];
		fileText[fileSize] = 0;
#ifdef USE_SDL2_LOAD
		file->read(file, fileText, 1, fileSize);
#else
		if (fread(fileText, 1, fileSize, file) < 1)
		{
			error_text = "Error in reading or end of file.\n";
			return;
		}
#endif

#ifdef USE_SDL2_LOAD
		file->close(file);
#else
		fclose(file);
#endif

		// Copy the contents into a C++ string and delete it from memory.
		std::string text(fileText, fileText+fileSize);
		delete [] fileText;

		ParseText(text);		
	}

	void Map::ParseText(const string &text) 
	{
		// Create a tiny xml document and use it to parse the text.
		TiXmlDocument doc;
		doc.Parse(text.c_str());
	
		// Check for parsing errors.
		if (doc.Error()) 
		{
			has_error = true;
			error_code = TMX_PARSING_ERROR;
			error_text = doc.ErrorDesc();
			return;
		}

		TiXmlNode *mapNode = doc.FirstChild("map");
		TiXmlElement* mapElem = mapNode->ToElement();

		// Read the map attributes.
		mapElem->Attribute("version", &version);
		mapElem->Attribute("width", &width);
		mapElem->Attribute("height", &height);
		mapElem->Attribute("tilewidth", &tile_width);
		mapElem->Attribute("tileheight", &tile_height);

		// Read the orientation
		std::string orientationStr = mapElem->Attribute("orientation");

		if (!orientationStr.compare("orthogonal")) 
		{
			orientation = TMX_MO_ORTHOGONAL;
		} 
		else if (!orientationStr.compare("isometric")) 
		{
			orientation = TMX_MO_ISOMETRIC;
		}
		else if (!orientationStr.compare("staggered")) 
		{
			orientation = TMX_MO_STAGGERED;
		}
		

		const TiXmlNode *node = mapElem->FirstChild();
		int zOrder = 0;
		while( node )
		{
			// Read the map properties.
			if( strcmp( node->Value(), "properties" ) == 0 )
			{
				properties.Parse(node);			
			}

			// Iterate through all of the tileset elements.
			if( strcmp( node->Value(), "tileset" ) == 0 )
			{
				// Allocate a new tileset and parse it.
				Tileset *tileset = new Tileset();
				tileset->Parse(node->ToElement());

				// Add the tileset to the list.
				tilesets.push_back(tileset);
			}

			// Iterate through all of the layer elements.			
			if( strcmp( node->Value(), "layer" ) == 0 )
			{
				// Allocate a new layer and parse it.
				Layer *layer = new Layer(this);
				layer->Parse(node);
				layer->SetZOrder( zOrder );
				++zOrder;

				// Add the layer to the list.
				layers.push_back(layer);
			}

			// Iterate through all of the imagen layer elements.			
			if( strcmp( node->Value(), "imagelayer" ) == 0 )
			{
				// Allocate a new layer and parse it.
				ImageLayer *imageLayer = new ImageLayer(this);
				imageLayer->Parse(node);
				imageLayer->SetZOrder( zOrder );
				++zOrder;

				// Add the layer to the list.
				image_layers.push_back(imageLayer);
			}

			// Iterate through all of the objectgroup elements.
			if( strcmp( node->Value(), "objectgroup" ) == 0 )
			{
				// Allocate a new object group and parse it.
				ObjectGroup *objectGroup = new ObjectGroup();
				objectGroup->Parse(node);
				objectGroup->SetZOrder( zOrder );
				++zOrder;
		
				// Add the object group to the list.
				object_groups.push_back(objectGroup);
			}

			node = node->NextSibling();
		}
	}

	int Map::FindTilesetIndex(int gid) const
	{
		// Clean up the flags from the gid (thanks marwes91).
		gid &= ~(FlippedHorizontallyFlag | FlippedVerticallyFlag | FlippedDiagonallyFlag);

		for (int i = tilesets.size() - 1; i > -1; --i) 
		{
			// If the gid beyond the tileset gid return its index.
			if (gid >= tilesets[i]->GetFirstGid()) 
			{
				return i;
			}
		}
		
		return -1;
	}

	const Tileset *Map::FindTileset(int gid) const 
	{
		for (int i = tilesets.size() - 1; i > -1; --i) 
		{
			// If the gid beyond the tileset gid return it.
			if (gid >= tilesets[i]->GetFirstGid()) 
			{
				return tilesets[i];
			}
		}
		
		return NULL;
	}
};
