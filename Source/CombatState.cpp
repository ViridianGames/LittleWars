#include <algorithm>
#include <cmath>
#include <string>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "CombatEnvironment.h"
#include "CombatUnits.h"
#include "GameGlobals.h"
#include "LittlePeopleSprites.h"
#include "OverworldMap.h"

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
        // Same mapping as combat raycast / virtual render stretch.
        return GetCombatScaledMousePosition();
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
    m_MoveTargetIsAttack = false;
    m_HasHoverTarget = false;
    m_GestureUnitIndex = -1;
    m_GestureStartedOnUnit = false;
    m_IsGestureHold = false;
    m_PendingQuickClick = false;
    m_HasGestureFacingTarget = false;
    m_Projectiles.clear();

    // Red army faces north toward the demo wall (~z=64) for catapult demolition tests.
    const CombatUnitInstance demoUnits[] = {
        { 0, CombatUnitType::Swordsmen, Vector3{ 24.0f, 0.0f, 52.0f }, LittlePeopleArmy::White, LittlePeopleDirection::South },
        { 1, CombatUnitType::Swordsmen, Vector3{ 44.0f, 0.0f, 52.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 2, CombatUnitType::Swordsmen, Vector3{ 58.0f, 0.0f, 40.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 3, CombatUnitType::Swordsmen, Vector3{ 84.0f, 0.0f, 52.0f }, LittlePeopleArmy::Green, LittlePeopleDirection::South },
        { 4, CombatUnitType::Archers, Vector3{ 24.0f, 0.0f, 72.0f }, LittlePeopleArmy::White, LittlePeopleDirection::South },
        { 5, CombatUnitType::Archers, Vector3{ 44.0f, 0.0f, 72.0f }, LittlePeopleArmy::Blue, LittlePeopleDirection::South },
        { 6, CombatUnitType::Archers, Vector3{ 70.0f, 0.0f, 40.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
        { 7, CombatUnitType::Archers, Vector3{ 84.0f, 0.0f, 72.0f }, LittlePeopleArmy::Green, LittlePeopleDirection::South },
        { 8, CombatUnitType::Catapult, Vector3{ 64.0f, 0.0f, 38.0f }, LittlePeopleArmy::Red, LittlePeopleDirection::North },
    };

    for (const CombatUnitInstance& unit : demoUnits)
    {
        CombatUnitInstance initializedUnit = unit;
        InitCombatUnitHealth(initializedUnit);
        m_Units.push_back(initializedUnit);
    }
}

namespace
{
    bool IsCampaignCombatVisit()
    {
        return g_OverworldMap.IsGenerated() && !g_GameDatabase.m_Players.empty();
    }

    void SetupCombatSceneForActiveRegion(std::vector<CombatUnitInstance>& units, bool spawnDemoContent)
    {
        units.clear();

        RegionData* region = g_GameDatabase.GetActiveRegion();
        if (!region || !region->m_Heightfield.m_Generated)
        {
            g_CombatEnvironment.Clear();
            return;
        }

        if (spawnDemoContent)
        {
            InitializeDemoCombatEnvironment(region->m_Heightfield, region->m_HeightfieldSeed);
        }
        else
        {
            InitializeRegionCombatEnvironment(
                region->m_Heightfield,
                region->m_HeightfieldSeed,
                region->m_Resource);
        }
    }

    int CountArmyLivingSoldiers(const std::vector<CombatUnitInstance>& units, LittlePeopleArmy army)
    {
        int total = 0;
        for (const CombatUnitInstance& unit : units)
        {
            if (unit.m_Army == army && IsCombatUnitAlive(unit))
            {
                total += GetCombatUnitLivingSoldierCount(unit);
            }
        }
        return total;
    }

    // Overworld casualties are per combat *group* (1 group == 1 overworld unit), not per figure HP.
    // Spawn may cap groups (kMaxGroupsPerType / kMaxGroupsPerSide); unspawned reserves return intact.
    struct ArmyGroupCounts
    {
        int swordsmen = 0;
        int archers = 0;
        int knights = 0;
        int catapults = 0;
    };

    void CountArmyGroupsByType(
        const std::vector<CombatUnitInstance>& units,
        LittlePeopleArmy army,
        ArmyGroupCounts& living,
        ArmyGroupCounts& spawned)
    {
        living = {};
        spawned = {};
        for (const CombatUnitInstance& unit : units)
        {
            if (unit.m_Army != army)
            {
                continue;
            }
            int* livePtr = nullptr;
            int* spawnPtr = nullptr;
            switch (unit.m_Type)
            {
            case CombatUnitType::Swordsmen:
                livePtr = &living.swordsmen;
                spawnPtr = &spawned.swordsmen;
                break;
            case CombatUnitType::Archers:
                livePtr = &living.archers;
                spawnPtr = &spawned.archers;
                break;
            case CombatUnitType::Knights:
                livePtr = &living.knights;
                spawnPtr = &spawned.knights;
                break;
            case CombatUnitType::Catapult:
                livePtr = &living.catapults;
                spawnPtr = &spawned.catapults;
                break;
            }
            if (spawnPtr != nullptr)
            {
                ++(*spawnPtr);
                if (IsCombatUnitAlive(unit) && livePtr != nullptr)
                {
                    ++(*livePtr);
                }
            }
        }
    }

    // start - spawned + living = reserves that never deployed + surviving deployed groups.
    int RemainingOverworldUnits(int startCount, int spawnedGroups, int livingGroups)
    {
        return std::max(0, startCount - spawnedGroups + livingGroups);
    }

    int ScaleRemaining(int startCount, float ratio)
    {
        if (startCount <= 0)
        {
            return 0;
        }
        return std::max(0, static_cast<int>(std::lround(static_cast<float>(startCount) * std::clamp(ratio, 0.0f, 1.0f))));
    }

    // Cap groups so battles stay readable (4–8 groups per design doc).
    constexpr int kMaxGroupsPerType = 3;
    constexpr int kMaxGroupsPerSide = 6;

    void AppendArmyGroups(
        std::vector<CombatUnitInstance>& units,
        int& nextId,
        LittlePeopleArmy army,
        LittlePeopleDirection facing,
        float baseX,
        float baseZ,
        float zStep,
        int swordsmen,
        int archers,
        int knights,
        int catapults)
    {
        auto spawn = [&](CombatUnitType type, int count, float xSpread) {
            const int groups = std::min(kMaxGroupsPerType, count);
            for (int i = 0; i < groups; ++i)
            {
                if (static_cast<int>(units.size()) >= kMaxGroupsPerSide * 2)
                {
                    return;
                }
                CombatUnitInstance unit;
                unit.m_Id = nextId++;
                unit.m_Type = type;
                unit.m_Army = army;
                unit.m_Facing = facing;
                unit.m_Anchor = Vector3{
                    baseX + (i - groups * 0.5f) * xSpread,
                    0.0f,
                    baseZ
                };
                InitCombatUnitHealth(unit);
                units.push_back(unit);
            }
        };

        int spawned = 0;
        auto trySpawn = [&](CombatUnitType type, int count, float xSpread, float z) {
            if (count <= 0 || spawned >= kMaxGroupsPerSide)
            {
                return;
            }
            const int before = static_cast<int>(units.size());
            baseZ = z;
            spawn(type, count, xSpread);
            spawned += static_cast<int>(units.size()) - before;
        };

        trySpawn(CombatUnitType::Swordsmen, swordsmen, 10.0f, baseZ);
        trySpawn(CombatUnitType::Archers, archers, 10.0f, baseZ + zStep);
        trySpawn(CombatUnitType::Knights, knights, 12.0f, baseZ + zStep * 0.5f);
        trySpawn(CombatUnitType::Catapult, catapults, 8.0f, baseZ + zStep * 1.5f);

        // Always give each side at least one group so the battle is playable.
        if (spawned == 0)
        {
            CombatUnitInstance unit;
            unit.m_Id = nextId++;
            unit.m_Type = CombatUnitType::Swordsmen;
            unit.m_Army = army;
            unit.m_Facing = facing;
            unit.m_Anchor = Vector3{ baseX, 0.0f, baseZ };
            InitCombatUnitHealth(unit);
            units.push_back(unit);
        }
    }
}

bool CombatState::IsCampaignBattleActive() const
{
    return g_GameDatabase.m_PendingBattle.m_Active && !m_BattleEnded;
}

bool CombatState::IsCampaignInspectOnly() const
{
    return IsCampaignCombatVisit() && !g_GameDatabase.m_PendingBattle.m_Active;
}

void CombatState::InitializeCampaignBattleUnits()
{
    m_Units.clear();
    const PendingBattle& battle = g_GameDatabase.m_PendingBattle;
    if (!battle.m_Active)
    {
        return;
    }

    m_BattleStartAtkS = battle.m_AtkSwordsmen;
    m_BattleStartAtkA = battle.m_AtkArchers;
    m_BattleStartAtkK = battle.m_AtkKnights;
    m_BattleStartAtkC = battle.m_AtkCatapults;
    m_BattleStartDefS = battle.m_DefSwordsmen;
    m_BattleStartDefA = battle.m_DefArchers;
    m_BattleStartDefK = battle.m_DefKnights;
    m_BattleStartDefC = battle.m_DefCatapults;

    // Always use distinct armies so 8-player palette wrap can't make both sides friendly.
    const LittlePeopleArmy atkArmy = LittlePeopleArmy::Blue;
    const LittlePeopleArmy defArmy = LittlePeopleArmy::Red;

    // Human always controls the attacker army in on-map conquest battles.
    SetPlayerCombatArmy(atkArmy);

    int nextId = 0;
    // Attacker approaches from south; defender holds the north.
    AppendArmyGroups(
        m_Units, nextId, atkArmy, LittlePeopleDirection::North,
        64.0f, 90.0f, -8.0f,
        battle.m_AtkSwordsmen, battle.m_AtkArchers, battle.m_AtkKnights, battle.m_AtkCatapults);
    AppendArmyGroups(
        m_Units, nextId, defArmy, LittlePeopleDirection::South,
        64.0f, 36.0f, 8.0f,
        battle.m_DefSwordsmen, battle.m_DefArchers, battle.m_DefKnights, battle.m_DefCatapults);
}

void CombatState::ResolveCampaignBattle(bool attackerWon, bool retreated)
{
    if (!g_GameDatabase.m_PendingBattle.m_Active || m_BattleEnded)
    {
        return;
    }

    PendingBattle& battle = g_GameDatabase.m_PendingBattle;
    const LittlePeopleArmy atkArmy = GetPlayerCombatArmy();
    LittlePeopleArmy defArmy = LittlePeopleArmy::White;
    for (const CombatUnitInstance& unit : m_Units)
    {
        if (unit.m_Army != atkArmy)
        {
            defArmy = unit.m_Army;
            break;
        }
    }

    ArmyGroupCounts atkLivingGroups{};
    ArmyGroupCounts atkSpawnedGroups{};
    ArmyGroupCounts defLivingGroups{};
    ArmyGroupCounts defSpawnedGroups{};
    CountArmyGroupsByType(m_Units, atkArmy, atkLivingGroups, atkSpawnedGroups);
    CountArmyGroupsByType(m_Units, defArmy, defLivingGroups, defSpawnedGroups);

    // Remaining overworld units = unspawned reserves + living combat groups of that type.
    // Do NOT scale by figure HP ratio — that wrongly taxes every type when any figures die.
    battle.m_AtkSwordsmen = RemainingOverworldUnits(m_BattleStartAtkS, atkSpawnedGroups.swordsmen, atkLivingGroups.swordsmen);
    battle.m_AtkArchers = RemainingOverworldUnits(m_BattleStartAtkA, atkSpawnedGroups.archers, atkLivingGroups.archers);
    battle.m_AtkKnights = RemainingOverworldUnits(m_BattleStartAtkK, atkSpawnedGroups.knights, atkLivingGroups.knights);
    battle.m_AtkCatapults = RemainingOverworldUnits(m_BattleStartAtkC, atkSpawnedGroups.catapults, atkLivingGroups.catapults);
    battle.m_DefSwordsmen = RemainingOverworldUnits(m_BattleStartDefS, defSpawnedGroups.swordsmen, defLivingGroups.swordsmen);
    battle.m_DefArchers = RemainingOverworldUnits(m_BattleStartDefA, defSpawnedGroups.archers, defLivingGroups.archers);
    battle.m_DefKnights = RemainingOverworldUnits(m_BattleStartDefK, defSpawnedGroups.knights, defLivingGroups.knights);
    battle.m_DefCatapults = RemainingOverworldUnits(m_BattleStartDefC, defSpawnedGroups.catapults, defLivingGroups.catapults);

    if (retreated)
    {
        // Desertion / disorder on retreat: attacker keeps at most 60% of remaining, defender at least 70% of start.
        battle.m_AtkSwordsmen = std::min(battle.m_AtkSwordsmen, ScaleRemaining(m_BattleStartAtkS, 0.6f));
        battle.m_AtkArchers = std::min(battle.m_AtkArchers, ScaleRemaining(m_BattleStartAtkA, 0.6f));
        battle.m_AtkKnights = std::min(battle.m_AtkKnights, ScaleRemaining(m_BattleStartAtkK, 0.6f));
        battle.m_AtkCatapults = std::min(battle.m_AtkCatapults, ScaleRemaining(m_BattleStartAtkC, 0.6f));
        battle.m_DefSwordsmen = std::max(battle.m_DefSwordsmen, ScaleRemaining(m_BattleStartDefS, 0.7f));
        battle.m_DefArchers = std::max(battle.m_DefArchers, ScaleRemaining(m_BattleStartDefA, 0.7f));
        battle.m_DefKnights = std::max(battle.m_DefKnights, ScaleRemaining(m_BattleStartDefK, 0.7f));
        battle.m_DefCatapults = std::max(battle.m_DefCatapults, ScaleRemaining(m_BattleStartDefC, 0.7f));
        attackerWon = false;
    }

    battle.m_AttackerWon = attackerWon && !retreated;
    battle.m_Retreated = retreated;
    battle.m_Resolved = true;
    m_BattleEnded = true;

    g_GameDatabase.FinalizePendingBattle(g_OverworldMap);
    g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
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

    const bool campaignVisit = IsCampaignCombatVisit();
    const bool campaignBattle = g_GameDatabase.m_PendingBattle.m_Active;
    SetupCombatSceneForActiveRegion(m_Units, !campaignVisit);

    m_SelectedUnitIndex = -1;
    m_HasMoveTarget = false;
    m_MoveTargetIsAttack = false;
    m_HasHoverTarget = false;
    m_GestureUnitIndex = -1;
    m_GestureStartedOnUnit = false;
    m_IsGestureHold = false;
    m_PendingQuickClick = false;
    m_HasGestureFacingTarget = false;
    m_Projectiles.clear();
    m_BattleEnded = false;

    if (campaignBattle)
    {
        InitializeCampaignBattleUnits();
    }
    else if (!campaignVisit)
    {
        SetPlayerCombatArmy(LittlePeopleArmy::Red);
        InitializeDemoUnits();
    }
    else
    {
        // Inspect mode — no units.
        m_Units.clear();
    }
}

void CombatState::OnExit()
{

}

bool CombatState::HasSelectedPlayerUnit() const
{
    if (m_SelectedUnitIndex < 0 || m_SelectedUnitIndex >= static_cast<int>(m_Units.size()))
    {
        return false;
    }

    const CombatUnitInstance& unit = m_Units[static_cast<size_t>(m_SelectedUnitIndex)];
    return IsCombatUnitPlayerControlled(unit) && IsCombatUnitAlive(unit);
}

CombatUnitInstance* CombatState::GetSelectedPlayerUnit()
{
    if (!HasSelectedPlayerUnit())
    {
        return nullptr;
    }

    return &m_Units[static_cast<size_t>(m_SelectedUnitIndex)];
}

void CombatState::UpdateTerrainTargetPreview(const RegionHeightfield& heightfield)
{
    m_HasHoverTarget = false;

    if (!HasSelectedPlayerUnit())
    {
        return;
    }

    if (IsPointInRegionSidePanel(GetCombatScaledMousePosition()))
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

    // Left-click: select / start move-or-face gesture.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        m_LeftMousePressTime = GetTime();
        m_IsGestureHold = false;
        m_GestureUnitIndex = -1;
        m_GestureStartedOnUnit = false;
        m_PendingQuickClick = false;
        m_HasGestureFacingTarget = false;

        const int pickedUnitIndex = PickCombatUnitMarkerAtMouse(camera, heightfield, m_Units, mousePosition);
        if (pickedUnitIndex >= 0)
        {
            const CombatUnitInstance& pickedUnit = m_Units[static_cast<size_t>(pickedUnitIndex)];
            if (IsCombatUnitPlayerControlled(pickedUnit) && IsCombatUnitAlive(pickedUnit))
            {
                m_SelectedUnitIndex = pickedUnitIndex;
                m_GestureUnitIndex = pickedUnitIndex;
                m_GestureStartedOnUnit = true;
                m_PendingQuickClick = true;
                m_HasMoveTarget = false;
                m_MoveTargetIsAttack = false;
                m_HasHoverTarget = false;
            }
            return;
        }

        if (HasSelectedPlayerUnit())
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

    // Hold left: face formation toward the cursor (does not issue a move).
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)
        && m_GestureUnitIndex >= 0
        && m_GestureUnitIndex < static_cast<int>(m_Units.size())
        && IsCombatUnitPlayerControlled(m_Units[static_cast<size_t>(m_GestureUnitIndex)])
        && IsCombatUnitAlive(m_Units[static_cast<size_t>(m_GestureUnitIndex)]))
    {
        if (IsGestureHold(m_LeftMousePressTime))
        {
            m_IsGestureHold = true;
            m_PendingQuickClick = false; // release must not issue a move
            m_HasMoveTarget = false;
            m_MoveTargetIsAttack = false;

            const Ray ray = GetCombatMouseRay(camera);
            Vector3 terrainHit{};
            if (RaycastCombatTerrain(ray, heightfield, terrainHit))
            {
                FaceCombatUnitToward(m_Units[static_cast<size_t>(m_GestureUnitIndex)], terrainHit);
                m_GestureFacingTarget = terrainHit;
                m_HasGestureFacingTarget = true;
                m_PendingTerrainTarget = terrainHit;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        // Move only on quick click+release (never after a hold-to-face).
        const bool shouldMove = m_PendingQuickClick
            && !m_IsGestureHold
            && !m_GestureStartedOnUnit
            && m_GestureUnitIndex >= 0
            && m_GestureUnitIndex < static_cast<int>(m_Units.size())
            && IsCombatUnitPlayerControlled(m_Units[static_cast<size_t>(m_GestureUnitIndex)])
            && IsCombatUnitAlive(m_Units[static_cast<size_t>(m_GestureUnitIndex)]);

        if (shouldMove)
        {
            CombatUnitInstance& gestureUnit = m_Units[static_cast<size_t>(m_GestureUnitIndex)];
            BeginCombatUnitMove(gestureUnit, m_PendingTerrainTarget);
            m_MoveTarget = m_PendingTerrainTarget;
            m_HasMoveTarget = true;
            m_MoveTargetIsAttack = false;
        }

        m_GestureUnitIndex = -1;
        m_IsGestureHold = false;
        m_PendingQuickClick = false;
        m_HasGestureFacingTarget = false;
    }

    // Right-click: attack enemy, or catapult-fire at ground.
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        CombatUnitInstance* selectedUnit = GetSelectedPlayerUnit();
        if (!selectedUnit || !CanCombatUnitAttack(*selectedUnit))
        {
            return;
        }

        const int pickedUnitIndex = PickCombatUnitMarkerAtMouse(camera, heightfield, m_Units, mousePosition);
        if (pickedUnitIndex >= 0)
        {
            const CombatUnitInstance& pickedUnit = m_Units[static_cast<size_t>(pickedUnitIndex)];
            if (AreCombatUnitsHostile(*selectedUnit, pickedUnit))
            {
                if (selectedUnit->m_Type == CombatUnitType::Catapult)
                {
                    const Vector3 aim = GetCombatApproachPosition(*selectedUnit, pickedUnit);
                    if (TryFireCatapultAt(
                            *selectedUnit,
                            m_SelectedUnitIndex,
                            pickedUnit.m_Anchor,
                            m_Projectiles,
                            heightfield))
                    {
                        m_MoveTarget = pickedUnit.m_Anchor;
                        m_HasMoveTarget = true;
                        m_MoveTargetIsAttack = true;
                    }
                    else
                    {
                        BeginCombatUnitAttackMove(*selectedUnit, pickedUnitIndex, m_Units);
                        m_MoveTarget = aim;
                        m_HasMoveTarget = true;
                        m_MoveTargetIsAttack = true;
                    }
                }
                else
                {
                    BeginCombatUnitAttackMove(*selectedUnit, pickedUnitIndex, m_Units);
                    m_MoveTarget = GetCombatApproachPosition(*selectedUnit, pickedUnit);
                    m_HasMoveTarget = true;
                    m_MoveTargetIsAttack = true;
                }
                m_HasHoverTarget = false;
            }
            return;
        }

        if (selectedUnit->m_Type == CombatUnitType::Catapult)
        {
            const Ray ray = GetCombatMouseRay(camera);
            Vector3 terrainHit{};
            if (RaycastCombatTerrain(ray, heightfield, terrainHit)
                && TryFireCatapultAt(
                    *selectedUnit,
                    m_SelectedUnitIndex,
                    terrainHit,
                    m_Projectiles,
                    heightfield))
            {
                m_MoveTarget = terrainHit;
                m_HasMoveTarget = true;
                m_MoveTargetIsAttack = true;
                m_HasHoverTarget = false;
            }
        }
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
        UpdateCombatProjectiles(m_Projectiles, m_Units, *heightfield, g_CombatEnvironment, deltaTime);
        UpdateCombatWallCubePhysics(*heightfield, deltaTime);
        UpdateCombatUnitsCombat(m_Units, m_Projectiles, *heightfield, deltaTime);
        UpdateCombatUnitsRetaliationDelays(m_Units, deltaTime);

        if (m_SelectedUnitIndex >= 0
            && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && !IsCombatUnitAlive(m_Units[static_cast<size_t>(m_SelectedUnitIndex)]))
        {
            m_SelectedUnitIndex = -1;
            m_HasMoveTarget = false;
            m_MoveTargetIsAttack = false;
            m_HasHoverTarget = false;
        }

        // Clear move markers once the unit stops; leave attack/fire markers briefly visible.
        if (m_HasMoveTarget
            && !m_MoveTargetIsAttack
            && m_SelectedUnitIndex >= 0
            && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && !m_Units[static_cast<size_t>(m_SelectedUnitIndex)].m_IsMoving)
        {
            m_HasMoveTarget = false;
        }

        UpdateTerrainTargetPreview(*heightfield);
        HandleCombatInput(*heightfield);
    }

    // Campaign battle: auto-resolve when one side is wiped out.
    if (IsCampaignBattleActive() && !m_BattleEnded)
    {
        const LittlePeopleArmy atkArmy = GetPlayerCombatArmy();
        const int atkLiving = CountArmyLivingSoldiers(m_Units, atkArmy);
        int defLiving = 0;
        for (const CombatUnitInstance& unit : m_Units)
        {
            if (unit.m_Army != atkArmy && IsCombatUnitAlive(unit))
            {
                defLiving += GetCombatUnitLivingSoldierCount(unit);
            }
        }

        if (defLiving <= 0 && atkLiving > 0)
        {
            ResolveCampaignBattle(true, false);
            return;
        }
        if (atkLiving <= 0)
        {
            ResolveCampaignBattle(false, false);
            return;
        }

        // Manual end: V = claim victory if any enemies left? No — only when clear.
        // Enter = auto-resolve remaining as strength comparison.
        if (IsKeyPressed(KEY_ENTER))
        {
            ResolveCampaignBattle(atkLiving >= defLiving, false);
            return;
        }
        if (IsKeyPressed(KEY_R))
        {
            // Retreat from campaign battle.
            ResolveCampaignBattle(false, true);
            return;
        }
    }

    if (IsKeyPressed(KEY_R) && !IsCampaignBattleActive())
    {
        if (RegionData* region = g_GameDatabase.GetActiveRegion())
        {
            g_GameDatabase.RegenerateRegionHeightfield(region->m_Id);
            const bool campaignVisit = IsCampaignCombatVisit();
            SetupCombatSceneForActiveRegion(m_Units, !campaignVisit);
            if (!campaignVisit)
            {
                SetPlayerCombatArmy(LittlePeopleArmy::Red);
                InitializeDemoUnits();
            }
            else if (g_GameDatabase.m_PendingBattle.m_Active)
            {
                InitializeCampaignBattleUnits();
            }
            else
            {
                m_Units.clear();
            }
            m_SelectedUnitIndex = -1;
            m_HasMoveTarget = false;
            m_MoveTargetIsAttack = false;
            m_HasHoverTarget = false;
            m_Projectiles.clear();
        }
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (IsCampaignBattleActive())
        {
            // Esc during battle = retreat.
            ResolveCampaignBattle(false, true);
            return;
        }

        // Return to the campaign map when one is loaded; otherwise fall back to the title screen.
        if (IsCampaignCombatVisit())
        {
            g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
        }
        else
        {
            g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
        }
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
    DrawCombatEnvironment(heightfield, g_CombatEnvironment);

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
        Color markerColor = m_MoveTargetIsAttack
            ? Color{ 255, 160, 60, 255 }
            : Color{ 80, 255, 120, 255 };
        if (!m_MoveTargetIsAttack
            && m_SelectedUnitIndex >= 0
            && m_SelectedUnitIndex < static_cast<int>(m_Units.size())
            && m_Units[static_cast<size_t>(m_SelectedUnitIndex)].m_IsMoving)
        {
            markerColor.a = 200;
        }
        DrawCombatMoveMarker(heightfield, m_MoveTarget, markerColor);
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
            GetLittlePeopleArmyColor(unit.m_Army)
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
    if (IsCampaignInspectOnly())
    {
        panelY = DrawRegionSidePanelOutlinedParagraph("County Map", panelY, g_fontDrawSize, WHITE);
        panelY += 2.0f;
        panelY = DrawRegionSidePanelOutlinedParagraph(
            "Inspect terrain only. WASD: pan. Wheel: zoom. Minimap: go. Esc: map. R: regen.",
            panelY, g_smallFontDrawSize, Color{ 180, 185, 195, 255 });
        panelY += 4.0f;
        if (RegionData* activeRegion = g_GameDatabase.GetActiveRegion())
        {
            panelY = DrawRegionSidePanelOutlinedLine(
                string("County ") + to_string(activeRegion->m_Id),
                panelY,
                Color{ 255, 230, 90, 255 });
            panelY = DrawRegionSidePanelOutlinedLine(
                string("Resource: ") + CountyResourceName(static_cast<CountyResource>(activeRegion->m_Resource)),
                panelY,
                Color{ 210, 210, 210, 255 });
        }
        return;
    }

    if (IsCampaignBattleActive())
    {
        panelY = DrawRegionSidePanelOutlinedParagraph("Battle!", panelY, g_fontDrawSize, WHITE);
        panelY += 2.0f;
        panelY = DrawRegionSidePanelOutlinedParagraph(
            "LMB: move. Hold LMB: face. RMB: attack. Enter: resolve. Esc/R: retreat.",
            panelY, g_smallFontDrawSize, Color{ 220, 220, 220, 255 });
        panelY += 4.0f;
        if (const RegionData* activeRegion = g_GameDatabase.GetActiveRegion())
        {
            panelY = DrawRegionSidePanelOutlinedLine(
                string("County ") + to_string(activeRegion->m_Id),
                panelY,
                Color{ 255, 230, 90, 255 });
        }
        const LittlePeopleArmy atkArmy = GetPlayerCombatArmy();
        panelY = DrawRegionSidePanelOutlinedLine(
            "Your troops: " + to_string(CountArmyLivingSoldiers(m_Units, atkArmy)),
            panelY,
            Color{ 120, 200, 255, 255 });
        int enemyLiving = 0;
        for (const CombatUnitInstance& unit : m_Units)
        {
            if (unit.m_Army != atkArmy && IsCombatUnitAlive(unit))
            {
                enemyLiving += GetCombatUnitLivingSoldierCount(unit);
            }
        }
        panelY = DrawRegionSidePanelOutlinedLine(
            "Enemy troops: " + to_string(enemyLiving),
            panelY,
            Color{ 255, 140, 120, 255 });
        panelY += 4.0f;
    }
    else
    {
        panelY = DrawRegionSidePanelOutlinedParagraph("Combat", panelY, g_fontDrawSize, WHITE);
        panelY += 2.0f;
        panelY = DrawRegionSidePanelOutlinedParagraph(
            "LMB: select / move. Hold LMB: face. RMB: attack (catapult: fire).",
            panelY,
            g_smallFontDrawSize,
            Color{ 220, 220, 220, 255 });
        panelY += 4.0f;
        panelY = DrawRegionSidePanelOutlinedParagraph(
            "WASD: pan. Wheel: zoom. Minimap: go. Esc: title. R: regen.",
            panelY, g_smallFontDrawSize, Color{ 180, 185, 195, 255 });
        panelY += 6.0f;
    }

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