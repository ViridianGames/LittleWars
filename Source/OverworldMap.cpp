#include "OverworldMap.h"

#include "GameGlobals.h"
#include "MapTilesSprites.h"
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

    unsigned long long BorderKey(int regionA, int regionB)
    {
        if (regionA > regionB)
        {
            std::swap(regionA, regionB);
        }

        return (static_cast<unsigned long long>(static_cast<unsigned int>(regionA)) << 32)
            | static_cast<unsigned int>(regionB);
    }

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
    m_TargetConquerableRegions = 0;
    m_Cells.clear();
    m_RegionIds.clear();
    m_Regions.clear();
    m_BorderTypes.clear();
}

int OverworldMap::GetConquerableRegionCount() const
{
    int count = 0;
    for (const OverworldRegionData& region : m_Regions)
    {
        if (!region.m_IsWater)
        {
            ++count;
        }
    }

    return count;
}

int OverworldMap::GetLakeRegionCount() const
{
    int count = 0;
    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_IsWater)
        {
            ++count;
        }
    }

    return count;
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
    return type == OW_CLEAR;
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
    SculptCoastline(rng);
}

void OverworldMap::SculptCoastline(RNG& rng)
{
    constexpr int kMinCoastDepth = 6;
    constexpr int kMaxCoastDepth = 14;
    constexpr int kNoOceanConstraint = OVERWORLD_MAP_SIZE;

    const int oceanEdgeCount = rng.RandomRange(1, 4);
    bool oceanLeft = false;
    bool oceanRight = false;
    bool oceanTop = false;
    bool oceanBottom = false;

    std::vector<int> edges = { 0, 1, 2, 3 };
    for (int i = static_cast<int>(edges.size()); i > 1; --i)
    {
        const size_t swapIndex = static_cast<size_t>(rng.Random(static_cast<unsigned int>(i)));
        std::swap(edges[static_cast<size_t>(i - 1)], edges[swapIndex]);
    }

    for (int i = 0; i < oceanEdgeCount; ++i)
    {
        switch (edges[static_cast<size_t>(i)])
        {
        case 0:
            oceanLeft = true;
            break;
        case 1:
            oceanRight = true;
            break;
        case 2:
            oceanTop = true;
            break;
        case 3:
        default:
            oceanBottom = true;
            break;
        }
    }

    auto BuildCoastProfile = [&](int variation) -> std::vector<int>
    {
        std::vector<int> profile(static_cast<size_t>(OVERWORLD_MAP_SIZE));
        int depth = rng.RandomRange(kMinCoastDepth, kMaxCoastDepth + 1);
        for (int i = 0; i < OVERWORLD_MAP_SIZE; ++i)
        {
            depth = std::clamp(depth + rng.RandomRange(-variation, variation + 1), kMinCoastDepth, kMaxCoastDepth);
            const int wave = static_cast<int>(std::sin(static_cast<float>(i) * 0.17f) * 2.5f);
            profile[static_cast<size_t>(i)] = std::clamp(depth + wave, 4, kMaxCoastDepth + 3);
        }

        return profile;
    };

    const std::vector<int> leftCoast = oceanLeft ? BuildCoastProfile(2) : std::vector<int>();
    const std::vector<int> rightCoast = oceanRight ? BuildCoastProfile(2) : std::vector<int>();
    const std::vector<int> topCoast = oceanTop ? BuildCoastProfile(2) : std::vector<int>();
    const std::vector<int> bottomCoast = oceanBottom ? BuildCoastProfile(2) : std::vector<int>();

    for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
    {
        for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
        {
            const int inlandFromLeft = oceanLeft ? (x - leftCoast[static_cast<size_t>(y)]) : kNoOceanConstraint;
            const int inlandFromRight = oceanRight
                ? ((OVERWORLD_MAP_SIZE - 1 - x) - rightCoast[static_cast<size_t>(y)])
                : kNoOceanConstraint;
            const int inlandFromTop = oceanTop ? (y - topCoast[static_cast<size_t>(x)]) : kNoOceanConstraint;
            const int inlandFromBottom = oceanBottom
                ? ((OVERWORLD_MAP_SIZE - 1 - y) - bottomCoast[static_cast<size_t>(x)])
                : kNoOceanConstraint;
            const int inlandMargin = std::min({ inlandFromLeft, inlandFromRight, inlandFromTop, inlandFromBottom });

            const size_t cellIndex = static_cast<size_t>(GetCellIndex(x, y));
            if (inlandMargin < 0)
            {
                m_Cells[cellIndex] = OW_WATER;
                continue;
            }

            if (inlandMargin < 5 && rng.Random(100) < (58 - (inlandMargin * 11)))
            {
                m_Cells[cellIndex] = OW_WATER;
            }
        }
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        std::vector<OverworldCellType> nextCells = m_Cells;
        for (int y = 1; y < OVERWORLD_MAP_SIZE - 1; ++y)
        {
            for (int x = 1; x < OVERWORLD_MAP_SIZE; ++x)
            {
                const size_t cellIndex = static_cast<size_t>(GetCellIndex(x, y));
                if (m_Cells[cellIndex] == OW_WATER)
                {
                    continue;
                }

                bool nearOceanEdge = false;
                if (oceanLeft && x < kMaxCoastDepth + 5)
                {
                    nearOceanEdge = true;
                }
                if (oceanRight && x > OVERWORLD_MAP_SIZE - 1 - (kMaxCoastDepth + 5))
                {
                    nearOceanEdge = true;
                }
                if (oceanTop && y < kMaxCoastDepth + 5)
                {
                    nearOceanEdge = true;
                }
                if (oceanBottom && y > OVERWORLD_MAP_SIZE - 1 - (kMaxCoastDepth + 5))
                {
                    nearOceanEdge = true;
                }

                if (!nearOceanEdge)
                {
                    continue;
                }

                const int waterNeighbors = CountNeighborsOfType(m_Cells, x, y, OW_WATER);
                if (waterNeighbors >= 3 && rng.Random(100) < 42)
                {
                    nextCells[cellIndex] = OW_WATER;
                }
            }
        }

        m_Cells.swap(nextCells);
    }

    if (oceanTop)
    {
        for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
        {
            m_Cells[static_cast<size_t>(x)] = OW_WATER;
        }
    }
    if (oceanBottom)
    {
        for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
        {
            m_Cells[static_cast<size_t>((OVERWORLD_MAP_SIZE - 1) * OVERWORLD_MAP_SIZE + x)] = OW_WATER;
        }
    }
    if (oceanLeft)
    {
        for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
        {
            m_Cells[static_cast<size_t>(y * OVERWORLD_MAP_SIZE)] = OW_WATER;
        }
    }
    if (oceanRight)
    {
        for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
        {
            m_Cells[static_cast<size_t>(y * OVERWORLD_MAP_SIZE + (OVERWORLD_MAP_SIZE - 1))] = OW_WATER;
        }
    }
}

void OverworldMap::PartitionIntoRegions(RNG& rng, int targetRegionCount, int minKeptRegions)
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

    while (static_cast<int>(seeds.size()) < targetRegions && minSeedDistance >= 3)
    {
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

        if (static_cast<int>(seeds.size()) >= targetRegions)
        {
            break;
        }

        --minSeedDistance;
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

    const std::vector<OverworldRegionData> voronoiRegions = m_Regions;
    const int requiredKeptRegions = std::max(minKeptRegions, 1);

    int minCellsPerRegion = std::max(6, landCells / std::max(requiredKeptRegions * 5, 1));
    std::vector<int> oldToNewId(voronoiRegions.size(), -1);
    int keptRegionCount = 0;

    while (true)
    {
        keptRegionCount = 0;
        std::fill(oldToNewId.begin(), oldToNewId.end(), -1);

        for (size_t oldId = 0; oldId < voronoiRegions.size(); ++oldId)
        {
            if (voronoiRegions[oldId].m_CellCount < minCellsPerRegion)
            {
                continue;
            }

            oldToNewId[oldId] = keptRegionCount;
            ++keptRegionCount;
        }

        if (keptRegionCount >= requiredKeptRegions || minCellsPerRegion <= 4)
        {
            break;
        }

        --minCellsPerRegion;
    }

    std::vector<OverworldRegionData> keptRegions;
    keptRegions.reserve(static_cast<size_t>(keptRegionCount));
    for (size_t oldId = 0; oldId < voronoiRegions.size(); ++oldId)
    {
        if (oldToNewId[oldId] < 0)
        {
            continue;
        }

        OverworldRegionData region = voronoiRegions[oldId];
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
        if (regionId < 0)
        {
            continue;
        }
    }
}

void OverworldMap::DesignateWaterRegions(RNG& rng, int minConquerableRegions)
{
    if (m_Regions.empty())
    {
        return;
    }

    const int desiredWaterCount = std::max(1, static_cast<int>((m_Regions.size() * 25) / 100));
    const int maxWaterCount = std::max(0, static_cast<int>(m_Regions.size()) - std::max(minConquerableRegions, 0));
    const int waterCount = std::min(desiredWaterCount, maxWaterCount);
    if (waterCount <= 0)
    {
        return;
    }
    std::vector<int> candidates;
    candidates.reserve(m_Regions.size());

    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_IsWater || region.m_CellCount < 12)
        {
            continue;
        }

        const int edgeDistance = std::min({
            region.m_SeedX,
            region.m_SeedY,
            OVERWORLD_MAP_SIZE - 1 - region.m_SeedX,
            OVERWORLD_MAP_SIZE - 1 - region.m_SeedY
        });

        if (edgeDistance < 4)
        {
            continue;
        }

        candidates.push_back(region.m_Id);
    }

    if (candidates.empty())
    {
        for (const OverworldRegionData& region : m_Regions)
        {
            if (!region.m_IsWater && region.m_CellCount >= 8)
            {
                candidates.push_back(region.m_Id);
            }
        }
    }

    for (size_t i = candidates.size(); i > 1; --i)
    {
        const size_t swapIndex = static_cast<size_t>(rng.Random(static_cast<unsigned int>(i)));
        std::swap(candidates[i - 1], candidates[swapIndex]);
    }

    const int lakesToCreate = std::min(waterCount, static_cast<int>(candidates.size()));
    for (int i = 0; i < lakesToCreate; ++i)
    {
        OverworldRegionData* region = GetRegion(candidates[static_cast<size_t>(i)]);
        if (!region)
        {
            continue;
        }

        region->m_IsWater = true;
        region->m_OwnerId = -1;
        region->m_HasCastle = false;

        for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
        {
            for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
            {
                if (GetRegionId(x, y) != region->m_Id)
                {
                    continue;
                }

                m_Cells[static_cast<size_t>(GetCellIndex(x, y))] = OW_WATER;
            }
        }
    }
}

void OverworldMap::ClearLandRegionInteriors()
{
    for (int y = 0; y < OVERWORLD_MAP_SIZE; ++y)
    {
        for (int x = 0; x < OVERWORLD_MAP_SIZE; ++x)
        {
            const int regionId = GetRegionId(x, y);
            if (regionId < 0)
            {
                continue;
            }

            const OverworldRegionData* region = GetRegion(regionId);
            if (!region || region->m_IsWater)
            {
                continue;
            }

            m_Cells[static_cast<size_t>(GetCellIndex(x, y))] = OW_CLEAR;
        }
    }
}

RegionBorderType OverworldMap::GetBorderType(int regionA, int regionB) const
{
    if (regionA < 0 || regionB < 0 || regionA == regionB)
    {
        return RegionBorderType::Open;
    }

    const auto it = m_BorderTypes.find(BorderKey(regionA, regionB));
    if (it == m_BorderTypes.end())
    {
        return RegionBorderType::Open;
    }

    return it->second;
}

std::vector<int> OverworldMap::GetTraversableAdjacentRegions(int regionId) const
{
    const OverworldRegionData* region = GetRegion(regionId);
    if (!region || region->m_IsWater)
    {
        return {};
    }

    std::vector<int> traversable;
    traversable.reserve(region->m_AdjacentRegionIds.size());
    for (int neighborId : region->m_AdjacentRegionIds)
    {
        const OverworldRegionData* neighbor = GetRegion(neighborId);
        if (!neighbor || neighbor->m_IsWater)
        {
            continue;
        }

        if (GetBorderType(regionId, neighborId) != RegionBorderType::Mountain)
        {
            traversable.push_back(neighborId);
        }
    }

    return traversable;
}

void OverworldMap::CarveInterRegionMountains()
{
    for (size_t i = 0; i < m_Cells.size(); ++i)
    {
        if (m_RegionIds[i] >= 0 || m_Cells[i] == OW_WATER)
        {
            continue;
        }

        m_Cells[i] = OW_MOUNTAIN;
    }

    std::vector<bool> carve(static_cast<size_t>(kCellCount), false);
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

            const OverworldRegionData* region = GetRegion(regionId);
            if (!region || region->m_IsWater)
            {
                continue;
            }

            for (const auto& offset : offsets)
            {
                const int neighborX = x + offset[0];
                const int neighborY = y + offset[1];
                if (!IsInBounds(neighborX, neighborY))
                {
                    continue;
                }

                const int neighborRegionId = GetRegionId(neighborX, neighborY);
                if (neighborRegionId < 0 || neighborRegionId == regionId)
                {
                    continue;
                }

                const OverworldRegionData* neighbor = GetRegion(neighborRegionId);
                if (!neighbor || neighbor->m_IsWater)
                {
                    continue;
                }

                if (regionId >= neighborRegionId)
                {
                    continue;
                }

                if (GetBorderType(regionId, neighborRegionId) != RegionBorderType::Mountain)
                {
                    continue;
                }

                carve[static_cast<size_t>(GetCellIndex(neighborX, neighborY))] = true;
            }
        }
    }

    for (size_t i = 0; i < carve.size(); ++i)
    {
        if (!carve[i] || m_Cells[i] == OW_WATER)
        {
            continue;
        }

        m_Cells[i] = OW_MOUNTAIN;
        m_RegionIds[i] = -1;
    }
}

void OverworldMap::RecalculateRegionCellCounts()
{
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
}

void OverworldMap::AssignRegionBorders(RNG& rng)
{
    m_BorderTypes.clear();

    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_IsWater)
        {
            continue;
        }

        for (int neighborId : region.m_AdjacentRegionIds)
        {
            if (neighborId < 0 || neighborId <= region.m_Id)
            {
                continue;
            }

            const OverworldRegionData* neighbor = GetRegion(neighborId);
            if (!neighbor || neighbor->m_IsWater)
            {
                continue;
            }

            RegionBorderType borderType = RegionBorderType::Open;
            if (rng.Random(100) < 18)
            {
                borderType = RegionBorderType::Mountain;
            }

            m_BorderTypes[BorderKey(region.m_Id, neighborId)] = borderType;
        }
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
        if (region.m_IsWater || region.m_Id < 0 || region.m_Id >= static_cast<int>(terrainStats.size()))
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

    auto firstUnassignedIndex = [&]() -> int
    {
        for (size_t i = 0; i < assigned.size(); ++i)
        {
            if (m_Regions[i].m_IsWater || assigned[static_cast<size_t>(i)])
            {
                continue;
            }

            return static_cast<int>(i);
        }

        return -1;
    };

    auto pickBestRegion = [&](auto scoreForRegion) -> int
    {
        int bestIndex = -1;
        int bestScore = -1;

        for (size_t i = 0; i < m_Regions.size(); ++i)
        {
            if (m_Regions[i].m_IsWater || assigned[i])
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

        if (m_Regions[static_cast<size_t>(regionIndex)].m_IsWater)
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
            if (!m_Regions[i].m_IsWater)
            {
                regionIndices.push_back(static_cast<int>(i));
            }
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

        int landRegionCount = 0;
        for (const OverworldRegionData& region : m_Regions)
        {
            if (!region.m_IsWater)
            {
                ++landRegionCount;
            }
        }

        std::vector<int> quotas(4, landRegionCount / 4);
        for (int i = 0; i < landRegionCount % 4; ++i)
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

                if (regionId < 0 || regionId >= static_cast<int>(m_Regions.size()) || claimed[static_cast<size_t>(regionId)])
                {
                    continue;
                }

                const OverworldRegionData* region = GetRegion(regionId);
                if (!region || region->m_IsWater)
                {
                    continue;
                }

                claimed[static_cast<size_t>(regionId)] = true;
                assignResource(regionId, resource);
                --quota;

                for (int neighborId : region->m_AdjacentRegionIds)
                {
                    const OverworldRegionData* neighbor = GetRegion(neighborId);
                    if (neighbor && !neighbor->m_IsWater && neighborId >= 0
                        && neighborId < static_cast<int>(m_Regions.size())
                        && !claimed[static_cast<size_t>(neighborId)])
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
                if (m_Regions[i].m_IsWater || claimed[i])
                {
                    continue;
                }

                int score = 0;
                const OverworldRegionData& region = m_Regions[i];
                for (int neighborId : region.m_AdjacentRegionIds)
                {
                    if (neighborId >= 0 && neighborId < static_cast<int>(m_Regions.size())
                        && claimed[static_cast<size_t>(neighborId)])
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

    auto countAvailableInComponent = [&](int seedRegionId) -> int
    {
        const OverworldRegionData* seed = GetRegion(seedRegionId);
        if (!seed || seed->m_IsWater || seed->m_OwnerId >= 0)
        {
            return 0;
        }

        std::queue<int> open;
        std::unordered_set<int> visited;
        open.push(seedRegionId);
        visited.insert(seedRegionId);
        int count = 0;

        while (!open.empty())
        {
            const int regionId = open.front();
            open.pop();

            const OverworldRegionData* region = GetRegion(regionId);
            if (!region || region->m_IsWater || region->m_OwnerId >= 0)
            {
                continue;
            }

            ++count;

            for (int neighborId : GetTraversableAdjacentRegions(regionId))
            {
                if (!visited.insert(neighborId).second)
                {
                    continue;
                }

                const OverworldRegionData* neighbor = GetRegion(neighborId);
                if (!neighbor || neighbor->m_IsWater || neighbor->m_OwnerId >= 0)
                {
                    continue;
                }

                open.push(neighborId);
            }
        }

        return count;
    };

    auto pickSeedRegion = [&](const std::unordered_set<int>& excludedSeeds) -> int
    {
        std::vector<int> candidates;
        candidates.reserve(m_Regions.size());
        for (const OverworldRegionData& candidate : m_Regions)
        {
            if (candidate.m_IsWater || candidate.m_OwnerId >= 0 || excludedSeeds.count(candidate.m_Id) > 0)
            {
                continue;
            }

            candidates.push_back(candidate.m_Id);
        }

        if (candidates.empty())
        {
            return -1;
        }

        std::vector<int> viable;
        viable.reserve(candidates.size());
        for (int candidateId : candidates)
        {
            if (countAvailableInComponent(candidateId) >= regionsPerPlayer)
            {
                viable.push_back(candidateId);
            }
        }

        const std::vector<int>* pool = &candidates;
        if (!viable.empty())
        {
            pool = &viable;
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
            if (!viable.empty())
            {
                return viable[static_cast<size_t>(rng.Random(static_cast<unsigned int>(viable.size())))];
            }

            int bestRegionId = -1;
            int bestAvailable = -1;
            for (int candidateId : candidates)
            {
                const int available = countAvailableInComponent(candidateId);
                if (available > bestAvailable)
                {
                    bestAvailable = available;
                    bestRegionId = candidateId;
                }
            }

            return bestRegionId;
        }

        int bestRegionId = -1;
        int bestScore = -1;
        for (const int candidateId : *pool)
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

    auto unclaimPlayerRegions = [&](int ownerId)
    {
        for (OverworldRegionData& region : m_Regions)
        {
            if (region.m_OwnerId == ownerId)
            {
                region.m_OwnerId = -1;
                region.m_HasCastle = false;
            }
        }
    };

    auto claimFromSeed = [&](int ownerId, int seedRegionId, int regionQuota) -> int
    {
        if (seedRegionId < 0)
        {
            return 0;
        }

        std::queue<int> open;
        std::unordered_set<int> visited;
        open.push(seedRegionId);
        visited.insert(seedRegionId);
        int claimed = 0;

        while (!open.empty() && claimed < regionQuota)
        {
            const int regionId = open.front();
            open.pop();

            OverworldRegionData* region = GetRegion(regionId);
            if (!region || region->m_IsWater || region->m_OwnerId >= 0)
            {
                continue;
            }

            region->m_OwnerId = ownerId;
            if (claimed == 0)
            {
                region->m_HasCastle = true;
            }
            ++claimed;

            std::vector<int> neighbors = GetTraversableAdjacentRegions(regionId);
            for (size_t i = neighbors.size(); i > 1; --i)
            {
                const size_t swapIndex = static_cast<size_t>(rng.Random(static_cast<unsigned int>(i)));
                std::swap(neighbors[i - 1], neighbors[swapIndex]);
            }

            for (int neighborId : neighbors)
            {
                if (!visited.insert(neighborId).second)
                {
                    continue;
                }

                const OverworldRegionData* neighbor = GetRegion(neighborId);
                if (!neighbor || neighbor->m_IsWater || neighbor->m_OwnerId >= 0)
                {
                    continue;
                }

                open.push(neighborId);
            }
        }

        return claimed;
    };

    constexpr int kMaxPlacementAttempts = 48;
    for (int ownerId = 0; ownerId < playerCount; ++ownerId)
    {
        std::unordered_set<int> excludedSeeds;
        for (int attempt = 0; attempt < kMaxPlacementAttempts; ++attempt)
        {
            const int seedRegionId = pickSeedRegion(excludedSeeds);
            if (seedRegionId < 0)
            {
                break;
            }

            const int claimed = claimFromSeed(ownerId, seedRegionId, regionsPerPlayer);
            if (claimed >= regionsPerPlayer)
            {
                break;
            }

            unclaimPlayerRegions(ownerId);
            excludedSeeds.insert(seedRegionId);
        }
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

    const int landRegionTarget = MapSizeRegionCount(resolvedSetup.m_MapSize);
    m_TargetConquerableRegions = landRegionTarget;
    const int minKeptRegions = static_cast<int>(std::ceil(landRegionTarget / 0.75f));
    const int partitionTarget = static_cast<int>(std::ceil(minKeptRegions * 1.25f));

    for (int attempt = 0; attempt < 6; ++attempt)
    {
        PartitionIntoRegions(rng, partitionTarget + (attempt * 6), minKeptRegions);
        if (static_cast<int>(m_Regions.size()) >= minKeptRegions)
        {
            break;
        }
    }

    DesignateWaterRegions(rng, landRegionTarget);
    ClearLandRegionInteriors();
    CarveInterRegionMountains();
    BuildAdjacency();
    AssignRegionBorders(rng);
    CarveInterRegionMountains();
    RecalculateRegionCellCounts();
    BuildAdjacency();
    AssignRegionResources(rng, static_cast<int>(resolvedSetup.m_ResourceDistribution));
    AssignRegionCampaignState(rng, resolvedSetup.m_EnemyCount, MapSizeStartingRegions(resolvedSetup.m_MapSize));

    m_Generated = true;
}

void OverworldMap::Draw(int x, int y, int pixelsPerCell, int selectedRegionId) const
{
    if (!m_Generated || pixelsPerCell <= 0)
    {
        return;
    }

    const int mapPixelWidth = OVERWORLD_MAP_SIZE * pixelsPerCell;
    const int mapPixelHeight = OVERWORLD_MAP_SIZE * pixelsPerCell;
    constexpr int kRegionBorderWidth = 1;
    constexpr int kMountainBorderWidth = 2;
    const Color unownedRegionBorderColor = WHITE;
    const Color mountainBorderColor = CellColor(OW_MOUNTAIN);

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
            const OverworldCellType cellType = GetCell(cellX, cellY);
            if (regionId >= 0 && cellType == OW_CLEAR)
            {
                const OverworldRegionData* region = GetRegion(regionId);
                if (region && !region->m_IsWater)
                {
                    const Color tint = RegionTint(regionId);
                    color.r = static_cast<unsigned char>((color.r + tint.r) / 2);
                    color.g = static_cast<unsigned char>((color.g + tint.g) / 2);
                    color.b = static_cast<unsigned char>((color.b + tint.b) / 2);
                }
            }

            DrawRectangle(pixelX, pixelY, pixelsPerCell, pixelsPerCell, color);
        }
    }

    auto IsLandRegion = [this](int regionId) -> bool
    {
        if (regionId < 0)
        {
            return false;
        }

        const OverworldRegionData* region = GetRegion(regionId);
        return region && !region->m_IsWater;
    };

    constexpr int kFortifiedBorderWidth = 2;

    std::vector<bool> fortifiedRegions(m_Regions.size(), false);
    for (const OverworldRegionData& region : m_Regions)
    {
        if (!region.m_HasCastle)
        {
            continue;
        }

        fortifiedRegions[static_cast<size_t>(region.m_Id)] = true;
        for (int neighborId : GetTraversableAdjacentRegions(region.m_Id))
        {
            if (neighborId >= 0 && neighborId < static_cast<int>(fortifiedRegions.size()))
            {
                fortifiedRegions[static_cast<size_t>(neighborId)] = true;
            }
        }
    }

    auto IsFortifiedRegion = [&](int regionId) -> bool
    {
        return regionId >= 0
            && regionId < static_cast<int>(fortifiedRegions.size())
            && fortifiedRegions[static_cast<size_t>(regionId)];
    };

    auto IsMountainBorderBetween = [&](int regionA, int regionB) -> bool
    {
        return IsLandRegion(regionA)
            && IsLandRegion(regionB)
            && regionA != regionB
            && GetBorderType(regionA, regionB) == RegionBorderType::Mountain;
    };

    auto IsMountainBorderEdge = [&](int landRegionId, int cellX, int cellY, int deltaX, int deltaY) -> bool
    {
        const int towardX = cellX + deltaX;
        const int towardY = cellY + deltaY;
        if (!IsInBounds(towardX, towardY) || GetCell(towardX, towardY) != OW_MOUNTAIN)
        {
            return false;
        }

        int probeX = towardX + deltaX;
        int probeY = towardY + deltaY;
        while (IsInBounds(probeX, probeY) && GetCell(probeX, probeY) == OW_MOUNTAIN)
        {
            probeX += deltaX;
            probeY += deltaY;
        }

        if (!IsInBounds(probeX, probeY))
        {
            return false;
        }

        const int beyondRegionId = GetRegionId(probeX, probeY);
        return IsMountainBorderBetween(landRegionId, beyondRegionId);
    };

    auto ResolveBorderWidth = [&](int regionA, int regionB) -> int
    {
        if (IsMountainBorderBetween(regionA, regionB))
        {
            return kMountainBorderWidth;
        }

        if (IsFortifiedRegion(regionA) || IsFortifiedRegion(regionB))
        {
            return kFortifiedBorderWidth;
        }

        return kRegionBorderWidth;
    };

    auto ResolveRegionEdgeBorderColor = [&](int landRegionId, int otherRegionId, bool& outShouldDraw) -> Color
    {
        outShouldDraw = false;
        const OverworldRegionData* landRegion = GetRegion(landRegionId);
        if (!landRegion || landRegion->m_OwnerId < 0)
        {
            return unownedRegionBorderColor;
        }

        if (!IsLandRegion(otherRegionId) || landRegionId != otherRegionId)
        {
            outShouldDraw = true;
            return PlayerOwnerColor(landRegion->m_OwnerId);
        }

        return unownedRegionBorderColor;
    };

    auto ResolveHorizontalBorderColor = [&](int leftRegionId, int rightRegionId, bool& outShouldDraw) -> Color
    {
        outShouldDraw = false;

        if (IsLandRegion(leftRegionId))
        {
            bool shouldDrawLeftEdge = false;
            const Color leftColor = ResolveRegionEdgeBorderColor(leftRegionId, rightRegionId, shouldDrawLeftEdge);
            if (shouldDrawLeftEdge)
            {
                outShouldDraw = true;
                return leftColor;
            }
        }

        if (IsLandRegion(rightRegionId))
        {
            bool shouldDrawRightEdge = false;
            const Color rightColor = ResolveRegionEdgeBorderColor(rightRegionId, leftRegionId, shouldDrawRightEdge);
            if (shouldDrawRightEdge)
            {
                outShouldDraw = true;
                return rightColor;
            }
        }

        if (IsLandRegion(leftRegionId) && IsLandRegion(rightRegionId) && leftRegionId != rightRegionId)
        {
            outShouldDraw = true;
            return unownedRegionBorderColor;
        }

        return unownedRegionBorderColor;
    };

    auto ResolveVerticalBorderColor = [&](int topRegionId, int bottomRegionId, bool& outShouldDraw) -> Color
    {
        return ResolveHorizontalBorderColor(topRegionId, bottomRegionId, outShouldDraw);
    };

    auto DrawOwnedMapEdgeBorder = [&](int pixelX, int pixelY, int drawWidth, int drawHeight,
        const OverworldRegionData& region)
    {
        if (region.m_OwnerId < 0)
        {
            return;
        }

        DrawRectangle(
            pixelX,
            pixelY,
            drawWidth,
            drawHeight,
            PlayerOwnerColor(region.m_OwnerId));
    };

    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            const int regionId = GetRegionId(cellX, cellY);
            if (!IsLandRegion(regionId))
            {
                continue;
            }

            const int pixelX = x + (cellX * pixelsPerCell);
            const int pixelY = y + (cellY * pixelsPerCell);
            const OverworldRegionData* region = GetRegion(regionId);
            if (!region)
            {
                continue;
            }

            const int edgeBorderWidth = IsFortifiedRegion(regionId) ? kFortifiedBorderWidth : kRegionBorderWidth;

            if (cellX == 0)
            {
                DrawOwnedMapEdgeBorder(
                    pixelX,
                    pixelY,
                    edgeBorderWidth,
                    pixelsPerCell,
                    *region);
            }

            if (cellY == 0)
            {
                DrawOwnedMapEdgeBorder(
                    pixelX,
                    pixelY,
                    pixelsPerCell,
                    edgeBorderWidth,
                    *region);
            }

            if (cellX + 1 < OVERWORLD_MAP_SIZE)
            {
                const int rightRegionId = GetRegionId(cellX + 1, cellY);
                const int boundaryX = x + ((cellX + 1) * pixelsPerCell);

                if (IsMountainBorderEdge(regionId, cellX, cellY, 1, 0))
                {
                    DrawRectangle(
                        boundaryX - kMountainBorderWidth,
                        pixelY,
                        kMountainBorderWidth,
                        pixelsPerCell,
                        mountainBorderColor);
                }
                else if (IsMountainBorderBetween(regionId, rightRegionId))
                {
                    DrawRectangle(
                        boundaryX - kMountainBorderWidth,
                        pixelY,
                        kMountainBorderWidth,
                        pixelsPerCell,
                        mountainBorderColor);
                }
                else
                {
                    bool shouldDrawBorder = false;
                    const Color borderColor = ResolveHorizontalBorderColor(regionId, rightRegionId, shouldDrawBorder);
                    if (shouldDrawBorder)
                    {
                        const int borderWidth = ResolveBorderWidth(regionId, rightRegionId);
                        DrawRectangle(boundaryX - borderWidth, pixelY, borderWidth, pixelsPerCell, borderColor);
                    }
                }
            }
            else
            {
                const int boundaryX = x + ((cellX + 1) * pixelsPerCell);
                DrawOwnedMapEdgeBorder(
                    boundaryX - edgeBorderWidth,
                    pixelY,
                    edgeBorderWidth,
                    pixelsPerCell,
                    *region);
            }

            if (cellY + 1 < OVERWORLD_MAP_SIZE)
            {
                const int downRegionId = GetRegionId(cellX, cellY + 1);
                const int boundaryY = y + ((cellY + 1) * pixelsPerCell);

                if (IsMountainBorderEdge(regionId, cellX, cellY, 0, 1))
                {
                    DrawRectangle(
                        pixelX,
                        boundaryY - kMountainBorderWidth,
                        pixelsPerCell,
                        kMountainBorderWidth,
                        mountainBorderColor);
                }
                else if (IsMountainBorderBetween(regionId, downRegionId))
                {
                    DrawRectangle(
                        pixelX,
                        boundaryY - kMountainBorderWidth,
                        pixelsPerCell,
                        kMountainBorderWidth,
                        mountainBorderColor);
                }
                else
                {
                    bool shouldDrawBorder = false;
                    const Color borderColor = ResolveVerticalBorderColor(regionId, downRegionId, shouldDrawBorder);
                    if (shouldDrawBorder)
                    {
                        const int borderWidth = ResolveBorderWidth(regionId, downRegionId);
                        DrawRectangle(pixelX, boundaryY - borderWidth, pixelsPerCell, borderWidth, borderColor);
                    }
                }
            }
            else
            {
                const int boundaryY = y + ((cellY + 1) * pixelsPerCell);
                DrawOwnedMapEdgeBorder(
                    pixelX,
                    boundaryY - edgeBorderWidth,
                    pixelsPerCell,
                    edgeBorderWidth,
                    *region);
            }
        }
    }

    if (selectedRegionId >= 0)
    {
        DrawRegionHighlight(x, y, pixelsPerCell, selectedRegionId);
    }

    struct RegionCellBounds
    {
        int m_MinX = OVERWORLD_MAP_SIZE;
        int m_MinY = OVERWORLD_MAP_SIZE;
        int m_MaxX = -1;
        int m_MaxY = -1;

        bool IsValid() const
        {
            return m_MaxX >= m_MinX && m_MaxY >= m_MinY;
        }
    };

    std::vector<RegionCellBounds> regionCellBounds(m_Regions.size());
    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            const int regionId = GetRegionId(cellX, cellY);
            if (regionId < 0 || regionId >= static_cast<int>(regionCellBounds.size()))
            {
                continue;
            }

            const OverworldRegionData* region = GetRegion(regionId);
            if (!region || region->m_IsWater)
            {
                continue;
            }

            RegionCellBounds& bounds = regionCellBounds[static_cast<size_t>(regionId)];
            bounds.m_MinX = std::min(bounds.m_MinX, cellX);
            bounds.m_MinY = std::min(bounds.m_MinY, cellY);
            bounds.m_MaxX = std::max(bounds.m_MaxX, cellX);
            bounds.m_MaxY = std::max(bounds.m_MaxY, cellY);
        }
    }

    auto RegionCenterPixels = [&](const OverworldRegionData& region) -> Vector2
    {
        if (region.m_Id >= 0 && region.m_Id < static_cast<int>(regionCellBounds.size()))
        {
            const RegionCellBounds& bounds = regionCellBounds[static_cast<size_t>(region.m_Id)];
            if (bounds.IsValid())
            {
                const int centerPixelX = x + (((bounds.m_MinX + bounds.m_MaxX + 1) * pixelsPerCell) / 2);
                const int centerPixelY = y + (((bounds.m_MinY + bounds.m_MaxY + 1) * pixelsPerCell) / 2);
                return Vector2{ static_cast<float>(centerPixelX), static_cast<float>(centerPixelY) };
            }
        }

        return Vector2{
            static_cast<float>(x + (region.m_SeedX * pixelsPerCell) + (pixelsPerCell / 2)),
            static_cast<float>(y + (region.m_SeedY * pixelsPerCell) + (pixelsPerCell / 2))
        };
    };

    constexpr int kResourceIconUpOffset = 1;
    for (const OverworldRegionData& region : m_Regions)
    {
        if (region.m_IsWater)
        {
            continue;
        }

        const Vector2 regionCenter = RegionCenterPixels(region);
        const int centerX = static_cast<int>(regionCenter.x);
        const int centerY = static_cast<int>(regionCenter.y);

        DrawRegionResourceIcon(
            region.m_Resource,
            Vector2{ static_cast<float>(centerX), static_cast<float>(centerY - kResourceIconUpOffset) },
            WHITE);
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

    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            if (GetRegionId(cellX, cellY) != regionId)
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
    DrawAccessibilityGrid(x, y, width, height, selectedRegionId);
}

void OverworldMap::DrawAccessibilityGrid(int x, int y, int width, int height, int selectedRegionId) const
{
    if (!m_Generated || width <= 0 || height <= 0 || m_Regions.empty())
    {
        return;
    }

    DrawRectangle(x, y, width, height, Color{ 32, 36, 46, 255 });
    DrawRectangleLines(x, y, width, height, Color{ 90, 90, 100, 255 });

    const int landRegionCount = GetConquerableRegionCount();
    const int lakeRegionCount = GetLakeRegionCount();
    const int targetRegionCount = m_TargetConquerableRegions;

    if (g_font)
    {
        DrawOutlinedText(g_font, "Accessibility", Vector2{ static_cast<float>(x + 4), static_cast<float>(y + 3) },
            g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });
    }

    if (g_smallFont)
    {
        const Color landCountColor = landRegionCount >= targetRegionCount
            ? Color{ 140, 210, 150, 255 }
            : Color{ 230, 120, 120, 255 };
        const char* countLine = TextFormat("Land %d  Lakes %d  Target %d", landRegionCount, lakeRegionCount, targetRegionCount);
        DrawOutlinedText(g_smallFont, countLine,
            Vector2{ static_cast<float>(x + 4), static_cast<float>(y + 16) },
            g_smallFontDrawSize, 1, landCountColor);
    }

    auto RegionColor = [](int regionId) -> Color
    {
        const unsigned char shade = static_cast<unsigned char>(90 + ((regionId * 47) % 70));
        return Color{ shade, shade, static_cast<unsigned char>(shade + 20), 255 };
    };

    auto EdgeColorForBorder = [](RegionBorderType borderType) -> Color
    {
        if (borderType == RegionBorderType::Mountain)
        {
            return Color{ 170, 80, 80, 220 };
        }

        return Color{ 210, 210, 175, 200 };
    };

    std::vector<bool> reachable(static_cast<size_t>(m_Regions.size()), false);
    if (selectedRegionId >= 0 && selectedRegionId < static_cast<int>(m_Regions.size()))
    {
        std::queue<int> open;
        reachable[static_cast<size_t>(selectedRegionId)] = true;
        open.push(selectedRegionId);

        while (!open.empty())
        {
            const int regionId = open.front();
            open.pop();

            for (int neighborId : GetTraversableAdjacentRegions(regionId))
            {
                if (neighborId < 0 || neighborId >= static_cast<int>(reachable.size()))
                {
                    continue;
                }

                if (reachable[static_cast<size_t>(neighborId)])
                {
                    continue;
                }

                reachable[static_cast<size_t>(neighborId)] = true;
                open.push(neighborId);
            }
        }
    }

    constexpr float kPadding = 12.0f;
    constexpr float kTopInset = 30.0f;
    const float usableWidth = static_cast<float>(width) - (2.0f * kPadding);
    const float usableHeight = static_cast<float>(height) - kTopInset - kPadding;
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
            static_cast<float>(y) + kTopInset + (static_cast<float>(region.m_SeedY) / mapScale) * usableHeight
        };
    }

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

            const OverworldRegionData* neighbor = GetRegion(neighborId);
            Color edgeColor = EdgeColorForBorder(GetBorderType(region.m_Id, neighborId));
            float thickness = 1.0f;
            if (region.m_IsWater || (neighbor && neighbor->m_IsWater))
            {
                edgeColor = Color{ 80, 160, 255, 200 };
            }
            else if (GetBorderType(region.m_Id, neighborId) == RegionBorderType::Mountain)
            {
                thickness = 2.0f;
            }

            DrawLineEx(from, nodePositions[static_cast<size_t>(neighborId)], thickness, edgeColor);
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
        const bool isReachable = selectedRegionId < 0 || reachable[static_cast<size_t>(region.m_Id)];

        Color fill = region.m_IsWater
            ? Color{ 70, 140, 230, 255 }
            : RegionColor(region.m_Id);
        if (isSelected)
        {
            fill = Color{ 255, 230, 90, 255 };
        }
        else if (selectedRegionId >= 0 && !region.m_IsWater)
        {
            fill = isReachable
                ? Color{ 90, 180, 110, 255 }
                : Color{ 70, 74, 84, 255 };
        }

        const float nodeRadius = isSelected ? kNodeRadius + 2.0f : kNodeRadius;
        DrawCircleV(center, nodeRadius, fill);
        DrawCircleLinesV(center, nodeRadius, isSelected ? Color{ 255, 255, 180, 255 } : Color{ 20, 20, 20, 255 });

        const char* label = TextFormat("%d", region.m_Id);
        const int labelWidth = MeasureText(label, 8);
        DrawText(label, static_cast<int>(center.x) - (labelWidth / 2), static_cast<int>(center.y) - 4, 8, BLACK);
    }

    if (g_smallFont)
    {
        DrawOutlinedText(g_smallFont, "Tan land  Blue lake  Red mountain",
            Vector2{ static_cast<float>(x + 4), static_cast<float>(y + height - 12) },
            g_smallFontDrawSize, 1, Color{ 180, 180, 190, 255 });
        if (selectedRegionId >= 0)
        {
            DrawOutlinedText(g_smallFont, "Green = reachable from selection",
                Vector2{ static_cast<float>(x + 4), static_cast<float>(y + height - 22) },
                g_smallFontDrawSize, 1, Color{ 140, 210, 150, 255 });
        }
    }
}