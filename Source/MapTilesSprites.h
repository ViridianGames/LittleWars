#ifndef _MAPTILESSPRITES_H_
#define _MAPTILESSPRITES_H_

#include "OverworldMap.h"
#include "raylib.h"

constexpr const char* MAP_TILES_ATLAS_PATH = "Images/tiles256.png";
constexpr int MAP_TILES_FLAG_X = 80;
constexpr int MAP_TILES_FLAG_Y = 345;
constexpr int MAP_TILES_FLAG_WIDTH = 8;
constexpr int MAP_TILES_FLAG_HEIGHT = 11;

struct MapTilesSpriteSpec
{
    Rectangle m_Source = { 0.0f, 0.0f, 0.0f, 0.0f };
    int m_Width = 0;
    int m_Height = 0;
};

Rectangle GetMapTilesFlagSourceRect();
MapTilesSpriteSpec GetMapTilesResourceSpec(CountyResource resource);

void InitMapTilesSprites();
void ShutdownMapTilesSprites();
Texture* GetMapTilesAtlasTexture();

void DrawMapTilesSprite(const MapTilesSpriteSpec& spec, Vector2 center, Color tint = WHITE);
void DrawRegionFlag(Vector2 center, Color tint = WHITE);
void DrawRegionResourceIcon(CountyResource resource, Vector2 center, Color tint = WHITE);

#endif