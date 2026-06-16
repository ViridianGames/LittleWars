#ifndef _SETUPGAMESTATE_H_
#define _SETUPGAMESTATE_H_

#include "Geist/State.h"

class SetupGameState : public State
{
public:
    SetupGameState() {};
    ~SetupGameState() override {};

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;

    void OnEnter() override;
    void OnExit() override;
};

#endif