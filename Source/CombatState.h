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
   void UpdateTerrainTargetPreview(const RegionHeightfield& heightfield);
   void HandleCombatInput(const RegionHeightfield& heightfield);

   std::vector<CombatUnitInstance> m_Units;
   int m_SelectedUnitIndex = -1;
   bool m_HasMoveTarget = false;
   Vector3 m_MoveTarget{};
   bool m_HasHoverTarget = false;
   Vector3 m_HoverTarget{};

   int m_GestureUnitIndex = -1;
   bool m_GestureStartedOnUnit = false;
   bool m_IsGestureHold = false;
   bool m_PendingQuickClick = false;
   Vector3 m_PendingTerrainTarget{};
   double m_LeftMousePressTime = 0.0;
   Vector2 m_LeftMousePressPosition{};
   bool m_HasGestureFacingTarget = false;
   Vector3 m_GestureFacingTarget{};
};

#endif