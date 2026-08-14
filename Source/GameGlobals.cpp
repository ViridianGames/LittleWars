#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../Geist/Source/Globals.h"
#include "../Geist/Source/IO.h"
#include "../Geist/Source/Logging.h"

#include "CampaignAI.h"
#include "GameGlobals.h"
#include "OverworldMap.h"
#include "PlayerTasksConfig.h"
#include "../Geist/Source/RNG.h"

#include "Engine.h"
#include "raylib.h"
#include "rlgl.h"

using namespace std;

GameDatabase g_GameDatabase;

const char* MapSizeName(MapSize size)
{
    switch (size)
    {
    case MapSize::Small:
        return "Small";
    case MapSize::Medium:
        return "Medium";
    case MapSize::Large:
        return "Large";
    case MapSize::Huge:
        return "Huge";
    default:
        return "Unknown";
    }
}

int MapSizeRegionCount(MapSize size)
{
    switch (size)
    {
    case MapSize::Small:
        return 24;
    case MapSize::Medium:
        return 35;
    case MapSize::Large:
        return 64;
    case MapSize::Huge:
        return 80;
    default:
        return 35;
    }
}

int MapSizeStartingRegions(MapSize size)
{
    switch (size)
    {
    case MapSize::Small:
        return 1;
    case MapSize::Medium:
        return 2;
    case MapSize::Large:
        return 3;
    case MapSize::Huge:
        return 4;
    default:
        return 2;
    }
}

const char* DifficultyName(Difficulty difficulty)
{
    return DifficultyName(difficulty, RulerGender::Male);
}

const char* DifficultyName(Difficulty difficulty, RulerGender gender)
{
    if (gender == RulerGender::Female)
    {
        switch (difficulty)
        {
        case Difficulty::Squire:
            return "Lady";
        case Difficulty::Baron:
            return "Baroness";
        case Difficulty::Viscount:
            return "Duchess";
        case Difficulty::Marquis:
            return "Princess";
        case Difficulty::King:
            return "Queen";
        default:
            return "Unknown";
        }
    }

    switch (difficulty)
    {
    case Difficulty::Squire:
        return "Esquire";
    case Difficulty::Baron:
        return "Baron";
    case Difficulty::Viscount:
        return "Duke";
    case Difficulty::Marquis:
        return "Prince";
    case Difficulty::King:
        return "King";
    default:
        return "Unknown";
    }
}

const char* RulerGenderName(RulerGender gender)
{
    switch (gender)
    {
    case RulerGender::Male:
        return "Male";
    case RulerGender::Female:
        return "Female";
    default:
        return "Unknown";
    }
}

const char* BattleModeName(BattleMode mode)
{
    switch (mode)
    {
    case BattleMode::Automatic:
        return "Automatic";
    case BattleMode::OnMap:
        return "On Map";
    default:
        return "Unknown";
    }
}

const char* ResourceDistributionName(ResourceDistribution distribution)
{
    switch (distribution)
    {
    case ResourceDistribution::Clumped:
        return "Clumped";
    case ResourceDistribution::Random:
        return "Random";
    case ResourceDistribution::Balanced:
        return "Balanced";
    default:
        return "Unknown";
    }
}

void ClampCampaignSetup(CampaignSetup& setup)
{
    const int difficultyValue = static_cast<int>(setup.m_Difficulty);
    if (difficultyValue < 0 || difficultyValue >= kDifficultyCount)
    {
        setup.m_Difficulty = Difficulty::Baron;
    }

    const int genderValue = static_cast<int>(setup.m_RulerGender);
    if (genderValue < 0 || genderValue >= kRulerGenderCount)
    {
        setup.m_RulerGender = RulerGender::Male;
    }

    if (setup.m_AllAi)
    {
        setup.m_EnemyCount = std::clamp(setup.m_EnemyCount, kMinAiObservePlayers, kMaxAiObservePlayers);
    }
    else
    {
        setup.m_EnemyCount = std::clamp(setup.m_EnemyCount, kMinOpponents, kMaxOpponents);
    }

    const int mapSizeValue = static_cast<int>(setup.m_MapSize);
    if (mapSizeValue < static_cast<int>(MapSize::Small) || mapSizeValue > static_cast<int>(MapSize::Huge))
    {
        setup.m_MapSize = MapSize::Medium;
    }
}

namespace
{
    void SerializePlayer(ostream& stream, const Player& player)
    {
        IO::Serialize(stream, player.m_Id);
        IO::Serialize(stream, player.m_IsHuman);
        IO::Serialize(stream, player.m_Food);
        IO::Serialize(stream, player.m_Iron);
        IO::Serialize(stream, player.m_Gold);
        IO::Serialize(stream, player.m_Wood);
        IO::Serialize(stream, player.m_FoodRegions);
        IO::Serialize(stream, player.m_IronRegions);
        IO::Serialize(stream, player.m_GoldRegions);
        IO::Serialize(stream, player.m_WoodRegions);
        IO::Serialize(stream, player.m_TotalRegions);
        IO::Serialize(stream, player.m_Castles);
        IO::Serialize(stream, player.m_Swordsmen);
        IO::Serialize(stream, player.m_Archers);
        IO::Serialize(stream, player.m_Knights);
        IO::Serialize(stream, player.m_Catapults);
        IO::Serialize(stream, player.m_SiegeTowers);
        IO::Serialize(stream, player.m_Happiness);

        const unsigned int relationCount = static_cast<unsigned int>(player.m_Relations.size());
        IO::Serialize(stream, relationCount);
        for (int relation : player.m_Relations)
        {
            IO::Serialize(stream, relation);
        }

        const unsigned int activeTaskCount = static_cast<unsigned int>(player.m_ActiveTasks.size());
        IO::Serialize(stream, activeTaskCount);
        for (const auto& activeTask : player.m_ActiveTasks)
        {
            IO::Serialize(stream, activeTask.first);
            IO::Serialize(stream, activeTask.second);
        }
    }

    void DeserializePlayer(istream& stream, Player& player)
    {
        IO::Serialize(stream, player.m_Id);
        IO::Serialize(stream, player.m_IsHuman);
        IO::Serialize(stream, player.m_Food);
        IO::Serialize(stream, player.m_Iron);
        IO::Serialize(stream, player.m_Gold);
        IO::Serialize(stream, player.m_Wood);
        IO::Serialize(stream, player.m_FoodRegions);
        IO::Serialize(stream, player.m_IronRegions);
        IO::Serialize(stream, player.m_GoldRegions);
        IO::Serialize(stream, player.m_WoodRegions);
        IO::Serialize(stream, player.m_TotalRegions);
        IO::Serialize(stream, player.m_Castles);
        IO::Serialize(stream, player.m_Swordsmen);
        IO::Serialize(stream, player.m_Archers);
        IO::Serialize(stream, player.m_Knights);
        IO::Serialize(stream, player.m_Catapults);
        IO::Serialize(stream, player.m_SiegeTowers);
        IO::Serialize(stream, player.m_Happiness);

        unsigned int relationCount = 0;
        IO::Serialize(stream, relationCount);
        player.m_Relations.resize(relationCount);
        for (unsigned int i = 0; i < relationCount; ++i)
        {
            IO::Serialize(stream, player.m_Relations[i]);
        }

        unsigned int activeTaskCount = 0;
        IO::Serialize(stream, activeTaskCount);
        player.m_ActiveTasks.clear();
        player.m_ActiveTasks.reserve(activeTaskCount);
        for (unsigned int taskIndex = 0; taskIndex < activeTaskCount; ++taskIndex)
        {
            std::string taskId;
            int taskCount = 0;
            IO::Serialize(stream, taskId);
            IO::Serialize(stream, taskCount);
            if (!taskId.empty() && taskCount > 0)
            {
                player.m_ActiveTasks.emplace_back(taskId, taskCount);
            }
        }
    }

    void PreparePixelFont(Font& font)
    {
        if (font.texture.id == 0)
        {
            return;
        }

        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    }

    void DrawGameText(Font& font, const char* text, Vector2 position, float fontSize, int spacing, Color color)
    {
        rlDrawRenderBatchActive();
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
        DrawTextEx(font, text, position, fontSize, spacing, color);
    }
}

void InitGameFonts()
{
    g_font = make_shared<Font>(LoadFontEx("Fonts/softsquare.ttf", FONT_TEXTURE_LOAD_SIZE, nullptr, 0));
    PreparePixelFont(*g_font);

    g_smallFont = make_shared<Font>(LoadFontEx("Fonts/littleleague.ttf", SMALL_FONT_TEXTURE_LOAD_SIZE, nullptr, 0));
    PreparePixelFont(*g_smallFont);
}

void ShutdownGameFonts()
{
    if (g_font)
    {
        UnloadFont(*g_font);
        g_font.reset();
    }

    if (g_smallFont)
    {
        UnloadFont(*g_smallFont);
        g_smallFont.reset();
    }
}

namespace
{
    unsigned int DeriveRegionSeed(unsigned int campaignSeed, int regionId)
    {
        return campaignSeed ^ static_cast<unsigned int>(regionId * 2654435761u);
    }

    void ResolveMapDimensions(CampaignSetup& setup)
    {
        switch (setup.m_MapSize)
        {
        case MapSize::Small:
            setup.m_RegionColumns = 3;
            setup.m_RegionRows = 3;
            break;
        case MapSize::Large:
            setup.m_RegionColumns = 6;
            setup.m_RegionRows = 6;
            break;
        case MapSize::Huge:
            setup.m_RegionColumns = 8;
            setup.m_RegionRows = 8;
            break;
        case MapSize::Medium:
        default:
            setup.m_RegionColumns = 4;
            setup.m_RegionRows = 4;
            break;
        }
    }

    void SerializeVector(std::istream& stream, std::vector<float>& data)
    {
        unsigned int count = 0;
        IO::Serialize(stream, count);
        data.resize(count);
        for (unsigned int i = 0; i < count; ++i)
        {
            IO::Serialize(stream, data[i]);
        }
    }

    void SerializeVector(std::ostream& stream, const std::vector<float>& data)
    {
        unsigned int count = static_cast<unsigned int>(data.size());
        IO::Serialize(stream, count);
        for (unsigned int i = 0; i < count; ++i)
        {
            IO::Serialize(stream, data[i]);
        }
    }

    void SerializeVector(std::istream& stream, std::vector<unsigned char>& data)
    {
        unsigned int count = 0;
        IO::Serialize(stream, count);
        data.resize(count);
        for (unsigned int i = 0; i < count; ++i)
        {
            IO::Serialize(stream, data[i]);
        }
    }

    void SerializeVector(std::ostream& stream, const std::vector<unsigned char>& data)
    {
        unsigned int count = static_cast<unsigned int>(data.size());
        IO::Serialize(stream, count);
        for (unsigned int i = 0; i < count; ++i)
        {
            IO::Serialize(stream, data[i]);
        }
    }

    void SerializeHeightfield(std::istream& stream, RegionHeightfield& heightfield)
    {
        IO::Serialize(stream, heightfield.m_Generated);
        IO::Serialize(stream, heightfield.m_Seed);
        if (heightfield.m_Generated)
        {
            SerializeVector(stream, heightfield.m_Heights);
            SerializeVector(stream, heightfield.m_TerrainTypes);
        }
        else
        {
            heightfield.m_Heights.clear();
            heightfield.m_TerrainTypes.clear();
        }
    }

    void SerializeHeightfield(std::ostream& stream, const RegionHeightfield& heightfield)
    {
        IO::Serialize(stream, heightfield.m_Generated);
        IO::Serialize(stream, heightfield.m_Seed);
        if (heightfield.m_Generated)
        {
            SerializeVector(stream, heightfield.m_Heights);
            SerializeVector(stream, heightfield.m_TerrainTypes);
        }
    }

    constexpr float kBattlefieldBaseHeight = 2.2f;
    constexpr float kMaxTerrainPeakHeight = 7.5f;

    void AddSmoothHill(RegionHeightfield& heightfield, RNG& rng, bool rollingHill = false)
    {
        const float radius = rollingHill
            ? static_cast<float>(rng.RandomRange(7, 12))
            : static_cast<float>(rng.RandomRange(11, 20));
        const float peakHeight = rollingHill
            ? static_cast<float>(rng.RandomRange(8, 18)) / 10.0f
            : static_cast<float>(rng.RandomRange(22, 48)) / 10.0f;
        const int hillMargin = REGION_CELLS / 8;
        const int centerX = rng.RandomRange(hillMargin, REGION_CELLS - hillMargin);
        const int centerY = rng.RandomRange(hillMargin, REGION_CELLS - hillMargin);

        for (int y = 0; y < REGION_VERTICES; ++y)
        {
            for (int x = 0; x < REGION_VERTICES; ++x)
            {
                const float dx = (static_cast<float>(x) - static_cast<float>(centerX)) / radius;
                const float dy = (static_cast<float>(y) - static_cast<float>(centerY)) / radius;
                const float distSq = dx * dx + dy * dy;
                if (distSq > 1.0f)
                {
                    continue;
                }

                const float falloff = 1.0f - distSq;
                const float bump = peakHeight * falloff * falloff;
                heightfield.SetHeight(x, y, heightfield.GetHeight(x, y) + bump);
            }
        }
    }

    void SmoothHeightfield(RegionHeightfield& heightfield, int passes)
    {
        std::vector<float> scratch(static_cast<size_t>(REGION_VERTICES * REGION_VERTICES), 0.0f);

        for (int pass = 0; pass < passes; ++pass)
        {
            for (int y = 0; y < REGION_VERTICES; ++y)
            {
                for (int x = 0; x < REGION_VERTICES; ++x)
                {
                    float sum = 0.0f;
                    int count = 0;
                    for (int offsetY = -1; offsetY <= 1; ++offsetY)
                    {
                        for (int offsetX = -1; offsetX <= 1; ++offsetX)
                        {
                            sum += heightfield.GetHeight(x + offsetX, y + offsetY);
                            ++count;
                        }
                    }

                    scratch[static_cast<size_t>(y * REGION_VERTICES + x)] = sum / static_cast<float>(count);
                }
            }

            heightfield.m_Heights = scratch;
        }
    }

    void DepressCircularLowland(RegionHeightfield& heightfield, float centerX, float centerY, float radius, float depth)
    {
        for (int y = 0; y < REGION_VERTICES; ++y)
        {
            for (int x = 0; x < REGION_VERTICES; ++x)
            {
                const float dx = (static_cast<float>(x) - centerX) / radius;
                const float dy = (static_cast<float>(y) - centerY) / radius;
                const float distSq = dx * dx + dy * dy;
                if (distSq > 1.0f)
                {
                    continue;
                }

                const float falloff = 1.0f - distSq;
                const float depression = depth * falloff * falloff;
                float value = heightfield.GetHeight(x, y) - depression;
                if (value < 0.0f)
                {
                    value = 0.0f;
                }
                heightfield.SetHeight(x, y, value);
            }
        }
    }

    void AddLowlandFeature(RegionHeightfield& heightfield, RNG& rng)
    {
        const int quadrant = rng.Random(4);
        const int margin = REGION_CELLS / 11;
        const int innerMargin = REGION_CELLS / 4;
        int centerX = REGION_CELLS / 2;
        int centerY = REGION_CELLS / 2;

        switch (quadrant)
        {
        case 0:
            centerX = rng.RandomRange(margin, innerMargin);
            centerY = rng.RandomRange(margin, innerMargin);
            break;
        case 1:
            centerX = rng.RandomRange(REGION_CELLS - innerMargin, REGION_CELLS - margin);
            centerY = rng.RandomRange(margin, innerMargin);
            break;
        case 2:
            centerX = rng.RandomRange(REGION_CELLS - innerMargin, REGION_CELLS - margin);
            centerY = rng.RandomRange(REGION_CELLS - innerMargin, REGION_CELLS - margin);
            break;
        case 3:
        default:
            centerX = rng.RandomRange(margin, innerMargin);
            centerY = rng.RandomRange(REGION_CELLS - innerMargin, REGION_CELLS - margin);
            break;
        }

        const bool swampOnly = rng.Random(3) == 0;
        const float radius = swampOnly
            ? static_cast<float>(rng.RandomRange(4, 6))
            : static_cast<float>(rng.RandomRange(3, 5));
        const float depth = swampOnly
            ? static_cast<float>(rng.RandomRange(10, 16)) / 10.0f
            : static_cast<float>(rng.RandomRange(18, 28)) / 10.0f;

        DepressCircularLowland(
            heightfield,
            static_cast<float>(centerX),
            static_cast<float>(centerY),
            radius,
            depth);
    }

    void ApplyBattlefieldBorderConstraints(RegionHeightfield& heightfield)
    {
        for (int x = 0; x < REGION_VERTICES; ++x)
        {
            for (int y = 0; y < REGION_VERTICES; ++y)
            {
                if (x == 0 || x == REGION_CELLS || y == 0 || y == REGION_CELLS)
                {
                    heightfield.SetHeight(x, y, 0.0f);
                    continue;
                }

                if (x == 1 || x == REGION_CELLS - 1 || y == 1 || y == REGION_CELLS - 1)
                {
                    float edgeHeight = heightfield.GetHeight(x, y);
                    if (edgeHeight < 1.0f)
                    {
                        edgeHeight = 1.0f;
                    }
                    if (edgeHeight > 2.4f)
                    {
                        edgeHeight = 2.4f;
                    }
                    heightfield.SetHeight(x, y, edgeHeight);
                }
            }
        }
    }

    bool IsWaterCell(const RegionHeightfield& heightfield, int cellX, int cellY)
    {
        if (cellX < 0 || cellY < 0 || cellX >= REGION_CELLS || cellY >= REGION_CELLS)
        {
            return true;
        }

        return heightfield.GetTerrainType(cellX, cellY) == RTT_WATER;
    }

    bool CanTraverseBetweenEdges(const RegionHeightfield& heightfield, bool horizontal)
    {
        std::vector<unsigned char> visited(static_cast<size_t>(REGION_CELLS * REGION_CELLS), 0);
        std::queue<std::pair<int, int>> open;

        const int edgeProbeStart = REGION_CELLS / 32;
        const int edgeProbeEnd = REGION_CELLS / 8;

        if (horizontal)
        {
            const int row = REGION_CELLS / 2;
            for (int x = edgeProbeStart; x < edgeProbeEnd; ++x)
            {
                if (!IsWaterCell(heightfield, x, row))
                {
                    open.emplace(x, row);
                    visited[static_cast<size_t>(row * REGION_CELLS + x)] = 1;
                }
            }
        }
        else
        {
            const int column = REGION_CELLS / 2;
            for (int y = edgeProbeStart; y < edgeProbeEnd; ++y)
            {
                if (!IsWaterCell(heightfield, column, y))
                {
                    open.emplace(column, y);
                    visited[static_cast<size_t>(y * REGION_CELLS + column)] = 1;
                }
            }
        }

        while (!open.empty())
        {
            const auto [cellX, cellY] = open.front();
            open.pop();

            if (horizontal)
            {
                if (cellX >= REGION_CELLS - edgeProbeEnd)
                {
                    return true;
                }
            }
            else if (cellY >= REGION_CELLS - edgeProbeEnd)
            {
                return true;
            }

            static constexpr int kNeighbors[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
            for (const auto& offset : kNeighbors)
            {
                const int nextX = cellX + offset[0];
                const int nextY = cellY + offset[1];
                if (nextX < 0 || nextY < 0 || nextX >= REGION_CELLS || nextY >= REGION_CELLS)
                {
                    continue;
                }

                const size_t index = static_cast<size_t>(nextY * REGION_CELLS + nextX);
                if (visited[index] != 0 || IsWaterCell(heightfield, nextX, nextY))
                {
                    continue;
                }

                visited[index] = 1;
                open.emplace(nextX, nextY);
            }
        }

        return false;
    }

    void FillWaterCell(RegionHeightfield& heightfield, int cellX, int cellY)
    {
        heightfield.SetTerrainType(cellX, cellY, RTT_GRASS);
        const float restoredHeight = kBattlefieldBaseHeight;
        heightfield.SetHeight(cellX, cellY, restoredHeight);
        heightfield.SetHeight(cellX + 1, cellY, restoredHeight);
        heightfield.SetHeight(cellX, cellY + 1, restoredHeight);
        heightfield.SetHeight(cellX + 1, cellY + 1, restoredHeight);
    }

    void RemoveBisectingWater(RegionHeightfield& heightfield)
    {
        for (int attempt = 0; attempt < 48; ++attempt)
        {
            const bool horizontalPassable = CanTraverseBetweenEdges(heightfield, true);
            const bool verticalPassable = CanTraverseBetweenEdges(heightfield, false);
            if (horizontalPassable && verticalPassable)
            {
                return;
            }

            int bestCellX = -1;
            int bestCellY = -1;
            int bestScore = -1;
            for (int cellY = 1; cellY < REGION_CELLS - 1; ++cellY)
            {
                for (int cellX = 1; cellX < REGION_CELLS - 1; ++cellX)
                {
                    if (!IsWaterCell(heightfield, cellX, cellY))
                    {
                        continue;
                    }

                    int landNeighbors = 0;
                    for (int offsetY = -1; offsetY <= 1; ++offsetY)
                    {
                        for (int offsetX = -1; offsetX <= 1; ++offsetX)
                        {
                            if (offsetX == 0 && offsetY == 0)
                            {
                                continue;
                            }

                            if (!IsWaterCell(heightfield, cellX + offsetX, cellY + offsetY))
                            {
                                ++landNeighbors;
                            }
                        }
                    }

                    if (landNeighbors > bestScore)
                    {
                        bestScore = landNeighbors;
                        bestCellX = cellX;
                        bestCellY = cellY;
                    }
                }
            }

            if (bestCellX < 0)
            {
                return;
            }

            FillWaterCell(heightfield, bestCellX, bestCellY);
        }
    }

    void ClassifyTerrainFromHeights(RegionHeightfield& heightfield, CountyResource resource)
    {
        const bool rocky = resource == CountyResource::Iron || resource == CountyResource::Gold;
        const bool farmland = resource == CountyResource::Food;
        const float stoneThreshold = rocky ? 4.0f : (farmland ? 9.0f : 6.0f);

        for (int x = 0; x < REGION_CELLS; ++x)
        {
            for (int y = 0; y < REGION_CELLS; ++y)
            {
                const float h00 = heightfield.GetHeight(x, y);
                const float h10 = heightfield.GetHeight(x + 1, y);
                const float h01 = heightfield.GetHeight(x, y + 1);
                const float h11 = heightfield.GetHeight(x + 1, y + 1);
                const float avg = (h00 + h10 + h01 + h11) * 0.25f;
                const float slope = std::max({
                    std::abs(h10 - h00),
                    std::abs(h01 - h00),
                    std::abs(h11 - h00)
                });

                unsigned char terrainType = RTT_GRASS;
                if (avg <= 0.12f)
                {
                    terrainType = RTT_WATER;
                }
                else if (avg <= 0.55f)
                {
                    terrainType = RTT_BEACH;
                }
                else if (avg <= 1.55f)
                {
                    terrainType = RTT_SWAMP;
                }
                else if (avg >= stoneThreshold || (rocky && slope >= 0.85f && avg >= 2.8f))
                {
                    terrainType = RTT_STONE;
                }
                else if (farmland && avg >= 1.8f && avg <= 3.4f && slope <= 0.45f)
                {
                    terrainType = RTT_FARM;
                }

                heightfield.SetTerrainType(x, y, terrainType);
            }
        }
    }

    void FlattenForFarmland(RegionHeightfield& heightfield)
    {
        // Pull interior heights toward the battlefield base so wheat plains stay open and level.
        for (int x = 2; x < REGION_VERTICES - 2; ++x)
        {
            for (int y = 2; y < REGION_VERTICES - 2; ++y)
            {
                const float height = heightfield.GetHeight(x, y);
                const float flattened = kBattlefieldBaseHeight * 0.65f + height * 0.35f;
                heightfield.SetHeight(x, y, flattened);
            }
        }
    }

    void AmplifyRockyTerrain(RegionHeightfield& heightfield, RNG& rng)
    {
        // Extra sharp outcrops for mining counties.
        const int outcropCount = rng.RandomRange(8, 14);
        for (int i = 0; i < outcropCount; ++i)
        {
            const float radius = static_cast<float>(rng.RandomRange(3, 7));
            const float peakHeight = static_cast<float>(rng.RandomRange(18, 36)) / 10.0f;
            const int margin = REGION_CELLS / 7;
            const int centerX = rng.RandomRange(margin, REGION_CELLS - margin);
            const int centerY = rng.RandomRange(margin, REGION_CELLS - margin);

            for (int y = 0; y < REGION_VERTICES; ++y)
            {
                for (int x = 0; x < REGION_VERTICES; ++x)
                {
                    const float dx = (static_cast<float>(x) - static_cast<float>(centerX)) / radius;
                    const float dy = (static_cast<float>(y) - static_cast<float>(centerY)) / radius;
                    const float distSq = dx * dx + dy * dy;
                    if (distSq >= 1.0f)
                    {
                        continue;
                    }

                    const float falloff = (1.0f - distSq);
                    const float boost = peakHeight * falloff * falloff;
                    heightfield.SetHeight(x, y, heightfield.GetHeight(x, y) + boost);
                }
            }
        }
    }
}

void DrawOutlinedText(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float fontSize, int spacing, Color color)
{
	if (!font)
	{
		return;
	}

	const int x = static_cast<int>(floorf(position.x + 0.5f));
	const int y = static_cast<int>(floorf(position.y + 0.5f));
	const Color outline = Color{ 0, 0, 0, color.a };

	DrawGameText(*font, text.c_str(), Vector2{ static_cast<float>(x - 1), static_cast<float>(y) }, fontSize, spacing, outline);
	DrawGameText(*font, text.c_str(), Vector2{ static_cast<float>(x + 1), static_cast<float>(y) }, fontSize, spacing, outline);
	DrawGameText(*font, text.c_str(), Vector2{ static_cast<float>(x), static_cast<float>(y - 1) }, fontSize, spacing, outline);
	DrawGameText(*font, text.c_str(), Vector2{ static_cast<float>(x), static_cast<float>(y + 1) }, fontSize, spacing, outline);
	DrawGameText(*font, text.c_str(), Vector2{ static_cast<float>(x), static_cast<float>(y) }, fontSize, spacing, color);
}

void DrawParagraph(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float maxwidth, float fontSize, int spacing, Color color, bool outlined)
{
	std::istringstream iss(text);
	std::string word;
	std::vector<std::string> lines;
	float lineWidth = 0;

	string rawline;
	string line;
	while (getline(iss, rawline))
	{
		std::stringstream lineStream(rawline);
		while (lineStream >> word)
		{
			int currentLineWidth = MeasureTextEx(*font, (line + word).c_str(), fontSize, spacing).x;
			if (currentLineWidth > maxwidth)
			{
				lines.push_back(line);
				line.clear();
				line += word + " ";
			}
			else
			{
				line += word + " ";
			}
		}

		lines.push_back(line);

		line.clear();
	}

	auto it = lines.begin();
	float y = position.y;
	while (it != lines.end())
	{
		if (outlined)
		{
			DrawOutlinedText(font, (*it).c_str(), Vector2{ position.x, y }, fontSize, spacing, color);
		}
		else
		{
			DrawTextEx(*font, (*it).c_str(), Vector2{ position.x, y }, fontSize, spacing, color);
		}
		y += fontSize * 1.2f;
		++it;
	}
}

std::vector<ConsoleString> g_ConsoleStrings;

void DrawConsole()
{
	int counter = 0;
	vector<ConsoleString>::iterator node = g_ConsoleStrings.begin();
	float shadowOffset = 1;
	if (shadowOffset < 1)
	{
		shadowOffset = 1;
	}
	for (node; node != g_ConsoleStrings.end(); ++node)
	{
		float elapsed = GetTime() - (*node).m_StartTime;
		if (elapsed > 9)
		{
			float alpha = float(9 - elapsed);
			if (alpha == 1.0f)
			{
				alpha = 0;
			}
			(*node).m_Color.a = alpha * 255;
		}

		if (elapsed < 10)
		{
			DrawOutlinedText(g_smallFont, (*node).m_String.c_str(), Vector2{ 0, float(counter * (g_smallFontDrawSize + 2)) }, g_smallFontDrawSize, 1, (*node).m_Color);

		}
		++counter;
	}

	node = g_ConsoleStrings.begin();
	for (node; node != g_ConsoleStrings.end();)
	{
		if (GetTime() - (*node).m_StartTime > 10)
		{
			node = g_ConsoleStrings.erase(node);
		}
		else
		{
			++node;
		}
	}
}

void DrawPerfCounter(Font* font, int loc)
{
	int vpos = 0;
	int hpos = 0;
	int width = g_Engine->m_RenderWidth * .20f;
	int height = g_Engine->m_RenderHeight * .20f;
	switch (loc)
	{
	case 0: // Bottom-left
		hpos = 0;
		vpos = g_Engine->m_RenderHeight - height;
		break;
	case 1: // Top-left
		hpos = 0;
		vpos = 0;
		break;
	case 2: // Bottom-right
		hpos = g_Engine->m_RenderWidth - width;
		vpos = g_Engine->m_RenderHeight - height;
		break;
	case 3: // Top-right
		hpos = g_Engine->m_RenderWidth - width;
		vpos = 0;
		break;
	}
	DrawRectangle(hpos, vpos, width, height, BLACK);
	DrawRectangleLines(hpos, vpos, width, height, BLUE);

	string perf_temp = to_string(int(1.0f / GetFrameTime())) + " fps (" + to_string(int(GetFrameTime() * 1000.0f)) + " mspf)";
	DrawTextEx(*font, perf_temp.c_str(), {hpos + (width * .05f), vpos + height - (font->baseSize * 1.01f)}, font->baseSize, 1, WHITE);
	// if (font)
	// {
	// 	DrawTextEx(*font, perf_temp.c_str(), {hpos + (width * .05f), vpos + height - (font->baseSize * 1.01f)}, font->baseSize, 1, WHITE);
	// }
	DrawTextEx(*font, "Draw", {hpos + (width * .05f), g_Engine->m_RenderHeight * .95f}, font->baseSize, 1, GREEN);
	DrawTextEx(*font, "Update", {hpos + (width * .3f), g_Engine->m_RenderHeight * .95f}, font->baseSize, 1,  YELLOW);
	DrawTextEx(*font, "Network", {hpos + (width * .65f), g_Engine->m_RenderHeight * .95f}, font->baseSize, 1, BLUE);
	int perf_i;
	for (perf_i = 0; perf_i < 50 - 1; perf_i++)
	{
		int h = std::max(1, int(g_Engine->m_UpdateFrames[perf_i] * 1000));
		int h2 = std::max(1, int(g_Engine->m_DrawFrames[perf_i] * 1000));
		DrawRectangle(hpos + 4 + (perf_i * 2), int(g_Engine->m_RenderHeight * .94f) - h, 2, h, GREEN);
		DrawRectangle(hpos + 4 + (perf_i * 2), int(g_Engine->m_RenderHeight * .94f) - (h + h2), 2, h2, YELLOW);
	}
}

void RegionHeightfield::Clear()
{
    m_Generated = false;
    m_Seed = 0;
    m_Heights.clear();
    m_TerrainTypes.clear();
}

float RegionHeightfield::GetHeight(int x, int y) const
{
    if (x < 0 || x >= REGION_VERTICES || y < 0 || y >= REGION_VERTICES)
    {
        return 0.0f;
    }

    return m_Heights[static_cast<size_t>(y * REGION_VERTICES + x)];
}

float RegionHeightfield::SampleHeight(float x, float z) const
{
    if (m_Heights.empty() || x < 0.0f || z < 0.0f || x > REGION_CELLS || z > REGION_CELLS)
    {
        return 0.0f;
    }

    int cellX = static_cast<int>(x);
    int cellZ = static_cast<int>(z);
    if (cellX >= REGION_CELLS)
    {
        cellX = REGION_CELLS - 1;
    }
    if (cellZ >= REGION_CELLS)
    {
        cellZ = REGION_CELLS - 1;
    }

    const float fracX = x - static_cast<float>(cellX);
    const float fracZ = z - static_cast<float>(cellZ);

    const float h00 = GetHeight(cellX, cellZ);
    const float h10 = GetHeight(cellX + 1, cellZ);
    const float h01 = GetHeight(cellX, cellZ + 1);
    const float h11 = GetHeight(cellX + 1, cellZ + 1);

    const float alongX0 = h00 + (h10 - h00) * fracX;
    const float alongX1 = h01 + (h11 - h01) * fracX;
    return alongX0 + (alongX1 - alongX0) * fracZ;
}

void RegionHeightfield::SetHeight(int x, int y, float height)
{
    if (x < 0 || x >= REGION_VERTICES || y < 0 || y >= REGION_VERTICES)
    {
        return;
    }

    m_Heights[static_cast<size_t>(y * REGION_VERTICES + x)] = height;
}

unsigned char RegionHeightfield::GetTerrainType(int x, int y) const
{
    if (x < 0 || x >= REGION_CELLS || y < 0 || y >= REGION_CELLS)
    {
        return RTT_GRASS;
    }

    return m_TerrainTypes[static_cast<size_t>(y * REGION_CELLS + x)];
}

void RegionHeightfield::SetTerrainType(int x, int y, unsigned char type)
{
    if (x < 0 || x >= REGION_CELLS || y < 0 || y >= REGION_CELLS)
    {
        return;
    }

    m_TerrainTypes[static_cast<size_t>(y * REGION_CELLS + x)] = type;
}

void GameDatabase::Clear()
{
    m_Setup = CampaignSetup{};
    m_Turn = 0;
    m_ActiveRegionId = -1;
    m_Outcome = CampaignOutcome::None;
    ClearPendingBattle();
    m_Players.clear();
    m_Regions.clear();
}

void GameDatabase::ClearPendingBattle()
{
    m_PendingBattle = PendingBattle{};
}

void GameDatabase::InitNewCampaign(const CampaignSetup& setup)
{
    Clear();
    m_Setup = setup;
    if (m_Setup.m_Seed == 0)
    {
        m_Setup.m_Seed = static_cast<unsigned int>(GetTime() * 1000.0);
    }

    ResolveMapDimensions(m_Setup);
    GenerateOverworldRegions();
    GenerateAllRegionHeightfields();
}

void GameDatabase::GenerateOverworldRegions()
{
    RNG rng;
    rng.SeedRNG(m_Setup.m_Seed);

    const int playerCount = GetCampaignPlayerCount(m_Setup);
    InitializeCampaignPlayers(m_Players, playerCount, !m_Setup.m_AllAi);

    int regionId = 0;
    for (int mapY = 0; mapY < m_Setup.m_RegionRows; ++mapY)
    {
        for (int mapX = 0; mapX < m_Setup.m_RegionColumns; ++mapX)
        {
            RegionData region;
            region.m_Id = regionId++;
            region.m_MapX = mapX;
            region.m_MapY = mapY;
            region.m_HeightfieldSeed = DeriveRegionSeed(m_Setup.m_Seed, region.m_Id);
            region.m_Income = rng.RandomRange(5, 15);
            region.m_OwnerId = (mapX == 0 && mapY == 0) ? 0 : -1;
            region.m_Resource = static_cast<unsigned char>(
                static_cast<CountyResource>(rng.Random(4))); // Food/Gold/Iron/Wood cycle for demos
            m_Regions.push_back(region);
        }
    }
}

void GameDatabase::BuildRegionsFromOverworld(const OverworldMap& map)
{
    m_Regions.clear();
    m_ActiveRegionId = -1;

    if (!map.IsGenerated())
    {
        return;
    }

    if (m_Setup.m_Seed == 0)
    {
        m_Setup.m_Seed = map.GetSeed();
    }

    for (const OverworldRegionData& overworldRegion : map.GetRegions())
    {
        if (overworldRegion.m_IsWater || overworldRegion.m_Id < 0)
        {
            continue;
        }

        RegionData region;
        region.m_Id = overworldRegion.m_Id;
        region.m_MapX = overworldRegion.m_SeedX;
        region.m_MapY = overworldRegion.m_SeedY;
        region.m_OwnerId = overworldRegion.m_OwnerId;
        region.m_HasCastle = overworldRegion.m_HasCastle;
        region.m_Income = GetRegionTurnIncome(map, overworldRegion);
        region.m_Resource = static_cast<unsigned char>(overworldRegion.m_Resource);
        region.m_HeightfieldSeed = DeriveRegionSeed(m_Setup.m_Seed, overworldRegion.m_Id);
        m_Regions.push_back(region);
    }
}

void GameDatabase::GenerateAllRegionHeightfields()
{
    for (RegionData& region : m_Regions)
    {
        GenerateRegionHeightfield(region);
    }
}

RegionData* GameDatabase::GetRegion(int regionId)
{
    for (RegionData& region : m_Regions)
    {
        if (region.m_Id == regionId)
        {
            return &region;
        }
    }

    return nullptr;
}

const RegionData* GameDatabase::GetRegion(int regionId) const
{
    for (const RegionData& region : m_Regions)
    {
        if (region.m_Id == regionId)
        {
            return &region;
        }
    }

    return nullptr;
}

RegionData* GameDatabase::GetRegionAtMapPos(int mapX, int mapY)
{
    for (RegionData& region : m_Regions)
    {
        if (region.m_MapX == mapX && region.m_MapY == mapY)
        {
            return &region;
        }
    }

    return nullptr;
}

const RegionData* GameDatabase::GetRegionAtMapPos(int mapX, int mapY) const
{
    for (const RegionData& region : m_Regions)
    {
        if (region.m_MapX == mapX && region.m_MapY == mapY)
        {
            return &region;
        }
    }

    return nullptr;
}

void GameDatabase::GenerateRegionHeightfield(RegionData& region)
{
    RegionHeightfield& heightfield = region.m_Heightfield;
    if (heightfield.m_Generated)
    {
        return;
    }

    heightfield.Clear();
    heightfield.m_Seed = region.m_HeightfieldSeed;
    heightfield.m_Heights.assign(static_cast<size_t>(REGION_VERTICES * REGION_VERTICES), kBattlefieldBaseHeight);
    heightfield.m_TerrainTypes.assign(static_cast<size_t>(REGION_CELLS * REGION_CELLS), RTT_GRASS);

    RNG rng;
    rng.SeedRNG(heightfield.m_Seed);

    const CountyResource resource = static_cast<CountyResource>(region.m_Resource);
    const bool farmland = resource == CountyResource::Food;
    const bool rocky = resource == CountyResource::Iron || resource == CountyResource::Gold;
    const bool woodland = resource == CountyResource::Wood;

    int hillCount = rng.RandomRange(10, 16);
    int rollingHillCount = rng.RandomRange(3, 6);
    int smoothPasses = 2;
    float maxPeakHeight = kMaxTerrainPeakHeight;

    if (farmland)
    {
        // Open wheat plain: few gentle rises, heavily smoothed.
        hillCount = rng.RandomRange(1, 4);
        rollingHillCount = rng.RandomRange(1, 3);
        smoothPasses = 4;
        maxPeakHeight = 3.6f;
    }
    else if (rocky)
    {
        // Mining country: denser, steeper relief and more stone later.
        hillCount = rng.RandomRange(18, 28);
        rollingHillCount = rng.RandomRange(6, 11);
        smoothPasses = 1;
        maxPeakHeight = 8.2f;
    }
    else if (woodland)
    {
        // Rolling forested country — moderate hills, trees added at decorate time.
        hillCount = rng.RandomRange(8, 14);
        rollingHillCount = rng.RandomRange(3, 7);
        smoothPasses = 2;
        maxPeakHeight = 6.5f;
    }

    for (int i = 0; i < hillCount; ++i)
    {
        AddSmoothHill(heightfield, rng, false);
    }

    for (int i = 0; i < rollingHillCount; ++i)
    {
        AddSmoothHill(heightfield, rng, true);
    }

    if (rocky)
    {
        AmplifyRockyTerrain(heightfield, rng);
    }

    SmoothHeightfield(heightfield, smoothPasses);

    if (farmland)
    {
        FlattenForFarmland(heightfield);
        SmoothHeightfield(heightfield, 2);
    }

    ApplyBattlefieldBorderConstraints(heightfield);

    // Every combat map has at least a 50% chance of water and/or marsh lowlands.
    if (rng.Random(2) == 0)
    {
        const int lowlandFeatureCount = rng.RandomRange(1, 3);
        for (int i = 0; i < lowlandFeatureCount; ++i)
        {
            AddLowlandFeature(heightfield, rng);
        }
    }

    float highestHeight = kBattlefieldBaseHeight;
    for (int x = 0; x < REGION_VERTICES; ++x)
    {
        for (int y = 0; y < REGION_VERTICES; ++y)
        {
            highestHeight = std::max(highestHeight, heightfield.GetHeight(x, y));
        }
    }

    if (highestHeight > maxPeakHeight)
    {
        const float scalar = maxPeakHeight / highestHeight;
        for (int x = 0; x < REGION_VERTICES; ++x)
        {
            for (int y = 0; y < REGION_VERTICES; ++y)
            {
                const float scaledHeight = kBattlefieldBaseHeight
                    + (heightfield.GetHeight(x, y) - kBattlefieldBaseHeight) * scalar;
                heightfield.SetHeight(x, y, scaledHeight);
            }
        }
    }

    ClassifyTerrainFromHeights(heightfield, resource);
    RemoveBisectingWater(heightfield);
    heightfield.m_Generated = true;
}

RegionHeightfield* GameDatabase::EnsureRegionHeightfield(int regionId)
{
    RegionData* region = GetRegion(regionId);
    if (!region)
    {
        return nullptr;
    }

    GenerateRegionHeightfield(*region);
    return &region->m_Heightfield;
}

void GameDatabase::RegenerateRegionHeightfield(int regionId)
{
    RegionData* region = GetRegion(regionId);
    if (!region)
    {
        return;
    }

    region->m_HeightfieldSeed = static_cast<unsigned int>(GetTime() * 1000.0);
    region->m_Heightfield.Clear();
    GenerateRegionHeightfield(*region);
}

void GameDatabase::SetActiveRegion(int regionId)
{
    m_ActiveRegionId = regionId;
}

RegionData* GameDatabase::GetActiveRegion()
{
    return GetRegion(m_ActiveRegionId);
}

const RegionData* GameDatabase::GetActiveRegion() const
{
    return GetRegion(m_ActiveRegionId);
}

void GameDatabase::SyncPlayersFromOverworld(const OverworldMap& map, bool resetAssets)
{
    if (m_Players.empty())
    {
        const int playerCount = GetCampaignPlayerCount(m_Setup);
        InitializeCampaignPlayers(m_Players, playerCount, !m_Setup.m_AllAi);
    }

    ::SyncPlayersFromOverworld(map, m_Players, resetAssets);
}

void GameDatabase::AdvanceTurn(OverworldMap& map)
{
    if (m_Outcome != CampaignOutcome::None)
    {
        return; // Campaign finished — no more turns.
    }

    if (m_Players.empty())
    {
        const int playerCount = GetCampaignPlayerCount(m_Setup);
        InitializeCampaignPlayers(m_Players, playerCount, !m_Setup.m_AllAi);
        AssignCampaignAiPersonalities(m_Players, m_Setup);
    }

    // Income and construction first, then pay the armies, then AI acts with the new purse.
    CollectTurnIncomeFromRegions(map, m_Players);
    ProcessCastleConstruction(map, m_Players);
    for (Player& player : m_Players)
    {
        g_PlayerTasksConfig.ApplyMaintenance(player);
    }

    const int actingTurn = m_Turn + 1;
    if (g_vitalRNG)
    {
        RunAllCampaignAiTurns(map, m_Players, *g_vitalRNG, actingTurn);
    }
    else if (g_nonVitalRNG)
    {
        RunAllCampaignAiTurns(map, m_Players, *g_nonVitalRNG, actingTurn);
    }
    else
    {
        RNG localRng;
        localRng.SeedRNG(m_Setup.m_Seed + static_cast<unsigned int>(m_Turn) + 1u);
        RunAllCampaignAiTurns(map, m_Players, localRng, actingTurn);
    }

    ++m_Turn;

    // New turn: everyone may issue one attack again.
    for (Player& player : m_Players)
    {
        player.m_AttacksThisTurn = 0;
    }

    EvaluateCampaignOutcome(map);
}

void GameDatabase::EvaluateCampaignOutcome(const OverworldMap& map)
{
    if (m_Setup.m_AllAi)
    {
        m_Outcome = CampaignOutcome::None;
        return;
    }

    Player* human = GetHumanPlayer(m_Players);
    if (!human)
    {
        m_Outcome = CampaignOutcome::None;
        return;
    }

    // Keep region counts current before judging.
    ::SyncPlayersFromOverworld(map, m_Players, false);

    if (human->m_TotalRegions <= 0)
    {
        m_Outcome = CampaignOutcome::Defeat;
        g_AiObserverLog.Add(human->m_Id, "DEFEAT — you hold no counties.");
        return;
    }

    bool rivalHoldsLand = false;
    for (const Player& player : m_Players)
    {
        if (player.m_Id == human->m_Id)
        {
            continue;
        }
        if (player.m_TotalRegions > 0)
        {
            rivalHoldsLand = true;
            break;
        }
    }

    if (!rivalHoldsLand)
    {
        m_Outcome = CampaignOutcome::Victory;
        g_AiObserverLog.Add(human->m_Id, "VICTORY — all rivals have been driven from the map!");
    }
}

bool GameDatabase::BeginPendingBattle(int attackerId, int targetRegionId, OverworldMap& map)
{
    if (attackerId < 0 || attackerId >= static_cast<int>(m_Players.size()))
    {
        return false;
    }

    Player& attacker = m_Players[static_cast<size_t>(attackerId)];
    if (attacker.m_AttacksThisTurn >= 1)
    {
        return false;
    }
    if (!CanPlayerAttackRegion(attacker, map, targetRegionId))
    {
        return false;
    }

    const PlayerTaskDefinition* attackTask = g_PlayerTasksConfig.FindTaskById("attack");
    if (attackTask && !attackTask->m_Cost.CanAfford(attacker))
    {
        return false;
    }

    OverworldRegionData* target = map.GetRegion(targetRegionId);
    if (!target || target->m_IsWater)
    {
        return false;
    }

    if (attackTask)
    {
        attackTask->m_Cost.Deduct(attacker);
    }

    // Counts as this turn's one attack (even if battle is later resolved on the combat map).
    ++attacker.m_AttacksThisTurn;

    ClearPendingBattle();
    m_PendingBattle.m_Active = true;
    m_PendingBattle.m_RegionId = targetRegionId;
    m_PendingBattle.m_AttackerId = attackerId;
    m_PendingBattle.m_DefenderId = target->m_OwnerId;

    m_PendingBattle.m_AtkSwordsmen = attacker.m_Swordsmen;
    m_PendingBattle.m_AtkArchers = attacker.m_Archers;
    m_PendingBattle.m_AtkKnights = attacker.m_Knights;
    m_PendingBattle.m_AtkCatapults = attacker.m_Catapults;

    if (m_PendingBattle.m_DefenderId >= 0
        && m_PendingBattle.m_DefenderId < static_cast<int>(m_Players.size()))
    {
        const Player& defender = m_Players[static_cast<size_t>(m_PendingBattle.m_DefenderId)];
        m_PendingBattle.m_DefSwordsmen = defender.m_Swordsmen;
        m_PendingBattle.m_DefArchers = defender.m_Archers;
        m_PendingBattle.m_DefKnights = defender.m_Knights;
        m_PendingBattle.m_DefCatapults = defender.m_Catapults;
    }
    else
    {
        // Neutral garrison: 1S + 1A, 50% chance of one extra S or A.
        int neutralS = 1;
        int neutralA = 1;
        RollNeutralRegionGarrison(neutralS, neutralA, g_vitalRNG ? g_vitalRNG.get() : nullptr);
        m_PendingBattle.m_DefSwordsmen = neutralS;
        m_PendingBattle.m_DefArchers = neutralA;
        m_PendingBattle.m_DefKnights = 0;
        m_PendingBattle.m_DefCatapults = 0;
    }

    SetActiveRegion(targetRegionId);
    EnsureRegionHeightfield(targetRegionId);
    return true;
}

namespace
{
    void ClampActiveTaskStacksToArmy(Player& player)
    {
        for (auto& entry : player.m_ActiveTasks)
        {
            if (entry.first == "recruitInfantry")
            {
                entry.second = std::min(entry.second, std::max(0, player.m_Swordsmen));
            }
            else if (entry.first == "recruitArchers")
            {
                entry.second = std::min(entry.second, std::max(0, player.m_Archers));
            }
            else if (entry.first == "recruitKnights")
            {
                entry.second = std::min(entry.second, std::max(0, player.m_Knights));
            }
        }
        player.m_ActiveTasks.erase(
            std::remove_if(player.m_ActiveTasks.begin(), player.m_ActiveTasks.end(),
                [](const std::pair<std::string, int>& e) { return e.second <= 0; }),
            player.m_ActiveTasks.end());
    }
}

void GameDatabase::FinalizePendingBattle(OverworldMap& map)
{
    if (!m_PendingBattle.m_Active || !m_PendingBattle.m_Resolved)
    {
        ClearPendingBattle();
        return;
    }

    // CombatState rewrites army fields to REMAINING counts before setting m_Resolved.
    PendingBattle battle = m_PendingBattle;
    ClearPendingBattle();

    if (battle.m_AttackerId < 0 || battle.m_AttackerId >= static_cast<int>(m_Players.size()))
    {
        return;
    }

    Player& attacker = m_Players[static_cast<size_t>(battle.m_AttackerId)];
    OverworldRegionData* target = map.GetRegion(battle.m_RegionId);

    attacker.m_Swordsmen = std::max(0, battle.m_AtkSwordsmen);
    attacker.m_Archers = std::max(0, battle.m_AtkArchers);
    attacker.m_Knights = std::max(0, battle.m_AtkKnights);
    attacker.m_Catapults = std::max(0, battle.m_AtkCatapults);
    ClampActiveTaskStacksToArmy(attacker);

    if (battle.m_DefenderId >= 0 && battle.m_DefenderId < static_cast<int>(m_Players.size()))
    {
        Player& defender = m_Players[static_cast<size_t>(battle.m_DefenderId)];
        defender.m_Swordsmen = std::max(0, battle.m_DefSwordsmen);
        defender.m_Archers = std::max(0, battle.m_DefArchers);
        defender.m_Knights = std::max(0, battle.m_DefKnights);
        defender.m_Catapults = std::max(0, battle.m_DefCatapults);
        ClampActiveTaskStacksToArmy(defender);
    }

    if (battle.m_AttackerWon && !battle.m_Retreated && target)
    {
        target->m_OwnerId = attacker.m_Id;
        if (target->m_CastleBuildTurnsRemaining > 0 && !target->m_HasCastle)
        {
            target->m_CastleBuildTurnsRemaining = 0;
        }

        if (RegionData* dbRegion = GetRegion(battle.m_RegionId))
        {
            dbRegion->m_OwnerId = attacker.m_Id;
            dbRegion->m_HasCastle = target->m_HasCastle;
        }

        g_AiObserverLog.Add(attacker.m_Id,
            string(attacker.GetColorName()) + " captured county "
            + to_string(battle.m_RegionId) + " in battle");
    }
    else if (battle.m_Retreated)
    {
        g_AiObserverLog.Add(attacker.m_Id,
            string(attacker.GetColorName()) + " retreated from county " + to_string(battle.m_RegionId));
    }
    else
    {
        g_AiObserverLog.Add(attacker.m_Id,
            string(attacker.GetColorName()) + " lost the battle for county " + to_string(battle.m_RegionId));
    }

    ::SyncPlayersFromOverworld(map, m_Players, false);
    EvaluateCampaignOutcome(map);
}

bool GameDatabase::SaveCampaign(const std::string& path) const
{
    ofstream stream(path, ios::binary);
    if (!stream)
    {
        Log("GameDatabase::SaveCampaign - failed to open " + path);
        return false;
    }

    stream.write(SAVE_MAGIC, 4);
    IO::Serialize(stream, SAVE_VERSION);

    IO::Serialize(stream, m_Setup.m_Seed);
    IO::Serialize(stream, static_cast<int>(m_Setup.m_Difficulty));
    IO::Serialize(stream, static_cast<int>(m_Setup.m_RulerGender));
    IO::Serialize(stream, m_Setup.m_AllAi);
    IO::Serialize(stream, m_Setup.m_EnemyCount);
    IO::Serialize(stream, static_cast<int>(m_Setup.m_BattleMode));
    IO::Serialize(stream, static_cast<int>(m_Setup.m_ResourceDistribution));
    IO::Serialize(stream, static_cast<int>(m_Setup.m_MapSize));
    IO::Serialize(stream, m_Setup.m_RegionColumns);
    IO::Serialize(stream, m_Setup.m_RegionRows);

    IO::Serialize(stream, m_Turn);
    IO::Serialize(stream, m_ActiveRegionId);

    const unsigned int playerCount = static_cast<unsigned int>(m_Players.size());
    IO::Serialize(stream, playerCount);
    for (const Player& player : m_Players)
    {
        SerializePlayer(stream, player);
    }

    const unsigned int regionCount = static_cast<unsigned int>(m_Regions.size());
    IO::Serialize(stream, regionCount);
    for (const RegionData& region : m_Regions)
    {
        IO::Serialize(stream, region.m_Id);
        IO::Serialize(stream, region.m_MapX);
        IO::Serialize(stream, region.m_MapY);
        IO::Serialize(stream, region.m_OwnerId);
        IO::Serialize(stream, region.m_Income);
        IO::Serialize(stream, region.m_HasCastle);
        IO::Serialize(stream, region.m_Resource);
        IO::Serialize(stream, region.m_HeightfieldSeed);
        SerializeHeightfield(stream, region.m_Heightfield);
    }

    unsigned int overworldOverlayCount = 0;
    if (g_OverworldMap.IsGenerated())
    {
        for (const OverworldRegionData& region : g_OverworldMap.GetRegions())
        {
            if (!region.m_IsWater)
            {
                ++overworldOverlayCount;
            }
        }
    }

    IO::Serialize(stream, overworldOverlayCount);
    if (g_OverworldMap.IsGenerated())
    {
        for (const OverworldRegionData& region : g_OverworldMap.GetRegions())
        {
            if (region.m_IsWater)
            {
                continue;
            }

            IO::Serialize(stream, region.m_Id);
            IO::Serialize(stream, region.m_OutputMultiplier);
            IO::Serialize(stream, region.m_CastleBuildTurnsRemaining);
            IO::Serialize(stream, region.m_HasCastle);
            IO::Serialize(stream, region.m_OwnerId);
        }
    }

    Log("GameDatabase::SaveCampaign - saved to " + path);
    return true;
}

bool GameDatabase::LoadCampaign(const std::string& path)
{
    ifstream stream(path, ios::binary);
    if (!stream)
    {
        Log("GameDatabase::LoadCampaign - failed to open " + path);
        return false;
    }

    char magic[4] = {};
    stream.read(magic, 4);
    if (strncmp(magic, SAVE_MAGIC, 4) != 0)
    {
        Log("GameDatabase::LoadCampaign - invalid save magic in " + path);
        return false;
    }

    int version = 0;
    IO::Serialize(stream, version);
    // v11: no ownerId in overworld overlay. v12: ownerId + RegionData ownership restore.
    if (version < 11 || version > SAVE_VERSION)
    {
        Log("GameDatabase::LoadCampaign - unsupported save version in " + path);
        return false;
    }

    Clear();

    IO::Serialize(stream, m_Setup.m_Seed);
    int difficulty = 0;
    IO::Serialize(stream, difficulty);
    m_Setup.m_Difficulty = static_cast<Difficulty>(difficulty);
    int rulerGender = 0;
    IO::Serialize(stream, rulerGender);
    m_Setup.m_RulerGender = static_cast<RulerGender>(rulerGender);
    IO::Serialize(stream, m_Setup.m_AllAi);
    IO::Serialize(stream, m_Setup.m_EnemyCount);
    int battleMode = 0;
    IO::Serialize(stream, battleMode);
    m_Setup.m_BattleMode = static_cast<BattleMode>(battleMode);
    int resourceDistribution = 0;
    IO::Serialize(stream, resourceDistribution);
    m_Setup.m_ResourceDistribution = static_cast<ResourceDistribution>(resourceDistribution);
    int mapSize = 0;
    IO::Serialize(stream, mapSize);
    m_Setup.m_MapSize = static_cast<MapSize>(mapSize);
    IO::Serialize(stream, m_Setup.m_RegionColumns);
    IO::Serialize(stream, m_Setup.m_RegionRows);
    ClampCampaignSetup(m_Setup);

    IO::Serialize(stream, m_Turn);
    IO::Serialize(stream, m_ActiveRegionId);

    unsigned int playerCount = 0;
    IO::Serialize(stream, playerCount);
    m_Players.resize(playerCount);
    for (Player& player : m_Players)
    {
        DeserializePlayer(stream, player);
    }

    unsigned int regionCount = 0;
    IO::Serialize(stream, regionCount);
    m_Regions.resize(regionCount);
    for (RegionData& region : m_Regions)
    {
        IO::Serialize(stream, region.m_Id);
        IO::Serialize(stream, region.m_MapX);
        IO::Serialize(stream, region.m_MapY);
        IO::Serialize(stream, region.m_OwnerId);
        IO::Serialize(stream, region.m_Income);
        IO::Serialize(stream, region.m_HasCastle);
        IO::Serialize(stream, region.m_Resource);
        IO::Serialize(stream, region.m_HeightfieldSeed);
        SerializeHeightfield(stream, region.m_Heightfield);
    }

    unsigned int overworldOverlayCount = 0;
    IO::Serialize(stream, overworldOverlayCount);

    g_OverworldMap.Generate(m_Setup.m_Seed, m_Setup);
    for (unsigned int overlayIndex = 0; overlayIndex < overworldOverlayCount; ++overlayIndex)
    {
        int regionId = 0;
        int outputMultiplier = 1;
        int castleBuildTurnsRemaining = 0;
        bool hasCastle = false;
        int ownerId = -1;
        IO::Serialize(stream, regionId);
        IO::Serialize(stream, outputMultiplier);
        IO::Serialize(stream, castleBuildTurnsRemaining);
        IO::Serialize(stream, hasCastle);
        if (version >= 12)
        {
            IO::Serialize(stream, ownerId);
        }
        g_OverworldMap.ApplyRegionCampaignOverlay(
            regionId,
            outputMultiplier,
            castleBuildTurnsRemaining,
            hasCastle,
            ownerId);
    }

    // Restore ownership from serialized RegionData (authoritative for older saves too).
    for (const RegionData& region : m_Regions)
    {
        if (OverworldRegionData* ow = g_OverworldMap.GetRegion(region.m_Id))
        {
            if (!ow->m_IsWater)
            {
                ow->m_OwnerId = region.m_OwnerId;
                ow->m_HasCastle = region.m_HasCastle;
            }
        }
    }

    SyncPlayersFromOverworld(g_OverworldMap, false);
    m_Outcome = CampaignOutcome::None;
    ClearPendingBattle();
    EvaluateCampaignOutcome(g_OverworldMap);

    Log("GameDatabase::LoadCampaign - loaded from " + path);
    return true;
}

bool GameDatabase::HasSaveFile(const std::string& path) const
{
    ifstream stream(path, ios::binary);
    if (!stream)
    {
        return false;
    }
    char magic[4] = {};
    stream.read(magic, 4);
    return stream && strncmp(magic, SAVE_MAGIC, 4) == 0;
}