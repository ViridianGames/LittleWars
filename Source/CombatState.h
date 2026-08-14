#ifndef _COMBATSTATE_H_
#define _COMBATSTATE_H_

#include "../Geist/Source/State.h"
#include "CombatUnits.h"

#include <vector>

class CombatState : public State
{
public:
   CombatState() {};
   ~CombatState() override {};

   void Init(const std::string& configfile) override;
   void Shutdown() override;
   void Update() override;
   void Draw() override;

   void OnEnter() override;
   void OnExit() override;

private:
   void InitializeDemoUnits();
   void InitializeCampaignBattleUnits();
   void UpdateTerrainTargetPreview(const RegionHeightfield& heightfield);
   void HandleCombatInput(const RegionHeightfield& heightfield);
   void ResolveCampaignBattle(bool attackerWon, bool retreated);
   bool IsCampaignBattleActive() const;
   bool IsCampaignInspectOnly() const;

   bool HasSelectedPlayerUnit() const;
   CombatUnitInstance* GetSelectedPlayerUnit();

   std::vector<CombatUnitInstance> m_Units;
   std::vector<CombatProjectile> m_Projectiles;
   int m_SelectedUnitIndex = -1;
   bool m_HasMoveTarget = false;
   Vector3 m_MoveTarget{};
   bool m_MoveTargetIsAttack = false; // true when marker is an attack / fire aim point
   bool m_HasHoverTarget = false;
   Vector3 m_HoverTarget{};

   // Left-click gesture: quick-click = move; hold = face formation toward point.
   int m_GestureUnitIndex = -1;
   bool m_GestureStartedOnUnit = false;
   bool m_IsGestureHold = false;
   bool m_PendingQuickClick = false;
   Vector3 m_PendingTerrainTarget{};
   double m_LeftMousePressTime = 0.0;
   bool m_HasGestureFacingTarget = false;
   Vector3 m_GestureFacingTarget{};

   // Snapshot of overworld army sizes at battle start (for casualty ratios).
   int m_BattleStartAtkS = 0;
   int m_BattleStartAtkA = 0;
   int m_BattleStartAtkK = 0;
   int m_BattleStartAtkC = 0;
   int m_BattleStartDefS = 0;
   int m_BattleStartDefA = 0;
   int m_BattleStartDefK = 0;
   int m_BattleStartDefC = 0;
   bool m_BattleEnded = false;
};

#endif
