#ifndef _TERRAINTEXTURES_H_
#define _TERRAINTEXTURES_H_

#include "GameGlobals.h"
#include "raylib.h"

constexpr int TERRAIN_ATLAS_TILE_SIZE = 16;
constexpr int TERRAIN_ATLAS_COLUMNS = 4;
constexpr int TERRAIN_ATLAS_ROWS = 4;
constexpr const char* TERRAIN_ATLAS_PATH = "Images/terrain.png";

// Sprite order in terrain.png (row-major, top-left to bottom-right).
int GetTerrainAtlasIndex(RegionTerrainType type);

Rectangle GetTerrainSpriteSourceRect(RegionTerrainType type);

void GetTerrainSpriteUV(RegionTerrainType type, float& u0, float& v0, float& u1, float& v1);

void InitTerrainTextures();
void ShutdownTerrainTextures();
Texture* GetTerrainAtlasTexture();

#endif