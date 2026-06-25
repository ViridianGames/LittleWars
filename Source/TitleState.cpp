#include <string>
#include <iomanip>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "GameGlobals.h"

using namespace std;

void TitleState::Init(const std::string& configfile)
{

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
	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, BLUE);
	//DrawTextEx(*g_font, "Hello!", {0, 0}, g_fontDrawSize, 1, WHITE);
	DrawOutlinedText(g_font, "Hello!", {0, 0}, g_fontDrawSize, 1, WHITE);

	DrawOutlinedText(g_smallFont, "Hello!", {0, 20}, g_smallFontDrawSize, 1, WHITE);
	DrawOutlinedText(g_smallFont, "N: new campaign", {0, 36}, g_smallFontDrawSize, 1, WHITE);
	DrawOutlinedText(g_smallFont, "C: combat test", {0, 52}, g_smallFontDrawSize, 1, WHITE);
	DrawOutlinedText(g_smallFont, "M: main map", {0, 68}, g_smallFontDrawSize, 1, WHITE);

}