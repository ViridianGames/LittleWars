#include "MapTilesSprites.h"

#include "../Geist/Source/Config.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/ResourceManager.h"

namespace
{
    bool g_MapTilesSpritesInitialized = false;

    MapTilesSpriteSpec MakeSpriteSpec(int sourceX, int sourceY, int width, int height)
    {
        MapTilesSpriteSpec spec;
        spec.m_Source = Rectangle{
            static_cast<float>(sourceX),
            static_cast<float>(sourceY),
            static_cast<float>(width),
            static_cast<float>(height)
        };
        spec.m_Width = width;
        spec.m_Height = height;
        return spec;
    }

    void DrawSpriteSpec(const MapTilesSpriteSpec& spec, Vector2 center, Color tint)
    {
        Texture* texture = GetMapTilesAtlasTexture();
        if (!texture || texture->id == 0 || spec.m_Width <= 0 || spec.m_Height <= 0)
        {
            return;
        }

        const int drawX = static_cast<int>(center.x) - (spec.m_Width / 2);
        const int drawY = static_cast<int>(center.y) - (spec.m_Height / 2);
        const Rectangle dest{
            static_cast<float>(drawX),
            static_cast<float>(drawY),
            static_cast<float>(spec.m_Width),
            static_cast<float>(spec.m_Height)
        };

        DrawTexturePro(*texture, spec.m_Source, dest, Vector2{ 0.0f, 0.0f }, 0.0f, tint);
    }
}

Rectangle GetMapTilesFlagSourceRect()
{
    return Rectangle{
        static_cast<float>(MAP_TILES_FLAG_X),
        static_cast<float>(MAP_TILES_FLAG_Y),
        static_cast<float>(MAP_TILES_FLAG_WIDTH),
        static_cast<float>(MAP_TILES_FLAG_HEIGHT)
    };
}

MapTilesSpriteSpec GetMapTilesResourceSpec(CountyResource resource)
{
    switch (resource)
    {
    case CountyResource::Gold:
        return MakeSpriteSpec(57, 352, 7, 5);
    case CountyResource::Iron:
        return MakeSpriteSpec(89, 352, 7, 6);
    case CountyResource::Wood:
        return MakeSpriteSpec(105, 352, 7, 7);
    case CountyResource::Food:
        return MakeSpriteSpec(105, 336, 7, 7);
    default:
        return MakeSpriteSpec(105, 336, 7, 7);
    }
}

void InitMapTilesSprites()
{
    if (g_MapTilesSpritesInitialized)
    {
        return;
    }

    g_ResourceManager->AddTexture(MAP_TILES_ATLAS_PATH, false);
    if (Texture* texture = g_ResourceManager->GetTexture(MAP_TILES_ATLAS_PATH, false))
    {
        SetTextureFilter(*texture, TEXTURE_FILTER_POINT);
    }

    g_MapTilesSpritesInitialized = true;
}

void ShutdownMapTilesSprites()
{
    g_MapTilesSpritesInitialized = false;
}

Texture* GetMapTilesAtlasTexture()
{
    if (!g_MapTilesSpritesInitialized)
    {
        InitMapTilesSprites();
    }

    return g_ResourceManager->GetTexture(MAP_TILES_ATLAS_PATH, false);
}

void DrawMapTilesSprite(const MapTilesSpriteSpec& spec, Vector2 center, Color tint)
{
    DrawSpriteSpec(spec, center, tint);
}

void DrawRegionFlag(Vector2 center, Color tint)
{
    const MapTilesSpriteSpec spec{
        GetMapTilesFlagSourceRect(),
        MAP_TILES_FLAG_WIDTH,
        MAP_TILES_FLAG_HEIGHT
    };
    DrawSpriteSpec(spec, center, tint);
}

void DrawRegionResourceIcon(CountyResource resource, Vector2 center, Color tint)
{
    DrawSpriteSpec(GetMapTilesResourceSpec(resource), center, tint);
}