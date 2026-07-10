#ifndef _COMBATENVIRONMENT_H_
#define _COMBATENVIRONMENT_H_

#include "CastleParts.h"
#include "raylib.h"

#include <vector>

class RegionHeightfield;

struct CombatTreeObstacle
{
    Vector3 m_Position{};
    float m_TrunkRadius = 0.35f;
    float m_TrunkHeight = 3.2f;
    float m_CrownRadius = 1.4f;
};

struct CombatEnvironment
{
    std::vector<CastlePartPlacement> m_CastleParts;
    std::vector<CombatTreeObstacle> m_Trees;

    void Clear();
};

extern CombatEnvironment g_CombatEnvironment;

void GenerateCombatTrees(const RegionHeightfield& heightfield, unsigned int seed, int treeCount = 28);
void InitializeDemoCombatEnvironment(const RegionHeightfield& heightfield, unsigned int seed);

bool SegmentIntersectsCombatTree(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CombatTreeObstacle& tree,
    const RegionHeightfield& heightfield,
    float& outHitDistance);

void DrawCombatTrees(const RegionHeightfield& heightfield, const std::vector<CombatTreeObstacle>& trees);
void DrawCombatEnvironment(const RegionHeightfield& heightfield, const CombatEnvironment& environment);

#endif