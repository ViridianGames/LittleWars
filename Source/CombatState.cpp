#include <string>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "CombatUnits.h"
#include "GameGlobals.h"
#include "LittlePeopleSprites.h"
#include "RegionTerrainMesh.h"
#include "RegionView.h"
#include "raymath.h"

using namespace std;

namespace
{
    constexpr double kGestureTimeThreshold = 0.12;
    constexpr float kGestureMoveThreshold = 4.0f;

    const char* LittlePeopleArmyName(LittlePeopleArmy army)
    {
        switch (army)
        {
        case LittlePeopleArmy::White:
            return "White";
        case LittlePeopleArmy::Blue:
            return "Blue";
        case LittlePeopleArmy::Red:
            return "Red";
        case LittlePeopleArmy::Green:
            return "Green";
        default:
            return "Unknown";
        }
    }

    bool IsGestureHold(double pressTime, Vector2 pressPosition, Vector2 currentPosition)
    {
        const double holdDuration = GetTime() - pressTime;
        const float mouseTravel = Vector2Distance(currentPosition, pressPosition);
        return holdDuration >= kGestureTimeThreshold || mouseTravel >= kGestureMoveThreshold;
    }
}

void CombatState::Init(const std::string& configfile)
{
    (void)configfile;
    g_RegionView.Init();
}

void CombatState::Shutdown()
{

}

void CombatState::InitializeDemoUnits()
{
    m_Units.clear();
    m_SelectedUnitIndex = -1;
    m_HasMoveTarget = false;
    m_HasHoverTarget = false;
    m_GestureUnitIndex = -1;
    m_IsGestureHold = false;
    m_PendingQuickClick = false;
    m_HasGestureFacingTarget = false;

    const CombatUnitInstance demoUnits[] = {
        { 0, CombatUnitType::Swordsmen, Vector3{ 18.0f, 0.0f, 22.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 1, CombatUnitType::Archers, Vector3{ 34.0f, 0.0f, 22.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 2, CombatUnitType::Knights, Vector3{ 52.0f, 0.0f, 22.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 3, CombatUnitType::Catapult, Vector3{ 62.0f, 0.0f, 22.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 4, CombatUnitType::Swordsmen, Vector3{ 18.0f, 0.0f, 40.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 5, CombatUnitType::Archers, Vector3{ 34.0f, 0.0f, 40.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 6, CombatUnitType::Knights, Vector3{ 52.0f, 0.0f, 40.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 7, CombatUnitType::Catapult, Vector3{ 62.0f, 0.0f, 40.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
    };

    for (const CombatUnitInstance& unit : demoUnits)
    {
        m_Units.push_back(unit);
    }
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

    InitializeDemoUnits();
}

void CombatState::OnExit()
{

}

void CombatState::UpdateTerrainTargetPreview(const RegionHeightfield& heightfield)
{
    m_HasHoverTarget = false;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    if (m_SelectedUnitIndex < 0 || m_SelectedUnitIndex >= static_cast<int>(m_Units.size()))
    {
        return;
    }

    const Ray ray = GetCombatMouseRay(g_RegionView.GetCamera());
    Vector3 terrainHit{};
    if (RaycastCombatTerrain(ray, heightfield, terrainHit))
    {
        m_HoverTarget = terrainHit;
        m_HasHoverTarget = true;
    }
}

void CombatState::HandleCombatInput(const RegionHeightfield& heightfield)
{
    const Camera3D& camera = g_RegionView.GetCamera();
    const Vector2 mousePosition = GetCombatScaledMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        m_LeftMousePressTime = GetTime();
        m_LeftMousePressPosition = mousePosition;
        m_IsGestureHold = false;
        m_GestureUnitIndex = -1;
        m_GestureStartedOnUnit = false;
        m_PendingQuickClick = false;
        m_HasGestureFacingTarget = false;

        const int pickedUnitIndex = PickCombatUnitAtMouse(camera, heightfield, m_Units, mousePosition);
        if (pickedUnitIndex >= 0)
        {
            m_SelectedUnitIndex = pickedUnitIndex;
            m_GestureUnitIndex = pickedUnitIndex;
            m_GestureStartedOnUnit = true;
            m_PendingQuickClick = true;
            m_HasMoveTarget = false;
            return;
        }

        if (m_SelectedUnitIndex >= 0 && m_SelectedUnitIndex < static_cast<int>(m_Units.size()))
        {
            const Ray ray = GetCombatMouseRay(camera);
            Vector3 terrainHit{};
            if (RaycastCombatTerrain(ray, heightfield, terrainHit))
            {
                m_GestureUnitIndex = m_SelectedUnitIndex;
                m_GestureStartedOnUnit = false;
                m_PendingTerrainTarget = terrainHit;
                m_PendingQuickClick = true;
            }
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && m_GestureUnitIndex >= 0
        && m_GestureUnitIndex < static_cast<int>(m_Units.size()))
    {
        if (IsGestureHold(m_LeftMousePressTime, m_LeftMousePressPosition, mousePosition))
        {
            m_IsGestureHold = true;
            m_PendingQuickClick = false;
        }

        const Ray ray = GetCombatMouseRay(camera);
        Vector3 terrainHit{};
        if (RaycastCombatTerrain(ray, heightfield, terrainHit))
        {
            FaceCombatUnitToward(m_Units[static_cast<size_t>(m_GestureUnitIndex)], terrainHit);
            m_GestureFacingTarget = terrainHit;
            m_HasGestureFacingTarget = true;
        }
    }
    else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        m_HasGestureFacingTarget = false;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        if (m_PendingQuickClick && !m_IsGestureHold && !m_GestureStartedOnUnit
            && m_GestureUnitIndex >= 0 && m_GestureUnitIndex < static_cast<int>(m_Units.size()))
        {
            MoveCombatUnit(m_Units[static_cast<size_t>(m_GestureUnitIndex)], m_PendingTerrainTarget);
            m_MoveTarget = m_PendingTerrainTarget;
            m_HasMoveTarget = true;
        }

        m_GestureUnitIndex = -1;
        m_IsGestureHold = false;
        m_PendingQuickClick = false;
        m_HasGestureFacingTarget = false;
    }
}

void CombatState::Update()
{
    RegionHeightfield* heightfield = nullptr;
    if (RegionData* region = g_GameDatabase.GetActiveRegion())
    {
        heightfield = g_GameDatabase.EnsureRegionHeightfield(region->m_Id);
    }

    g_RegionView.Update(heightfield);

    if (heightfield)
    {
        UpdateTerrainTargetPreview(*heightfield);
        HandleCombatInput(*heightfield);
    }

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

    g_RegionTerrainMesh.SetHeightfield(&heightfield);
    g_RegionTerrainMesh.RebuildIfNeeded();

    g_RegionView.Begin3D();
    g_RegionTerrainMesh.Draw();

    for (int unitIndex = 0; unitIndex < static_cast<int>(m_Units.size()); ++unitIndex)
    {
        const bool selected = (unitIndex == m_SelectedUnitIndex);
        DrawCombatUnit(
            g_RegionView.GetCamera(),
            heightfield,
            m_Units[static_cast<size_t>(unitIndex)],
            walkFrame,
            selected
        );
    }

    if (m_SelectedUnitIndex >= 0 && m_SelectedUnitIndex < static_cast<int>(m_Units.size()))
    {
        DrawCombatUnitSelectionIndicator(heightfield, m_Units[static_cast<size_t>(m_SelectedUnitIndex)]);
    }

    if (m_HasGestureFacingTarget)
    {
        DrawCombatMoveMarker(heightfield, m_GestureFacingTarget, Color{ 255, 180, 60, 255 });
    }
    else if (m_HasHoverTarget)
    {
        DrawCombatMoveMarker(heightfield, m_HoverTarget, Color{ 120, 220, 255, 180 });
    }

    if (m_HasMoveTarget)
    {
        DrawCombatMoveMarker(heightfield, m_MoveTarget, Color{ 80, 255, 120, 255 });
    }

    g_RegionView.End3D();

    DrawOutlinedText(g_font, "Combat  Click/hold to aim, release to set facing", { 4.0f, 4.0f }, g_fontDrawSize, 1, WHITE);
    DrawOutlinedText(g_smallFont, "Quick click terrain to move  Q/E:rotate  Esc:title", { 4.0f, 16.0f }, g_smallFontDrawSize, 1, WHITE);

    float labelY = 30.0f;
    for (int typeIndex = 0; typeIndex < 4; ++typeIndex)
    {
        const auto type = static_cast<CombatUnitType>(typeIndex);
        const CombatUnitFormation& formation = GetCombatUnitFormation(type);
        const string label = string(CombatUnitTypeName(type)) + ": "
            + to_string(formation.m_SoldierCount) + " ("
            + to_string(formation.m_Rows) + "x" + to_string(formation.m_Columns) + ")";
        DrawOutlinedText(g_smallFont, label, { 4.0f, labelY }, g_smallFontDrawSize, 1, Color{ 210, 210, 210, 255 });
        labelY += 10.0f;
    }

    if (m_SelectedUnitIndex >= 0 && m_SelectedUnitIndex < static_cast<int>(m_Units.size()))
    {
        const CombatUnitInstance& selectedUnit = m_Units[static_cast<size_t>(m_SelectedUnitIndex)];
        const string selectedText = string("Selected: ") + CombatUnitTypeName(selectedUnit.m_Type)
            + " (" + LittlePeopleArmyName(selectedUnit.m_Army) + ")";
        DrawOutlinedText(g_smallFont, selectedText, { 4.0f, labelY + 4.0f }, g_smallFontDrawSize, 1, Color{ 255, 230, 90, 255 });
    }
}