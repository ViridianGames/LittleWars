#include "OverworldMap.h"

#include "GameGlobals.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>

#include "../Geist/Source/RNG.h"
#include "raylib.h"

OverworldMap g_OverworldMap;

namespace
{
    constexpr int kCellCount = OVERWORLD_MAP_SIZE * OVERWORLD_MAP_SIZE;

    Color CellColor(OverworldCellType type)
    {
        switch (type)
        {
        case OW_CLEAR:
            return Color{ 72, 140, 48, 255 };
        case OW_MOUNTAIN:
            return Color{ 130, 130, 130, 255 };
        case OW_WATER:
            return Color{ 50, 100, 200, 255 };
        case OW_TREES:
            return Color{ 34, 100, 34, 255 };
        case OW_MARSH:
            return Color{ 120, 90, 50, 255 };
        default:
            return Color{ 72, 140, 48, 255 };
        }
    }

    int CountNeighborsOfType(const std::vector<OverworldCellType>& cells, int x, int y, OverworldCellType type)
    {
        int count = 0;
        for (int offsetY = -1; offsetY <= 1; ++offsetY)
        {
            for (int offsetX = -1; offsetX <= 1; ++offsetX)
            {
                if (offsetX == 0 && offsetY == 0)
                {
                    continue;
                }

                const int sampleX = x + offsetX;
                const int sampleY = y + offsetY;
                if (sampleX < 0 || sampleY < 0 || sampleX >= OVERWORLD_MAP_SIZE || sampleY >= OVERWORLD_MAP_SIZE)
                {
                    continue;
                }

                if (cells[static_cast<size_t>(sampleY * OVERWORLD_MAP_SIZE + sampleX)] == type)
                {
                    ++count;
                }
            }
        }

        return count;
    }

    OverworldCellType ResolveCellType(const std::vector<OverworldCellType>& cells, int x, int y)
    {
        const int waterNeighbors = CountNeighborsOfType(cells, x, y, OW_WATER);
        const int mountainNeighbors = CountNeighborsOfType(cells, x, y, OW_MOUNTAIN);
        const int treeNeighbors = CountNeighborsOfType(cells, x, y, OW_TREES);
        const int marshNeighbors = CountNeighborsOfType(cells, x, y, OW_MARSH);

        const size_t index = static_cast<size_t>(y * OVERWORLD_MAP_SIZE + x);
        const OverworldCellType current = cells[index];

        if (waterNeighbors >= 5 || (current == OW_WATER && waterNeighbors >= 3))
        {
            return OW_WATER;
        }
        if (mountainNeighbors >= 5 || (current == OW_MOUNTAIN && mountainNeighbors >= 3))
        {
            return OW_MOUNTAIN;
        }
        if (treeNeighbors >= 5 || (current == OW_TREES && treeNeighbors >= 4))
        {
            return OW_TREES;
        }
        if ((marshNeighbors >= 3 && waterNeighbors >= 1) || (current == OW_MARSH && marshNeighbors >= 2))
        {
            return OW_MARSH;
        }

        return OW_CLEAR;
    }
}

const char* CountyResourceName(CountyResource resource)
{
    switch (resource)
    {
    case CountyResource::Food:
        return "Food";
    case CountyResource::Gold:
        return "Gold";
    case CountyResource::Iron:
        return "Iron";
    case CountyResource::Wood:
        return "Wood";
    default:
        return "Unknown";
    }
}

char CountyResourceMarker(CountyResource resource)
{
    switch (resource)
    {
    case CountyResource::Food:
        return 'F';
    case CountyResource::Gold:
        return 'G';
    case CountyResource::Iron:
        return 'I';
    case CountyResource::Wood:
        return 'W';
    default:
        return '?';
    }
}

void OverworldMap::Clear()
{
    m_Generated = false;
    m_Seed = 0;
    m_Cells.clear();
    m_RegionIds.clear();
    m_Regions.clear();
}

bool OverworldMap::IsInBounds(int x, int y) const
{
    return x >= 0 && y >= 0 && x < OVERWORLD_MAP_SIZE && y < OVERWORLD_MAP_SIZE;
}

int OverworldMap::GetCellIndex(int x, int y) const
{
    return y * OVERWORLD_MAP_SIZE + x;
}

bool OverworldMap::IsLandCell(OverworldCellType type) const
{
    return type == OW_CLEAR || type == OW_TREES || type == OW_MARSH;
}

OverworldCellType OverworldMap::GetCell(int x, int y) const
{
    if (!IsInBounds(x, y) || m_Cells.size() != static_cast<size_t>(kCellCount))
    {
        return OW_WATER;
    }

    return m_Cells[static_cast<size_t>(GetCellIndex(x, y))];
}

int OverworldMap::GetRegionId(int x, int y) const
{
    if (!IsInBounds(x, y) || m_RegionIds.size() != static_cast<size_t>(kCellCount))
    {
        return -1;
    }

    return m_RegionIds[static_cast<size_t>(GetCellIndex(x, y))];
}

const OverworldRegionData* OverworldMap::GetRegion(int regionId) const
{
    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_Id == regionId)
        {
            return &region;
        }
    }

    return nullptr;
}

OverworldRegionData* OverworldMap::GetRegion(int regionId)
{
    for (OverworldRegionData& region : m_Regions)
    {
        if (region.m_Id == regionId)
        {
            return &region;
        }
    }

    return nullptr;
}

void OverworldMap::GenerateTerrain(RNG& rng)
{
    m_Cells.assign(static_cast<size_t>(kCellCount), OW_CLEAR);

    for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
    {
        for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
        {
            const int roll = rng.RandomRange(0, 99);
            OverworldCellType type = OW_CLEAR;
            if (roll < 12)
            {
                type = OW_WATER;
            }
            else if (roll < 20)
            {
                type = OW_MOUNTAIN;
            }
            else if (roll < 35)
            {
                type = OW_TREES;
            }
            else if (roll < 42)
            {
                type = OW_MARSH;
            }

            m_Cells[static_cast<size_t>(GetCellIndex(x, y))] = type;
        }
    }

    std::vector<OverworldCellType> nextCells = m_Cells;
    for (int pass = 0; pass < 5; ++pass)
    {
        for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
        {
            for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
            {
                nextCells[static_cast<size_t>(GetCellIndex(x, y))] = ResolveCellType(m_Cells, x, y);
            }
        }
        m_Cells.swap(nextCells);
    }

    for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
    {
        m_Cells[static_cast<size_t>(x)] = OW_WATER;
        m_Cells[static_cast<size_t>((OVERWORLD_MAP_SIZE - 1) * OVERWORLD_MAP_SIZE + x)] = OW_WATER;
    }
    for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
    {
        m_Cells[static_cast<size_t>(y * OVERWORLD_MAP_SIZE)] = OW_WATER;
        m_Cells[static_cast<size_t>(y * OVERWORLD_MAP_SIZE + (OVERWORLD_MAP_SIZE - 1))] = OW_WATER;
    }
}

void OverworldMap::PartitionIntoRegions(RNG& rng, int targetRegionCount)
{
    m_RegionIds.assign(static_cast<size_t>(kCellCount), -1);
    m_Regions.clear();

    int landCells = 0;
    for (OverworldCellType cell : m_Cells)
    {
        if (IsLandCell(cell))
        {
            ++landCells;
        }
    }

    int targetRegions = std::max(targetRegionCount, 1);

    struct SeedPoint
    {
        int x;
        int y;
    };

    std::vector<SeedPoint> seeds;
    const float distanceScale = std::sqrt(35.0f / static_cast<float>(targetRegions));
    int minSeedDistance = static_cast<int>(10.0f * distanceScale);
    minSeedDistance = std::clamp(minSeedDistance, 4, 18);
    int attempts = 0;
    while (static_cast<int>(seeds.size()) < targetRegions && attempts < 8000)
    {
        ++attempts;
        const int x = rng.Random(OVERWORLD_MAP_SIZE);
        const int y = rng.Random(OVERWORLD_MAP_SIZE);
        if (!IsLandCell(GetCell(x, y)))
        {
            continue;
        }

        bool tooClose = false;
        for (const SeedPoint& seed : seeds)
        {
            const int dx = x - seed.x;
            const int dy = y - seed.y;
            if ((dx * dx) + (dy * dy) < (minSeedDistance * minSeedDistance))
            {
                tooClose = true;
                break;
            }
        }

        if (tooClose)
        {
            continue;
        }

        seeds.push_back({ x, y });
    }

    struct VoronoiNode
    {
        int x;
        int y;
        int regionId;
        int distance;
    };

    std::queue<VoronoiNode> open;
    std::vector<int> bestDistance(static_cast<size_t>(kCellCount), -1);

    for (size_t seedIndex = 0; seedIndex < seeds.size(); ++seedIndex)
    {
        OverworldRegionData region;
        region.m_Id = static_cast<int>(seedIndex);
        region.m_SeedX = seeds[seedIndex].x;
        region.m_SeedY = seeds[seedIndex].y;
        m_Regions.push_back(region);

        const int index = GetCellIndex(seeds[seedIndex].x, seeds[seedIndex].y);
        bestDistance[static_cast<size_t>(index)] = 0;
        m_RegionIds[static_cast<size_t>(index)] = region.m_Id;
        open.push({ seeds[seedIndex].x, seeds[seedIndex].y, region.m_Id, 0 });
    }

    const int offsets[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    while (!open.empty())
    {
        const VoronoiNode node = open.front();
        open.pop();

        for (const auto& offset : offsets)
        {
            const int nextX = node.x + offset[0];
            const int nextY = node.y + offset[1];
            if (!IsInBounds(nextX, nextY) || !IsLandCell(GetCell(nextX, nextY)))
            {
                continue;
            }

            const int nextIndex = GetCellIndex(nextX, nextY);
            const int nextDistance = node.distance + 1;
            if (bestDistance[static_cast<size_t>(nextIndex)] >= 0
                && bestDistance[static_cast<size_t>(nextIndex)] <= nextDistance)
            {
                continue;
            }

            bestDistance[static_cast<size_t>(nextIndex)] = nextDistance;
            m_RegionIds[static_cast<size_t>(nextIndex)] = node.regionId;
            open.push({ nextX, nextY, node.regionId, nextDistance });
        }
    }

    for (OverworldRegionData& region : m_Regions)
    {
        region.m_CellCount = 0;
    }

    for (int regionId : m_RegionIds)
    {
        if (regionId < 0)
        {
            continue;
        }

        if (OverworldRegionData* region = GetRegion(regionId))
        {
            ++region->m_CellCount;
        }
    }

    const int minCellsPerRegion = std::max(8, landCells / std::max(targetRegions * 3, 1));

    std::vector<int> oldToNewId(m_Regions.size(), -1);
    int nextRegionId = 0;
    for (size_t oldId = 0; oldId < m_Regions.size(); ++oldId)
    {
        if (m_Regions[oldId].m_CellCount < minCellsPerRegion)
        {
            continue;
        }

        oldToNewId[oldId] = nextRegionId;
        ++nextRegionId;
    }

    std::vector<OverworldRegionData> keptRegions;
    keptRegions.reserve(static_cast<size_t>(nextRegionId));
    for (size_t oldId = 0; oldId < m_Regions.size(); ++oldId)
    {
        if (oldToNewId[oldId] < 0)
        {
            continue;
        }

        OverworldRegionData region = m_Regions[oldId];
        region.m_Id = oldToNewId[oldId];
        keptRegions.push_back(region);
    }
    m_Regions = std::move(keptRegions);

    for (int& regionId : m_RegionIds)
    {
        if (regionId < 0)
        {
            continue;
        }

        if (regionId >= static_cast<int>(oldToNewId.size()))
        {
            regionId = -1;
            continue;
        }

        regionId = oldToNewId[static_cast<size_t>(regionId)];
    }
}

void OverworldMap::BuildAdjacency()
{
    for (OverworldRegionData& region : m_Regions)
    {
        region.m_AdjacentRegionIds.clear();
    }

    const int offsets[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
    {
        for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
        {
            const int regionId = GetRegionId(x, y);
            if (regionId < 0)
            {
                continue;
            }

            OverworldRegionData* region = GetRegion(regionId);
            if (!region)
            {
                continue;
            }

            std::unordered_set<int> neighbors(region->m_AdjacentRegionIds.begin(), region->m_AdjacentRegionIds.end());
            for (const auto& offset : offsets)
            {
                const int neighborRegionId = GetRegionId(x + offset[0], y + offset[1]);
                if (neighborRegionId < 0 || neighborRegionId == regionId)
                {
                    continue;
                }

                neighbors.insert(neighborRegionId);
            }

            region->m_AdjacentRegionIds.assign(neighbors.begin(), neighbors.end());
            std::sort(region->m_AdjacentRegionIds.begin(), region->m_AdjacentRegionIds.end());
        }
    }
}

void OverworldMap::AssignRegionResources(RNG& rng, int resourceDistribution)
{
    if (m_Regions.empty())
    {
        return;
    }

    const auto distribution = static_cast<ResourceDistribution>(resourceDistribution);

    struct RegionTerrainStats
    {
        int clearCount = 0;
        int treeCount = 0;
        int marshCount = 0;
        int mountainNearby = 0;
    };

    std::vector<RegionTerrainStats> terrainStats(m_Regions.size());

    for (OverworldRegionData& region : m_Regions)
    {
        if (region.m_Id < 0 || region.m_Id >= static_cast<int>(terrainStats.size()))
        {
            continue;
        }

        RegionTerrainStats& stats = terrainStats[static_cast<size_t>(region.m_Id)];

        for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
        {
            for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
            {
                if (GetRegionId(x, y) != region.m_Id)
                {
                    continue;
                }

                const OverworldCellType type = GetCell(x, y);
                if (type == OW_CLEAR)
                {
                    ++stats.clearCount;
                }
                else if (type == OW_TREES)
                {
                    ++stats.treeCount;
                }
                else if (type == OW_MARSH)
                {
                    ++stats.marshCount;
                }

                if (GetCell(x + 1, y) == OW_MOUNTAIN || GetCell(x - 1, y) == OW_MOUNTAIN
                    || GetCell(x, y + 1) == OW_MOUNTAIN || GetCell(x, y - 1) == OW_MOUNTAIN)
                {
                    ++stats.mountainNearby;
                }
            }
        }
    }

    std::vector<bool> assigned(m_Regions.size(), false);

    auto firstUnassignedIndex = [&assigned]() -> int
    {
        for (size_t i = 0; i < assigned.size(); ++i)
        {
            if (!assigned[static_cast<size_t>(i)])
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    };

    auto pickBestRegion = [&](auto scoreForRegion) -> int
    {
        int bestIndex = -1;
        int bestScore = -1;

        for (size_t i = 0; i < m_Regions.size(); ++i)
        {
            if (assigned[i])
            {
                continue;
            }

            const int score = scoreForRegion(static_cast<int>(i));
            if (score > bestScore)
            {
                bestScore = score;
                bestIndex = static_cast<int>(i);
            }
        }

        if (bestIndex >= 0)
        {
            return bestIndex;
        }

        return firstUnassignedIndex();
    };

    auto assignResource = [&](int regionIndex, CountyResource resource)
    {
        if (regionIndex < 0 || regionIndex >= static_cast<int>(m_Regions.size()))
        {
            return;
        }

        m_Regions[static_cast<size_t>(regionIndex)].m_Resource = resource;
        assigned[static_cast<size_t>(regionIndex)] = true;
    };

    auto rollWeightedResource = [&rng]() -> CountyResource
    {
        const int roll = rng.Random(100);
        if (roll < 50)
        {
            return CountyResource::Food;
        }
        if (roll < 78)
        {
            return CountyResource::Wood;
        }
        if (roll < 93)
        {
            return CountyResource::Iron;
        }

        return CountyResource::Gold;
    };

    if (distribution == ResourceDistribution::Balanced)
    {
        std::vector<int> regionIndices;
        regionIndices.reserve(m_Regions.size());
        for (size_t i = 0; i < m_Regions.size(); ++i)
        {
            regionIndices.push_back(static_cast<int>(i));
        }

        for (size_t i = regionIndices.size(); i > 1; --i)
        {
            const size_t swapIndex = static_cast<size_t>(rng.Random(static_cast<unsigned int>(i)));
            std::swap(regionIndices[i - 1], regionIndices[swapIndex]);
        }

        const CountyResource cycle[] = {
            CountyResource::Food,
            CountyResource::Wood,
            CountyResource::Iron,
            CountyResource::Gold
        };

        for (size_t i = 0; i < regionIndices.size(); ++i)
        {
            assignResource(regionIndices[i], cycle[i % 4]);
        }

        return;
    }

    if (distribution == ResourceDistribution::Clumped)
    {
        const CountyResource resourceTypes[] = {
            CountyResource::Food,
            CountyResource::Wood,
            CountyResource::Iron,
            CountyResource::Gold
        };

        const int regionCount = static_cast<int>(m_Regions.size());
        std::vector<int> quotas(4, regionCount / 4);
        for (int i = 0; i < regionCount % 4; ++i)
        {
            ++quotas[static_cast<size_t>(i)];
        }

        std::vector<bool> claimed(m_Regions.size(), false);

        auto claimNeighbors = [&](int startRegionId, CountyResource resource, int quota)
        {
            if (startRegionId < 0 || quota <= 0)
            {
                return;
            }

            std::queue<int> open;
            open.push(startRegionId);

            while (!open.empty() && quota > 0)
            {
                const int regionId = open.front();
                open.pop();

                if (regionId < 0 || regionId >= regionCount || claimed[static_cast<size_t>(regionId)])
                {
                    continue;
                }

                claimed[static_cast<size_t>(regionId)] = true;
                assignResource(regionId, resource);
                --quota;

                const OverworldRegionData* region = GetRegion(regionId);
                if (!region)
                {
                    continue;
                }

                for (int neighborId : region->m_AdjacentRegionIds)
                {
                    if (neighborId >= 0 && neighborId < regionCount && !claimed[static_cast<size_t>(neighborId)])
                    {
                        open.push(neighborId);
                    }
                }
            }

            while (quota > 0)
            {
                const int fallback = firstUnassignedIndex();
                if (fallback < 0)
                {
                    break;
                }

                claimed[static_cast<size_t>(fallback)] = true;
                assignResource(fallback, resource);
                --quota;
            }
        };

        for (int resourceIndex = 0; resourceIndex < 4; ++resourceIndex)
        {
            int bestSeed = -1;
            int bestScore = -1;
            for (size_t i = 0; i < m_Regions.size(); ++i)
            {
                if (claimed[i])
                {
                    continue;
                }

                int score = 0;
                const OverworldRegionData& region = m_Regions[i];
                for (int neighborId : region.m_AdjacentRegionIds)
                {
                    if (neighborId >= 0 && neighborId < regionCount && claimed[static_cast<size_t>(neighborId)])
                    {
                        ++score;
                    }
                }

                if (score < bestScore || bestSeed < 0)
                {
                    bestScore = score;
                    bestSeed = static_cast<int>(i);
                }
            }

            if (bestSeed < 0)
            {
                bestSeed = firstUnassignedIndex();
            }

            claimNeighbors(bestSeed, resourceTypes[resourceIndex], quotas[static_cast<size_t>(resourceIndex)]);
        }

        for (size_t i = 0; i < m_Regions.size(); ++i)
        {
            if (!assigned[i])
            {
                m_Regions[i].m_Resource = rollWeightedResource();
            }
        }

        return;
    }

    if (static_cast<int>(m_Regions.size()) >= 4)
    {
        assignResource(pickBestRegion([&](int regionIndex) -> int
        {
            return terrainStats[static_cast<size_t>(regionIndex)].treeCount;
        }), CountyResource::Wood);

        assignResource(pickBestRegion([&](int regionIndex) -> int
        {
            return terrainStats[static_cast<size_t>(regionIndex)].mountainNearby;
        }), CountyResource::Iron);

        assignResource(pickBestRegion([&](int regionIndex) -> int
        {
            const RegionTerrainStats& stats = terrainStats[static_cast<size_t>(regionIndex)];
            return stats.marshCount + stats.clearCount;
        }), CountyResource::Food);

        const int goldIndex = firstUnassignedIndex();
        assignResource(goldIndex, CountyResource::Gold);
    }

    for (size_t i = 0; i < m_Regions.size(); ++i)
    {
        if (assigned[i])
        {
            continue;
        }

        m_Regions[i].m_Resource = rollWeightedResource();
    }
}

void OverworldMap::AssignRegionCampaignState(RNG& rng, int enemyCount, int startingRegionsPerPlayer)
{
    for (OverworldRegionData& region : m_Regions)
    {
        region.m_OwnerId = -1;
        region.m_HasCastle = false;
    }

    if (m_Regions.empty())
    {
        return;
    }

    const int clampedEnemies = std::clamp(enemyCount, kMinOpponents, kMaxOpponents);
    const int playerCount = std::clamp(1 + clampedEnemies, 1, kMaxCampaignPlayers);
    const int regionsPerPlayer = std::max(startingRegionsPerPlayer, 1);

    auto squaredDistance = [](const OverworldRegionData& a, const OverworldRegionData& b) -> int
    {
        const int dx = a.m_SeedX - b.m_SeedX;
        const int dy = a.m_SeedY - b.m_SeedY;
        return (dx * dx) + (dy * dy);
    };

    auto pickSeedRegion = [&]() -> int
    {
        std::vector<int> unownedRegionIds;
        unownedRegionIds.reserve(m_Regions.size());
        for (const OverworldRegionData& candidate : m_Regions)
        {
            if (candidate.m_OwnerId < 0)
            {
                unownedRegionIds.push_back(candidate.m_Id);
            }
        }

        if (unownedRegionIds.empty())
        {
            return -1;
        }

        bool anyOwned = false;
        for (const OverworldRegionData& region : m_Regions)
        {
            if (region.m_OwnerId >= 0)
            {
                anyOwned = true;
                break;
            }
        }

        if (!anyOwned)
        {
            return unownedRegionIds[static_cast<size_t>(rng.Random(static_cast<unsigned int>(unownedRegionIds.size())))];
        }

        int bestRegionId = -1;
        int bestScore = -1;
        for (const int candidateId : unownedRegionIds)
        {
            const OverworldRegionData* candidate = GetRegion(candidateId);
            if (!candidate)
            {
                continue;
            }

            int nearestOwnedDistance = OVERWORLD_MAP_SIZE * OVERWORLD_MAP_SIZE;
            for (const OverworldRegionData& owned : m_Regions)
            {
                if (owned.m_OwnerId < 0)
                {
                    continue;
                }

                nearestOwnedDistance = std::min(nearestOwnedDistance, squaredDistance(*candidate, owned));
            }

            if (nearestOwnedDistance > bestScore)
            {
                bestScore = nearestOwnedDistance;
                bestRegionId = candidateId;
            }
        }

        return bestRegionId;
    };

    auto claimContiguousTerritory = [&](int ownerId, int regionQuota) -> bool
    {
        const int seedRegionId = pickSeedRegion();
        if (seedRegionId < 0)
        {
            return false;
        }

        std::queue<int> open;
        open.push(seedRegionId);
        int claimed = 0;

        while (!open.empty() && claimed < regionQuota)
        {
            const int regionId = open.front();
            open.pop();

            OverworldRegionData* region = GetRegion(regionId);
            if (!region || region->m_OwnerId >= 0)
            {
                continue;
            }

            region->m_OwnerId = ownerId;
            if (claimed == 0)
            {
                region->m_HasCastle = true;
            }
            ++claimed;

            std::vector<int> neighbors = region->m_AdjacentRegionIds;
            for (size_t i = neighbors.size(); i > 1; --i)
            {
                const size_t swapIndex = static_cast<size_t>(rng.Random(static_cast<unsigned int>(i)));
                std::swap(neighbors[i - 1], neighbors[swapIndex]);
            }

            for (int neighborId : neighbors)
            {
                const OverworldRegionData* neighbor = GetRegion(neighborId);
                if (neighbor && neighbor->m_OwnerId < 0)
                {
                    open.push(neighborId);
                }
            }
        }

        return claimed > 0;
    };

    for (int ownerId = 0; ownerId < playerCount; ++ownerId)
    {
        claimContiguousTerritory(ownerId, regionsPerPlayer);
    }
}

void OverworldMap::Generate(unsigned int seed, const CampaignSetup& setup)
{
    Clear();
    m_Seed = seed;
    if (m_Seed == 0)
    {
        m_Seed = 1;
    }

    CampaignSetup resolvedSetup = setup;
    ClampCampaignSetup(resolvedSetup);

    RNG rng;
    rng.SeedRNG(m_Seed);

    GenerateTerrain(rng);
    PartitionIntoRegions(rng, MapSizeRegionCount(resolvedSetup.m_MapSize));
    BuildAdjacency();
    AssignRegionResources(rng, static_cast<int>(resolvedSetup.m_ResourceDistribution));
    AssignRegionCampaignState(rng, resolvedSetup.m_EnemyCount, MapSizeStartingRegions(resolvedSetup.m_MapSize));

    m_Generated = true;
}

void OverworldMap::Draw(int x, int y, int pixelsPerCell) const
{
    if (!m_Generated || pixelsPerCell <= 0)
    {
        return;
    }

    const int mapPixelWidth = OVERWORLD_MAP_SIZE * pixelsPerCell;
    const int mapPixelHeight = OVERWORLD_MAP_SIZE * pixelsPerCell;
    constexpr int kRegionBorderWidth = 2;
    const Color outerBorderColor = Color{ 48, 48, 56, 255 };

    auto RegionTint = [](int regionId) -> Color
    {
        const unsigned char shade = static_cast<unsigned char>(90 + ((regionId * 47) % 70));
        return Color{ shade, shade, static_cast<unsigned char>(shade + 20), 50 };
    };

    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            const int pixelX = x + (cellX * pixelsPerCell);
            const int pixelY = y + (cellY * pixelsPerCell);
            Color color = CellColor(GetCell(cellX, cellY));

            const int regionId = GetRegionId(cellX, cellY);
            if (regionId >= 0)
            {
                const Color tint = RegionTint(regionId);
                color.r = static_cast<unsigned char>((color.r + tint.r) / 2);
                color.g = static_cast<unsigned char>((color.g + tint.g) / 2);
                color.b = static_cast<unsigned char>((color.b + tint.b) / 2);
            }

            DrawRectangle(pixelX, pixelY, pixelsPerCell, pixelsPerCell, color);
        }
    }

    auto BorderColorForRegion = [this](int regionId) -> Color
    {
        if (regionId < 0)
        {
            return PlayerOwnerColor(-1);
        }

        const OverworldRegionData* region = GetRegion(regionId);
        if (!region)
        {
            return PlayerOwnerColor(-1);
        }

        return PlayerOwnerColor(region->m_OwnerId);
    };

    auto IsCountyCell = [this](int cellX, int cellY) -> bool
    {
        return GetRegionId(cellX, cellY) >= 0;
    };

    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            const int regionId = GetRegionId(cellX, cellY);
            const int pixelX = x + (cellX * pixelsPerCell);
            const int pixelY = y + (cellY * pixelsPerCell);

            if (cellX + 1 < OVERWORLD_MAP_SIZE)
            {
                const int rightRegionId = GetRegionId(cellX + 1, cellY);
                if (IsCountyCell(cellX, cellY) && IsCountyCell(cellX + 1, cellY) && regionId != rightRegionId)
                {
                    const int boundaryX = x + ((cellX + 1) * pixelsPerCell);
                    DrawRectangle(boundaryX - 1, pixelY, kRegionBorderWidth, pixelsPerCell, outerBorderColor);
                }
            }

            if (cellY + 1 < OVERWORLD_MAP_SIZE)
            {
                const int downRegionId = GetRegionId(cellX, cellY + 1);
                if (IsCountyCell(cellX, cellY) && IsCountyCell(cellX, cellY + 1) && regionId != downRegionId)
                {
                    const int boundaryY = y + ((cellY + 1) * pixelsPerCell);
                    DrawRectangle(pixelX, boundaryY - 1, pixelsPerCell, kRegionBorderWidth, outerBorderColor);
                }
            }
        }
    }

    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            const int regionId = GetRegionId(cellX, cellY);
            if (regionId < 0)
            {
                continue;
            }

            const Color borderColor = BorderColorForRegion(regionId);
            const int pixelX = x + (cellX * pixelsPerCell);
            const int pixelY = y + (cellY * pixelsPerCell);

            if (cellX + 1 < OVERWORLD_MAP_SIZE)
            {
                const int rightRegionId = GetRegionId(cellX + 1, cellY);
                if (IsCountyCell(cellX + 1, cellY) && regionId != rightRegionId)
                {
                    const int boundaryX = x + ((cellX + 1) * pixelsPerCell);
                    DrawRectangle(boundaryX - 1, pixelY, 1, pixelsPerCell, borderColor);
                }
            }

            if (cellY + 1 < OVERWORLD_MAP_SIZE)
            {
                const int downRegionId = GetRegionId(cellX, cellY + 1);
                if (IsCountyCell(cellX, cellY + 1) && regionId != downRegionId)
                {
                    const int boundaryY = y + ((cellY + 1) * pixelsPerCell);
                    DrawRectangle(pixelX, boundaryY - 1, pixelsPerCell, 1, borderColor);
                }
            }

            if (cellX > 0)
            {
                const int leftRegionId = GetRegionId(cellX - 1, cellY);
                if (IsCountyCell(cellX - 1, cellY) && regionId != leftRegionId)
                {
                    const int boundaryX = x + (cellX * pixelsPerCell);
                    DrawRectangle(boundaryX, pixelY, 1, pixelsPerCell, borderColor);
                }
            }

            if (cellY > 0)
            {
                const int upRegionId = GetRegionId(cellX, cellY - 1);
                if (IsCountyCell(cellX, cellY - 1) && regionId != upRegionId)
                {
                    const int boundaryY = y + (cellY * pixelsPerCell);
                    DrawRectangle(pixelX, boundaryY, pixelsPerCell, 1, borderColor);
                }
            }
        }
    }

    if (g_smallFont)
    {
        for (const OverworldRegionData& region : m_Regions)
        {
            const std::string label(1, CountyResourceMarker(region.m_Resource));
            const float centerX = static_cast<float>(x) + (static_cast<float>(region.m_SeedX) * static_cast<float>(pixelsPerCell))
                + (static_cast<float>(pixelsPerCell) * 0.5f);
            const float centerY = static_cast<float>(y) + (static_cast<float>(region.m_SeedY) * static_cast<float>(pixelsPerCell))
                + (static_cast<float>(pixelsPerCell) * 0.5f);
            const Vector2 textSize = MeasureTextEx(*g_smallFont, label.c_str(), g_smallFontDrawSize, 1);
            DrawOutlinedText(g_smallFont, label,
                Vector2{ centerX - (textSize.x * 0.5f), centerY - (textSize.y * 0.5f) },
                g_smallFontDrawSize, 1, WHITE);
        }
    }

    DrawRectangleLines(x, y, mapPixelWidth, mapPixelHeight, Color{ 0, 0, 0, 255 });
}

void OverworldMap::DrawRegionHighlight(int x, int y, int pixelsPerCell, int regionId) const
{
    if (!m_Generated || pixelsPerCell <= 0 || regionId < 0)
    {
        return;
    }

    const Color fill = Color{ 255, 230, 90, 90 };

    auto IsInteriorCell = [&](int cellX, int cellY) -> bool
    {
        if (GetRegionId(cellX, cellY) != regionId)
        {
            return false;
        }

        if (cellX > 0 && GetRegionId(cellX - 1, cellY) != regionId)
        {
            return false;
        }

        if (cellX + 1 < OVERWORLD_MAP_SIZE && GetRegionId(cellX + 1, cellY) != regionId)
        {
            return false;
        }

        if (cellY > 0 && GetRegionId(cellX, cellY - 1) != regionId)
        {
            return false;
        }

        if (cellY + 1 < OVERWORLD_MAP_SIZE && GetRegionId(cellX, cellY + 1) != regionId)
        {
            return false;
        }

        return true;
    };

    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            if (!IsInteriorCell(cellX, cellY))
            {
                continue;
            }

            const int pixelX = x + (cellX * pixelsPerCell);
            const int pixelY = y + (cellY * pixelsPerCell);
            DrawRectangle(pixelX, pixelY, pixelsPerCell, pixelsPerCell, fill);
        }
    }
}

void OverworldMap::DrawAdjacencyGraph(int x, int y, int width, int height, int selectedRegionId) const
{
    if (!m_Generated || width <= 0 || height <= 0 || m_Regions.empty())
    {
        return;
    }

    DrawRectangle(x, y, width, height, Color{ 32, 36, 46, 255 });
    DrawRectangleLines(x, y, width, height, Color{ 90, 90, 100, 255 });

    auto RegionColor = [](int regionId) -> Color
    {
        const unsigned char shade = static_cast<unsigned char>(90 + ((regionId * 47) % 70));
        return Color{ shade, shade, static_cast<unsigned char>(shade + 20), 255 };
    };

    constexpr float kPadding = 12.0f;
    const float usableWidth = static_cast<float>(width) - (2.0f * kPadding);
    const float usableHeight = static_cast<float>(height) - (2.0f * kPadding);
    const float mapScale = static_cast<float>(OVERWORLD_MAP_SIZE - 1);

    std::vector<Vector2> nodePositions(static_cast<size_t>(m_Regions.size()));
    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_Id < 0 || region.m_Id >= static_cast<int>(nodePositions.size()))
        {
            continue;
        }

        nodePositions[static_cast<size_t>(region.m_Id)] = Vector2{
            static_cast<float>(x) + kPadding + (static_cast<float>(region.m_SeedX) / mapScale) * usableWidth,
            static_cast<float>(y) + kPadding + (static_cast<float>(region.m_SeedY) / mapScale) * usableHeight
        };
    }

    const Color edgeColor = Color{ 210, 210, 175, 200 };
    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_Id < 0 || region.m_Id >= static_cast<int>(nodePositions.size()))
        {
            continue;
        }

        const Vector2 from = nodePositions[static_cast<size_t>(region.m_Id)];
        for (int neighborId : region.m_AdjacentRegionIds)
        {
            if (neighborId <= region.m_Id || neighborId < 0 || neighborId >= static_cast<int>(nodePositions.size()))
            {
                continue;
            }

            DrawLineEx(from, nodePositions[static_cast<size_t>(neighborId)], 1.0f, edgeColor);
        }
    }

    constexpr float kNodeRadius = 4.0f;
    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_Id < 0 || region.m_Id >= static_cast<int>(nodePositions.size()))
        {
            continue;
        }

        const Vector2 center = nodePositions[static_cast<size_t>(region.m_Id)];
        const bool isSelected = region.m_Id == selectedRegionId;
        const float nodeRadius = isSelected ? kNodeRadius + 2.0f : kNodeRadius;
        const Color fill = isSelected ? Color{ 255, 230, 90, 255 } : RegionColor(region.m_Id);
        DrawCircleV(center, nodeRadius, fill);
        DrawCircleLinesV(center, nodeRadius, isSelected ? Color{ 255, 255, 180, 255 } : Color{ 20, 20, 20, 255 });

        const char* label = TextFormat("%d", region.m_Id);
        const int labelWidth = MeasureText(label, 8);
        DrawText(label, static_cast<int>(center.x) - (labelWidth / 2), static_cast<int>(center.y) - 4, 8, BLACK);
    }
}