#include <string>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "CombatUnits.h"
#include "GameGlobals.h"
#include "LittlePeopleSprites.h"

#include <vector>
#include "RegionMinimap.h"
#include "RegionUILayout.h"
#include "RegionTerrainMesh.h"
#include "RegionView.h"
#include "raymath.h"

using namespace std;

namespace
{
    constexpr double kGestureTimeThreshold = 0.25;

    Vector2 GetScaledMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        const float inputScale = g_Engine->GetInputScale();
        mouse.x /= inputScale;
        mouse.y /= inputScale;
        return mouse;
    }

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

    Color LittlePeopleArmyColor(LittlePeopleArmy army)
    {
        switch (army)
        {
        case LittlePeopleArmy::White:
            return Color{ 235, 235, 235, 255 };
        case LittlePeopleArmy::Blue:
            return Color{ 80, 140, 255, 255 };
        case LittlePeopleArmy::Red:
            return Color{ 230, 70, 70, 255 };
        case LittlePeopleArmy::Green:
            return Color{ 70, 200, 90, 255 };
        default:
            return Color{ 200, 200, 200, 255 };
        }
    }

    bool IsGestureHold(double pressTime)
    {
        return (GetTime() - pressTime) >= kGestureTimeThreshold;
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
    m_Projectiles.clear();

    const CombatUnitInstance demoUnits[] = {
        { 0, CombatUnitType::Swordsmen, Vector3{ 36.0f, 0.0f, 44.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 1, CombatUnitType::Archers, Vector3{ 68.0f, 0.0f, 44.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 2, CombatUnitType::Knights, Vector3{ 100.0f, 0.0f, 44.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 3, CombatUnitType::Catapult, Vector3{ 118.0f, 0.0f, 44.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 4, CombatUnitType::Swordsmen, Vector3{ 36.0f, 0.0f, 84.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 5, CombatUnitType::Archers, Vector3{ 68.0f, 0.0f, 84.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 6, CombatUnitType::Knights, Vector3{ 100.0f, 0.0f, 84.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 7, CombatUnitType::Catapult, Vector3{ 118.0f, 0.0f, 84.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
    };

    for (const CombatUnitInstance& unit : demoUnits)
    {
        CombatUnitInstance initializedUnit = unit;
        InitCombatUnitHealth(initializedUnit);
        m_Units.push_back(initializedUnit);
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

    if (IsPointInRegionSidePanel(mousePosition))
    {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        m_LeftMousePressTime = GetTime();
        m_LeftMousePressPosition = mousePosition;
        m_IsGestureHold = false;
        m_GestureUnitIndex = -1;
        m_GestureStartedOnUnit = false;
        m_PendingQuickClick = false;
        m_HasGestureFacingTarget = false;

        const int pickedUnitIndex = PickCombatUnitMarkerAtMouse(camera, heightfield, m_Units, mousePosition);

        if (pickedUnitIndex >= 0)
        {
            const CombatUnitInstance& pickedUnit = m_Units[static_cast<size_t>(pickedUnitIndex)];

            if (m_SelectedUnitIndex >= 0
                && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
                && CanCombatUnitAttack(m_Units[static_cast<size_t>(m_SelectedUnitIndex)])
                && AreCombatUnitsHostile(m_Units[static_cast<size_t>(m_SelectedUnitIndex)], pickedUnit))
            {
                CombatUnitInstance& selectedUnit = m_Units[static_cast<size_t>(m_SelectedUnitIndex)];
                BeginCombatUnitAttackMove(selectedUnit, pickedUnitIndex, m_Units);
                m_MoveTarget = GetCombatApproachPosition(selectedUnit, pickedUnit);
                m_HasMoveTarget = true;
                m_HasHoverTarget = false;
                return;
            }

            if (IsCombatUnitPlayerControlled(pickedUnit))
            {
                m_SelectedUnitIndex = pickedUnitIndex;
                m_GestureUnitIndex = pickedUnitIndex;
                m_GestureStartedOnUnit = true;
                m_PendingQuickClick = true;
                m_HasMoveTarget = false;
            }

            return;
        }

        if (m_SelectedUnitIndex >= 0 && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && IsCombatUnitPlayerControlled(m_Units[static_cast<size_t>(m_SelectedUnitIndex)])
            && IsCombatUnitAlive(m_Units[static_cast<size_t>(m_SelectedUnitIndex)]))
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
        && m_GestureUnitIndex < static_cast<int>(m_Units.size())
        && IsCombatUnitPlayerControlled(m_Units[static_cast<size_t>(m_GestureUnitIndex)]))
    {
        if (IsGestureHold(m_LeftMousePressTime))
        {
            m_IsGestureHold = true;
            m_PendingQuickClick = false;

            const Ray ray = GetCombatMouseRay(camera);
            Vector3 terrainHit{};
            if (RaycastCombatTerrain(ray, heightfield, terrainHit))
            {
                FaceCombatUnitToward(m_Units[static_cast<size_t>(m_GestureUnitIndex)], terrainHit);
                m_GestureFacingTarget = terrainHit;
                m_HasGestureFacingTarget = true;
            }
        }
    }
    else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        m_HasGestureFacingTarget = false;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        if (m_PendingQuickClick && !m_IsGestureHold && !m_GestureStartedOnUnit
            && m_GestureUnitIndex >= 0 && m_GestureUnitIndex < static_cast<int>(m_Units.size())
            && IsCombatUnitPlayerControlled(m_Units[static_cast<size_t>(m_GestureUnitIndex)])
            && IsCombatUnitAlive(m_Units[static_cast<size_t>(m_GestureUnitIndex)]))
        {
            BeginCombatUnitMove(m_Units[static_cast<size_t>(m_GestureUnitIndex)], m_PendingTerrainTarget);
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

    float minimapWorldX = 0.0f;
    float minimapWorldZ = 0.0f;
    bool minimapClicked = false;
    g_RegionMinimap.HandleInput(GetScaledMousePosition(), minimapWorldX, minimapWorldZ, minimapClicked);
    if (minimapClicked)
    {
        g_RegionView.SetLookAtPosition(minimapWorldX, minimapWorldZ);
    }

    g_RegionView.Update(heightfield);

    if (heightfield)
    {
        const float deltaTime = GetFrameTime();
        UpdateCombatUnitsAttackOrders(m_Units, *heightfield);
        UpdateCombatUnitsFormationRecovery(m_Units);
        UpdateCombatUnitsMovement(m_Units, deltaTime);
        UpdateCombatProjectiles(m_Projectiles, m_Units, deltaTime);
        UpdateCombatUnitsCombat(m_Units, m_Projectiles, *heightfield, deltaTime);
        UpdateCombatUnitsRetaliationDelays(m_Units, deltaTime);

        if (m_SelectedUnitIndex >= 0
            && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && !IsCombatUnitAlive(m_Units[static_cast<size_t>(m_SelectedUnitIndex)]))
        {
            m_SelectedUnitIndex = -1;
            m_HasMoveTarget = false;
            m_HasHoverTarget = false;
        }

        if (m_HasMoveTarget && m_SelectedUnitIndex >= 0
            && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && !m_Units[static_cast<size_t>(m_SelectedUnitIndex)].m_IsMoving)
        {
            m_HasMoveTarget = false;
        }

        UpdateTerrainTargetPreview(*heightfield);
        HandleCombatInput(*heightfield);
    }

    if (IsKeyPressed(KEY_R))
    {
        if (RegionData* region = g_GameDatabase.GetActiveRegion())
        {
            g_GameDatabase.RegenerateRegionHeightfield(region->m_Id);
            m_HasMoveTarget = false;
            m_HasHoverTarget = false;
        }
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
        DrawRegionSidePanelBackground();
        DrawRegionSidePanelOutlinedParagraph("Combat (no region terrain)", GetRegionSidePanelTextBounds().y,
            g_fontDrawSize, WHITE);
        return;
    }

    const RegionHeightfield& heightfield = region->m_Heightfield;
    g_RegionTerrainMesh.SetHeightfield(&heightfield);
    g_RegionTerrainMesh.SetFlatShaded(true);
    g_RegionTerrainMesh.RebuildIfNeeded();

    g_RegionView.Begin3D();
    g_RegionTerrainMesh.Draw();

    std::vector<LittlePersonBillboardDrawRequest> billboardDrawRequests;
    billboardDrawRequests.reserve(128);
    for (int unitIndex = 0; unitIndex < static_cast<int>(m_Units.size()); ++unitIndex)
    {
        const CombatUnitInstance& unit = m_Units[static_cast<size_t>(unitIndex)];
        if (!IsCombatUnitAlive(unit))
        {
            continue;
        }

        const bool selected = (unitIndex == m_SelectedUnitIndex);
        AppendCombatUnitBillboardDrawRequests(
            g_RegionView.GetCamera(),
            heightfield,
            unit,
            selected,
            billboardDrawRequests);
        DrawCombatUnit(
            g_RegionView.GetCamera(),
            heightfield,
            unit,
            0,
            selected
        );
    }

    DrawLittlePeopleBillboardsSorted(g_RegionView.GetCamera(), billboardDrawRequests);

    DrawCombatProjectiles(m_Projectiles);

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
        const Color moveMarkerColor = (m_SelectedUnitIndex >= 0
            && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && m_Units[static_cast<size_t>(m_SelectedUnitIndex)].m_IsMoving)
            ? Color{ 80, 255, 120, 200 }
            : Color{ 80, 255, 120, 255 };
        DrawCombatMoveMarker(heightfield, m_MoveTarget, moveMarkerColor);
    }

    g_RegionView.End3D();

    std::vector<RegionMinimapMarker> minimapMarkers;
    minimapMarkers.reserve(m_Units.size());
    for (const CombatUnitInstance& unit : m_Units)
    {
        if (!IsCombatUnitAlive(unit))
        {
            continue;
        }

        minimapMarkers.push_back(RegionMinimapMarker{
            unit.m_Anchor.x,
            unit.m_Anchor.z,
            LittlePeopleArmyColor(unit.m_Army)
        });
    }

    DrawRegionSidePanelBackground();

    g_RegionMinimap.Draw(
        heightfield,
        region->m_HeightfieldSeed,
        g_RegionView.GetCamera(),
        &minimapMarkers);

    DrawCombatUnitMarkers(
        g_RegionView.GetCamera(),
        heightfield,
        m_Units,
        m_SelectedUnitIndex);

    float panelY = GetRegionSidePanelTextBounds().y;
    panelY = DrawRegionSidePanelOutlinedParagraph("Combat", panelY, g_fontDrawSize, WHITE);
    panelY += 2.0f;
    panelY = DrawRegionSidePanelOutlinedParagraph("Red units only. Click markers to select or attack.", panelY,
        g_smallFontDrawSize, Color{ 220, 220, 220, 255 });
    panelY += 4.0f;
    panelY = DrawRegionSidePanelOutlinedParagraph(
        "Terrain: move. Hold: aim. WASD: pan. Wheel: zoom. Minimap: go. Esc: title.",
        panelY, g_smallFontDrawSize, Color{ 180, 185, 195, 255 });
    panelY += 6.0f;

    for (int typeIndex = 0; typeIndex < 4; ++typeIndex)
    {
        const auto type = static_cast<CombatUnitType>(typeIndex);
        const CombatUnitFormation& formation = GetCombatUnitFormation(type);
        const string label = string(CombatUnitTypeName(type)) + ": "
            + to_string(formation.m_SoldierCount) + " ("
            + to_string(formation.m_Rows) + "x" + to_string(formation.m_Columns) + ")";
        panelY = DrawRegionSidePanelOutlinedLine(label, panelY, Color{ 210, 210, 210, 255 });
    }

    if (m_SelectedUnitIndex >= 0 && m_SelectedUnitIndex < static_cast<int>(m_Units.size()))
    {
        panelY += 4.0f;
        const CombatUnitInstance& selectedUnit = m_Units[static_cast<size_t>(m_SelectedUnitIndex)];
        const int maxHP = GetCombatUnitMaxHitPoints(selectedUnit);
        const int livingSoldiers = GetCombatUnitLivingSoldierCount(selectedUnit);
        const int maxSoldiers = GetCombatUnitSoldierCount(selectedUnit.m_Type);
        const string selectedText = string("Selected: ") + CombatUnitTypeName(selectedUnit.m_Type)
            + " (" + LittlePeopleArmyName(selectedUnit.m_Army) + ")";
        const string hpText = "HP: " + to_string(GetCombatUnitCurrentHitPoints(selectedUnit)) + "/" + to_string(maxHP)
            + "  Figs: " + to_string(livingSoldiers) + "/" + to_string(maxSoldiers)
            + "  Dmg: " + to_string(GetCombatUnitAttackDamage(selectedUnit));

        panelY = DrawRegionSidePanelOutlinedLine(selectedText, panelY, Color{ 255, 230, 90, 255 });
        panelY = DrawRegionSidePanelOutlinedParagraph(hpText, panelY, g_smallFontDrawSize, Color{ 220, 220, 220, 255 });
        panelY += 2.0f;

        const Rectangle textBounds = GetRegionSidePanelTextBounds();
        constexpr int kHealthBarHeight = 5;
        const int barWidth = static_cast<int>(textBounds.width);
        const int barX = static_cast<int>(textBounds.x);
        const int barY = static_cast<int>(panelY);
        const float healthFraction = maxHP > 0
            ? static_cast<float>(GetCombatUnitCurrentHitPoints(selectedUnit)) / static_cast<float>(maxHP)
            : 0.0f;
        DrawRectangle(barX, barY, barWidth, kHealthBarHeight, Color{ 40, 40, 48, 255 });
        DrawRectangle(barX, barY, static_cast<int>(barWidth * healthFraction), kHealthBarHeight, Color{ 80, 200, 90, 255 });
        DrawRectangleLines(barX, barY, barWidth, kHealthBarHeight, Color{ 120, 120, 130, 255 });
    }
}