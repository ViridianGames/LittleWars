#ifndef _CASTLEPARTS_H_
#define _CASTLEPARTS_H_

#include "raylib.h"

#include <vector>

class RegionHeightfield;

enum class CastlePartType
{
    RoundTower = 0,
    SquareTower,
    WallCube,
    Gate,
    Moat,
    Count
};

struct CastlePartPlacement
{
    CastlePartType m_Type = CastlePartType::RoundTower;
    // For most parts, x/z are ground anchors and y is unused (drawn on terrain).
    // For WallCube, m_Position is the cube center in world space (y is absolute).
    Vector3 m_Position{};
    int m_RotationDegrees = 0;
};

constexpr float kWallCubeSize = 1.0f;
constexpr float kWallCubeHalfSize = kWallCubeSize * 0.5f;

const char* CastlePartTypeName(CastlePartType type);
Color CastlePartTypeColor(CastlePartType type);
bool IsStackableCastlePart(CastlePartType type);
void GetCastlePartHalfExtents(CastlePartType type, float& halfX, float& halfZ);
float GetCastlePartPickRadius(CastlePartType type);
void GetCastlePartCollisionSize(CastlePartType type, Vector3& outSize);
bool CastlePartBlocksProjectiles(CastlePartType type);

// World-space center used for drawing / collision (terrain-snapped for non-cubes).
Vector3 GetCastlePartWorldCenter(
    const CastlePartPlacement& placement,
    const RegionHeightfield& heightfield);

bool SegmentIntersectsCastlePart(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CastlePartPlacement& placement,
    const RegionHeightfield& heightfield,
    float& outHitDistance);

void DrawCastlePartShape(
    const RegionHeightfield& heightfield,
    CastlePartType type,
    Vector3 position,
    int rotationDegrees,
    Color color,
    float alphaScale = 1.0f);

void DrawCastleParts3D(
    const RegionHeightfield& heightfield,
    const std::vector<CastlePartPlacement>& placements);

#endif
