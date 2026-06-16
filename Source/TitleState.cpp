#include <string>
#include <iomanip>
#include "Geist/Engine.h"
#include "Geist/Globals.h"

#include "LWGlobals.h"

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
	// Check for exit
	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_Engine->m_Done = true;
	}
}

void TitleState::Draw()
{

}