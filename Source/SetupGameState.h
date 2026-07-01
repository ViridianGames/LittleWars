#ifndef _SETUPGAMESTATE_H_
#define _SETUPGAMESTATE_H_

#include "../Geist/Source/State.h"
#include "CampaignSetupConfig.h"
#include "GameGlobals.h"

#include <string>

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

private:
    CampaignSetup m_DraftSetup;
    int m_SelectedOption = 0;

    void AdjustOption(int delta);
    void StartCampaign();
    bool IsMouseOverOption(int optionIndex) const;
    bool IsMouseOverAdjustLeft(int optionIndex) const;
    bool IsMouseOverAdjustRight(int optionIndex) const;
    Rectangle GetOptionRect(int optionIndex) const;
    Rectangle GetAdjustLeftRect(int optionIndex) const;
    Rectangle GetAdjustRightRect(int optionIndex) const;
};

#endif