#include <algorithm>
#include <string>
#include <vector>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "CampaignAI.h"
#include "GameGlobals.h"
#include "MapTilesSprites.h"
#include "OverworldMap.h"
#include "Player.h"
#include "PlayerTasksConfig.h"

using namespace std;

namespace
{
    constexpr int kMapPixelsPerCell = 2;
    constexpr int kMapDrawX = 4;
    constexpr int kResourceBarHeight = 12;
    constexpr int kResourceBarY = 4;
    constexpr int kMapDrawY = kResourceBarY + kResourceBarHeight + 1;
    constexpr int kCountyInfoHeight = 78;
    constexpr int kPlayerBoxHeight = 40;
    constexpr int kPlayerBoxGap = 2;
    constexpr int kPlayerColumnGap = 2;
    constexpr int kPlayerColumns = 2;
    constexpr int kNextTurnButtonWidth = 96;
    constexpr int kNextTurnButtonHeight = 22;
    constexpr int kVisitButtonWidth = 48;
    constexpr int kVisitButtonHeight = 14;
    constexpr int kPanelMargin = 4;
    constexpr bool kShowPlayerStatusBoxes = false;
    constexpr bool kShowAccessibilityGrid = true;
    constexpr int kResourceTooltipPadding = 4;
    constexpr int kResourceTooltipLineHeight = 12;
    constexpr int kResourceIconCenterOffset = 5;

    constexpr CountyResource kResourceBarSlotOrder[4] = {
        CountyResource::Food,
        CountyResource::Wood,
        CountyResource::Iron,
        CountyResource::Gold
    };

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

    void DrawResourceBarEntry(
        int slotX,
        int barY,
        CountyResource resource,
        int amount,
        int delta)
    {
        const float centerY = static_cast<float>(barY) + static_cast<float>(kResourceBarHeight) * 0.5f;
        const Vector2 iconCenter{
            static_cast<float>(slotX + kResourceIconCenterOffset),
            centerY
        };
        DrawRegionResourceIcon(resource, iconCenter, WHITE);

        const string amountText = to_string(amount);
        const Vector2 amountSize = MeasureTextEx(*g_smallFont, amountText.c_str(), g_smallFontDrawSize, 1.0f);
        const float textX = static_cast<float>(slotX + 12);
        const float textY = centerY - amountSize.y * 0.5f;

        DrawOutlinedText(g_smallFont, amountText, Vector2{ textX, textY }, g_smallFontDrawSize, 1, WHITE);

        if (delta != 0)
        {
            const string deltaText = delta > 0 ? "+" + to_string(delta) : to_string(delta);
            const Color deltaColor = delta > 0
                ? Color{ 120, 220, 120, 255 }
                : Color{ 220, 120, 120, 255 };

            DrawOutlinedText(
                g_smallFont,
                deltaText,
                Vector2{ textX + amountSize.x + 3.0f, textY },
                g_smallFontDrawSize,
                1,
                deltaColor);
        }
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
            g_GameDatabase.m_Setup.m_Seed = seed;
        }

        g_OverworldMap.Generate(seed, g_GameDatabase.m_Setup);
    }

    if (g_GameDatabase.m_Players.empty())
    {
        const int playerCount = GetCampaignPlayerCount(g_GameDatabase.m_Setup);
        InitializeCampaignPlayers(g_GameDatabase.m_Players, playerCount, !g_GameDatabase.m_Setup.m_AllAi);
        AssignCampaignAiPersonalities(g_GameDatabase.m_Players, g_GameDatabase.m_Setup);
    }

    g_GameDatabase.SyncPlayersFromOverworld(g_OverworldMap, false);

    // All-AI campaigns open the observer by default so you can see what is happening.
    m_AiObserverOpen = IsAllAiGame();
    m_AiAutoPlay = false;
    m_AiAutoPlayTimer = 0.0f;
    m_AiObserverFilter = -1;
    m_WatchedPlayerId = 0;
    if (!g_GameDatabase.m_Players.empty())
    {
        m_WatchedPlayerId = g_GameDatabase.m_Players.front().m_Id;
    }

    if (g_GameDatabase.m_Regions.empty() && g_OverworldMap.IsGenerated())
    {
        g_GameDatabase.BuildRegionsFromOverworld(g_OverworldMap);
        g_GameDatabase.GenerateAllRegionHeightfields();
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

    const OverworldRegionData* region = g_OverworldMap.GetRegion(regionId);
    if (!region || region->m_IsWater)
    {
        m_SelectedRegionId = -1;
        m_SelectedImpassable = true;
        m_SelectedImpassableCellType = static_cast<unsigned char>(OW_WATER);
        return;
    }

    m_SelectedImpassable = false;
    m_SelectedRegionId = regionId;
    g_GameDatabase.SetActiveRegion(regionId);
}

Rectangle MainState::GetResourceBarSlotRect(int barX, int barWidth, int slotIndex) const
{
    const int slotWidth = barWidth / 4;
    return Rectangle{
        static_cast<float>(barX + (slotIndex * slotWidth)),
        static_cast<float>(kResourceBarY),
        static_cast<float>(slotWidth),
        static_cast<float>(kResourceBarHeight)
    };
}

void MainState::HandleResourceBarInput(int barX, int barWidth)
{
    m_HoveredResourceBarSlot = -1;
    const Vector2 mouse = GetScaledMousePosition();

    for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
    {
        if (CheckCollisionPointRec(mouse, GetResourceBarSlotRect(barX, barWidth, slotIndex)))
        {
            m_HoveredResourceBarSlot = slotIndex;
            break;
        }
    }
}

int MainState::GetResourceBarIconLeftX(int barX, int barWidth, int slotIndex) const
{
    const int slotWidth = barWidth / 4;
    const int slotX = barX + (slotIndex * slotWidth);
    const CountyResource resource = kResourceBarSlotOrder[slotIndex];
    const MapTilesSpriteSpec iconSpec = GetMapTilesResourceSpec(resource);
    return slotX + kResourceIconCenterOffset - (iconSpec.m_Width / 2);
}

void MainState::DrawResourceBarTooltip(int barX, int barWidth) const
{
    if (m_HoveredResourceBarSlot < 0 || m_HoveredResourceBarSlot >= 4)
    {
        return;
    }

    const Player* watched = GetWatchedPlayer();
    if (!watched)
    {
        return;
    }

    const CountyResource resource = kResourceBarSlotOrder[m_HoveredResourceBarSlot];
    std::vector<ResourceTurnLine> lines;
    ComputePlayerResourceBreakdown(g_OverworldMap, *watched, resource, lines);
    if (lines.empty())
    {
        return;
    }

    struct TooltipRow
    {
        string m_Text;
        Color m_Color;
    };

    std::vector<TooltipRow> rows;
    rows.reserve(lines.size());
    for (const ResourceTurnLine& line : lines)
    {
        TooltipRow row{};
        if (line.m_Amount > 0)
        {
            row.m_Text = "+" + to_string(line.m_Amount) + " " + line.m_Label;
            row.m_Color = Color{ 120, 220, 120, 255 };
        }
        else
        {
            row.m_Text = to_string(line.m_Amount) + " " + line.m_Label;
            row.m_Color = Color{ 220, 120, 120, 255 };
        }

        rows.push_back(row);
    }

    float maxTextWidth = 0.0f;
    for (const TooltipRow& row : rows)
    {
        const Vector2 textSize = MeasureTextEx(*g_smallFont, row.m_Text.c_str(), g_smallFontDrawSize, 1.0f);
        maxTextWidth = std::max(maxTextWidth, textSize.x);
    }

    const int tooltipX = GetResourceBarIconLeftX(barX, barWidth, m_HoveredResourceBarSlot);
    const int tooltipY = kMapDrawY;
    const int tooltipWidth = static_cast<int>(maxTextWidth) + (kResourceTooltipPadding * 2);
    const int tooltipHeight = (kResourceTooltipPadding * 2) + (static_cast<int>(rows.size()) * kResourceTooltipLineHeight);

    DrawRectangle(tooltipX, tooltipY, tooltipWidth, tooltipHeight, Color{ 24, 28, 36, 245 });
    DrawRectangleLines(tooltipX, tooltipY, tooltipWidth, tooltipHeight, Color{ 90, 90, 100, 255 });

    int lineY = tooltipY + kResourceTooltipPadding;
    for (const TooltipRow& row : rows)
    {
        DrawOutlinedText(
            g_smallFont,
            row.m_Text,
            Vector2{ static_cast<float>(tooltipX + kResourceTooltipPadding), static_cast<float>(lineY) },
            g_smallFontDrawSize,
            1,
            row.m_Color);
        lineY += kResourceTooltipLineHeight;
    }
}

bool MainState::IsAllAiGame() const
{
    return g_GameDatabase.m_Setup.m_AllAi || GetHumanPlayer(g_GameDatabase.m_Players) == nullptr;
}

const Player* MainState::GetWatchedPlayer() const
{
    if (const Player* human = GetHumanPlayer(g_GameDatabase.m_Players))
    {
        return human;
    }

    if (m_WatchedPlayerId >= 0
        && m_WatchedPlayerId < static_cast<int>(g_GameDatabase.m_Players.size()))
    {
        return &g_GameDatabase.m_Players[static_cast<size_t>(m_WatchedPlayerId)];
    }

    return g_GameDatabase.m_Players.empty() ? nullptr : &g_GameDatabase.m_Players.front();
}

void MainState::CycleObserverFilter(int delta)
{
    const int playerCount = static_cast<int>(g_GameDatabase.m_Players.size());
    if (playerCount <= 0)
    {
        m_AiObserverFilter = -1;
        return;
    }

    // Cycle: All (-1) -> 0 -> 1 -> ... -> last -> All
    int index = m_AiObserverFilter; // -1 means All
    index += delta;
    if (index < -1)
    {
        index = playerCount - 1;
    }
    if (index >= playerCount)
    {
        index = -1;
    }
    m_AiObserverFilter = index;
    if (m_AiObserverFilter >= 0)
    {
        m_WatchedPlayerId = m_AiObserverFilter;
    }
}

void MainState::HandleAiObserverInput()
{
    if (IsKeyPressed(KEY_O))
    {
        m_AiObserverOpen = !m_AiObserverOpen;
    }

    if (IsKeyPressed(KEY_LEFT_BRACKET) || IsKeyPressed(KEY_COMMA))
    {
        CycleObserverFilter(-1);
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET) || IsKeyPressed(KEY_PERIOD))
    {
        CycleObserverFilter(1);
    }

    if (IsAllAiGame() && IsKeyPressed(KEY_A))
    {
        m_AiAutoPlay = !m_AiAutoPlay;
        m_AiAutoPlayTimer = 0.0f;
    }

    if (m_AiAutoPlay && IsAllAiGame())
    {
        m_AiAutoPlayTimer += GetFrameTime();
        if (m_AiAutoPlayTimer >= 0.85f)
        {
            m_AiAutoPlayTimer = 0.0f;
            g_GameDatabase.AdvanceTurn(g_OverworldMap);
        }
    }
}

void MainState::DrawAiObserverPane() const
{
    if (!m_AiObserverOpen)
    {
        return;
    }

    const int mapPixelWidth = OVERWORLD_MAP_SIZE * kMapPixelsPerCell;
    const int paneX = kMapDrawX;
    const int paneY = kMapDrawY + 2;
    const int paneW = mapPixelWidth;
    const int paneH = static_cast<int>(g_Engine->m_RenderHeight) - paneY - 18;

    DrawRectangle(paneX, paneY, paneW, paneH, Color{ 12, 14, 20, 220 });
    DrawRectangleLines(paneX, paneY, paneW, paneH, Color{ 120, 160, 200, 255 });

    string title = "AI Observer";
    if (IsAllAiGame())
    {
        title += m_AiAutoPlay ? "  [AUTO]" : "  [watch]";
    }
    DrawOutlinedText(g_font, title,
        Vector2{ static_cast<float>(paneX + 4), static_cast<float>(paneY + 2) },
        g_fontDrawSize, 1, Color{ 160, 210, 255, 255 });

    string filterLabel = "Filter: All";
    if (m_AiObserverFilter >= 0
        && m_AiObserverFilter < static_cast<int>(g_GameDatabase.m_Players.size()))
    {
        const Player& p = g_GameDatabase.m_Players[static_cast<size_t>(m_AiObserverFilter)];
        filterLabel = string("Filter: ") + p.GetColorName() + " (" + AiPersonalityName(p.m_AiPersonality) + ")";
    }
    DrawOutlinedText(g_smallFont, filterLabel,
        Vector2{ static_cast<float>(paneX + 4), static_cast<float>(paneY + 14) },
        g_smallFontDrawSize, 1, Color{ 200, 200, 210, 255 });

    // Compact roster strip
    float rosterX = static_cast<float>(paneX + 4);
    const float rosterY = static_cast<float>(paneY + 24);
    for (const Player& player : g_GameDatabase.m_Players)
    {
        if (player.m_IsHuman)
        {
            continue;
        }
        if (player.m_TotalRegions <= 0 && !IsAllAiGame())
        {
            // Still show eliminated lightly
        }

        const string chip = string(player.GetColorName()) + " "
            + string(AiPersonalityName(player.m_AiPersonality)).substr(0, 3)
            + " r" + to_string(player.m_TotalRegions)
            + " a" + to_string(GetPlayerArmyStrength(player));
        const Vector2 chipSize = MeasureTextEx(*g_smallFont, chip.c_str(), g_smallFontDrawSize, 1.0f);
        if (rosterX + chipSize.x > static_cast<float>(paneX + paneW - 4))
        {
            break;
        }
        const Color chipColor = (m_AiObserverFilter == player.m_Id) ? WHITE : player.GetColor();
        DrawOutlinedText(g_smallFont, chip, Vector2{ rosterX, rosterY }, g_smallFontDrawSize, 1, chipColor);
        rosterX += chipSize.x + 6.0f;
    }

    // Action log (newest first)
    std::vector<const AiLogEntry*> lines;
    g_AiObserverLog.CollectFiltered(m_AiObserverFilter, 18, lines);

    float lineY = rosterY + 12.0f;
    DrawOutlinedText(g_smallFont, "Recent AI actions:",
        Vector2{ static_cast<float>(paneX + 4), lineY },
        g_smallFontDrawSize, 1, Color{ 180, 180, 120, 255 });
    lineY += 10.0f;

    if (lines.empty())
    {
        DrawOutlinedText(g_smallFont, "(no AI actions yet — press Next Turn)",
            Vector2{ static_cast<float>(paneX + 4), lineY },
            g_smallFontDrawSize, 1, GRAY);
    }
    else
    {
        for (const AiLogEntry* entry : lines)
        {
            if (lineY + g_smallFontDrawSize > static_cast<float>(paneY + paneH - 16))
            {
                break;
            }

            Color color = Color{ 200, 200, 200, 255 };
            if (entry->m_PlayerId >= 0
                && entry->m_PlayerId < static_cast<int>(g_GameDatabase.m_Players.size()))
            {
                color = g_GameDatabase.m_Players[static_cast<size_t>(entry->m_PlayerId)].GetColor();
            }

            const string row = "T" + to_string(entry->m_Turn) + " " + entry->m_Message;
            DrawOutlinedText(g_smallFont, row,
                Vector2{ static_cast<float>(paneX + 4), lineY },
                g_smallFontDrawSize, 1, color);
            lineY += g_smallFontDrawSize + 1.0f;
        }
    }

    DrawOutlinedText(g_smallFont, "O:close  [/]:filter  A:auto (all-AI)  Next Turn advances",
        Vector2{ static_cast<float>(paneX + 4), static_cast<float>(paneY + paneH - 12) },
        g_smallFontDrawSize, 1, Color{ 150, 150, 160, 255 });
}

void MainState::DrawPlayerResourceBar(int barX, int barY, int barWidth) const
{
    const Player* watched = GetWatchedPlayer();
    if (!watched)
    {
        return;
    }

    DrawRectangle(barX, barY, barWidth, kResourceBarHeight, Color{ 30, 34, 42, 255 });
    DrawRectangleLines(barX, barY, barWidth, kResourceBarHeight, Color{ 90, 90, 100, 255 });

    // In observer mode, tag whose resources we are showing.
    if (IsAllAiGame() || GetHumanPlayer(g_GameDatabase.m_Players) == nullptr)
    {
        const string tag = string(watched->GetColorName()) + " res";
        DrawOutlinedText(g_smallFont, tag,
            Vector2{ static_cast<float>(barX + barWidth - 48), static_cast<float>(barY + 1) },
            g_smallFontDrawSize, 1, watched->GetColor());
    }

    ResourceAmount turnDelta{};
    ComputePlayerTurnDelta(g_OverworldMap, *watched, turnDelta);

    const int slotWidth = barWidth / 4;
    DrawResourceBarEntry(barX, barY, CountyResource::Food, watched->m_Food, turnDelta.m_Food);
    DrawResourceBarEntry(barX + slotWidth, barY, CountyResource::Wood, watched->m_Wood, turnDelta.m_Wood);
    DrawResourceBarEntry(barX + (slotWidth * 2), barY, CountyResource::Iron, watched->m_Iron, turnDelta.m_Iron);
    DrawResourceBarEntry(barX + (slotWidth * 3), barY, CountyResource::Gold, watched->m_Gold, turnDelta.m_Gold);
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
    string ownerText = string("Owner: ") + PlayerOwnerName(region->m_OwnerId);
    if (region->m_OwnerId >= 0
        && region->m_OwnerId < static_cast<int>(g_GameDatabase.m_Players.size()))
    {
        const Player& owner = g_GameDatabase.m_Players[static_cast<size_t>(region->m_OwnerId)];
        if (!owner.m_IsHuman)
        {
            ownerText += string(" (") + AiPersonalityName(owner.m_AiPersonality) + ")";
        }
    }
    string castleText = string("Castle: ");
    if (region->m_HasCastle)
    {
        castleText += "Yes";
    }
    else if (region->m_CastleBuildTurnsRemaining > 0)
    {
        castleText += "Building (" + to_string(region->m_CastleBuildTurnsRemaining) + " turns)";
    }
    else
    {
        castleText += "No";
    }

    const string outputText = "Output: " + to_string(GetRegionTurnIncome(*region))
        + "/turn (x" + to_string(GetRegionIncomeMultiplier(*region)) + ")";

    DrawOutlinedText(g_smallFont, resourceText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 16) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, ownerText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 28) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, castleText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 40) },
        g_smallFontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, outputText, Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 52) },
        g_smallFontDrawSize, 1, Color{ 180, 220, 180, 255 });

    DrawVisitButton(panelX, panelY, panelWidth);
}

bool MainState::CanVisitSelectedRegion() const
{
    if (m_SelectedRegionId < 0)
    {
        return false;
    }

    const OverworldRegionData* region = g_OverworldMap.GetRegion(m_SelectedRegionId);
    return region && !region->m_IsWater;
}

Rectangle MainState::GetVisitButtonRect(int panelX, int panelY, int panelWidth) const
{
    return Rectangle{
        static_cast<float>(panelX + panelWidth - kVisitButtonWidth - 4),
        static_cast<float>(panelY + kCountyInfoHeight - kVisitButtonHeight - 4),
        static_cast<float>(kVisitButtonWidth),
        static_cast<float>(kVisitButtonHeight)
    };
}

bool MainState::IsMouseOverVisitButton(int panelX, int panelY, int panelWidth) const
{
    if (!CanVisitSelectedRegion())
    {
        return false;
    }

    return CheckCollisionPointRec(GetScaledMousePosition(), GetVisitButtonRect(panelX, panelY, panelWidth));
}

void MainState::DrawVisitButton(int panelX, int panelY, int panelWidth) const
{
    if (!CanVisitSelectedRegion())
    {
        return;
    }

    const Rectangle buttonRect = GetVisitButtonRect(panelX, panelY, panelWidth);
    const bool hovered = IsMouseOverVisitButton(panelX, panelY, panelWidth);
    const Color fill = hovered ? Color{ 70, 110, 70, 255 } : Color{ 50, 80, 50, 255 };
    const Color border = hovered ? Color{ 160, 220, 160, 255 } : Color{ 110, 160, 110, 255 };

    DrawRectangleRec(buttonRect, fill);
    DrawRectangleLinesEx(buttonRect, 1.0f, border);

    const string label = "Visit";
    const Vector2 textSize = MeasureTextEx(*g_smallFont, label.c_str(), g_smallFontDrawSize, 1.0f);
    DrawOutlinedText(
        g_smallFont,
        label,
        Vector2{
            buttonRect.x + (buttonRect.width - textSize.x) * 0.5f,
            buttonRect.y + (buttonRect.height - textSize.y) * 0.5f
        },
        g_smallFontDrawSize,
        1,
        WHITE);
}

void MainState::HandleVisitButton(int panelX, int panelY, int panelWidth)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    if (!IsMouseOverVisitButton(panelX, panelY, panelWidth))
    {
        return;
    }

    g_GameDatabase.SetActiveRegion(m_SelectedRegionId);
    g_GameDatabase.EnsureRegionHeightfield(m_SelectedRegionId);
    g_StateMachine->MakeStateTransition(STATE_COMBATSTATE);
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

int MainState::GetTaskPanelHeight() const
{
    if (IsAllAiGame())
    {
        // Compact strip: all-AI mode has no player tasks.
        return 28;
    }

    const int taskCount = g_PlayerTasksConfig.GetTaskCount();
    if (taskCount <= 0)
    {
        return 0;
    }

    const PlayerTaskLayout& layout = g_PlayerTasksConfig.GetLayout();
    return 16 + (taskCount * layout.m_RowHeight) + 14;
}

Rectangle MainState::GetTaskRowRect(int panelX, int panelY, int taskIndex) const
{
    const PlayerTaskLayout& layout = g_PlayerTasksConfig.GetLayout();
    const float y = static_cast<float>(panelY + 16 + (taskIndex * layout.m_RowHeight));
    return Rectangle{
        static_cast<float>(panelX),
        y,
        static_cast<float>(layout.m_CostX + 80),
        static_cast<float>(layout.m_RowHeight)
    };
}

bool MainState::IsMouseOverTaskRow(int panelX, int panelY, int taskIndex) const
{
    return CheckCollisionPointRec(GetScaledMousePosition(), GetTaskRowRect(panelX, panelY, taskIndex));
}

void MainState::HandleTaskPanelInput(int panelX, int panelY, int panelWidth)
{
    (void)panelWidth;

    // Observer / all-AI games have no task orders to issue.
    if (IsAllAiGame())
    {
        m_HoveredTaskIndex = -1;
        return;
    }

    const int taskCount = g_PlayerTasksConfig.GetTaskCount();
    if (taskCount <= 0)
    {
        m_HoveredTaskIndex = -1;
        return;
    }

    m_HoveredTaskIndex = -1;
    for (int taskIndex = 0; taskIndex < taskCount; ++taskIndex)
    {
        if (IsMouseOverTaskRow(panelX, panelY, taskIndex))
        {
            m_HoveredTaskIndex = taskIndex;
            break;
        }
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || m_HoveredTaskIndex < 0)
    {
        return;
    }

    Player* humanPlayer = GetHumanPlayer(g_GameDatabase.m_Players);
    if (!humanPlayer)
    {
        return;
    }

    const PlayerTaskDefinition& task = g_PlayerTasksConfig.GetTask(m_HoveredTaskIndex);
    if (g_PlayerTasksConfig.ExecuteTask(*humanPlayer, task, g_OverworldMap, m_SelectedRegionId))
    {
        if (task.m_Effect.m_Type == "attack")
        {
            m_TaskStatusMessage = "Attack succeeded — county seized";
        }
        else
        {
            m_TaskStatusMessage = "Performed: " + task.m_Name;
        }
        if (task.m_Effect.m_Type == "startCastleBuild")
        {
            m_TaskStatusMessage += " (construction started)";
        }
        else if (!task.m_Maintenance.IsEmpty())
        {
            m_TaskStatusMessage += " (+" + task.m_Maintenance.ToShortLabel() + "/turn)";
        }
        else if (task.m_Effect.m_Type == "scouting"
            || task.m_Effect.m_Type == "saboteur"
            || task.m_Effect.m_Type == "merchant"
            || task.m_Effect.m_Type == "sendDiplomat"
            || task.m_Effect.m_Type == "sendSpy")
        {
            m_TaskStatusMessage += " (not yet implemented)";
        }
    }
    else
    {
        m_TaskStatusMessage = g_PlayerTasksConfig.GetTaskFailureReason(
            *humanPlayer, task, g_OverworldMap, m_SelectedRegionId);
    }
}

void MainState::DrawTaskPanel(int panelX, int panelY, int panelWidth) const
{
    if (IsAllAiGame())
    {
        const int panelHeight = GetTaskPanelHeight();
        DrawRectangle(panelX, panelY, panelWidth, panelHeight, Color{ 34, 38, 48, 255 });
        DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, Color{ 90, 90, 100, 255 });
        DrawOutlinedText(g_font, "Observe Mode",
            Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 2) },
            g_fontDrawSize, 1, Color{ 160, 210, 255, 255 });
        DrawOutlinedText(g_smallFont, "All AI game. O:observer  A:auto  Next Turn",
            Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 14) },
            g_smallFontDrawSize, 1, Color{ 180, 180, 190, 255 });
        return;
    }

    const int taskCount = g_PlayerTasksConfig.GetTaskCount();
    if (taskCount <= 0)
    {
        return;
    }

    const int panelHeight = GetTaskPanelHeight();
    const PlayerTaskLayout& layout = g_PlayerTasksConfig.GetLayout();
    const Player* humanPlayer = GetHumanPlayer(g_GameDatabase.m_Players);

    DrawRectangle(panelX, panelY, panelWidth, panelHeight, Color{ 34, 38, 48, 255 });
    DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, Color{ 90, 90, 100, 255 });

    DrawOutlinedText(g_font, g_PlayerTasksConfig.GetTitle(),
        Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + 2) },
        g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });

    for (int taskIndex = 0; taskIndex < taskCount; ++taskIndex)
    {
        const PlayerTaskDefinition& task = g_PlayerTasksConfig.GetTask(taskIndex);
        const Rectangle rowRect = GetTaskRowRect(panelX, panelY, taskIndex);
        const bool hovered = taskIndex == m_HoveredTaskIndex;
        const bool affordable = humanPlayer
            && g_PlayerTasksConfig.CanPlayerPerformTask(*humanPlayer, task, g_OverworldMap, m_SelectedRegionId);

        if (hovered)
        {
            DrawRectangle(static_cast<int>(rowRect.x), static_cast<int>(rowRect.y),
                panelWidth, static_cast<int>(rowRect.height), Color{ 58, 72, 98, 255 });
        }

        const Color textColor = affordable ? WHITE : Color{ 140, 140, 150, 255 };
        const Vector2 iconCenter{
            rowRect.x + static_cast<float>(layout.m_IconX) + static_cast<float>(layout.m_IconSize) * 0.5f,
            rowRect.y + rowRect.height * 0.5f
        };
        DrawPlayerTaskIcon(task, iconCenter, textColor);

        DrawOutlinedText(g_smallFont, task.m_Name,
            Vector2{ rowRect.x + static_cast<float>(layout.m_NameX), rowRect.y + 3.0f },
            g_smallFontDrawSize, 1, textColor);

        std::string costLabel = task.m_Cost.ToShortLabel();
        const bool hasUpkeep = humanPlayer
            && GetPlayerActiveTaskCount(*humanPlayer, task.m_Id) > 0
            && !task.m_Maintenance.IsEmpty();
        if (hasUpkeep)
        {
            costLabel += "  Upkeep " + task.m_Maintenance.ToShortLabel();
        }

        DrawOutlinedText(g_smallFont, costLabel,
            Vector2{ rowRect.x + static_cast<float>(layout.m_CostX), rowRect.y + 3.0f },
            g_smallFontDrawSize, 1, Color{ 255, 220, 120, 255 });
    }

    if (!m_TaskStatusMessage.empty())
    {
        DrawOutlinedText(g_smallFont, m_TaskStatusMessage,
            Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + panelHeight - 12) },
            g_smallFontDrawSize, 1, Color{ 180, 220, 180, 255 });
    }
    else
    {
        DrawOutlinedText(g_smallFont, g_PlayerTasksConfig.GetHelpText(),
            Vector2{ static_cast<float>(panelX + 4), static_cast<float>(panelY + panelHeight - 12) },
            g_smallFontDrawSize, 1, Color{ 170, 170, 180, 255 });
    }
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
            + " Ct" + to_string(player.m_Catapults)
            + " St" + to_string(player.m_SiegeTowers);
        DrawOutlinedText(g_smallFont, units,
            Vector2{ static_cast<float>(boxX + 3), static_cast<float>(boxY + 29) },
            g_smallFontDrawSize, 1, Color{ 210, 210, 210, 255 });
    }
}

void MainState::Update()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (m_AiObserverOpen && !IsAllAiGame())
        {
            m_AiObserverOpen = false;
            return;
        }
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
    }

    if (IsKeyPressed(KEY_R))
    {
        unsigned int seed = static_cast<unsigned int>(GetTime() * 1000.0);
        g_GameDatabase.m_Setup.m_Seed = seed;
        g_OverworldMap.Generate(seed, g_GameDatabase.m_Setup);
        m_SelectedRegionId = -1;
        g_GameDatabase.SyncPlayersFromOverworld(g_OverworldMap, true);
        g_GameDatabase.BuildRegionsFromOverworld(g_OverworldMap);
        g_GameDatabase.GenerateAllRegionHeightfields();
    }

    HandleAiObserverInput();

    const int mapRight = kMapDrawX + (OVERWORLD_MAP_SIZE * kMapPixelsPerCell);
    const int panelX = mapRight + 8;
    const int panelWidth = static_cast<int>(g_Engine->m_RenderWidth) - panelX - 4;
    const int taskPanelY = kMapDrawY + kCountyInfoHeight + kPlayerBoxGap;

    const int mapPixelWidth = OVERWORLD_MAP_SIZE * kMapPixelsPerCell;

    HandleNextTurnButton();
    HandleResourceBarInput(kMapDrawX, mapPixelWidth);
    HandleTaskPanelInput(panelX, taskPanelY, panelWidth);
    HandleVisitButton(panelX, kMapDrawY, panelWidth);
    if (!m_AiObserverOpen && !IsMouseOverVisitButton(panelX, kMapDrawY, panelWidth))
    {
        HandleMapSelection();
    }
}

void MainState::Draw()
{
    DrawRectangle(0, 0, static_cast<int>(g_Engine->m_RenderWidth), static_cast<int>(g_Engine->m_RenderHeight),
        Color{ 24, 28, 36, 255 });

    const int mapPixelWidth = OVERWORLD_MAP_SIZE * kMapPixelsPerCell;
    DrawPlayerResourceBar(kMapDrawX, kResourceBarY, mapPixelWidth);
    g_OverworldMap.Draw(kMapDrawX, kMapDrawY, kMapPixelsPerCell, m_SelectedRegionId);
    DrawResourceBarTooltip(kMapDrawX, mapPixelWidth);

    const int mapRight = kMapDrawX + mapPixelWidth;
    const int panelX = mapRight + 8;
    const int panelWidth = static_cast<int>(g_Engine->m_RenderWidth) - panelX - 4;
    const int taskPanelY = kMapDrawY + kCountyInfoHeight + kPlayerBoxGap;
    const int taskPanelHeight = GetTaskPanelHeight();
    const int sidePanelY = taskPanelY + taskPanelHeight + kPlayerBoxGap;
    const int sidePanelHeight = static_cast<int>(g_Engine->m_RenderHeight) - sidePanelY - kNextTurnButtonHeight - (kPanelMargin * 2);

    DrawCountyInfo(panelX, kMapDrawY, panelWidth);
    DrawTaskPanel(panelX, taskPanelY, panelWidth);

    if (kShowAccessibilityGrid && sidePanelHeight > 0)
    {
        g_OverworldMap.DrawAccessibilityGrid(panelX, sidePanelY, panelWidth, sidePanelHeight, m_SelectedRegionId);
    }

    if (kShowPlayerStatusBoxes)
    {
        DrawPlayerSummaries(panelX, sidePanelY, panelWidth);
    }

    DrawNextTurnButton();
    DrawAiObserverPane();

    const char* help = m_AiObserverOpen
        ? "O:close observer  [/]:filter  A:auto(all-AI)  Esc: title"
        : "O: AI observer  R: new map  Esc: title";
    DrawOutlinedText(g_smallFont, help, Vector2{ 4.0f, static_cast<float>(g_Engine->m_RenderHeight - 14.0f) },
        g_smallFontDrawSize, 1, WHITE);
}