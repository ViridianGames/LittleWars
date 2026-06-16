#include <string>
#include <iomanip>
#include "Geist/Engine.h"
#include "Geist/Globals.h"

#include "GameGlobals.h"

using namespace std;

void CastleDesignState::Init(const std::string& configfile)
{

}

void CastleDesignState::Shutdown()
{

}

void CastleDesignState::OnEnter()
{

}

void CastleDesignState::OnExit()
{

}

void CastleDesignState::Update()
{
    // Check for exit
    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_Engine->m_Done = true;
    }
}

void CastleDesignState::Draw()
{

}