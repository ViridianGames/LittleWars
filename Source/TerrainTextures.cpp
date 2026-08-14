#include "TerrainTextures.h"

#include "../Geist/Source/Config.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/ResourceManager.h"

namespace
{
    bool g_TerrainTexturesInitialized = false;

    int AtlasIndexForGrid(int column, int row)
    {
        return row * TERRAIN_ATLAS_COLUMNS + column;
    }
}

int GetTerrainAtlasIndex(RegionTerrainType type)
{
    switch (type)
    {
    case RTT_GRASS:
        return AtlasIndexForGrid(0, 0);
    case RTT_BEACH:
        return AtlasIndexForGrid(1, 0);
    case RTT_SWAMP:
        return AtlasIndexForGrid(2, 0);
    case RTT_MARKED_PATH:
        return AtlasIndexForGrid(3, 0);
    case RTT_WATER:
        return AtlasIndexForGrid(0, 1);
    case RTT_LAVA:
        return AtlasIndexForGrid(1, 1);
    case RTT_FARM:
        return AtlasIndexForGrid(2, 1);
    case RTT_STONE:
        return AtlasIndexForGrid(3, 1);
    default:
        return AtlasIndexForGrid(0, 0);
    }
}

Rectangle GetTerrainSpriteSourceRect(RegionTerrainType type)
{
    const int index = GetTerrainAtlasIndex(type);
    const int column = index % TERRAIN_ATLAS_COLUMNS;
    const int row = index / TERRAIN_ATLAS_COLUMNS;

    return Rectangle{
        static_cast<float>(column * TERRAIN_ATLAS_TILE_SIZE),
        static_cast<float>(row * TERRAIN_ATLAS_TILE_SIZE),
        static_cast<float>(TERRAIN_ATLAS_TILE_SIZE),
        static_cast<float>(TERRAIN_ATLAS_TILE_SIZE)
    };
}

void GetTerrainSpriteUV(RegionTerrainType type, float& u0, float& v0, float& u1, float& v1)
{
    const Rectangle source = GetTerrainSpriteSourceRect(type);
    const float atlasWidth = static_cast<float>(TERRAIN_ATLAS_COLUMNS * TERRAIN_ATLAS_TILE_SIZE);
    const float atlasHeight = static_cast<float>(TERRAIN_ATLAS_ROWS * TERRAIN_ATLAS_TILE_SIZE);

    u0 = source.x / atlasWidth;
    v0 = source.y / atlasHeight;
    u1 = (source.x + source.width) / atlasWidth;
    v1 = (source.y + source.height) / atlasHeight;
}

Color GetTerrainBaseColor(RegionTerrainType type)
{
    switch (type)
    {
    case RTT_GRASS:
        return Color{ 78, 140, 58, 255 };
    case RTT_BEACH:
        return Color{ 194, 178, 128, 255 };
    case RTT_SWAMP:
        return Color{ 72, 96, 52, 255 };
    case RTT_MARKED_PATH:
        return Color{ 148, 118, 72, 255 };
    case RTT_WATER:
        return Color{ 48, 98, 168, 255 };
    case RTT_LAVA:
        return Color{ 196, 72, 36, 255 };
    case RTT_FARM:
        return Color{ 132, 148, 62, 255 };
    case RTT_STONE:
        return Color{ 128, 128, 136, 255 };
    default:
        return Color{ 78, 140, 58, 255 };
    }
}

void InitTerrainTextures()
{
    if (g_TerrainTexturesInitialized)
    {
        return;
    }

    g_ResourceManager->AddTexture(TERRAIN_ATLAS_PATH, false);
    if (Texture* terrainAtlas = g_ResourceManager->GetTexture(TERRAIN_ATLAS_PATH, false))
    {
        SetTextureFilter(*terrainAtlas, TEXTURE_FILTER_POINT);
    }
    g_TerrainTexturesInitialized = true;
}

void ShutdownTerrainTextures()
{
    g_TerrainTexturesInitialized = false;
}

Texture* GetTerrainAtlasTexture()
{
    if (!g_TerrainTexturesInitialized)
    {
        InitTerrainTextures();
    }

    return g_ResourceManager->GetTexture(TERRAIN_ATLAS_PATH, false);
}