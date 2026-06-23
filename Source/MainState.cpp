#include <string>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "GameGlobals.h"
#include "OverworldMap.h"

using namespace std;

namespace
{
    constexpr int kMapPixelsPerCell = 2;
    constexpr int kMapDrawX = 4;
    constexpr int kMapDrawY = 4;
    constexpr int kCountyInfoHeight = 78;
}

void MainState::Init(const std::string& configfile)
{
    (void)configfile;
}

void MainState::Shutdown()
{

}

void MainState::OnEnter()
{
    if (!g_OverworldMap.IsGenerated())
    {
        unsigned int seed = g_GameDatabase.m_Setup.m_Seed;
        if (seed == 0)
        {
            seed = static_cast<unsigned int>(GetTime() * 1000.0);
        }

        g_OverworldMap.Generate(seed);
    }
}

void MainState::OnExit()
{

}

void MainState::HandleMapSelection()
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    Vector2 mouse = GetMousePosition();
    const float inputScale = g_Engine->GetInputScale();
    mouse.x /= inputScale;
    mouse.y /= inputScale;

    const int mapPixelWidth = OVERWORLD_MAP_SIZE * kMapPixelsPerCell;
    const int mapPixelHeight = OVERWORLD_MAP_SIZE * kMapPixelsPerCell;

    if (mouse.x < static_cast<float>(kMapDrawX) || mouse.y < static_cast<float>(kMapDrawY)
        || mouse.x >= static_cast<float>(kMapDrawX + mapPixelWidth)
        || mouse.y >= static_cast<float>(kMapDrawY + mapPixelHeight))
    {
        return;
    }

    const int cellX = static_cast<int>((mouse.x - static_cast<float>(kMapDrawX)) / static_cast<float>(kMapPixelsPerCell));
    const int cellY = static_cast<int>((mouse.y - static_cast<float>(kMapDrawY)) / static_cast<float>(kMapPixelsPerCell));
    const int regionId = g_OverworldMap.GetRegionId(cellX, cellY);

    if (regionId < 0)
    {
        m_SelectedRegionId = -1;
        return;
    }

    m_SelectedRegionId = regionId;
    g_GameDatabase.SetActiveRegion(regionId);
}

namespace
{
    string OwnerName(int ownerId)
    {
        if (ownerId < 0)
        {
            return "Unclaimed";
        }

        if (ownerId == 0)
        {
            return "Player";
        }

        return "Faction " + to_string(ownerId);
    }
}

void MainState::DrawCountyInfo(int panelX, int panelY, int panelWidth) const
{
    DrawRectangle(panelX, panelY, panelWidth, kCountyInfoHeight, Color{ 40, 44, 54, 255 });
    DrawRectangleLines(panelX, panelY, panelWidth, kCountyInfoHeight, Color{ 90, 90, 100, 255 });

    if (m_SelectedRegionId < 0)
    {
        DrawOutlinedText(g_font, "County", Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 4) },
            g_fontDrawSize, 1, WHITE);
        DrawOutlinedText(g_smallFont, "Click a county on the map", Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 20) },
            g_smallFontDrawSize, 1, Color{ 200, 200, 200, 255 });
        return;
    }

    const OverworldRegionData* region = g_OverworldMap.GetRegion(m_SelectedRegionId);
    if (!region)
    {
        return;
    }

    const string title = "County " + to_string(region->m_Id);
    DrawOutlinedText(g_font, title, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 4) },
        g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });

    const string resourceText = string("Resource: ") + CountyResourceName(region->m_Resource);
    const string ownerText = string("Owner: ") + OwnerName(region->m_OwnerId);
    const string castleText = string("Castle: ") + (region->m_HasCastle ? "Yes" : "No");
    const string sizeText = "Size: " + to_string(region->m_CellCount) + " cells";
    const string neighborText = "Neighbors: " + to_string(region->m_AdjacentRegionIds.size());

    DrawOutlinedText(g_smallFont, resourceText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 18) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, ownerText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 30) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, castleText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 42) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, sizeText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 54) },
        g_smallFontDrawSize, 1, Color{ 180, 180, 180, 255 });
    DrawOutlinedText(g_smallFont, neighborText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 66) },
        g_smallFontDrawSize, 1, Color{ 180, 180, 180, 255 });
}

void MainState::Update()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
    }

    if (IsKeyPressed(KEY_R))
    {
        unsigned int seed = static_cast<unsigned int>(GetTime() * 1000.0);
        g_OverworldMap.Generate(seed);
        m_SelectedRegionId = -1;
    }

    HandleMapSelection();
}

void MainState::Draw()
{
    DrawRectangle(0, 0, static_cast<int>(g_Engine->m_RenderWidth), static_cast<int>(g_Engine->m_RenderHeight),
        Color{ 24, 28, 36, 255 });

    g_OverworldMap.Draw(kMapDrawX, kMapDrawY, kMapPixelsPerCell);
    if (m_SelectedRegionId >= 0)
    {
        g_OverworldMap.DrawRegionHighlight(kMapDrawX, kMapDrawY, kMapPixelsPerCell, m_SelectedRegionId);
    }

    const int mapRight = kMapDrawX + (OVERWORLD_MAP_SIZE * kMapPixelsPerCell);
    const int panelX = mapRight + 8;
    const int panelWidth = static_cast<int>(g_Engine->m_RenderWidth) - panelX - 4;
    DrawCountyInfo(panelX, kMapDrawY, panelWidth);

    const string regionTotalText = "Regions: " + to_string(g_OverworldMap.GetRegions().size());
    DrawOutlinedText(g_smallFont, regionTotalText,
        Vector2{ static_cast<float>(panelX), static_cast<float>(kMapDrawY + kCountyInfoHeight + 2) },
        g_smallFontDrawSize, 1, Color{ 180, 180, 180, 255 });

    DrawOutlinedText(g_smallFont, "R: new map  Esc: title", Vector2{ 4.0f, static_cast<float>(g_Engine->m_RenderHeight - 14.0f) },
        g_smallFontDrawSize, 1, WHITE);
}