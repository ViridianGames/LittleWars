#ifndef _MAINSTATE_H_
#define _MAINSTATE_H_

#include "../Geist/Source/State.h"
#include "raylib.h"

#include <string>

class Player;

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
    struct SideLayout
    {
        int m_PanelX = 0;
        int m_PanelW = 0;
        int m_TopY = 0;
        int m_TopH = 0;
        int m_MidY = 0;
        int m_MidH = 0;
        int m_BotY = 0;
        int m_BotH = 0;
    };

    SideLayout ComputeSideLayout() const;

    void HandleMapSelection();
    void HandleTopBarInput();
    void DrawTopBar() const;
    void DrawTopBarTooltip() const;
    Rectangle GetTopBarSlotRect(int slotIndex) const;

    void DrawSelectionPanel(const SideLayout& layout) const;
    void DrawTurnConsole(const SideLayout& layout) const;
    void DrawActionPanel(const SideLayout& layout) const;

    void HandleTurnLogInput(const SideLayout& layout);
    Rectangle GetTurnLogContentRect(const SideLayout& layout) const;
    Rectangle GetTurnLogScrollBarRect(const SideLayout& layout) const;
    void BuildTurnLogDisplayLines(
        const SideLayout& layout,
        std::vector<std::pair<std::string, Color>>& outLines) const;
    float GetTurnLogLineStep() const;

    void HandleActionPanelInput(const SideLayout& layout);
    void HandleNextTurnButton(const SideLayout& layout);
    Rectangle GetActionButtonRect(const SideLayout& layout, int actionIndex) const;
    Rectangle GetNextTurnButtonRect(const SideLayout& layout) const;
    bool IsMouseOverNextTurnButton(const SideLayout& layout) const;

    void TryPerformAction(int actionIndex);
    const char* GetActionTaskId(int actionIndex) const;
    const char* GetActionLabel(int actionIndex) const;

    void DrawVisitRegionButton(const SideLayout& layout) const;
    void HandleVisitRegionButton(const SideLayout& layout);
    Rectangle GetVisitRegionButtonRect(const SideLayout& layout) const;
    bool CanVisitSelectedRegion() const;

    // Optional full-map AI observer overlay (debug / all-AI).
    void HandleAiObserverInput();
    void DrawAiObserverPane() const;
    void CycleObserverFilter(int delta);
    const Player* GetWatchedPlayer() const;
    bool IsAllAiGame() const;

    int m_SelectedRegionId = -1;
    int m_HoveredTopBarSlot = -1;
    int m_HoveredActionIndex = -1;
    std::string m_TaskStatusMessage;
    bool m_SelectedImpassable = false;
    unsigned char m_SelectedImpassableCellType = 0;

    bool m_AiObserverOpen = false;
    int m_AiObserverFilter = -1; // -1 = all, else player id
    bool m_AiAutoPlay = false;
    float m_AiAutoPlayTimer = 0.0f;
    int m_WatchedPlayerId = 0;

    // Turn log scroll (pixels from top of content; 0 = oldest visible at top).
    float m_TurnLogScroll = 0.0f;
    bool m_TurnLogStickToBottom = true;
    bool m_TurnLogDragging = false;
    float m_TurnLogDragGrabY = 0.0f;
    int m_TurnLogLastEntryCount = 0;
};

#endif
