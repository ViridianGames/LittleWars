#ifndef _COMBATSTATE_H_
#define _COMBATSTATE_H_

#include "Geist/State.h"

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
};

#endif