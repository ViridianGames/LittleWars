#ifndef _MAINSTATE_H_
#define _MAINSTATE_H_

#include "../Geist/Source/State.h"
#include "raylib.h"

#include <string>

class MainState : public State
{
public:
    MainState() {};
    ~MainState() override {};

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;

    void OnEnter() override;
    void OnExit() override;

private:
    void HandleMapSelection();
    void HandleResourceBarInput(int barX, int barWidth);
    void DrawPlayerResourceBar(int barX, int barY, int barWidth) const;
    void DrawResourceBarTooltip(int barX, int barWidth) const;
    Rectangle GetResourceBarSlotRect(int barX, int barWidth, int slotIndex) const;
    int GetResourceBarIconLeftX(int barX, int barWidth, int slotIndex) const;
    void DrawCountyInfo(int panelX, int panelY, int panelWidth) const;
    void DrawPlayerSummaries(int panelX, int panelY, int panelWidth) const;
    void DrawNextTurnButton() const;
    void HandleNextTurnButton();
    bool IsMouseOverNextTurnButton() const;
    void DrawTaskPanel(int panelX, int panelY, int panelWidth) const;
    void HandleTaskPanelInput(int panelX, int panelY, int panelWidth);
    Rectangle GetTaskRowRect(int panelX, int panelY, int taskIndex) const;
    int GetTaskPanelHeight() const;
    bool IsMouseOverTaskRow(int panelX, int panelY, int taskIndex) const;

    int m_SelectedRegionId = -1;
    int m_HoveredResourceBarSlot = -1;
    int m_HoveredTaskIndex = -1;
    std::string m_TaskStatusMessage;
    bool m_SelectedImpassable = false;
    unsigned char m_SelectedImpassableCellType = 0;
};

#endif