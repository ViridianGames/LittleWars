#include <string>
#include <iomanip>
#include "Geist/Engine.h"
#include "Geist/Globals.h"

#include "LWGlobals.h"

using namespace std;

void CombatState::Init(const std::string& configfile)
{

}

void CombatState::Shutdown()
{

}

void CombatState::OnEnter()
{

}

void CombatState::OnExit()
{

}

void CombatState::Update()
{
    // Check for exit
    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_Engine->m_Done = true;
    }
}

void CombatState::Draw()
{

}