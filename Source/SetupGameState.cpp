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
    Vector2 GetScaledMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        const float inputScale = g_Engine->GetInputScale();
        mouse.x /= inputScale;
        mouse.y /= inputScale;
        return mouse;
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
    g_CampaignSetupScreenConfig.ApplyDefaults(m_DraftSetup);
    m_SelectedOption = 0;
}

void SetupGameState::OnExit()
{
}

Rectangle SetupGameState::GetOptionRect(int optionIndex) const
{
    const SetupOptionLayout& layout = g_CampaignSetupScreenConfig.GetLayout();
    const float y = static_cast<float>(layout.m_RowStartY + (optionIndex * layout.m_RowHeight));
    return Rectangle{
        static_cast<float>(layout.m_PanelX),
        y,
        static_cast<float>(layout.m_PanelWidth),
        static_cast<float>(layout.m_RowHeight)
    };
}

Rectangle SetupGameState::GetAdjustLeftRect(int optionIndex) const
{
    const SetupOptionLayout& layout = g_CampaignSetupScreenConfig.GetLayout();
    const Rectangle row = GetOptionRect(optionIndex);
    return Rectangle{
        static_cast<float>(layout.m_ValueX - layout.m_ArrowWidth - 2),
        row.y + 2.0f,
        static_cast<float>(layout.m_ArrowWidth),
        row.height - 4.0f
    };
}

Rectangle SetupGameState::GetAdjustRightRect(int optionIndex) const
{
    const SetupOptionLayout& layout = g_CampaignSetupScreenConfig.GetLayout();
    const Rectangle row = GetOptionRect(optionIndex);
    return Rectangle{
        static_cast<float>(layout.m_PanelX + layout.m_PanelWidth - layout.m_ArrowWidth - 8),
        row.y + 2.0f,
        static_cast<float>(layout.m_ArrowWidth),
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

void SetupGameState::AdjustOption(int delta)
{
    g_CampaignSetupScreenConfig.AdjustOption(m_SelectedOption, delta, m_DraftSetup);
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
    const int optionCount = g_CampaignSetupScreenConfig.GetOptionCount();
    const int startOptionIndex = g_CampaignSetupScreenConfig.GetStartOptionIndex();
    if (optionCount <= 0)
    {
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
        return;
    }

    if (IsKeyPressed(KEY_UP))
    {
        m_SelectedOption = (m_SelectedOption + optionCount - 1) % optionCount;
    }

    if (IsKeyPressed(KEY_DOWN))
    {
        m_SelectedOption = (m_SelectedOption + 1) % optionCount;
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
    {
        if (m_SelectedOption != startOptionIndex)
        {
            AdjustOption(-1);
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
    {
        if (m_SelectedOption != startOptionIndex)
        {
            AdjustOption(1);
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        if (m_SelectedOption == startOptionIndex)
        {
            StartCampaign();
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (int optionIndex = 0; optionIndex < optionCount; ++optionIndex)
        {
            if (optionIndex == startOptionIndex)
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
    const SetupOptionLayout& layout = g_CampaignSetupScreenConfig.GetLayout();
    const int optionCount = g_CampaignSetupScreenConfig.GetOptionCount();
    const int startOptionIndex = g_CampaignSetupScreenConfig.GetStartOptionIndex();

    DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 28, 34, 48, 255 });

    DrawOutlinedText(g_font, g_CampaignSetupScreenConfig.GetTitle(),
        Vector2{ static_cast<float>(layout.m_PanelX), 12.0f },
        g_fontDrawSize, 1, Color{ 255, 230, 90, 255 });

    for (int optionIndex = 0; optionIndex < optionCount; ++optionIndex)
    {
        const SetupOptionDefinition& option = g_CampaignSetupScreenConfig.GetOption(optionIndex);
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
        DrawOutlinedText(g_smallFont, option.m_Label,
            Vector2{ static_cast<float>(layout.m_LabelX), rowRect.y + 3.0f },
            g_smallFontDrawSize, 1, labelColor);

        if (optionIndex == startOptionIndex)
        {
            const Rectangle buttonRect{
                rowRect.x + static_cast<float>(layout.m_StartButtonX),
                rowRect.y + 1.0f,
                static_cast<float>(layout.m_StartButtonWidth),
                static_cast<float>(layout.m_StartButtonHeight)
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

        const string value = g_CampaignSetupScreenConfig.GetValueLabel(optionIndex, m_DraftSetup);
        DrawOutlinedText(g_smallFont, value,
            Vector2{ static_cast<float>(layout.m_ValueX), rowRect.y + 3.0f },
            g_smallFontDrawSize, 1, Color{ 255, 230, 90, 255 });
    }

    DrawOutlinedText(g_smallFont, g_CampaignSetupScreenConfig.GetHelpText(),
        Vector2{ static_cast<float>(layout.m_PanelX), static_cast<float>(g_Engine->m_RenderHeight - 14.0f) },
        g_smallFontDrawSize, 1, Color{ 180, 180, 190, 255 });
}