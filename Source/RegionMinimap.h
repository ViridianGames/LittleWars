#ifndef _REGIONMINIMAP_H_
#define _REGIONMINIMAP_H_

#include "GameGlobals.h"
#include "raylib.h"

#include <vector>

struct RegionMinimapMarker
{
    float m_X = 0.0f;
    float m_Z = 0.0f;
    Color m_Color = WHITE;
};

class RegionMinimap
{
public:
    void Shutdown();
    void Draw(const RegionHeightfield& heightfield, unsigned int heightfieldSeed, const Camera3D& camera,
        const std::vector<RegionMinimapMarker>* markers = nullptr);

private:
    void RebuildTexture(const RegionHeightfield& heightfield);

    Texture2D m_Texture{};
    bool m_HasTexture = false;
    unsigned int m_CachedSeed = 0;
};

extern RegionMinimap g_RegionMinimap;

#endif