#include "OverworldMap.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>

#include "Geist/RNG.h"
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

void OverworldMap::PartitionIntoRegions(RNG& rng)
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

    int targetRegions = landCells / 450;
    targetRegions = std::max(targetRegions, 12);
    targetRegions = std::min(targetRegions, 40);

    struct SeedPoint
    {
        int x;
        int y;
    };

    std::vector<SeedPoint> seeds;
    const int minSeedDistance = 10;
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

    std::vector<int> oldToNewId(m_Regions.size(), -1);
    int nextRegionId = 0;
    for (size_t oldId = 0; oldId < m_Regions.size(); ++oldId)
    {
        if (m_Regions[oldId].m_CellCount < 20)
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

void OverworldMap::AssignRegionResources(RNG& rng)
{
    for (OverworldRegionData& region : m_Regions)
    {
        int clearCount = 0;
        int treeCount = 0;
        int marshCount = 0;
        int mountainNearby = 0;

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
                    ++clearCount;
                }
                else if (type == OW_TREES)
                {
                    ++treeCount;
                }
                else if (type == OW_MARSH)
                {
                    ++marshCount;
                }

                if (GetCell(x + 1, y) == OW_MOUNTAIN || GetCell(x - 1, y) == OW_MOUNTAIN
                    || GetCell(x, y + 1) == OW_MOUNTAIN || GetCell(x, y - 1) == OW_MOUNTAIN)
                {
                    ++mountainNearby;
                }
            }
        }

        if (treeCount >= clearCount && treeCount >= marshCount)
        {
            region.m_Resource = CountyResource::Wood;
        }
        else if (marshCount >= clearCount)
        {
            region.m_Resource = CountyResource::Food;
        }
        else if (mountainNearby > region.m_CellCount / 4)
        {
            region.m_Resource = CountyResource::Iron;
        }
        else if (rng.Random(100) < 25)
        {
            region.m_Resource = CountyResource::Gold;
        }
        else
        {
            region.m_Resource = CountyResource::Food;
        }
    }
}

void OverworldMap::AssignRegionCampaignState(RNG& rng)
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

    const size_t playerCountyIndex = static_cast<size_t>(rng.Random(static_cast<unsigned int>(m_Regions.size())));
    m_Regions[playerCountyIndex].m_OwnerId = 0;
    m_Regions[playerCountyIndex].m_HasCastle = true;

    for (OverworldRegionData& region : m_Regions)
    {
        if (region.m_OwnerId >= 0)
        {
            continue;
        }

        const int ownerRoll = rng.Random(100);
        if (ownerRoll < 18)
        {
            region.m_OwnerId = rng.RandomRange(1, 3);
        }

        const int castleRoll = rng.Random(100);
        if (castleRoll < 10)
        {
            region.m_HasCastle = true;
        }
    }
}

void OverworldMap::Generate(unsigned int seed)
{
    Clear();
    m_Seed = seed;
    if (m_Seed == 0)
    {
        m_Seed = 1;
    }

    RNG rng;
    rng.SeedRNG(m_Seed);

    GenerateTerrain(rng);
    PartitionIntoRegions(rng);
    BuildAdjacency();
    AssignRegionResources(rng);
    AssignRegionCampaignState(rng);

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
    const int borderThickness = std::max(1, pixelsPerCell / 2);

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

    const Color borderColor = Color{ 245, 245, 210, 255 };
    for (int cellY = 0; cellY < OVERWORLD_MAP_SIZE; ++cellY)
    {
        for (int cellX = 0; cellX < OVERWORLD_MAP_SIZE; ++cellX)
        {
            const int regionId = GetRegionId(cellX, cellY);

            if (cellX + 1 < OVERWORLD_MAP_SIZE)
            {
                const int rightRegionId = GetRegionId(cellX + 1, cellY);
                if (regionId != rightRegionId)
                {
                    const int lineX = x + ((cellX + 1) * pixelsPerCell) - borderThickness;
                    const int lineY = y + (cellY * pixelsPerCell);
                    DrawRectangle(lineX, lineY, borderThickness, pixelsPerCell, borderColor);
                }
            }

            if (cellY + 1 < OVERWORLD_MAP_SIZE)
            {
                const int downRegionId = GetRegionId(cellX, cellY + 1);
                if (regionId != downRegionId)
                {
                    const int lineX = x + (cellX * pixelsPerCell);
                    const int lineY = y + ((cellY + 1) * pixelsPerCell) - borderThickness;
                    DrawRectangle(lineX, lineY, pixelsPerCell, borderThickness, borderColor);
                }
            }
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
    const Color edge = Color{ 255, 255, 140, 255 };
    const int borderThickness = std::max(1, pixelsPerCell);

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

            if (cellX + 1 >= OVERWORLD_MAP_SIZE || GetRegionId(cellX + 1, cellY) != regionId)
            {
                DrawRectangle(pixelX + pixelsPerCell - borderThickness, pixelY, borderThickness, pixelsPerCell, edge);
            }

            if (cellY + 1 >= OVERWORLD_MAP_SIZE || GetRegionId(cellX, cellY + 1) != regionId)
            {
                DrawRectangle(pixelX, pixelY + pixelsPerCell - borderThickness, pixelsPerCell, borderThickness, edge);
            }
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