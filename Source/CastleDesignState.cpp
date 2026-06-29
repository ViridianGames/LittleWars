#include <string>
#include <iomanip>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "GameGlobals.h"
#include "RegionMinimap.h"
#include "RegionTerrainMesh.h"
#include "RegionView.h"

using namespace std;

void CastleDesignState::Init(const std::string& configfile)
{
    g_RegionView.Init();
}

void CastleDesignState::Shutdown()
{

}

void CastleDesignState::OnEnter()
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

void CastleDesignState::OnExit()
{

}

void CastleDesignState::Update()
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

void CastleDesignState::Draw()
{
    RegionData* region = g_GameDatabase.GetActiveRegion();
    if (!region || !region->m_Heightfield.m_Generated)
    {
        DrawOutlinedText(g_font, "Castle design (no region terrain)", { 4.0f, 4.0f }, g_fontDrawSize, 1, WHITE);
        return;
    }

    const RegionHeightfield& heightfield = region->m_Heightfield;

    g_RegionTerrainMesh.SetHeightfield(&heightfield);
    g_RegionTerrainMesh.SetFlatShaded(false);
    g_RegionTerrainMesh.RebuildIfNeeded();

    g_RegionView.Begin3D();
    g_RegionTerrainMesh.Draw();
    g_RegionView.End3D();

    g_RegionMinimap.Draw(
        heightfield,
        region->m_HeightfieldSeed,
        g_RegionView.GetCamera());

    DrawOutlinedText(g_font, "Castle  WASD:pan  Wheel:zoom  Q/E:rotate  Esc:title", { 4.0f, 4.0f }, g_fontDrawSize, 1, WHITE);
}