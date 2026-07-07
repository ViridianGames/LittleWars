#include <string>
#include <iomanip>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/ResourceManager.h"
#include "../Geist/Source/StateMachine.h"

#include "GameGlobals.h"

using namespace std;

namespace
{
    constexpr const char* TITLE_BACKGROUND_PATH = "Images/title_near_final_480x270.png";
    constexpr float TITLE_FONT_DRAW_SIZE = 36.0f;
    constexpr float TITLE_LINE_SPACING = 2.0f;
    constexpr float SHORTCUT_BOTTOM_MARGIN = 10.0f;
    constexpr float SHORTCUT_LINE_SPACING = 4.0f;

    void DrawOutlinedTextCentered(
        const std::shared_ptr<Font>& font,
        const std::string& text,
        float centerX,
        float y,
        float fontSize,
        int spacing,
        Color color)
    {
        if (!font)
        {
            return;
        }

        const Vector2 textSize = MeasureTextEx(*font, text.c_str(), fontSize, spacing);
        DrawOutlinedText(font, text, Vector2{ centerX - textSize.x * 0.5f, y }, fontSize, spacing, color);
    }
}

void TitleState::Init(const std::string& configfile)
{
    g_ResourceManager->AddTexture(TITLE_BACKGROUND_PATH, false);
    if (Texture* texture = g_ResourceManager->GetTexture(TITLE_BACKGROUND_PATH, false))
    {
        SetTextureFilter(*texture, TEXTURE_FILTER_POINT);
    }
}

void TitleState::Shutdown()
{

}

void TitleState::OnEnter()
{

}

void TitleState::OnExit()
{

}

void TitleState::Update()
{
	if (IsKeyPressed(KEY_C))
	{
		g_StateMachine->MakeStateTransition(STATE_COMBATSTATE);
	}

	if (IsKeyPressed(KEY_D))
	{
		g_StateMachine->MakeStateTransition(STATE_CASTLEDESIGNSTATE);
	}

	if (IsKeyPressed(KEY_N))
	{
		g_StateMachine->MakeStateTransition(STATE_SETUPGAMESTATE);
	}

	if (IsKeyPressed(KEY_M))
	{
		g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
	}

	// Check for exit
	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_Engine->m_Done = true;
	}
}

void TitleState::Draw()
{
    if (Texture* texture = g_ResourceManager->GetTexture(TITLE_BACKGROUND_PATH, false))
    {
        if (texture->id != 0)
        {
            const Rectangle source{
                0.0f,
                0.0f,
                static_cast<float>(texture->width),
                static_cast<float>(texture->height)
            };
            const Rectangle dest{
                0.0f,
                0.0f,
                static_cast<float>(g_Engine->m_RenderWidth),
                static_cast<float>(g_Engine->m_RenderHeight)
            };
            DrawTexturePro(*texture, source, dest, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
        }
    }

    const float quarterCenterX = static_cast<float>(g_Engine->m_RenderWidth) * 0.25f;
    const float quarterCenterY = static_cast<float>(g_Engine->m_RenderHeight) * 0.25f;
    const float titleLineHeight = TITLE_FONT_DRAW_SIZE + TITLE_LINE_SPACING;
    const float titleBlockHeight = titleLineHeight * 2.0f;
    const float titleStartY = quarterCenterY - titleBlockHeight * 0.5f;

    DrawOutlinedTextCentered(
        g_font,
        "Little",
        quarterCenterX,
        titleStartY,
        TITLE_FONT_DRAW_SIZE,
        1,
        WHITE);
    DrawOutlinedTextCentered(
        g_font,
        "Wars",
        quarterCenterX,
        titleStartY + titleLineHeight,
        TITLE_FONT_DRAW_SIZE,
        1,
        WHITE);

    const float shortcutLineHeight = g_smallFontDrawSize + SHORTCUT_LINE_SPACING;
    const std::string shortcuts[] = {
        "N: new campaign",
        "C: combat test",
        "D: castle design",
        "M: main map"
    };
    const float shortcutBlockHeight = shortcutLineHeight * static_cast<float>(std::size(shortcuts));
    const float shortcutStartY = static_cast<float>(g_Engine->m_RenderHeight)
        - SHORTCUT_BOTTOM_MARGIN
        - shortcutBlockHeight;
    const float screenCenterX = static_cast<float>(g_Engine->m_RenderWidth) * 0.5f;

    for (size_t i = 0; i < std::size(shortcuts); ++i)
    {
        DrawOutlinedTextCentered(
            g_smallFont,
            shortcuts[i],
            screenCenterX,
            shortcutStartY + shortcutLineHeight * static_cast<float>(i),
            g_smallFontDrawSize,
            1,
            WHITE);
    }
}