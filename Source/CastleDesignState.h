#ifndef _CASTLEDESIGNSTATE_H_
#define _CASTLEDESIGNSTATE_H_

#include "../Geist/Source/State.h"
#include "CastleParts.h"
#include "CombatUnits.h"
#include "raylib.h"

#include <vector>

class RegionHeightfield;

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

private:
    void HandleCastleDesignInput(const RegionHeightfield& heightfield);
    void UpdatePlacementPreview(const RegionHeightfield& heightfield);
    void HandleCastlePartToolbarInput();
    float LayoutPanelBeforeToolbar(float startY, bool draw) const;
    float DrawCastlePartToolbar(float startY);
    void DrawCastleParts3D(const RegionHeightfield& heightfield) const;
    void DrawReferenceSwordsmen(const RegionHeightfield& heightfield) const;
    void InitializeReferenceSwordsmen();
    bool TryGetTerrainHitUnderMouse(const RegionHeightfield& heightfield, Vector3& outHit) const;
    Vector3 SnapCastlePartPosition(CastlePartType type, int rotationDegrees, Vector3 rawPosition,
        int excludeIndex, bool& outSnappedToExisting) const;

    std::vector<CastlePartPlacement> m_Placements;
    CastlePartType m_SelectedPartType = CastlePartType::RoundTower;
    int m_HoveredButtonIndex = -1;
    bool m_PlacementToolActive = true;
    bool m_HasPlacementPreview = false;
    bool m_PlacementSnappedToExisting = false;
    Vector3 m_PlacementPreview{};
    int m_PlacementRotationDegrees = 0;
    int m_SelectedPlacementIndex = -1;
    bool m_IsDraggingPlacement = false;
    float m_ToolbarStartY = 0.0f;
    CombatUnitInstance m_ReferenceSwordsmen{};
};

#endif