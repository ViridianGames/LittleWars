#include "CombatEnvironment.h"

#include "../Geist/Source/RNG.h"
#include "GameGlobals.h"
#include "OverworldMap.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

CombatEnvironment g_CombatEnvironment;

namespace
{
    constexpr float kMinTreeSpacing = 4.5f;
    constexpr float kTreePlacementMargin = 8.0f;

    bool IsTreePlacementCellValid(const RegionHeightfield& heightfield, int cellX, int cellY)
    {
        if (cellX < 2 || cellY < 2 || cellX >= REGION_CELLS - 2 || cellY >= REGION_CELLS - 2)
        {
            return false;
        }

        if (heightfield.GetTerrainType(cellX, cellY) == RTT_WATER)
        {
            return false;
        }

        const float height = heightfield.SampleHeight(
            static_cast<float>(cellX) + 0.5f,
            static_cast<float>(cellY) + 0.5f);
        return height >= 1.2f && height <= 5.5f;
    }

    bool IsFarEnoughFromTrees(const std::vector<CombatTreeObstacle>& trees, float x, float z)
    {
        for (const CombatTreeObstacle& tree : trees)
        {
            const float dx = tree.m_Position.x - x;
            const float dz = tree.m_Position.z - z;
            if ((dx * dx + dz * dz) < (kMinTreeSpacing * kMinTreeSpacing))
            {
                return false;
            }
        }

        return true;
    }

    bool SegmentIntersectsCylinderY(
        Vector3 segmentStart,
        Vector3 segmentEnd,
        Vector3 baseCenter,
        float radius,
        float height,
        float& outHitDistance)
    {
        const Vector3 direction = Vector3Subtract(segmentEnd, segmentStart);
        const float segmentLength = Vector3Length(direction);
        if (segmentLength <= 1e-5f)
        {
            return false;
        }

        const Vector3 localStart{
            segmentStart.x - baseCenter.x,
            segmentStart.y - baseCenter.y,
            segmentStart.z - baseCenter.z
        };
        const Vector3 localEnd{
            segmentEnd.x - baseCenter.x,
            segmentEnd.y - baseCenter.y,
            segmentEnd.z - baseCenter.z
        };
        const Vector3 localDirection = Vector3Subtract(localEnd, localStart);

        float bestDistance = segmentLength + 1.0f;
        bool hit = false;

        const float a = localDirection.x * localDirection.x + localDirection.z * localDirection.z;
        if (a > 1e-5f)
        {
            const float b = 2.0f * (localStart.x * localDirection.x + localStart.z * localDirection.z);
            const float c = localStart.x * localStart.x + localStart.z * localStart.z - radius * radius;
            const float discriminant = b * b - 4.0f * a * c;
            if (discriminant >= 0.0f)
            {
                const float sqrtDisc = std::sqrt(discriminant);
                const float inv2a = 0.5f / a;
                const float roots[2] = {
                    (-b - sqrtDisc) * inv2a,
                    (-b + sqrtDisc) * inv2a
                };

                for (float root : roots)
                {
                    if (root < 0.0f || root > 1.0f)
                    {
                        continue;
                    }

                    const float y = localStart.y + localDirection.y * root;
                    if (y < 0.0f || y > height)
                    {
                        continue;
                    }

                    const float distanceAlongSegment = root * segmentLength;
                    if (distanceAlongSegment < bestDistance)
                    {
                        bestDistance = distanceAlongSegment;
                        hit = true;
                    }
                }
            }
        }

        if (hit)
        {
            outHitDistance = bestDistance;
        }

        return hit;
    }
}

void CombatEnvironment::Clear()
{
    m_CastleParts.clear();
    m_Trees.clear();
}

void GenerateCombatTrees(const RegionHeightfield& heightfield, unsigned int seed, int treeCount)
{
    g_CombatEnvironment.m_Trees.clear();

    if (!heightfield.m_Generated || treeCount <= 0)
    {
        return;
    }

    RNG rng;
    rng.SeedRNG(seed != 0 ? seed : 1u);
    const int maxAttempts = treeCount * 24;
    int attempts = 0;

    while (static_cast<int>(g_CombatEnvironment.m_Trees.size()) < treeCount && attempts < maxAttempts)
    {
        ++attempts;

        const float x = rng.RandomRangeFloat(kTreePlacementMargin, static_cast<float>(REGION_CELLS) - kTreePlacementMargin);
        const float z = rng.RandomRangeFloat(kTreePlacementMargin, static_cast<float>(REGION_CELLS) - kTreePlacementMargin);
        const int cellX = static_cast<int>(x);
        const int cellY = static_cast<int>(z);

        if (!IsTreePlacementCellValid(heightfield, cellX, cellY))
        {
            continue;
        }

        if (!IsFarEnoughFromTrees(g_CombatEnvironment.m_Trees, x, z))
        {
            continue;
        }

        CombatTreeObstacle tree{};
        tree.m_Position = Vector3{ x, 0.0f, z };
        tree.m_TrunkRadius = 0.32f + rng.RandomRangeFloat(0.0f, 0.12f);
        tree.m_TrunkHeight = 2.8f + rng.RandomRangeFloat(0.0f, 0.8f);
        tree.m_CrownRadius = 1.2f + rng.RandomRangeFloat(0.0f, 0.5f);
        g_CombatEnvironment.m_Trees.push_back(tree);
    }
}

void InitializeRegionCombatEnvironment(
    const RegionHeightfield& heightfield,
    unsigned int seed,
    unsigned char resourceType)
{
    g_CombatEnvironment.Clear();

    RNG rng;
    rng.SeedRNG(seed != 0 ? seed ^ 0xA5A5u : 1u);

    int treeCount = 20;
    switch (static_cast<CountyResource>(resourceType))
    {
    case CountyResource::Wood:
        treeCount = rng.RandomRange(55, 85);
        break;
    case CountyResource::Food:
        treeCount = rng.RandomRange(3, 10);
        break;
    case CountyResource::Iron:
    case CountyResource::Gold:
        treeCount = rng.RandomRange(6, 14);
        break;
    default:
        treeCount = rng.RandomRange(16, 28);
        break;
    }

    GenerateCombatTrees(heightfield, seed, treeCount);
}

void InitializeDemoCombatEnvironment(const RegionHeightfield& heightfield, unsigned int seed)
{
    g_CombatEnvironment.Clear();
    GenerateCombatTrees(heightfield, seed, 28);

    g_CombatEnvironment.m_CastleParts = {
        CastlePartPlacement{ CastlePartType::TallWall, Vector3{ 64.0f, 0.0f, 64.0f }, 0 },
        CastlePartPlacement{ CastlePartType::ShortWall, Vector3{ 58.0f, 0.0f, 64.0f }, 90 },
        CastlePartPlacement{ CastlePartType::ShortWall, Vector3{ 70.0f, 0.0f, 64.0f }, 90 },
        CastlePartPlacement{ CastlePartType::RoundTower, Vector3{ 52.0f, 0.0f, 64.0f }, 0 },
        CastlePartPlacement{ CastlePartType::RoundTower, Vector3{ 76.0f, 0.0f, 64.0f }, 0 },
    };
}

bool SegmentIntersectsCombatTree(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CombatTreeObstacle& tree,
    const RegionHeightfield& heightfield,
    float& outHitDistance)
{
    const float terrainY = heightfield.SampleHeight(tree.m_Position.x, tree.m_Position.z);
    const Vector3 baseCenter{
        tree.m_Position.x,
        terrainY,
        tree.m_Position.z
    };

    if (SegmentIntersectsCylinderY(
            segmentStart,
            segmentEnd,
            baseCenter,
            tree.m_TrunkRadius,
            tree.m_TrunkHeight,
            outHitDistance))
    {
        return true;
    }

    const Vector3 crownCenter{
        tree.m_Position.x,
        terrainY + tree.m_TrunkHeight + tree.m_CrownRadius * 0.35f,
        tree.m_Position.z
    };
    return SegmentIntersectsCylinderY(
        segmentStart,
        segmentEnd,
        crownCenter,
        tree.m_CrownRadius,
        tree.m_CrownRadius * 0.9f,
        outHitDistance);
}

void DrawCombatTrees(const RegionHeightfield& heightfield, const std::vector<CombatTreeObstacle>& trees)
{
    for (const CombatTreeObstacle& tree : trees)
    {
        const float terrainY = heightfield.SampleHeight(tree.m_Position.x, tree.m_Position.z);
        const Vector3 trunkCenter{
            tree.m_Position.x,
            terrainY + tree.m_TrunkHeight * 0.5f,
            tree.m_Position.z
        };
        const Vector3 crownCenter{
            tree.m_Position.x,
            terrainY + tree.m_TrunkHeight + tree.m_CrownRadius * 0.35f,
            tree.m_Position.z
        };

        DrawCylinder(
            trunkCenter,
            tree.m_TrunkRadius,
            tree.m_TrunkRadius,
            tree.m_TrunkHeight,
            8,
            Color{ 92, 62, 38, 255 });
        DrawSphere(crownCenter, tree.m_CrownRadius, Color{ 48, 118, 52, 255 });
    }
}

void DrawCombatEnvironment(const RegionHeightfield& heightfield, const CombatEnvironment& environment)
{
    DrawCombatTrees(heightfield, environment.m_Trees);
    DrawCastleParts3D(heightfield, environment.m_CastleParts);
}