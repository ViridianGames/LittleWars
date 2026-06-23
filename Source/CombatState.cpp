#include <string>
#include <iomanip>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "GameGlobals.h"
#include "LittlePeopleSprites.h"
#include "RegionTerrainMesh.h"
#include "RegionView.h"

using namespace std;

void CombatState::Init(const std::string& configfile)
{
    g_RegionView.Init();
}

void CombatState::Shutdown()
{

}

void CombatState::OnEnter()
{
    if (g_GameDatabase.m_Regions.empty())
    {
        g_GameDatabase.InitNewCampaign(CampaignSetup{});
        g_GameDatabase.SetActiveRegion(0);
    }

    if (g_GameDatabase.m_ActiveRegionId >= 0)
    {
        g_GameDatabase.EnsureRegionHeightfield(g_GameDatabase.m_ActiveRegionId);
    }
}

void CombatState::OnExit()
{

}

void CombatState::Update()
{
    RegionHeightfield* heightfield = nullptr;
    if (RegionData* region = g_GameDatabase.GetActiveRegion())
    {
        heightfield = g_GameDatabase.EnsureRegionHeightfield(region->m_Id);
    }

    g_RegionView.Update(heightfield);

    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
    }
}

void CombatState::Draw()
{
    RegionData* region = g_GameDatabase.GetActiveRegion();
    if (!region || !region->m_Heightfield.m_Generated)
    {
        DrawOutlinedText(g_font, "Combat (no region terrain)", { 4.0f, 4.0f }, g_fontDrawSize, 1, WHITE);
        return;
    }

    const RegionHeightfield& heightfield = region->m_Heightfield;
    const int walkFrame = LittlePeopleWalkFrameFromTime(GetTime());
    const float spriteHeight = 1.2f;

    g_RegionTerrainMesh.SetHeightfield(&heightfield);
    g_RegionTerrainMesh.RebuildIfNeeded();

    g_RegionView.Begin3D();
    g_RegionTerrainMesh.Draw();

    for (int armyIndex = 0; armyIndex < LITTLEPEOPLE_ARMY_COUNT; ++armyIndex)
    {
        const auto army = static_cast<LittlePeopleArmy>(armyIndex);

        for (int directionIndex = 0; directionIndex < LITTLEPEOPLE_DIRECTION_COUNT; ++directionIndex)
        {
            const auto worldDirection = static_cast<LittlePeopleDirection>(directionIndex);
            const float x = 12.0f + armyIndex * 8.0f;
            const float z = 12.0f + directionIndex * 3.5f;
            const float y = heightfield.SampleHeight(x, z);

            DrawLittlePersonBillboard(
                g_RegionView.GetCamera(),
                army,
                worldDirection,
                walkFrame,
                Vector3{ x, y, z },
                spriteHeight
            );
        }
    }

    g_RegionView.End3D();

    DrawOutlinedText(g_font, "Combat  Q/E:rotate  Arrows:pan  W/S:zoom  Esc:title", { 4.0f, 4.0f }, g_fontDrawSize, 1, WHITE);
}