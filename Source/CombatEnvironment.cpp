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
    constexpr float kCubeGravity = 18.0f;
    constexpr float kCubeGroundFriction = 6.0f;
    constexpr float kCubeRestitution = 0.12f;
    constexpr float kCubeSleepSpeed = 0.35f;
    constexpr float kCubeSeparationSlop = 0.001f;
    constexpr float kCubeSupportOverlap = 0.35f;

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

    bool SegmentIntersectsAabb(
        Vector3 segmentStart,
        Vector3 segmentEnd,
        Vector3 boxMin,
        Vector3 boxMax,
        float& outHitDistance)
    {
        const Vector3 direction = Vector3Subtract(segmentEnd, segmentStart);
        const float segmentLength = Vector3Length(direction);
        if (segmentLength <= 1e-5f)
        {
            return false;
        }

        float tMin = 0.0f;
        float tMax = segmentLength;

        const float axes[3] = { direction.x, direction.y, direction.z };
        const float starts[3] = { segmentStart.x, segmentStart.y, segmentStart.z };
        const float mins[3] = { boxMin.x, boxMin.y, boxMin.z };
        const float maxs[3] = { boxMax.x, boxMax.y, boxMax.z };

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(axes[axis]) < 1e-5f)
            {
                if (starts[axis] < mins[axis] || starts[axis] > maxs[axis])
                {
                    return false;
                }
            }
            else
            {
                const float inv = 1.0f / axes[axis];
                float t1 = (mins[axis] - starts[axis]) * inv;
                float t2 = (maxs[axis] - starts[axis]) * inv;
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                }

                tMin = std::max(tMin, t1);
                tMax = std::min(tMax, t2);
                if (tMin > tMax)
                {
                    return false;
                }
            }
        }

        outHitDistance = tMin;
        return tMin <= segmentLength;
    }

    void AddStackedWallColumn(
        const RegionHeightfield& heightfield,
        float x,
        float z,
        int stackHeight,
        std::vector<CombatPhysicsCube>& outCubes)
    {
        const float terrainY = heightfield.SampleHeight(x, z);
        for (int level = 0; level < stackHeight; ++level)
        {
            CombatPhysicsCube cube{};
            cube.m_HalfExtent = kWallCubeHalfSize;
            cube.m_Center = Vector3{
                x,
                terrainY + kWallCubeHalfSize + static_cast<float>(level) * kWallCubeSize,
                z
            };
            cube.m_Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
            cube.m_Sleeping = true;
            outCubes.push_back(cube);
        }
    }

    float CubeSpeed(const CombatPhysicsCube& cube)
    {
        return Vector3Length(cube.m_Velocity);
    }

    void WakeCube(CombatPhysicsCube& cube)
    {
        cube.m_Sleeping = false;
    }
}

void CombatEnvironment::Clear()
{
    m_CastleParts.clear();
    m_Trees.clear();
    m_WallCubes.clear();
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
    GenerateCombatTrees(heightfield, seed, 18);

    // Towers flank a free-standing wall made of individual physics cubes.
    g_CombatEnvironment.m_CastleParts = {
        CastlePartPlacement{ CastlePartType::RoundTower, Vector3{ 52.0f, 0.0f, 64.0f }, 0 },
        CastlePartPlacement{ CastlePartType::RoundTower, Vector3{ 76.0f, 0.0f, 64.0f }, 0 },
    };

    // Stacked cube wall between the towers for catapult demolition tests.
    for (int column = 0; column < 8; ++column)
    {
        const float x = 56.0f + static_cast<float>(column) * kWallCubeSize;
        const int height = (column == 0 || column == 7) ? 2 : 4;
        AddStackedWallColumn(heightfield, x, 64.0f, height, g_CombatEnvironment.m_WallCubes);
    }
}

void SeedCombatWallCubesFromPlacements(
    const RegionHeightfield& heightfield,
    const std::vector<CastlePartPlacement>& placements)
{
    g_CombatEnvironment.m_WallCubes.clear();
    g_CombatEnvironment.m_CastleParts.clear();

    for (const CastlePartPlacement& placement : placements)
    {
        if (placement.m_Type == CastlePartType::WallCube)
        {
            CombatPhysicsCube cube{};
            cube.m_HalfExtent = kWallCubeHalfSize;
            cube.m_Center = GetCastlePartWorldCenter(placement, heightfield);
            cube.m_Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
            cube.m_Sleeping = true;
            g_CombatEnvironment.m_WallCubes.push_back(cube);
        }
        else
        {
            g_CombatEnvironment.m_CastleParts.push_back(placement);
        }
    }
}

void UpdateCombatWallCubePhysics(const RegionHeightfield& heightfield, float deltaTime)
{
    if (g_CombatEnvironment.m_WallCubes.empty() || deltaTime <= 0.0f)
    {
        return;
    }

    const float dt = std::min(deltaTime, 0.05f);
    auto& cubes = g_CombatEnvironment.m_WallCubes;

    for (CombatPhysicsCube& cube : cubes)
    {
        if (cube.m_Sleeping)
        {
            continue;
        }

        cube.m_Velocity.y -= kCubeGravity * dt;
        cube.m_Center = Vector3Add(cube.m_Center, Vector3Scale(cube.m_Velocity, dt));
    }

    // Terrain support / bounce.
    for (CombatPhysicsCube& cube : cubes)
    {
        const float terrainY = heightfield.SampleHeight(cube.m_Center.x, cube.m_Center.z);
        const float minCenterY = terrainY + cube.m_HalfExtent;
        if (cube.m_Center.y < minCenterY)
        {
            cube.m_Center.y = minCenterY;
            if (cube.m_Velocity.y < 0.0f)
            {
                cube.m_Velocity.y = -cube.m_Velocity.y * kCubeRestitution;
            }

            // Ground friction.
            const float friction = std::exp(-kCubeGroundFriction * dt);
            cube.m_Velocity.x *= friction;
            cube.m_Velocity.z *= friction;

            if (CubeSpeed(cube) < kCubeSleepSpeed)
            {
                cube.m_Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
                cube.m_Sleeping = true;
            }
            else
            {
                WakeCube(cube);
            }
        }
    }

    // Cube-cube contacts (a few solver iterations for stacking stability).
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        for (int i = 0; i < static_cast<int>(cubes.size()); ++i)
        {
            for (int j = i + 1; j < static_cast<int>(cubes.size()); ++j)
            {
                CombatPhysicsCube& a = cubes[static_cast<size_t>(i)];
                CombatPhysicsCube& b = cubes[static_cast<size_t>(j)];

                const float combined = a.m_HalfExtent + b.m_HalfExtent;
                const float dx = b.m_Center.x - a.m_Center.x;
                const float dy = b.m_Center.y - a.m_Center.y;
                const float dz = b.m_Center.z - a.m_Center.z;
                const float overlapX = combined - std::fabs(dx);
                const float overlapY = combined - std::fabs(dy);
                const float overlapZ = combined - std::fabs(dz);
                if (overlapX <= kCubeSeparationSlop
                    || overlapY <= kCubeSeparationSlop
                    || overlapZ <= kCubeSeparationSlop)
                {
                    continue;
                }

                // Separate along the axis of least penetration.
                float nx = 0.0f;
                float ny = 0.0f;
                float nz = 0.0f;
                float penetration = overlapX;
                if (overlapX <= overlapY && overlapX <= overlapZ)
                {
                    nx = (dx < 0.0f) ? -1.0f : 1.0f;
                    penetration = overlapX;
                }
                else if (overlapY <= overlapX && overlapY <= overlapZ)
                {
                    ny = (dy < 0.0f) ? -1.0f : 1.0f;
                    penetration = overlapY;
                }
                else
                {
                    nz = (dz < 0.0f) ? -1.0f : 1.0f;
                    penetration = overlapZ;
                }

                const float aWeight = a.m_Sleeping ? 0.0f : 1.0f;
                const float bWeight = b.m_Sleeping ? 0.0f : 1.0f;
                float totalWeight = aWeight + bWeight;
                if (totalWeight <= 1e-5f)
                {
                    // Both sleeping but overlapping — nudge equally and wake them if stacked poorly.
                    totalWeight = 2.0f;
                    WakeCube(a);
                    WakeCube(b);
                    const float halfPush = penetration * 0.5f;
                    a.m_Center.x -= nx * halfPush;
                    a.m_Center.y -= ny * halfPush;
                    a.m_Center.z -= nz * halfPush;
                    b.m_Center.x += nx * halfPush;
                    b.m_Center.y += ny * halfPush;
                    b.m_Center.z += nz * halfPush;
                    continue;
                }

                const float aPush = penetration * (bWeight / totalWeight);
                const float bPush = penetration * (aWeight / totalWeight);
                a.m_Center.x -= nx * aPush;
                a.m_Center.y -= ny * aPush;
                a.m_Center.z -= nz * aPush;
                b.m_Center.x += nx * bPush;
                b.m_Center.y += ny * bPush;
                b.m_Center.z += nz * bPush;

                // Relative velocity along contact normal.
                const float relVel =
                    (b.m_Velocity.x - a.m_Velocity.x) * nx
                    + (b.m_Velocity.y - a.m_Velocity.y) * ny
                    + (b.m_Velocity.z - a.m_Velocity.z) * nz;

                if (relVel < 0.0f)
                {
                    const float impulse = -(1.0f + kCubeRestitution) * relVel * 0.5f;
                    if (!a.m_Sleeping || impulse > 0.4f)
                    {
                        WakeCube(a);
                        a.m_Velocity.x -= impulse * nx;
                        a.m_Velocity.y -= impulse * ny;
                        a.m_Velocity.z -= impulse * nz;
                    }
                    if (!b.m_Sleeping || impulse > 0.4f)
                    {
                        WakeCube(b);
                        b.m_Velocity.x += impulse * nx;
                        b.m_Velocity.y += impulse * ny;
                        b.m_Velocity.z += impulse * nz;
                    }
                }

                // Stacking support: if A is mostly under B, damp B into rest on A.
                if (ny > 0.5f
                    && std::fabs(dx) < combined * kCubeSupportOverlap
                    && std::fabs(dz) < combined * kCubeSupportOverlap)
                {
                    if (CubeSpeed(b) < kCubeSleepSpeed * 1.5f && a.m_Sleeping)
                    {
                        b.m_Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
                        b.m_Center.y = a.m_Center.y + combined;
                        b.m_Sleeping = true;
                    }
                }
                else if (ny < -0.5f
                    && std::fabs(dx) < combined * kCubeSupportOverlap
                    && std::fabs(dz) < combined * kCubeSupportOverlap)
                {
                    if (CubeSpeed(a) < kCubeSleepSpeed * 1.5f && b.m_Sleeping)
                    {
                        a.m_Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
                        a.m_Center.y = b.m_Center.y + combined;
                        a.m_Sleeping = true;
                    }
                }
            }
        }
    }

    // Final sleep pass.
    for (CombatPhysicsCube& cube : cubes)
    {
        if (!cube.m_Sleeping && CubeSpeed(cube) < kCubeSleepSpeed)
        {
            const float terrainY = heightfield.SampleHeight(cube.m_Center.x, cube.m_Center.z);
            const float groundGap = cube.m_Center.y - (terrainY + cube.m_HalfExtent);
            if (groundGap < 0.05f)
            {
                cube.m_Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
                cube.m_Sleeping = true;
            }
        }
    }
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

bool SegmentIntersectsCombatWallCube(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CombatPhysicsCube& cube,
    float& outHitDistance)
{
    const Vector3 boxMin{
        cube.m_Center.x - cube.m_HalfExtent,
        cube.m_Center.y - cube.m_HalfExtent,
        cube.m_Center.z - cube.m_HalfExtent
    };
    const Vector3 boxMax{
        cube.m_Center.x + cube.m_HalfExtent,
        cube.m_Center.y + cube.m_HalfExtent,
        cube.m_Center.z + cube.m_HalfExtent
    };
    return SegmentIntersectsAabb(segmentStart, segmentEnd, boxMin, boxMax, outHitDistance);
}

void ApplyImpulseToCombatWallCube(
    CombatPhysicsCube& cube,
    Vector3 impulse,
    Vector3 hitPosition)
{
    WakeCube(cube);
    cube.m_Velocity = Vector3Add(cube.m_Velocity, impulse);

    // Slight vertical kick so stacked blocks can topple off each other.
    const Vector3 offset = Vector3Subtract(hitPosition, cube.m_Center);
    cube.m_Velocity.y += std::max(0.0f, impulse.y) * 0.15f
        + Vector3Length(Vector3{ offset.x, 0.0f, offset.z }) * 0.05f;
}

void DrawCombatTrees(const RegionHeightfield& heightfield, const std::vector<CombatTreeObstacle>& trees)
{
    for (const CombatTreeObstacle& tree : trees)
    {
        const float terrainY = heightfield.SampleHeight(tree.m_Position.x, tree.m_Position.z);
        // DrawCylinder position is the base center (extends upward by height).
        const Vector3 trunkBase{
            tree.m_Position.x,
            terrainY,
            tree.m_Position.z
        };
        const Vector3 crownCenter{
            tree.m_Position.x,
            terrainY + tree.m_TrunkHeight + tree.m_CrownRadius * 0.35f,
            tree.m_Position.z
        };

        DrawCylinder(
            trunkBase,
            tree.m_TrunkRadius,
            tree.m_TrunkRadius,
            tree.m_TrunkHeight,
            8,
            Color{ 92, 62, 38, 255 });
        DrawSphere(crownCenter, tree.m_CrownRadius, Color{ 48, 118, 52, 255 });
    }
}

void DrawCombatWallCubes(const std::vector<CombatPhysicsCube>& cubes)
{
    for (const CombatPhysicsCube& cube : cubes)
    {
        const float size = cube.m_HalfExtent * 2.0f;
        DrawCube(cube.m_Center, size, size, size, Color{ 128, 126, 120, 255 });
        DrawCubeWires(cube.m_Center, size, size, size, Color{ 40, 40, 42, 255 });
    }
}

void DrawCombatEnvironment(const RegionHeightfield& heightfield, const CombatEnvironment& environment)
{
    DrawCombatTrees(heightfield, environment.m_Trees);
    DrawCastleParts3D(heightfield, environment.m_CastleParts);
    DrawCombatWallCubes(environment.m_WallCubes);
}
