#include <string>
#include <iomanip>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"

#include "GameGlobals.h"

using namespace std;

void SetupGameState::Init(const std::string& configfile)
{

}

void SetupGameState::Shutdown()
{

}

void SetupGameState::OnEnter()
{

}

void SetupGameState::OnExit()
{

}

void SetupGameState::Update()
{
    // Check for exit
    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_Engine->m_Done = true;
    }
}

void SetupGameState::Draw()
{

}