#include <algorithm>
#include <cstdio>
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
    constexpr int kMapDrawX = 1;
    constexpr int kTopBarHeight = 14;
    constexpr int kTopBarY = 1;
    constexpr int kMapDrawY = kTopBarY + kTopBarHeight + 1;
    constexpr int kSidePanelGap = 1;
    constexpr int kSidePanelRightMargin = 1;
    constexpr int kPaneGap = 2;

    // Top selection pane (shorter), bottom actions, middle console fills remainder.
    constexpr int kSelectionPaneHeight = 62;
    constexpr int kActionPaneMinHeight = 102;
    constexpr int kActionButtonRows = 3;
    constexpr int kActionButtonCols = 3;
    constexpr int kActionCount = 9;
    constexpr int kNextTurnButtonHeight = 16;
    constexpr int kActionButtonHeight = 18;
    constexpr int kActionPad = 3;

    constexpr int kTopBarSlotCount = 8; // 4 resources + 4 unit types
    constexpr int kResourceTooltipPadding = 4;
    constexpr int kResourceTooltipLineHeight = 12;
    constexpr int kResourceIconCenterOffset = 5;

    constexpr CountyResource kResourceBarSlotOrder[4] = {
        CountyResource::Food,
        CountyResource::Wood,
        CountyResource::Iron,
        CountyResource::Gold
    };

    // Action strip order (3x3 grid, row-major).
    constexpr const char* kActionTaskIds[kActionCount] = {
        "recruitInfantry",
        "recruitArchers",
        "recruitKnights",
        "buildCatapult",
        "improveRegion",
        "buildCastle",
        "sendDiplomat",
        "sendSpy",
        "merchant"
    };

    constexpr const char* kActionLabels[kActionCount] = {
        "Swordsman",
        "Archer",
        "Cavalry",
        "Catapult",
        "Improve",
        "Castle",
        "Diplomat",
        "Spy",
        "Market"
    };

    int MapPixelWidth()
    {
        return OVERWORLD_MAP_WIDTH * kMapPixelsPerCell;
    }

    int MapPixelHeight()
    {
        return OVERWORLD_MAP_HEIGHT * kMapPixelsPerCell;
    }

    int TopBarWidth()
    {
        return static_cast<int>(g_Engine->m_RenderWidth) - 2;
    }

    Vector2 GetScaledMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        const float inputScale = g_Engine->GetInputScale();
        mouse.x /= inputScale;
        mouse.y /= inputScale;
        return mouse;
    }

    void DrawBarStat(
        int slotX,
        int barY,
        int slotWidth,
        CountyResource resourceIcon,
        int amount,
        int delta,
        bool drawResourceIcon)
    {
        const float centerY = static_cast<float>(barY) + static_cast<float>(kTopBarHeight) * 0.5f;
        float textX = static_cast<float>(slotX + 2);

        if (drawResourceIcon)
        {
            const Vector2 iconCenter{
                static_cast<float>(slotX + kResourceIconCenterOffset),
                centerY
            };
            DrawRegionResourceIcon(resourceIcon, iconCenter, WHITE);
            textX = static_cast<float>(slotX + 12);
        }

        const string amountText = to_string(amount);
        const Vector2 amountSize = MeasureTextEx(*g_smallFont, amountText.c_str(), g_smallFontDrawSize, 1.0f);
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
                Vector2{ textX + amountSize.x + 2.0f, textY },
                g_smallFontDrawSize,
                1,
                deltaColor);
        }

        (void)slotWidth;
    }

    void DrawUnitBarEntry(int slotX, int barY, const char* tag, int amount, Color tagColor)
    {
        const float centerY = static_cast<float>(barY) + static_cast<float>(kTopBarHeight) * 0.5f;
        const string tagText = tag;
        const Vector2 tagSize = MeasureTextEx(*g_smallFont, tagText.c_str(), g_smallFontDrawSize, 1.0f);
        const float textY = centerY - tagSize.y * 0.5f;
        DrawOutlinedText(g_smallFont, tagText, Vector2{ static_cast<float>(slotX + 2), textY },
            g_smallFontDrawSize, 1, tagColor);
        DrawOutlinedText(g_smallFont, to_string(amount),
            Vector2{ static_cast<float>(slotX + 2) + tagSize.x + 2.0f, textY },
            g_smallFontDrawSize, 1, WHITE);
    }

    void DrawPanelFrame(int x, int y, int w, int h, const char* title)
    {
        DrawRectangle(x, y, w, h, Color{ 34, 38, 48, 255 });
        DrawRectangleLines(x, y, w, h, Color{ 90, 90, 100, 255 });
        if (title && g_font)
        {
            DrawUiText(g_font, title,
                Vector2{ static_cast<float>(x + 3), static_cast<float>(y + 2) },
                g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });
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
        InitializeCampaignPlayers(
            g_GameDatabase.m_Players,
            playerCount,
            !g_GameDatabase.m_Setup.m_AllAi,
            g_GameDatabase.m_Setup.m_HumanColorIndex);
        AssignCampaignAiPersonalities(g_GameDatabase.m_Players, g_GameDatabase.m_Setup);
    }
    else if (!g_GameDatabase.m_Setup.m_AllAi && GetHumanPlayer(g_GameDatabase.m_Players) == nullptr
        && !g_GameDatabase.m_Players.empty())
    {
        // Recover human seat if a prior bug left it unset.
        g_GameDatabase.m_Players.front().m_IsHuman = true;
    }

    g_GameDatabase.SyncPlayersFromOverworld(g_OverworldMap, false);

    m_AiObserverOpen = false;
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

    if (g_AiObserverLog.GetEntries().empty())
    {
        g_AiObserverLog.BeginTurn(std::max(0, g_GameDatabase.m_Turn));
        g_AiObserverLog.Add(-1, "Campaign ready. Issue orders, then Next Turn.");
    }
}

void MainState::OnExit()
{
}

MainState::SideLayout MainState::ComputeSideLayout() const
{
    SideLayout layout{};
    layout.m_PanelX = kMapDrawX + MapPixelWidth() + kSidePanelGap;
    layout.m_PanelW = static_cast<int>(g_Engine->m_RenderWidth) - layout.m_PanelX - kSidePanelRightMargin;
    if (layout.m_PanelW < 1)
    {
        layout.m_PanelW = 1;
    }

    layout.m_TopY = kMapDrawY;
    layout.m_TopH = kSelectionPaneHeight;

    const int mapBottom = kMapDrawY + MapPixelHeight();
    // All-AI uses this pane as a faction roster (up to 8 lines + Next Turn).
    layout.m_BotH = IsAllAiGame() ? 128 : kActionPaneMinHeight;
    layout.m_BotY = mapBottom - layout.m_BotH;

    layout.m_MidY = layout.m_TopY + layout.m_TopH + kPaneGap;
    layout.m_MidH = layout.m_BotY - kPaneGap - layout.m_MidY;
    if (layout.m_MidH < 24)
    {
        // Prefer keeping selection + actions; shrink middle if squeezed.
        layout.m_MidH = 24;
        layout.m_BotY = layout.m_MidY + layout.m_MidH + kPaneGap;
        layout.m_BotH = mapBottom - layout.m_BotY;
    }

    return layout;
}

const char* MainState::GetActionTaskId(int actionIndex) const
{
    if (actionIndex < 0 || actionIndex >= kActionCount)
    {
        return nullptr;
    }
    return kActionTaskIds[actionIndex];
}

const char* MainState::GetActionLabel(int actionIndex) const
{
    if (actionIndex < 0 || actionIndex >= kActionCount)
    {
        return "";
    }
    return kActionLabels[actionIndex];
}

Rectangle MainState::GetTopBarSlotRect(int slotIndex) const
{
    const int barX = 1;
    const int barW = TopBarWidth();
    const int slotW = barW / kTopBarSlotCount;
    return Rectangle{
        static_cast<float>(barX + slotIndex * slotW),
        static_cast<float>(kTopBarY),
        static_cast<float>(slotW),
        static_cast<float>(kTopBarHeight)
    };
}

void MainState::HandleTopBarInput()
{
    m_HoveredTopBarSlot = -1;
    const Vector2 mouse = GetScaledMousePosition();
    for (int i = 0; i < kTopBarSlotCount; ++i)
    {
        if (CheckCollisionPointRec(mouse, GetTopBarSlotRect(i)))
        {
            m_HoveredTopBarSlot = i;
            break;
        }
    }
}

void MainState::DrawTopBar() const
{
    const Player* watched = GetWatchedPlayer();
    if (!watched)
    {
        return;
    }

    const int barX = 1;
    const int barW = TopBarWidth();
    DrawRectangle(barX, kTopBarY, barW, kTopBarHeight, Color{ 30, 34, 42, 255 });
    DrawRectangleLines(barX, kTopBarY, barW, kTopBarHeight, Color{ 90, 90, 100, 255 });

    ResourceAmount turnDelta{};
    ComputePlayerTurnDelta(g_OverworldMap, *watched, turnDelta);

    const int slotW = barW / kTopBarSlotCount;
    DrawBarStat(barX + slotW * 0, kTopBarY, slotW, CountyResource::Food, watched->m_Food, turnDelta.m_Food, true);
    DrawBarStat(barX + slotW * 1, kTopBarY, slotW, CountyResource::Wood, watched->m_Wood, turnDelta.m_Wood, true);
    DrawBarStat(barX + slotW * 2, kTopBarY, slotW, CountyResource::Iron, watched->m_Iron, turnDelta.m_Iron, true);
    DrawBarStat(barX + slotW * 3, kTopBarY, slotW, CountyResource::Gold, watched->m_Gold, turnDelta.m_Gold, true);

    DrawUnitBarEntry(barX + slotW * 4, kTopBarY, "Sw", watched->m_Swordsmen, Color{ 200, 200, 210, 255 });
    DrawUnitBarEntry(barX + slotW * 5, kTopBarY, "Ar", watched->m_Archers, Color{ 160, 210, 160, 255 });
    DrawUnitBarEntry(barX + slotW * 6, kTopBarY, "Kn", watched->m_Knights, Color{ 220, 200, 120, 255 });
    DrawUnitBarEntry(barX + slotW * 7, kTopBarY, "Ct", watched->m_Catapults, Color{ 180, 160, 140, 255 });

    // Turn number on the far right edge of the last slot.
    const string turnText = "T" + to_string(g_GameDatabase.m_Turn);
    const Vector2 turnSize = MeasureTextEx(*g_smallFont, turnText.c_str(), g_smallFontDrawSize, 1.0f);
    DrawOutlinedText(g_smallFont, turnText,
        Vector2{ static_cast<float>(barX + barW) - turnSize.x - 3.0f, static_cast<float>(kTopBarY + 2) },
        g_smallFontDrawSize, 1, Color{ 180, 180, 200, 255 });
}

void MainState::DrawTopBarTooltip() const
{
    if (m_HoveredTopBarSlot < 0 || m_HoveredTopBarSlot >= 4)
    {
        return;
    }

    const Player* watched = GetWatchedPlayer();
    if (!watched)
    {
        return;
    }

    const CountyResource resource = kResourceBarSlotOrder[m_HoveredTopBarSlot];
    std::vector<ResourceTurnLine> lines;
    ComputePlayerResourceBreakdown(g_OverworldMap, *watched, resource, lines);
    if (lines.empty())
    {
        return;
    }

    float maxTextWidth = 0.0f;
    std::vector<string> rowTexts;
    std::vector<Color> rowColors;
    for (const ResourceTurnLine& line : lines)
    {
        string text;
        Color color;
        if (line.m_Amount > 0)
        {
            text = "+" + to_string(line.m_Amount) + " " + line.m_Label;
            color = Color{ 120, 220, 120, 255 };
        }
        else
        {
            text = to_string(line.m_Amount) + " " + line.m_Label;
            color = Color{ 220, 120, 120, 255 };
        }
        rowTexts.push_back(text);
        rowColors.push_back(color);
        const Vector2 textSize = MeasureTextEx(*g_smallFont, text.c_str(), g_smallFontDrawSize, 1.0f);
        maxTextWidth = std::max(maxTextWidth, textSize.x);
    }

    const Rectangle slot = GetTopBarSlotRect(m_HoveredTopBarSlot);
    const int tooltipX = static_cast<int>(slot.x);
    const int tooltipY = kMapDrawY;
    const int tooltipWidth = static_cast<int>(maxTextWidth) + (kResourceTooltipPadding * 2);
    const int tooltipHeight = (kResourceTooltipPadding * 2)
        + (static_cast<int>(rowTexts.size()) * kResourceTooltipLineHeight);

    DrawRectangle(tooltipX, tooltipY, tooltipWidth, tooltipHeight, Color{ 24, 28, 36, 245 });
    DrawRectangleLines(tooltipX, tooltipY, tooltipWidth, tooltipHeight, Color{ 90, 90, 100, 255 });

    int lineY = tooltipY + kResourceTooltipPadding;
    for (size_t i = 0; i < rowTexts.size(); ++i)
    {
        DrawOutlinedText(
            g_smallFont,
            rowTexts[i],
            Vector2{ static_cast<float>(tooltipX + kResourceTooltipPadding), static_cast<float>(lineY) },
            g_smallFontDrawSize,
            1,
            rowColors[i]);
        lineY += kResourceTooltipLineHeight;
    }
}

void MainState::HandleMapSelection()
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    Vector2 mouse = GetScaledMousePosition();

    const int mapPixelWidth = MapPixelWidth();
    const int mapPixelHeight = MapPixelHeight();

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

    // All-AI: pin the top-bar "watched" faction to this county's owner.
    if (IsAllAiGame() && region->m_OwnerId >= 0
        && region->m_OwnerId < static_cast<int>(g_GameDatabase.m_Players.size()))
    {
        m_WatchedPlayerId = region->m_OwnerId;
        m_AiObserverFilter = region->m_OwnerId;
    }
}

bool MainState::IsAllAiGame() const
{
    // Trust the setup flag only. (Previously ORing GetHumanPlayer==null forced
    // observer UI whenever the human seat flag was missing.)
    return g_GameDatabase.m_Setup.m_AllAi;
}

const Player* MainState::GetWatchedPlayer() const
{
    if (const Player* human = GetHumanPlayer(g_GameDatabase.m_Players))
    {
        return human;
    }

    // All-AI observe: top bar follows the owner of the selected county so you can
    // inspect their stockpiles and hover gain/drain breakdowns.
    if (IsAllAiGame() && m_SelectedRegionId >= 0)
    {
        if (const OverworldRegionData* region = g_OverworldMap.GetRegion(m_SelectedRegionId))
        {
            if (region->m_OwnerId >= 0
                && region->m_OwnerId < static_cast<int>(g_GameDatabase.m_Players.size()))
            {
                return &g_GameDatabase.m_Players[static_cast<size_t>(region->m_OwnerId)];
            }
        }
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

    int index = m_AiObserverFilter;
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

    const int mapPixelWidth = MapPixelWidth();
    const int paneX = kMapDrawX;
    const int paneY = kMapDrawY + 2;
    const int paneW = mapPixelWidth;
    const int paneH = MapPixelHeight() - 4;

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

    int relevantPlayerId = -1;
    if (!IsAllAiGame())
    {
        if (const Player* human = GetHumanPlayer(g_GameDatabase.m_Players))
        {
            relevantPlayerId = human->m_Id;
        }
    }

    std::vector<const AiLogEntry*> lines;
    g_AiObserverLog.CollectFiltered(m_AiObserverFilter, 22, lines, relevantPlayerId);

    float lineY = static_cast<float>(paneY + 28);
    if (lines.empty())
    {
        DrawOutlinedText(g_smallFont, "(no log entries yet)",
            Vector2{ static_cast<float>(paneX + 4), lineY },
            g_smallFontDrawSize, 1, GRAY);
    }
    else
    {
        for (const AiLogEntry* entry : lines)
        {
            if (lineY + g_smallFontDrawSize > static_cast<float>(paneY + paneH - 14))
            {
                break;
            }

            Color color = Color{ 200, 200, 200, 255 };
            if (entry->m_PlayerId >= 0
                && entry->m_PlayerId < static_cast<int>(g_GameDatabase.m_Players.size()))
            {
                color = g_GameDatabase.m_Players[static_cast<size_t>(entry->m_PlayerId)].GetColor();
            }

            DrawOutlinedText(g_smallFont, entry->m_Message,
                Vector2{ static_cast<float>(paneX + 4), lineY },
                g_smallFontDrawSize, 1, color);
            lineY += g_smallFontDrawSize + 1.0f;
        }
    }

    DrawOutlinedText(g_smallFont, "O:close  [/]:filter  A:auto (all-AI)",
        Vector2{ static_cast<float>(paneX + 4), static_cast<float>(paneY + paneH - 12) },
        g_smallFontDrawSize, 1, Color{ 150, 150, 160, 255 });
}

void MainState::DrawSelectionPanel(const SideLayout& layout) const
{
    DrawPanelFrame(layout.m_PanelX, layout.m_TopY, layout.m_PanelW, layout.m_TopH, "Selection");

    const int textX = layout.m_PanelX + 3;
    int textY = layout.m_TopY + 14;

    if (m_SelectedRegionId < 0)
    {
        if (m_SelectedImpassable && m_SelectedImpassableCellType == static_cast<unsigned char>(OW_MOUNTAIN))
        {
            DrawUiText(g_smallFont, "Mountains (impassable)",
                Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
                g_smallFontDrawSize, 1, Color{ 180, 180, 180, 255 });
            return;
        }
        if (m_SelectedImpassable && m_SelectedImpassableCellType == static_cast<unsigned char>(OW_WATER))
        {
            DrawUiText(g_smallFont, "Water (impassable)",
                Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
                g_smallFontDrawSize, 1, Color{ 140, 200, 255, 255 });
            return;
        }

        DrawUiText(g_smallFont, "Click a county on the map",
            Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
            g_smallFontDrawSize, 1, Color{ 180, 180, 190, 255 });
        return;
    }

    const OverworldRegionData* region = g_OverworldMap.GetRegion(m_SelectedRegionId);
    if (!region)
    {
        return;
    }

    // Fog of war: hide intel on unexplored counties.
    int visionPlayerId = -1;
    if (!IsAllAiGame())
    {
        if (const Player* human = GetHumanPlayer(g_GameDatabase.m_Players))
        {
            visionPlayerId = human->m_Id;
        }
    }
    if (visionPlayerId >= 0 && !g_OverworldMap.IsRegionVisibleToPlayer(visionPlayerId, region->m_Id))
    {
        DrawUiText(g_smallFont, "Unknown territory",
            Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
            g_smallFontDrawSize, 1, Color{ 160, 160, 170, 255 });
        textY += 10;
        DrawUiText(g_smallFont, "(unexplored)",
            Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
            g_smallFontDrawSize, 1, Color{ 120, 120, 130, 255 });
        return;
    }

    const string title = "County " + to_string(region->m_Id);
    DrawUiText(g_smallFont, title,
        Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
        g_smallFontDrawSize, 1, WHITE);
    textY += 10;

    string ownerText = string("Owner: ") + PlayerOwnerName(region->m_OwnerId);
    DrawUiText(g_smallFont, ownerText,
        Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
        g_smallFontDrawSize, 1, WHITE);
    textY += 10;

    const string resText = string(CountyResourceName(region->m_Resource))
        + "  +" + to_string(GetRegionTurnIncome(g_OverworldMap, *region)) + "/t";
    DrawUiText(g_smallFont, resText,
        Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
        g_smallFontDrawSize, 1, Color{ 180, 220, 180, 255 });
    textY += 10;

    string status;
    if (region->m_HasCastle)
    {
        status = "Castle";
    }
    else if (region->m_CastleBuildTurnsRemaining > 0)
    {
        status = "Building (" + to_string(region->m_CastleBuildTurnsRemaining) + "t)";
    }
    else if (g_OverworldMap.IsRegionFortified(region->m_Id))
    {
        status = "Fortified";
    }
    else
    {
        status = "Open";
    }
    DrawUiText(g_smallFont, status,
        Vector2{ static_cast<float>(textX), static_cast<float>(textY) },
        g_smallFontDrawSize, 1,
        g_OverworldMap.IsRegionFortified(region->m_Id)
            ? Color{ 180, 230, 180, 255 }
            : Color{ 200, 200, 210, 255 });

    if (CanVisitSelectedRegion())
    {
        DrawVisitRegionButton(layout);
    }
    if (CanAttackSelectedRegion())
    {
        DrawAttackButton(layout);
    }
}

bool MainState::CanVisitSelectedRegion() const
{
    if (m_SelectedRegionId < 0 || g_GameDatabase.m_Outcome != CampaignOutcome::None)
    {
        return false;
    }
    const OverworldRegionData* region = g_OverworldMap.GetRegion(m_SelectedRegionId);
    return region && !region->m_IsWater;
}

bool MainState::CanAttackSelectedRegion() const
{
    if (IsAllAiGame() || g_GameDatabase.m_Outcome != CampaignOutcome::None)
    {
        return false;
    }
    const Player* human = GetHumanPlayer(g_GameDatabase.m_Players);
    if (!human || human->m_AttacksThisTurn >= 1)
    {
        return false;
    }
    return CanPlayerAttackRegion(*human, g_OverworldMap, m_SelectedRegionId);
}

Rectangle MainState::GetVisitRegionButtonRect(const SideLayout& layout) const
{
    return Rectangle{
        static_cast<float>(layout.m_PanelX + layout.m_PanelW - 40),
        static_cast<float>(layout.m_TopY + layout.m_TopH - 14),
        36.0f,
        12.0f
    };
}

Rectangle MainState::GetAttackButtonRect(const SideLayout& layout) const
{
    return Rectangle{
        static_cast<float>(layout.m_PanelX + layout.m_PanelW - 80),
        static_cast<float>(layout.m_TopY + layout.m_TopH - 14),
        36.0f,
        12.0f
    };
}

void MainState::DrawVisitRegionButton(const SideLayout& layout) const
{
    const Rectangle rect = GetVisitRegionButtonRect(layout);
    const bool hovered = CheckCollisionPointRec(GetScaledMousePosition(), rect);
    DrawRectangleRec(rect, hovered ? Color{ 70, 110, 70, 255 } : Color{ 50, 80, 50, 255 });
    DrawRectangleLinesEx(rect, 1.0f, hovered ? Color{ 160, 220, 160, 255 } : Color{ 110, 160, 110, 255 });
    DrawUiText(g_smallFont, "Visit",
        Vector2{ rect.x + 4.0f, rect.y + 1.0f },
        g_smallFontDrawSize, 1, WHITE);
}

void MainState::DrawAttackButton(const SideLayout& layout) const
{
    const Rectangle rect = GetAttackButtonRect(layout);
    const bool hovered = CheckCollisionPointRec(GetScaledMousePosition(), rect);
    DrawRectangleRec(rect, hovered ? Color{ 140, 60, 50, 255 } : Color{ 100, 40, 36, 255 });
    DrawRectangleLinesEx(rect, 1.0f, hovered ? Color{ 255, 160, 140, 255 } : Color{ 200, 110, 100, 255 });
    DrawUiText(g_smallFont, "Attack",
        Vector2{ rect.x + 2.0f, rect.y + 1.0f },
        g_smallFontDrawSize, 1, WHITE);
}

void MainState::HandleVisitRegionButton(const SideLayout& layout)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !CanVisitSelectedRegion())
    {
        return;
    }
    if (!CheckCollisionPointRec(GetScaledMousePosition(), GetVisitRegionButtonRect(layout)))
    {
        return;
    }

    g_GameDatabase.SetActiveRegion(m_SelectedRegionId);
    g_GameDatabase.EnsureRegionHeightfield(m_SelectedRegionId);
    g_StateMachine->MakeStateTransition(STATE_COMBATSTATE);
}

void MainState::HandleAttackButton(const SideLayout& layout)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !CanAttackSelectedRegion())
    {
        return;
    }
    if (!CheckCollisionPointRec(GetScaledMousePosition(), GetAttackButtonRect(layout)))
    {
        return;
    }

    Player* human = GetHumanPlayer(g_GameDatabase.m_Players);
    if (!human)
    {
        return;
    }

    // On Map: launch real-time battle. Automatic: instant auto-resolve.
    if (g_GameDatabase.m_Setup.m_BattleMode == BattleMode::OnMap)
    {
        if (!g_GameDatabase.BeginPendingBattle(human->m_Id, m_SelectedRegionId, g_OverworldMap))
        {
            m_TaskStatusMessage = "Cannot start battle";
            return;
        }
        m_TaskStatusMessage = "Battle for county " + to_string(m_SelectedRegionId);
        g_AiObserverLog.Add(human->m_Id, m_TaskStatusMessage);
        g_StateMachine->MakeStateTransition(STATE_COMBATSTATE);
        return;
    }

    // Detailed multi-line report is written by ResolveRegionAttack into the turn log.
    std::string message;
    if (ResolveRegionAttack(*human, g_OverworldMap, g_GameDatabase.m_Players, m_SelectedRegionId, &message))
    {
        m_TaskStatusMessage = message;
        g_GameDatabase.EvaluateCampaignOutcome(g_OverworldMap);
    }
    else
    {
        m_TaskStatusMessage = message.empty() ? "Attack failed" : message;
    }
}

void MainState::DrawCampaignOutcomeOverlay() const
{
    if (g_GameDatabase.m_Outcome == CampaignOutcome::None)
    {
        return;
    }

    const int w = static_cast<int>(g_Engine->m_RenderWidth);
    const int h = static_cast<int>(g_Engine->m_RenderHeight);
    DrawRectangle(0, 0, w, h, Color{ 0, 0, 0, 160 });

    const bool victory = g_GameDatabase.m_Outcome == CampaignOutcome::Victory;
    const char* title = victory ? "VICTORY" : "DEFEAT";
    const char* subtitle = victory
        ? "Your rivals have been driven from the land."
        : "You hold no counties. The campaign is lost.";
    const Color titleColor = victory ? Color{ 255, 220, 90, 255 } : Color{ 255, 120, 100, 255 };

    const Vector2 titleSize = MeasureTextEx(*g_font, title, g_fontDrawSize * 2.0f, 1.0f);
    DrawOutlinedText(g_font, title,
        Vector2{ (w - titleSize.x) * 0.5f, h * 0.35f },
        g_fontDrawSize * 2.0f, 1, titleColor);

    const Vector2 subSize = MeasureTextEx(*g_smallFont, subtitle, g_smallFontDrawSize, 1.0f);
    DrawOutlinedText(g_smallFont, subtitle,
        Vector2{ (w - subSize.x) * 0.5f, h * 0.35f + titleSize.y + 8.0f },
        g_smallFontDrawSize, 1, WHITE);

    const char* hint = "Enter / Esc: return to title";
    const Vector2 hintSize = MeasureTextEx(*g_smallFont, hint, g_smallFontDrawSize, 1.0f);
    DrawOutlinedText(g_smallFont, hint,
        Vector2{ (w - hintSize.x) * 0.5f, h * 0.35f + titleSize.y + 24.0f },
        g_smallFontDrawSize, 1, Color{ 180, 185, 195, 255 });
}

void MainState::HandleCampaignOutcomeInput()
{
    if (g_GameDatabase.m_Outcome == CampaignOutcome::None)
    {
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE))
    {
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
    }
}

float MainState::GetTurnLogLineStep() const
{
    return g_smallFontDrawSize + 1.0f;
}

Rectangle MainState::GetTurnLogContentRect(const SideLayout& layout) const
{
    constexpr int kScrollBarWidth = 5;
    return Rectangle{
        static_cast<float>(layout.m_PanelX + 2),
        static_cast<float>(layout.m_MidY + 13),
        static_cast<float>(layout.m_PanelW - 4 - kScrollBarWidth - 1),
        static_cast<float>(layout.m_MidH - 15)
    };
}

Rectangle MainState::GetTurnLogScrollBarRect(const SideLayout& layout) const
{
    constexpr int kScrollBarWidth = 5;
    return Rectangle{
        static_cast<float>(layout.m_PanelX + layout.m_PanelW - 2 - kScrollBarWidth),
        static_cast<float>(layout.m_MidY + 13),
        static_cast<float>(kScrollBarWidth),
        static_cast<float>(layout.m_MidH - 15)
    };
}

void MainState::BuildTurnLogDisplayLines(
    const SideLayout& layout,
    std::vector<std::pair<std::string, Color>>& outLines) const
{
    outLines.clear();

    const Rectangle content = GetTurnLogContentRect(layout);
    const float maxTextWidth = std::max(8.0f, content.width - 2.0f);

    // Single-player: only system lines + anything involving the human (their
    // orders, battles they fight or are attacked in). All-AI observe: full log.
    int relevantPlayerId = -1;
    if (!IsAllAiGame())
    {
        if (const Player* human = GetHumanPlayer(g_GameDatabase.m_Players))
        {
            relevantPlayerId = human->m_Id;
        }
    }

    std::vector<const AiLogEntry*> entries;
    g_AiObserverLog.CollectFiltered(-1, AiObserverLog::kMaxEntries, entries, relevantPlayerId);
    // CollectFiltered is newest-first; reverse so oldest is at the top of the scroll view.
    std::reverse(entries.begin(), entries.end());

    auto WrapLogLine = [&](const string& text, std::vector<string>& outWrapped)
    {
        outWrapped.clear();
        if (text.empty() || !g_smallFont)
        {
            outWrapped.push_back(text);
            return;
        }

        size_t start = 0;
        while (start < text.size())
        {
            while (start < text.size() && text[start] == ' ')
            {
                ++start;
            }
            if (start >= text.size())
            {
                break;
            }

            size_t fitEnd = start;
            size_t lastSpace = start;
            for (size_t i = start; i < text.size(); ++i)
            {
                const string candidate = text.substr(start, i - start + 1);
                const Vector2 size = MeasureTextEx(
                    *g_smallFont, candidate.c_str(), g_smallFontDrawSize, 1.0f);
                if (size.x <= maxTextWidth)
                {
                    fitEnd = i + 1;
                    if (text[i] == ' ')
                    {
                        lastSpace = i;
                    }
                }
                else
                {
                    break;
                }
            }

            if (fitEnd == start)
            {
                fitEnd = start + 1;
            }

            size_t lineEnd = fitEnd;
            if (lastSpace > start && fitEnd < text.size())
            {
                lineEnd = lastSpace;
            }

            string piece = text.substr(start, lineEnd - start);
            while (!piece.empty() && piece.back() == ' ')
            {
                piece.pop_back();
            }
            if (!piece.empty())
            {
                outWrapped.push_back(piece);
            }
            start = lineEnd;
        }

        if (outWrapped.empty())
        {
            outWrapped.push_back(text);
        }
    };

    std::vector<string> wrapped;
    for (const AiLogEntry* entry : entries)
    {
        Color color = Color{ 190, 190, 200, 255 };
        if (entry->m_PlayerId >= 0
            && entry->m_PlayerId < static_cast<int>(g_GameDatabase.m_Players.size()))
        {
            color = g_GameDatabase.m_Players[static_cast<size_t>(entry->m_PlayerId)].GetColor();
        }

        WrapLogLine(entry->m_Message, wrapped);
        for (const string& piece : wrapped)
        {
            outLines.emplace_back(piece, color);
        }
    }
}

void MainState::HandleTurnLogInput(const SideLayout& layout)
{
    const Rectangle content = GetTurnLogContentRect(layout);
    const Rectangle bar = GetTurnLogScrollBarRect(layout);
    const Vector2 mouse = GetScaledMousePosition();
    const bool overConsole = CheckCollisionPointRec(mouse, content)
        || CheckCollisionPointRec(mouse, bar);

    std::vector<std::pair<string, Color>> displayLines;
    BuildTurnLogDisplayLines(layout, displayLines);

    const int entryCount = static_cast<int>(g_AiObserverLog.GetEntries().size());
    const float lineStep = GetTurnLogLineStep();
    const float contentHeight = static_cast<float>(displayLines.size()) * lineStep;
    const float viewHeight = content.height;
    const float maxScroll = std::max(0.0f, contentHeight - viewHeight);

    // New log lines: keep following the bottom unless the user has scrolled up.
    if (entryCount != m_TurnLogLastEntryCount)
    {
        if (m_TurnLogStickToBottom)
        {
            m_TurnLogScroll = maxScroll;
        }
        m_TurnLogLastEntryCount = entryCount;
    }

    if (overConsole)
    {
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            m_TurnLogScroll -= wheel * lineStep * 3.0f;
            m_TurnLogStickToBottom = false;
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, bar) && maxScroll > 0.0f)
    {
        m_TurnLogDragging = true;
        m_TurnLogStickToBottom = false;

        // Jump scroll so the thumb centers on the click.
        const float thumbH = std::max(10.0f, (viewHeight / contentHeight) * bar.height);
        const float track = std::max(1.0f, bar.height - thumbH);
        const float localY = std::clamp(mouse.y - bar.y - thumbH * 0.5f, 0.0f, track);
        m_TurnLogScroll = (localY / track) * maxScroll;
        m_TurnLogDragGrabY = mouse.y;
    }

    if (m_TurnLogDragging)
    {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && maxScroll > 0.0f)
        {
            const float thumbH = std::max(10.0f, (viewHeight / contentHeight) * bar.height);
            const float track = std::max(1.0f, bar.height - thumbH);
            const float localY = std::clamp(mouse.y - bar.y - thumbH * 0.5f, 0.0f, track);
            m_TurnLogScroll = (localY / track) * maxScroll;
            m_TurnLogStickToBottom = false;
            (void)m_TurnLogDragGrabY;
        }
        else
        {
            m_TurnLogDragging = false;
        }
    }

    m_TurnLogScroll = std::clamp(m_TurnLogScroll, 0.0f, maxScroll);
    if (maxScroll <= 0.0f || m_TurnLogScroll >= maxScroll - 0.5f)
    {
        m_TurnLogStickToBottom = true;
        m_TurnLogScroll = maxScroll;
    }
}

void MainState::DrawTurnConsole(const SideLayout& layout) const
{
    DrawPanelFrame(layout.m_PanelX, layout.m_MidY, layout.m_PanelW, layout.m_MidH, "Turn Log");

    const Rectangle content = GetTurnLogContentRect(layout);
    const Rectangle bar = GetTurnLogScrollBarRect(layout);
    const float textX = content.x + 1.0f;

    std::vector<std::pair<string, Color>> displayLines;
    BuildTurnLogDisplayLines(layout, displayLines);

    if (displayLines.empty())
    {
        DrawUiText(g_smallFont, "No events yet.",
            Vector2{ textX, content.y },
            g_smallFontDrawSize, 1, GRAY);
        return;
    }

    const float lineStep = GetTurnLogLineStep();
    const float contentHeight = static_cast<float>(displayLines.size()) * lineStep;
    const float viewHeight = content.height;
    const float maxScroll = std::max(0.0f, contentHeight - viewHeight);
    const float scroll = std::clamp(m_TurnLogScroll, 0.0f, maxScroll);

    // Clip log text to the console body.
    BeginScissorMode(
        static_cast<int>(content.x),
        static_cast<int>(content.y),
        static_cast<int>(content.width),
        static_cast<int>(content.height));

    float lineY = content.y - scroll;
    for (const auto& line : displayLines)
    {
        if (lineY + lineStep < content.y)
        {
            lineY += lineStep;
            continue;
        }
        if (lineY > content.y + content.height)
        {
            break;
        }

        DrawUiText(g_smallFont, line.first,
            Vector2{ textX, lineY },
            g_smallFontDrawSize, 1, line.second);
        lineY += lineStep;
    }

    EndScissorMode();

    // Scrollbar track + thumb (only when content overflows).
    DrawRectangleRec(bar, Color{ 24, 26, 32, 255 });
    DrawRectangleLinesEx(bar, 1.0f, Color{ 70, 74, 86, 255 });

    if (maxScroll > 0.0f)
    {
        const float thumbH = std::max(10.0f, (viewHeight / contentHeight) * bar.height);
        const float track = std::max(1.0f, bar.height - thumbH);
        const float thumbY = bar.y + (scroll / maxScroll) * track;
        const Rectangle thumb{
            bar.x + 1.0f,
            thumbY,
            bar.width - 2.0f,
            thumbH
        };
        DrawRectangleRec(thumb, Color{ 120, 130, 150, 255 });
    }
}

Rectangle MainState::GetActionButtonRect(const SideLayout& layout, int actionIndex) const
{
    const int gridX = layout.m_PanelX + kActionPad;
    const int gridY = layout.m_BotY + 14;
    const int gridW = layout.m_PanelW - (kActionPad * 2);
    const int col = actionIndex % kActionButtonCols;
    const int row = actionIndex / kActionButtonCols;
    const int btnW = (gridW - (kActionButtonCols - 1) * 2) / kActionButtonCols;
    const int btnH = kActionButtonHeight;
    return Rectangle{
        static_cast<float>(gridX + col * (btnW + 2)),
        static_cast<float>(gridY + row * (btnH + 2)),
        static_cast<float>(btnW),
        static_cast<float>(btnH)
    };
}

Rectangle MainState::GetNextTurnButtonRect(const SideLayout& layout) const
{
    const int x = layout.m_PanelX + kActionPad;
    const int w = layout.m_PanelW - (kActionPad * 2);
    const int y = layout.m_BotY + layout.m_BotH - kNextTurnButtonHeight - kActionPad;
    return Rectangle{
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(w),
        static_cast<float>(kNextTurnButtonHeight)
    };
}

bool MainState::IsMouseOverNextTurnButton(const SideLayout& layout) const
{
    return CheckCollisionPointRec(GetScaledMousePosition(), GetNextTurnButtonRect(layout));
}

void MainState::DrawActionPanel(const SideLayout& layout) const
{
    const Rectangle nextRect = GetNextTurnButtonRect(layout);

    if (IsAllAiGame())
    {
        DrawPanelFrame(layout.m_PanelX, layout.m_BotY, layout.m_PanelW, layout.m_BotH, "Factions");

        // Compact roster: one line per AI — resources + army + counties.
        const float lineStep = g_smallFontDrawSize + 1.0f;
        const float textX = static_cast<float>(layout.m_PanelX + 3);
        float lineY = static_cast<float>(layout.m_BotY + 13);
        const float maxY = nextRect.y - 2.0f;

        for (const Player& player : g_GameDatabase.m_Players)
        {
            if (lineY + lineStep > maxY)
            {
                break;
            }

            // Name padded short; color tint for living factions, gray for eliminated.
            const bool alive = player.m_TotalRegions > 0;
            Color textColor = alive ? player.GetColor() : Color{ 110, 110, 120, 255 };

            // Example: "Blue F20 W15 I10 G25  S2 A1 K0 C0  #3"
            char line[96];
            std::snprintf(
                line,
                sizeof(line),
                "%-5s F%d W%d I%d G%d  S%d A%d K%d C%d  #%d",
                player.GetColorName(),
                player.m_Food,
                player.m_Wood,
                player.m_Iron,
                player.m_Gold,
                player.m_Swordsmen,
                player.m_Archers,
                player.m_Knights,
                player.m_Catapults,
                player.m_TotalRegions);

            // Clip long lines to panel width (approximate: ~6px per char at small font).
            string display = line;
            const int maxChars = std::max(12, (layout.m_PanelW - 6) / 4);
            if (static_cast<int>(display.size()) > maxChars)
            {
                display = display.substr(0, static_cast<size_t>(maxChars - 1)) + "...";
            }

            DrawUiText(g_smallFont, display,
                Vector2{ textX, lineY },
                g_smallFontDrawSize, 1, textColor);
            lineY += lineStep;
        }

        if (m_AiAutoPlay)
        {
            DrawUiText(g_smallFont, "Auto (A)",
                Vector2{ textX, nextRect.y - 11.0f },
                g_smallFontDrawSize, 1, Color{ 180, 220, 180, 255 });
        }
    }
    else
    {
        DrawPanelFrame(layout.m_PanelX, layout.m_BotY, layout.m_PanelW, layout.m_BotH, "Actions");

        const Player* humanPlayer = GetHumanPlayer(g_GameDatabase.m_Players);
        const bool actionsEnabled = humanPlayer != nullptr;

        for (int i = 0; i < kActionCount; ++i)
        {
            const Rectangle rect = GetActionButtonRect(layout, i);
            const bool hovered = (i == m_HoveredActionIndex);
            const char* taskId = GetActionTaskId(i);
            const PlayerTaskDefinition* task = taskId ? g_PlayerTasksConfig.FindTaskById(taskId) : nullptr;

            bool affordable = false;
            if (actionsEnabled && humanPlayer && task)
            {
                affordable = g_PlayerTasksConfig.CanPlayerPerformTask(
                    *humanPlayer, *task, g_OverworldMap, m_SelectedRegionId);
            }

            Color fill = Color{ 44, 48, 58, 255 };
            Color border = Color{ 90, 90, 100, 255 };
            Color textColor = Color{ 140, 140, 150, 255 };
            if (actionsEnabled)
            {
                textColor = affordable ? WHITE : Color{ 140, 140, 150, 255 };
                fill = hovered
                    ? (affordable ? Color{ 58, 78, 108, 255 } : Color{ 52, 52, 60, 255 })
                    : (affordable ? Color{ 48, 56, 70, 255 } : Color{ 40, 42, 50, 255 });
                border = hovered ? Color{ 140, 160, 200, 255 } : Color{ 100, 105, 120, 255 };
            }

            DrawRectangleRec(rect, fill);
            DrawRectangleLinesEx(rect, 1.0f, border);

            if (task)
            {
                const Vector2 iconCenter{
                    rect.x + 8.0f,
                    rect.y + rect.height * 0.5f
                };
                DrawPlayerTaskIcon(*task, iconCenter, textColor);
            }

            const char* label = GetActionLabel(i);
            DrawUiText(g_smallFont, label,
                Vector2{ rect.x + 14.0f, rect.y + 4.0f },
                g_smallFontDrawSize, 1, textColor);
        }

        // Status / help line between grid and Next Turn.
        const float statusY = nextRect.y - 11.0f;
        if (!m_TaskStatusMessage.empty())
        {
            string status = m_TaskStatusMessage;
            if (status.size() > 36)
            {
                status = status.substr(0, 33) + "...";
            }
            DrawUiText(g_smallFont, status,
                Vector2{ static_cast<float>(layout.m_PanelX + 3), statusY },
                g_smallFontDrawSize, 1, Color{ 180, 220, 180, 255 });
        }
    }

    const bool nextHovered = IsMouseOverNextTurnButton(layout);
    DrawRectangleRec(nextRect, nextHovered ? Color{ 70, 120, 70, 255 } : Color{ 48, 88, 48, 255 });
    DrawRectangleLinesEx(nextRect, 1.0f, nextHovered ? Color{ 140, 220, 140, 255 } : Color{ 100, 160, 100, 255 });
    const string nextLabel = "Next Turn";
    const Vector2 nextSize = MeasureTextEx(*g_font, nextLabel.c_str(), g_fontDrawSize, 1.0f);
    DrawUiText(g_font, nextLabel,
        Vector2{
            nextRect.x + (nextRect.width - nextSize.x) * 0.5f,
            nextRect.y + (nextRect.height - nextSize.y) * 0.5f - 1.0f
        },
        g_fontDrawSize, 1, WHITE);
}

void MainState::TryPerformAction(int actionIndex)
{
    if (IsAllAiGame())
    {
        m_TaskStatusMessage = "Observe mode - no player orders";
        return;
    }

    Player* humanPlayer = GetHumanPlayer(g_GameDatabase.m_Players);
    if (!humanPlayer)
    {
        return;
    }

    const char* taskId = GetActionTaskId(actionIndex);
    if (!taskId)
    {
        return;
    }

    const PlayerTaskDefinition* task = g_PlayerTasksConfig.FindTaskById(taskId);
    if (!task)
    {
        m_TaskStatusMessage = string("Missing task: ") + taskId;
        return;
    }

    if (g_PlayerTasksConfig.ExecuteTask(*humanPlayer, *task, g_OverworldMap, m_SelectedRegionId))
    {
        m_TaskStatusMessage = string("OK: ") + task->m_Name;
        if (task->m_Effect.m_Type == "startCastleBuild")
        {
            m_TaskStatusMessage += " (building)";
        }
        else if (!task->m_Maintenance.IsEmpty())
        {
            m_TaskStatusMessage += " (+" + task->m_Maintenance.ToShortLabel() + "/t)";
        }
        g_AiObserverLog.Add(humanPlayer->m_Id,
            string(humanPlayer->GetColorName()) + ": " + task->m_Name);
    }
    else
    {
        m_TaskStatusMessage = g_PlayerTasksConfig.GetTaskFailureReason(
            *humanPlayer, *task, g_OverworldMap, m_SelectedRegionId);
    }
}

void MainState::HandleActionPanelInput(const SideLayout& layout)
{
    m_HoveredActionIndex = -1;
    if (IsAllAiGame())
    {
        return; // Factions roster is display-only.
    }

    const Vector2 mouse = GetScaledMousePosition();

    for (int i = 0; i < kActionCount; ++i)
    {
        if (CheckCollisionPointRec(mouse, GetActionButtonRect(layout, i)))
        {
            m_HoveredActionIndex = i;
            break;
        }
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || m_HoveredActionIndex < 0)
    {
        return;
    }

    TryPerformAction(m_HoveredActionIndex);
}

void MainState::HandleNextTurnButton(const SideLayout& layout)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }
    if (!IsMouseOverNextTurnButton(layout))
    {
        return;
    }
    if (g_GameDatabase.m_Outcome != CampaignOutcome::None)
    {
        return;
    }

    g_GameDatabase.AdvanceTurn(g_OverworldMap);
    m_TaskStatusMessage = "Turn " + to_string(g_GameDatabase.m_Turn);
}

void MainState::Update()
{
    HandleCampaignOutcomeInput();
    if (g_GameDatabase.m_Outcome != CampaignOutcome::None)
    {
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (m_AiObserverOpen)
        {
            m_AiObserverOpen = false;
            return;
        }
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
    }

    if (IsKeyPressed(KEY_F5))
    {
        if (g_GameDatabase.SaveCampaign(kDefaultCampaignSavePath))
        {
            m_TaskStatusMessage = "Saved campaign";
            g_AiObserverLog.Add(-1, "Campaign saved.");
        }
        else
        {
            m_TaskStatusMessage = "Save failed";
        }
    }

    if (IsKeyPressed(KEY_R) && IsAllAiGame())
    {
        unsigned int seed = static_cast<unsigned int>(GetTime() * 1000.0);
        g_GameDatabase.m_Setup.m_Seed = seed;
        g_OverworldMap.Generate(seed, g_GameDatabase.m_Setup);
        m_SelectedRegionId = -1;
        g_GameDatabase.SyncPlayersFromOverworld(g_OverworldMap, true);
        g_GameDatabase.BuildRegionsFromOverworld(g_OverworldMap);
        g_GameDatabase.GenerateAllRegionHeightfields();
        g_AiObserverLog.Clear();
        g_AiObserverLog.BeginTurn(0);
        g_AiObserverLog.Add(-1, "New map generated.");
    }

    HandleAiObserverInput();

    const SideLayout layout = ComputeSideLayout();

    HandleTopBarInput();
    HandleTurnLogInput(layout);
    HandleNextTurnButton(layout);
    HandleActionPanelInput(layout);
    HandleVisitRegionButton(layout);
    HandleAttackButton(layout);

    const Rectangle turnLogHit = Rectangle{
        static_cast<float>(layout.m_PanelX),
        static_cast<float>(layout.m_MidY),
        static_cast<float>(layout.m_PanelW),
        static_cast<float>(layout.m_MidH)
    };

    if (!m_AiObserverOpen
        && !IsMouseOverNextTurnButton(layout)
        && m_HoveredActionIndex < 0
        && !m_TurnLogDragging
        && !CheckCollisionPointRec(GetScaledMousePosition(), turnLogHit)
        && !CheckCollisionPointRec(GetScaledMousePosition(), GetVisitRegionButtonRect(layout))
        && !CheckCollisionPointRec(GetScaledMousePosition(), GetAttackButtonRect(layout)))
    {
        HandleMapSelection();
    }
}

void MainState::Draw()
{
    DrawRectangle(0, 0, static_cast<int>(g_Engine->m_RenderWidth), static_cast<int>(g_Engine->m_RenderHeight),
        Color{ 24, 28, 36, 255 });

    DrawTopBar();
    {
        // Human games: fog of war for the human seat. All-AI observe: full map.
        int visionPlayerId = -1;
        if (!IsAllAiGame())
        {
            if (const Player* human = GetHumanPlayer(g_GameDatabase.m_Players))
            {
                visionPlayerId = human->m_Id;
            }
        }
        g_OverworldMap.Draw(kMapDrawX, kMapDrawY, kMapPixelsPerCell, m_SelectedRegionId, visionPlayerId);
    }
    DrawTopBarTooltip();

    const SideLayout layout = ComputeSideLayout();
    DrawSelectionPanel(layout);
    DrawTurnConsole(layout);
    DrawActionPanel(layout);

    DrawAiObserverPane();
    DrawCampaignOutcomeOverlay();
}
