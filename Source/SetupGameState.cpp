#include <algorithm>
#include <string>

#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "SetupGameState.h"
#include "GameGlobals.h"
#include "OverworldMap.h"
#include "Player.h"

using namespace std;

namespace
{
    constexpr int kRowHeight = 18;
    constexpr int kRowStartY = 44;
    constexpr int kPanelX = 24;
    constexpr int kPanelWidth = 220;
    constexpr int kLabelX = 32;
    constexpr int kValueX = 150;
    constexpr int kArrowWidth = 14;
    constexpr int kStartButtonHeight = 20;

    Vector2 GetScaledMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        const float inputScale = g_Engine->GetInputScale();
        mouse.x /= inputScale;
        mouse.y /= inputScale;
        return mouse;
    }

    int CycleEnumValue(int value, int delta, int count)
    {
        value += delta;
        while (value < 0)
        {
            value += count;
        }
        value %= count;
        return value;
    }
}

void SetupGameState::Init(const std::string& configfile)
{
    (void)configfile;
}

void SetupGameState::Shutdown()
{
}

void SetupGameState::OnEnter()
{
    m_DraftSetup = CampaignSetup{};
    ClampCampaignSetup(m_DraftSetup);
    m_SelectedOption = 0;
}

void SetupGameState::OnExit()
{
}

Rectangle SetupGameState::GetOptionRect(int optionIndex) const
{
    const float y = static_cast<float>(kRowStartY + (optionIndex * kRowHeight));
    return Rectangle{
        static_cast<float>(kPanelX),
        y,
        static_cast<float>(kPanelWidth),
        static_cast<float>(kRowHeight)
    };
}

Rectangle SetupGameState::GetAdjustLeftRect(int optionIndex) const
{
    const Rectangle row = GetOptionRect(optionIndex);
    return Rectangle{
        static_cast<float>(kValueX - kArrowWidth - 2),
        row.y + 2.0f,
        static_cast<float>(kArrowWidth),
        row.height - 4.0f
    };
}

Rectangle SetupGameState::GetAdjustRightRect(int optionIndex) const
{
    const Rectangle row = GetOptionRect(optionIndex);
    return Rectangle{
        static_cast<float>(kPanelX + kPanelWidth - kArrowWidth - 8),
        row.y + 2.0f,
        static_cast<float>(kArrowWidth),
        row.height - 4.0f
    };
}

bool SetupGameState::IsMouseOverOption(int optionIndex) const
{
    return CheckCollisionPointRec(GetScaledMousePosition(), GetOptionRect(optionIndex));
}

bool SetupGameState::IsMouseOverAdjustLeft(int optionIndex) const
{
    return CheckCollisionPointRec(GetScaledMousePosition(), GetAdjustLeftRect(optionIndex));
}

bool SetupGameState::IsMouseOverAdjustRight(int optionIndex) const
{
    return CheckCollisionPointRec(GetScaledMousePosition(), GetAdjustRightRect(optionIndex));
}

std::string SetupGameState::GetOptionValueLabel(Option option) const
{
    switch (option)
    {
    case Option::Opponents:
        return to_string(m_DraftSetup.m_EnemyCount);
    case Option::MapSize:
        return string(MapSizeName(m_DraftSetup.m_MapSize))
            + " (" + to_string(MapSizeRegionCount(m_DraftSetup.m_MapSize)) + ")";
    case Option::BattleMode:
        return BattleModeName(m_DraftSetup.m_BattleMode);
    case Option::ResourceDistribution:
        return ResourceDistributionName(m_DraftSetup.m_ResourceDistribution);
    default:
        return "";
    }
}

void SetupGameState::AdjustOption(int delta)
{
    switch (static_cast<Option>(m_SelectedOption))
    {
    case Option::Opponents:
        m_DraftSetup.m_EnemyCount = std::clamp(m_DraftSetup.m_EnemyCount + delta, kMinOpponents, kMaxOpponents);
        break;
    case Option::MapSize:
        m_DraftSetup.m_MapSize = static_cast<MapSize>(
            CycleEnumValue(static_cast<int>(m_DraftSetup.m_MapSize), delta, 4));
        break;
    case Option::BattleMode:
        m_DraftSetup.m_BattleMode = static_cast<BattleMode>(
            CycleEnumValue(static_cast<int>(m_DraftSetup.m_BattleMode), delta, 2));
        break;
    case Option::ResourceDistribution:
        m_DraftSetup.m_ResourceDistribution = static_cast<ResourceDistribution>(
            CycleEnumValue(static_cast<int>(m_DraftSetup.m_ResourceDistribution), delta, 3));
        break;
    default:
        break;
    }
}

void SetupGameState::StartCampaign()
{
    ClampCampaignSetup(m_DraftSetup);

    g_GameDatabase.Clear();
    g_GameDatabase.m_Setup = m_DraftSetup;
    if (g_GameDatabase.m_Setup.m_Seed == 0)
    {
        g_GameDatabase.m_Setup.m_Seed = static_cast<unsigned int>(GetTime() * 1000.0);
    }

    g_OverworldMap.Generate(g_GameDatabase.m_Setup.m_Seed, g_GameDatabase.m_Setup);

    const int playerCount = std::clamp(1 + g_GameDatabase.m_Setup.m_EnemyCount, 1, kMaxCampaignPlayers);
    InitializeCampaignPlayers(g_GameDatabase.m_Players, playerCount);
    SyncPlayersFromOverworld(g_OverworldMap, g_GameDatabase.m_Players, true);
    g_GameDatabase.m_Turn = 0;
    g_GameDatabase.m_ActiveRegionId = -1;

    g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
}

void SetupGameState::Update()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
        return;
    }

    if (IsKeyPressed(KEY_UP))
    {
        m_SelectedOption = (m_SelectedOption + static_cast<int>(Option::Count) - 1) % static_cast<int>(Option::Count);
    }

    if (IsKeyPressed(KEY_DOWN))
    {
        m_SelectedOption = (m_SelectedOption + 1) % static_cast<int>(Option::Count);
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
    {
        if (m_SelectedOption != static_cast<int>(Option::Start))
        {
            AdjustOption(-1);
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
    {
        if (m_SelectedOption != static_cast<int>(Option::Start))
        {
            AdjustOption(1);
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        if (m_SelectedOption == static_cast<int>(Option::Start))
        {
            StartCampaign();
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (int optionIndex = 0; optionIndex < static_cast<int>(Option::Count); ++optionIndex)
        {
            if (optionIndex == static_cast<int>(Option::Start))
            {
                if (IsMouseOverOption(optionIndex))
                {
                    m_SelectedOption = optionIndex;
                    StartCampaign();
                }
                continue;
            }

            if (IsMouseOverAdjustLeft(optionIndex))
            {
                m_SelectedOption = optionIndex;
                AdjustOption(-1);
                continue;
            }

            if (IsMouseOverAdjustRight(optionIndex))
            {
                m_SelectedOption = optionIndex;
                AdjustOption(1);
                continue;
            }

            if (IsMouseOverOption(optionIndex))
            {
                m_SelectedOption = optionIndex;
            }
        }
    }
}

void SetupGameState::Draw()
{
    DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 28, 34, 48, 255 });

    DrawOutlinedText(g_font, "New Campaign", Vector2{ static_cast<float>(kPanelX), 12.0f },
        g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });

    const char* labels[] = {
        "Opponents",
        "Map Size",
        "Battles",
        "Resources",
        "Start Game"
    };

    for (int optionIndex = 0; optionIndex < static_cast<int>(Option::Count); ++optionIndex)
    {
        const Rectangle rowRect = GetOptionRect(optionIndex);
        const bool selected = optionIndex == m_SelectedOption;
        const bool hovered = IsMouseOverOption(optionIndex);

        if (selected || hovered)
        {
            const Color highlight = selected
                ? Color{ 70, 90, 130, 255 }
                : Color{ 50, 60, 82, 255 };
            DrawRectangle(static_cast<int>(rowRect.x), static_cast<int>(rowRect.y),
                static_cast<int>(rowRect.width), static_cast<int>(rowRect.height), highlight);
        }

        const Color labelColor = selected ? WHITE : Color{ 210, 210, 220, 255 };
        DrawOutlinedText(g_smallFont, labels[optionIndex],
            Vector2{ static_cast<float>(kLabelX), rowRect.y + 3.0f },
            g_smallFontDrawSize, 1, labelColor);

        if (optionIndex == static_cast<int>(Option::Start))
        {
            const Rectangle buttonRect{
                rowRect.x + 96.0f,
                rowRect.y + 1.0f,
                96.0f,
                static_cast<float>(kStartButtonHeight)
            };
            const Color fill = selected ? Color{ 70, 120, 70, 255 } : Color{ 48, 88, 48, 255 };
            DrawRectangle(static_cast<int>(buttonRect.x), static_cast<int>(buttonRect.y),
                static_cast<int>(buttonRect.width), static_cast<int>(buttonRect.height), fill);
            DrawRectangleLines(static_cast<int>(buttonRect.x), static_cast<int>(buttonRect.y),
                static_cast<int>(buttonRect.width), static_cast<int>(buttonRect.height),
                Color{ 120, 180, 120, 255 });
            continue;
        }

        const Rectangle leftRect = GetAdjustLeftRect(optionIndex);
        const Rectangle rightRect = GetAdjustRightRect(optionIndex);
        const Color arrowFill = selected ? Color{ 90, 110, 150, 255 } : Color{ 60, 70, 92, 255 };
        DrawRectangle(static_cast<int>(leftRect.x), static_cast<int>(leftRect.y),
            static_cast<int>(leftRect.width), static_cast<int>(leftRect.height), arrowFill);
        DrawRectangle(static_cast<int>(rightRect.x), static_cast<int>(rightRect.y),
            static_cast<int>(rightRect.width), static_cast<int>(rightRect.height), arrowFill);
        DrawOutlinedText(g_smallFont, "<",
            Vector2{ leftRect.x + 3.0f, leftRect.y + 1.0f }, g_smallFontDrawSize, 1, WHITE);
        DrawOutlinedText(g_smallFont, ">",
            Vector2{ rightRect.x + 3.0f, rightRect.y + 1.0f }, g_smallFontDrawSize, 1, WHITE);

        const string value = GetOptionValueLabel(static_cast<Option>(optionIndex));
        DrawOutlinedText(g_smallFont, value,
            Vector2{ static_cast<float>(kValueX), rowRect.y + 3.0f },
            g_smallFontDrawSize, 1, Color{ 255, 230, 90, 255 });
    }

    DrawOutlinedText(g_smallFont, "Arrows: select/adjust   Enter: start   Esc: title",
        Vector2{ static_cast<float>(kPanelX), static_cast<float>(g_Engine->m_RenderHeight - 14.0f) },
        g_smallFontDrawSize, 1, Color{ 180, 180, 190, 255 });
}