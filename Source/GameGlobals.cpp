#include <cmath>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../Geist/Source/Globals.h"
#include "../Geist/Source/IO.h"
#include "../Geist/Source/Logging.h"

#include "GameGlobals.h"

#include "Engine.h"
#include "raylib.h"
#include "rlgl.h"

using namespace std;

GameDatabase g_GameDatabase;

namespace
{
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

    void AddHill(RegionHeightfield& heightfield, RNG& rng)
    {
        const float radius = static_cast<float>(rng.RandomRange(1, 8));

        const int centerX = rng.Random(REGION_VERTICES);
        const int centerY = rng.Random(REGION_VERTICES);

        for (float i = -radius; i <= radius; ++i)
        {
            const float disc = radius * radius - i * i;
            if (disc < 0.0f)
            {
                continue;
            }

            const float span = std::sqrt(disc);
            for (float j = -span; j <= span; ++j)
            {
                float value = radius * radius - i * i - j * j;
                if (value < 0.0f)
                {
                    value = 0.0f;
                }

                const int x = centerX + static_cast<int>(i);
                const int y = centerY + static_cast<int>(j);
                if (x >= 0 && x < REGION_VERTICES && y >= 0 && y < REGION_VERTICES)
                {
                    heightfield.SetHeight(x, y, heightfield.GetHeight(x, y) + std::sqrt(value));
                }
            }
        }
    }

    void ClassifyTerrainFromHeights(RegionHeightfield& heightfield)
    {
        for (int x = 0; x < REGION_CELLS; ++x)
        {
            for (int y = 0; y < REGION_CELLS; ++y)
            {
                const float h00 = heightfield.GetHeight(x, y);
                const float h10 = heightfield.GetHeight(x + 1, y);
                const float h01 = heightfield.GetHeight(x, y + 1);
                const float h11 = heightfield.GetHeight(x + 1, y + 1);
                const float avg = (h00 + h10 + h01 + h11) * 0.25f;

                unsigned char terrainType = RTT_GRASS;
                if (avg <= 0.05f)
                {
                    terrainType = RTT_WATER;
                }
                else if (avg <= 0.35f)
                {
                    terrainType = RTT_BEACH;
                }
                else if (avg <= 0.75f)
                {
                    terrainType = RTT_SWAMP;
                }
                else if (avg >= 5.5f)
                {
                    terrainType = RTT_STONE;
                }

                heightfield.SetTerrainType(x, y, terrainType);
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
    m_Player = PlayerData{};
    m_Regions.clear();
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
}

void GameDatabase::GenerateOverworldRegions()
{
    RNG rng;
    rng.SeedRNG(m_Setup.m_Seed);

    m_Player.m_Id = 0;
    m_Player.m_Gold = 100;
    m_Player.m_Food = 50;

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
            m_Regions.push_back(region);
        }
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
    heightfield.m_Heights.assign(static_cast<size_t>(REGION_VERTICES * REGION_VERTICES), 0.0f);
    heightfield.m_TerrainTypes.assign(static_cast<size_t>(REGION_CELLS * REGION_CELLS), RTT_GRASS);

    RNG rng;
    rng.SeedRNG(heightfield.m_Seed);

    for (int i = 0; i < REGION_VERTICES + REGION_VERTICES; ++i)
    {
        AddHill(heightfield, rng);
    }

    float highestHeight = 0.0f;
    for (int x = 0; x < REGION_VERTICES; ++x)
    {
        for (int y = 0; y < REGION_VERTICES; ++y)
        {
            highestHeight = std::max(highestHeight, heightfield.GetHeight(x, y));
        }
    }

    if (highestHeight > 0.0f)
    {
        const float scalar = 8.0f / highestHeight;
        for (int x = 0; x < REGION_VERTICES; ++x)
        {
            for (int y = 0; y < REGION_VERTICES; ++y)
            {
                heightfield.SetHeight(x, y, heightfield.GetHeight(x, y) * scalar);
            }
        }
    }

    const int terrainSinker = rng.Random(3);
    for (int x = 0; x < REGION_VERTICES; ++x)
    {
        for (int y = 0; y < REGION_VERTICES; ++y)
        {
            float value = heightfield.GetHeight(x, y) - static_cast<float>(terrainSinker);
            if (value < 0.0f)
            {
                value = 0.0f;
            }
            heightfield.SetHeight(x, y, value);
        }
    }

    for (int x = 0; x < REGION_VERTICES; ++x)
    {
        for (int y = 0; y < REGION_VERTICES; ++y)
        {
            if (x == 0 || x == REGION_CELLS || y == 0 || y == REGION_CELLS)
            {
                heightfield.SetHeight(x, y, 0.0f);
            }
            else if (x == 1 || x == REGION_CELLS - 1 || y == 1 || y == REGION_CELLS - 1)
            {
                if (heightfield.GetHeight(x, y) > 1.0f)
                {
                    heightfield.SetHeight(x, y, 1.0f);
                }
            }
        }
    }

    ClassifyTerrainFromHeights(heightfield);
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
    IO::Serialize(stream, m_Setup.m_Difficulty);
    IO::Serialize(stream, m_Setup.m_EnemyCount);
    IO::Serialize(stream, static_cast<int>(m_Setup.m_MapSize));
    IO::Serialize(stream, m_Setup.m_RegionColumns);
    IO::Serialize(stream, m_Setup.m_RegionRows);

    IO::Serialize(stream, m_Turn);
    IO::Serialize(stream, m_ActiveRegionId);

    IO::Serialize(stream, m_Player.m_Id);
    IO::Serialize(stream, m_Player.m_Gold);
    IO::Serialize(stream, m_Player.m_Food);

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
        IO::Serialize(stream, region.m_HeightfieldSeed);
        SerializeHeightfield(stream, region.m_Heightfield);
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
    if (version != SAVE_VERSION)
    {
        Log("GameDatabase::LoadCampaign - unsupported save version in " + path);
        return false;
    }

    Clear();

    IO::Serialize(stream, m_Setup.m_Seed);
    IO::Serialize(stream, m_Setup.m_Difficulty);
    IO::Serialize(stream, m_Setup.m_EnemyCount);
    int mapSize = 0;
    IO::Serialize(stream, mapSize);
    m_Setup.m_MapSize = static_cast<MapSize>(mapSize);
    IO::Serialize(stream, m_Setup.m_RegionColumns);
    IO::Serialize(stream, m_Setup.m_RegionRows);

    IO::Serialize(stream, m_Turn);
    IO::Serialize(stream, m_ActiveRegionId);

    IO::Serialize(stream, m_Player.m_Id);
    IO::Serialize(stream, m_Player.m_Gold);
    IO::Serialize(stream, m_Player.m_Food);

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
        IO::Serialize(stream, region.m_HeightfieldSeed);
        SerializeHeightfield(stream, region.m_Heightfield);
    }

    Log("GameDatabase::LoadCampaign - loaded from " + path);
    return true;
}