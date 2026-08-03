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

// Destructible stacked wall block. Simulated with simple rigid-body translation.
struct CombatPhysicsCube
{
    Vector3 m_Center{};
    Vector3 m_Velocity{};
    float m_HalfExtent = kWallCubeHalfSize;
    bool m_Sleeping = false;
};

struct CombatEnvironment
{
    std::vector<CastlePartPlacement> m_CastleParts;
    std::vector<CombatTreeObstacle> m_Trees;
    std::vector<CombatPhysicsCube> m_WallCubes;

    void Clear();
};

extern CombatEnvironment g_CombatEnvironment;

void GenerateCombatTrees(const RegionHeightfield& heightfield, unsigned int seed, int treeCount = 28);

// Campaign/region view: trees only (count biased by county resource). No sample castle.
void InitializeRegionCombatEnvironment(
    const RegionHeightfield& heightfield,
    unsigned int seed,
    unsigned char resourceType);

// Title-screen combat sandbox: trees + sample castle pieces + stacked wall cubes.
void InitializeDemoCombatEnvironment(const RegionHeightfield& heightfield, unsigned int seed);

// Convert design-mode WallCube placements into physics cubes (towers/gates stay static).
void SeedCombatWallCubesFromPlacements(
    const RegionHeightfield& heightfield,
    const std::vector<CastlePartPlacement>& placements);

void UpdateCombatWallCubePhysics(const RegionHeightfield& heightfield, float deltaTime);

bool SegmentIntersectsCombatTree(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CombatTreeObstacle& tree,
    const RegionHeightfield& heightfield,
    float& outHitDistance);

bool SegmentIntersectsCombatWallCube(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CombatPhysicsCube& cube,
    float& outHitDistance);

void ApplyImpulseToCombatWallCube(
    CombatPhysicsCube& cube,
    Vector3 impulse,
    Vector3 hitPosition);

void DrawCombatTrees(const RegionHeightfield& heightfield, const std::vector<CombatTreeObstacle>& trees);
void DrawCombatWallCubes(const std::vector<CombatPhysicsCube>& cubes);
void DrawCombatEnvironment(const RegionHeightfield& heightfield, const CombatEnvironment& environment);

#endif
