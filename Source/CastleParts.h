#ifndef _CASTLEPARTS_H_
#define _CASTLEPARTS_H_

#include "raylib.h"

#include <vector>

class RegionHeightfield;

enum class CastlePartType
{
    RoundTower = 0,
    SquareTower,
    ShortWall,
    TallWall,
    Gate,
    Moat,
    Count
};

struct CastlePartPlacement
{
    CastlePartType m_Type = CastlePartType::RoundTower;
    Vector3 m_Position{};
    int m_RotationDegrees = 0;
};

const char* CastlePartTypeName(CastlePartType type);
Color CastlePartTypeColor(CastlePartType type);
void GetCastlePartHalfExtents(CastlePartType type, float& halfX, float& halfZ);
float GetCastlePartPickRadius(CastlePartType type);
void GetCastlePartCollisionSize(CastlePartType type, Vector3& outSize);
bool CastlePartBlocksProjectiles(CastlePartType type);

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