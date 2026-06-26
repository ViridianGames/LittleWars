#include <algorithm>
#include <string>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "GameGlobals.h"
#include "OverworldMap.h"
#include "Player.h"

using namespace std;

namespace
{
    constexpr int kMapPixelsPerCell = 2;
    constexpr int kMapDrawX = 4;
    constexpr int kMapDrawY = 4;
    constexpr int kCountyInfoHeight = 52;
    constexpr int kPlayerBoxHeight = 40;
    constexpr int kPlayerBoxGap = 2;
    constexpr int kPlayerColumnGap = 2;
    constexpr int kPlayerColumns = 2;
    constexpr int kNextTurnButtonWidth = 96;
    constexpr int kNextTurnButtonHeight = 22;
    constexpr int kPanelMargin = 4;
    constexpr bool kShowPlayerStatusBoxes = false;
    constexpr bool kShowAccessibilityGrid = true;

    Rectangle GetNextTurnButtonRect()
    {
        const float x = static_cast<float>(g_Engine->m_RenderWidth) - static_cast<float>(kNextTurnButtonWidth + kPanelMargin);
        const float y = static_cast<float>(g_Engine->m_RenderHeight) - static_cast<float>(kNextTurnButtonHeight + kPanelMargin);
        return Rectangle{ x, y, static_cast<float>(kNextTurnButtonWidth), static_cast<float>(kNextTurnButtonHeight) };
    }

    Vector2 GetScaledMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        const float inputScale = g_Engine->GetInputScale();
        mouse.x /= inputScale;
        mouse.y /= inputScale;
        return mouse;
    }

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

        g_OverworldMap.Generate(seed, g_GameDatabase.m_Setup);
    }

    if (g_GameDatabase.m_Players.empty())
    {
        const int playerCount = std::clamp(1 + g_GameDatabase.m_Setup.m_EnemyCount, 1, kMaxCampaignPlayers);
        InitializeCampaignPlayers(g_GameDatabase.m_Players, playerCount);
    }

    g_GameDatabase.SyncPlayersFromOverworld(g_OverworldMap, false);
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

    Vector2 mouse = GetScaledMousePosition();

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
    const OverworldCellType cellType = g_OverworldMap.GetCell(cellX, cellY);

    if (regionId < 0)
    {
        m_SelectedRegionId = -1;
        if (cellType == OW_MOUNTAIN || cellType == OW_WATER)
        {
            m_SelectedImpassable = true;
            m_SelectedImpassableCellType = static_cast<unsigned char>(cellType);
        }
        else
        {
            m_SelectedImpassable = false;
        }

        return;
    }

    m_SelectedImpassable = false;
    m_SelectedRegionId = regionId;
    g_GameDatabase.SetActiveRegion(regionId);
}

void MainState::DrawCountyInfo(int panelX, int panelY, int panelWidth) const
{
    DrawRectangle(panelX, panelY, panelWidth, kCountyInfoHeight, Color{ 40, 44, 54, 255 });
    DrawRectangleLines(panelX, panelY, panelWidth, kCountyInfoHeight, Color{ 90, 90, 100, 255 });

    if (m_SelectedRegionId < 0)
    {
        if (m_SelectedImpassable && m_SelectedImpassableCellType == static_cast<unsigned char>(OW_MOUNTAIN))
        {
            DrawOutlinedText(g_font, "Mountains", Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 4) },
                g_fontDrawSize, 1, Color{ 180, 180, 180, 255 });
            DrawOutlinedText(g_smallFont, "Impassable",
                Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 20) },
                g_smallFontDrawSize, 1, Color{ 200, 200, 200, 255 });
            return;
        }

        if (m_SelectedImpassable && m_SelectedImpassableCellType == static_cast<unsigned char>(OW_WATER))
        {
            DrawOutlinedText(g_font, "Water", Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 4) },
                g_fontDrawSize, 1, Color{ 140, 200, 255, 255 });
            DrawOutlinedText(g_smallFont, "Impassable",
                Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 20) },
                g_smallFontDrawSize, 1, Color{ 200, 200, 200, 255 });
            return;
        }

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

    const string title = region->m_IsWater
        ? "Lake " + to_string(region->m_Id)
        : "County " + to_string(region->m_Id);
    DrawOutlinedText(g_font, title, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 4) },
        g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });

    if (region->m_IsWater)
    {
        DrawOutlinedText(g_smallFont, "Inland water - impassable",
            Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 20) },
            g_smallFontDrawSize, 1, Color{ 140, 200, 255, 255 });
        return;
    }

    const string resourceText = string("Resource: ") + CountyResourceName(region->m_Resource);
    const string ownerText = string("Owner: ") + PlayerOwnerName(region->m_OwnerId);
    const string castleText = string("Castle: ") + (region->m_HasCastle ? "Yes" : "No");

    DrawOutlinedText(g_smallFont, resourceText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 16) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, ownerText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 28) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, castleText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 40) },
        g_smallFontDrawSize, 1, WHITE);
}

bool MainState::IsMouseOverNextTurnButton() const
{
    return CheckCollisionPointRec(GetScaledMousePosition(), GetNextTurnButtonRect());
}

void MainState::HandleNextTurnButton()
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    if (!IsMouseOverNextTurnButton())
    {
        return;
    }

    g_GameDatabase.AdvanceTurn(g_OverworldMap);
}

void MainState::DrawNextTurnButton() const
{
    const Rectangle buttonRect = GetNextTurnButtonRect();
    const bool hovered = IsMouseOverNextTurnButton();
    const Color fillColor = hovered ? Color{ 70, 120, 70, 255 } : Color{ 48, 88, 48, 255 };
    const Color borderColor = hovered ? Color{ 140, 220, 140, 255 } : Color{ 100, 160, 100, 255 };

    DrawRectangle(static_cast<int>(buttonRect.x), static_cast<int>(buttonRect.y),
        static_cast<int>(buttonRect.width), static_cast<int>(buttonRect.height), fillColor);
    DrawRectangleLines(static_cast<int>(buttonRect.x), static_cast<int>(buttonRect.y),
        static_cast<int>(buttonRect.width), static_cast<int>(buttonRect.height), borderColor);

    const string label = "Next Turn";
    const Vector2 textSize = MeasureTextEx(*g_font, label.c_str(), g_fontDrawSize, 1.0f);
    const float textX = buttonRect.x + (buttonRect.width - textSize.x) * 0.5f;
    const float textY = buttonRect.y + (buttonRect.height - textSize.y) * 0.5f - 1.0f;
    DrawOutlinedText(g_font, label, Vector2{ textX, textY }, g_fontDrawSize, 1, WHITE);
}

void MainState::DrawPlayerSummaries(int panelX, int panelY, int panelWidth) const
{
    const vector<Player>& players = g_GameDatabase.m_Players;
    if (players.empty())
    {
        return;
    }

    const int columnWidth = (panelWidth - kPlayerColumnGap) / kPlayerColumns;

    for (size_t playerIndex = 0; playerIndex < players.size(); ++playerIndex)
    {
        const Player& player = players[playerIndex];
        const int column = static_cast<int>(playerIndex) % kPlayerColumns;
        const int row = static_cast<int>(playerIndex) / kPlayerColumns;
        const int boxX = panelX + (column * (columnWidth + kPlayerColumnGap));
        const int boxY = panelY + (row * (kPlayerBoxHeight + kPlayerBoxGap));

        const Color playerColor = player.GetColor();
        const Color bgColor = Color{
            static_cast<unsigned char>(playerColor.r / 5 + 18),
            static_cast<unsigned char>(playerColor.g / 5 + 18),
            static_cast<unsigned char>(playerColor.b / 5 + 18),
            255
        };

        DrawRectangle(boxX, boxY, columnWidth, kPlayerBoxHeight, bgColor);
        DrawRectangleLines(boxX, boxY, columnWidth, kPlayerBoxHeight, playerColor);

        string title = player.GetColorName();
        if (player.m_IsHuman)
        {
            title += " (you)";
        }

        DrawOutlinedText(g_font, title,
            Vector2{ static_cast<float>(boxX + 3), static_cast<float>(boxY + 2) },
            g_fontDrawSize, 1, playerColor);

        const string resources = "F:" + to_string(player.m_Food)
            + " I:" + to_string(player.m_Iron)
            + " G:" + to_string(player.m_Gold)
            + " W:" + to_string(player.m_Wood);
        DrawOutlinedText(g_smallFont, resources,
            Vector2{ static_cast<float>(boxX + 3), static_cast<float>(boxY + 11) },
            g_smallFontDrawSize, 1, WHITE);

        const string regions = "Rgns F" + to_string(player.m_FoodRegions)
            + " I" + to_string(player.m_IronRegions)
            + " G" + to_string(player.m_GoldRegions)
            + " W" + to_string(player.m_WoodRegions)
            + " (" + to_string(player.m_TotalRegions) + ")";
        DrawOutlinedText(g_smallFont, regions,
            Vector2{ static_cast<float>(boxX + 3), static_cast<float>(boxY + 20) },
            g_smallFontDrawSize, 1, Color{ 210, 210, 210, 255 });

        const string units = "C" + to_string(player.m_Castles)
            + " Sw" + to_string(player.m_Swordsmen)
            + " Ar" + to_string(player.m_Archers)
            + " Kn" + to_string(player.m_Knights)
            + " Ct" + to_string(player.m_Catapults);
        DrawOutlinedText(g_smallFont, units,
            Vector2{ static_cast<float>(boxX + 3), static_cast<float>(boxY + 29) },
            g_smallFontDrawSize, 1, Color{ 210, 210, 210, 255 });
    }
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
        g_OverworldMap.Generate(seed, g_GameDatabase.m_Setup);
        m_SelectedRegionId = -1;
        g_GameDatabase.SyncPlayersFromOverworld(g_OverworldMap, true);
    }

    HandleNextTurnButton();
    HandleMapSelection();
}

void MainState::Draw()
{
    DrawRectangle(0, 0, static_cast<int>(g_Engine->m_RenderWidth), static_cast<int>(g_Engine->m_RenderHeight),
        Color{ 24, 28, 36, 255 });

    g_OverworldMap.Draw(kMapDrawX, kMapDrawY, kMapPixelsPerCell, m_SelectedRegionId);

    const int mapRight = kMapDrawX + (OVERWORLD_MAP_SIZE * kMapPixelsPerCell);
    const int panelX = mapRight + 8;
    const int panelWidth = static_cast<int>(g_Engine->m_RenderWidth) - panelX - 4;
    const int sidePanelY = kMapDrawY + kCountyInfoHeight + kPlayerBoxGap;
    const int sidePanelHeight = static_cast<int>(g_Engine->m_RenderHeight) - sidePanelY - kNextTurnButtonHeight - (kPanelMargin * 2);

    DrawCountyInfo(panelX, kMapDrawY, panelWidth);

    if (kShowAccessibilityGrid)
    {
        g_OverworldMap.DrawAccessibilityGrid(panelX, sidePanelY, panelWidth, sidePanelHeight, m_SelectedRegionId);
    }

    if (kShowPlayerStatusBoxes)
    {
        DrawPlayerSummaries(panelX, sidePanelY, panelWidth);
    }

    DrawNextTurnButton();

    DrawOutlinedText(g_smallFont, "R: new map  Esc: title", Vector2{ 4.0f, static_cast<float>(g_Engine->m_RenderHeight - 14.0f) },
        g_smallFontDrawSize, 1, WHITE);
}