#ifndef _CASTLEDESIGNSTATE_H_
#define _CASTLEDESIGNSTATE_H_

#include "Geist/State.h"

class CastleDesignState : public State
{
public:
    CastleDesignState() {};
    ~CastleDesignState() override {};

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;

    void OnEnter() override;
    void OnExit() override;
};

#endif