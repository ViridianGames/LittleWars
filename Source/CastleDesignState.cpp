#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/StateMachine.h"

#include "CastleParts.h"
#include "CombatUnits.h"
#include "GameGlobals.h"
#include "LittlePeopleSprites.h"
#include "RegionMinimap.h"
#include "RegionUILayout.h"
#include "RegionTerrainMesh.h"
#include "RegionView.h"
#include "raymath.h"
#include "rlgl.h"

using namespace std;

namespace
{
    constexpr int kCastlePartButtonHeight = 13;
    constexpr int kCastlePartButtonGap = 2;
    constexpr int kCastlePartButtonColumnGap = 2;
    constexpr int kCastlePartButtonColumns = 2;
    constexpr int kCastlePartButtonCount = static_cast<int>(CastlePartType::Count);
    constexpr float kParagraphLineSpacing = 1.2f;
    constexpr float kSnapGridSize = 0.5f;
    constexpr float kSnapSearchRadius = 3.0f;

    struct PlanarOffset
    {
        float m_X = 0.0f;
        float m_Z = 0.0f;
    };

    Vector2 GetScaledMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        const float inputScale = g_Engine->GetInputScale();
        mouse.x /= inputScale;
        mouse.y /= inputScale;
        return mouse;
    }

    const char* CastlePartTypeButtonLabel(CastlePartType type)
    {
        switch (type)
        {
        case CastlePartType::RoundTower:
            return "Round";
        case CastlePartType::SquareTower:
            return "Square";
        case CastlePartType::ShortWall:
            return "S.Wall";
        case CastlePartType::TallWall:
            return "T.Wall";
        case CastlePartType::Gate:
            return "Gate";
        case CastlePartType::Moat:
            return "Moat";
        default:
            return "?";
        }
    }

    std::vector<std::string> WrapSidePanelLines(const std::string& text, float maxWidth, float fontSize)
    {
        std::vector<std::string> lines;
        if (!g_smallFont)
        {
            return lines;
        }

        std::istringstream stream(text);
        std::string rawLine;
        while (std::getline(stream, rawLine))
        {
            std::stringstream lineStream(rawLine);
            std::string word;
            std::string line;
            while (lineStream >> word)
            {
                const std::string candidate = line.empty() ? word : (line + " " + word);
                const float candidateWidth = MeasureTextEx(*g_smallFont, candidate.c_str(), fontSize, 1.0f).x;
                if (candidateWidth > maxWidth && !line.empty())
                {
                    lines.push_back(line);
                    line = word;
                }
                else
                {
                    line = candidate;
                }
            }

            if (!line.empty())
            {
                lines.push_back(line);
            }
        }

        return lines;
    }

    float MeasureSidePanelParagraph(float y, const std::string& text, float fontSize)
    {
        const Rectangle textBounds = GetRegionSidePanelTextBounds();
        const std::vector<std::string> lines = WrapSidePanelLines(text, textBounds.width, fontSize);
        if (lines.empty())
        {
            return y;
        }

        return y + static_cast<float>(lines.size()) * fontSize * kParagraphLineSpacing;
    }

    float MeasureSidePanelLine(float y)
    {
        return y + g_smallFontDrawSize + 2.0f;
    }

    void RotateLocalOffset(int rotationDegrees, float localX, float localZ, float& outX, float& outZ)
    {
        const float radians = static_cast<float>(rotationDegrees) * DEG2RAD;
        const float cosine = cosf(radians);
        const float sine = sinf(radians);
        outX = localX * cosine - localZ * sine;
        outZ = localX * sine + localZ * cosine;
    }

    void AppendEdgeMidpointAnchors(float halfX, float halfZ, std::vector<PlanarOffset>& anchors)
    {
        anchors.push_back(PlanarOffset{ halfX, 0.0f });
        anchors.push_back(PlanarOffset{ -halfX, 0.0f });
        anchors.push_back(PlanarOffset{ 0.0f, halfZ });
        anchors.push_back(PlanarOffset{ 0.0f, -halfZ });
    }

    Vector3 ApplyGridSnap(Vector3 position)
    {
        position.x = roundf(position.x / kSnapGridSize) * kSnapGridSize;
        position.z = roundf(position.z / kSnapGridSize) * kSnapGridSize;
        return position;
    }

    Rectangle GetCastlePartButtonRect(int buttonIndex, float startY)
    {
        const Rectangle textBounds = GetRegionSidePanelTextBounds();
        const int column = buttonIndex % kCastlePartButtonColumns;
        const int row = buttonIndex / kCastlePartButtonColumns;
        const float columnWidth = (textBounds.width - static_cast<float>(kCastlePartButtonColumnGap))
            / static_cast<float>(kCastlePartButtonColumns);
        const float x = textBounds.x + static_cast<float>(column) * (columnWidth + static_cast<float>(kCastlePartButtonColumnGap));
        const float y = startY + static_cast<float>(row * (kCastlePartButtonHeight + kCastlePartButtonGap));
        return Rectangle{
            x,
            y,
            columnWidth,
            static_cast<float>(kCastlePartButtonHeight)
        };
    }

    float GetCastlePartToolbarHeight()
    {
        const int rowCount = (kCastlePartButtonCount + kCastlePartButtonColumns - 1) / kCastlePartButtonColumns;
        return static_cast<float>(rowCount * kCastlePartButtonHeight + (rowCount - 1) * kCastlePartButtonGap);
    }

    void DrawCastlePartButton(const Rectangle& rect, const char* label, bool selected, bool hovered)
    {
        Color fill = Color{ 34, 38, 48, 255 };
        if (selected)
        {
            fill = Color{ 58, 72, 98, 255 };
        }
        else if (hovered)
        {
            fill = Color{ 46, 52, 66, 255 };
        }

        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 1.0f, selected ? Color{ 120, 170, 230, 255 } : Color{ 72, 78, 92, 255 });

        const float fontSize = g_smallFontDrawSize - 1.0f;
        const Vector2 textSize = MeasureTextEx(*g_smallFont, label, fontSize, 1.0f);
        const float textX = rect.x + (rect.width - textSize.x) * 0.5f;
        const float textY = rect.y + (rect.height - textSize.y) * 0.5f;
        DrawOutlinedText(g_smallFont, label, Vector2{ textX, textY }, fontSize, 1, WHITE);
    }

    int PickCastlePartAtTerrainHit(const std::vector<CastlePartPlacement>& placements, Vector3 terrainHit)
    {
        int bestIndex = -1;
        float bestDistanceSquared = 0.0f;

        for (int index = 0; index < static_cast<int>(placements.size()); ++index)
        {
            const CastlePartPlacement& placement = placements[static_cast<size_t>(index)];
            const float pickRadius = GetCastlePartPickRadius(placement.m_Type);
            const float dx = terrainHit.x - placement.m_Position.x;
            const float dz = terrainHit.z - placement.m_Position.z;
            const float distanceSquared = dx * dx + dz * dz;
            if (distanceSquared > pickRadius * pickRadius)
            {
                continue;
            }

            if (bestIndex < 0 || distanceSquared < bestDistanceSquared)
            {
                bestIndex = index;
                bestDistanceSquared = distanceSquared;
            }
        }

        return bestIndex;
    }
}

void CastleDesignState::Init(const std::string& configfile)
{
    (void)configfile;
    g_RegionView.Init();
}

void CastleDesignState::Shutdown()
{

}

void CastleDesignState::OnEnter()
{
    m_Placements.clear();
    m_SelectedPartType = CastlePartType::RoundTower;
    m_HoveredButtonIndex = -1;
    m_PlacementToolActive = true;
    m_HasPlacementPreview = false;
    m_PlacementRotationDegrees = 0;
    m_SelectedPlacementIndex = -1;
    m_IsDraggingPlacement = false;

    if (g_GameDatabase.m_Regions.empty())
    {
        g_GameDatabase.InitNewCampaign(CampaignSetup{});
        g_GameDatabase.SetActiveRegion(0);
    }

    if (g_GameDatabase.m_ActiveRegionId >= 0)
    {
        g_GameDatabase.EnsureRegionHeightfield(g_GameDatabase.m_ActiveRegionId);
    }

    InitializeReferenceSwordsmen();
}

void CastleDesignState::InitializeReferenceSwordsmen()
{
    m_ReferenceSwordsmen = {};
    m_ReferenceSwordsmen.m_Id = 0;
    m_ReferenceSwordsmen.m_Type = CombatUnitType::Swordsmen;
    m_ReferenceSwordsmen.m_Anchor = Vector3{
        REGION_CELLS * 0.5f,
        0.0f,
        REGION_CELLS * 0.5f
    };
    m_ReferenceSwordsmen.m_Army = LittlePeopleArmy::Blue;
    m_ReferenceSwordsmen.m_Facing = LittlePeopleDirection::South;
    InitCombatUnitHealth(m_ReferenceSwordsmen);
}

void CastleDesignState::OnExit()
{

}

bool CastleDesignState::TryGetTerrainHitUnderMouse(const RegionHeightfield& heightfield, Vector3& outHit) const
{
    const Vector2 mousePosition = GetScaledMousePosition();
    if (!IsPointInRegionWorldView(mousePosition))
    {
        return false;
    }

    const Ray ray = GetCombatMouseRay(g_RegionView.GetCamera());
    return RaycastCombatTerrain(ray, heightfield, outHit);
}

Vector3 CastleDesignState::SnapCastlePartPosition(CastlePartType type, int rotationDegrees, Vector3 rawPosition,
    int excludeIndex, bool& outSnappedToExisting) const
{
    outSnappedToExisting = false;

    float movingHalfX = 0.0f;
    float movingHalfZ = 0.0f;
    GetCastlePartHalfExtents(type, movingHalfX, movingHalfZ);

    std::vector<PlanarOffset> movingAnchorsLocal;
    AppendEdgeMidpointAnchors(movingHalfX, movingHalfZ, movingAnchorsLocal);

    Vector3 bestPosition = ApplyGridSnap(rawPosition);
    float bestScore = Vector2Distance(
        Vector2{ rawPosition.x, rawPosition.z },
        Vector2{ bestPosition.x, bestPosition.z });

    for (int index = 0; index < static_cast<int>(m_Placements.size()); ++index)
    {
        if (index == excludeIndex)
        {
            continue;
        }

        const CastlePartPlacement& existing = m_Placements[static_cast<size_t>(index)];
        float existingHalfX = 0.0f;
        float existingHalfZ = 0.0f;
        GetCastlePartHalfExtents(existing.m_Type, existingHalfX, existingHalfZ);

        std::vector<PlanarOffset> existingAnchorsLocal;
        AppendEdgeMidpointAnchors(existingHalfX, existingHalfZ, existingAnchorsLocal);

        for (const PlanarOffset& existingLocal : existingAnchorsLocal)
        {
            float existingWorldOffsetX = 0.0f;
            float existingWorldOffsetZ = 0.0f;
            RotateLocalOffset(
                existing.m_RotationDegrees,
                existingLocal.m_X,
                existingLocal.m_Z,
                existingWorldOffsetX,
                existingWorldOffsetZ);

            const float anchorWorldX = existing.m_Position.x + existingWorldOffsetX;
            const float anchorWorldZ = existing.m_Position.z + existingWorldOffsetZ;

            for (const PlanarOffset& movingLocal : movingAnchorsLocal)
            {
                float movingWorldOffsetX = 0.0f;
                float movingWorldOffsetZ = 0.0f;
                RotateLocalOffset(
                    rotationDegrees,
                    movingLocal.m_X,
                    movingLocal.m_Z,
                    movingWorldOffsetX,
                    movingWorldOffsetZ);

                const Vector3 candidate{
                    anchorWorldX - movingWorldOffsetX,
                    rawPosition.y,
                    anchorWorldZ - movingWorldOffsetZ
                };

                const float score = Vector2Distance(
                    Vector2{ rawPosition.x, rawPosition.z },
                    Vector2{ candidate.x, candidate.z });

                if (score <= kSnapSearchRadius && score < bestScore)
                {
                    bestScore = score;
                    bestPosition = candidate;
                    outSnappedToExisting = true;
                }
            }
        }
    }

    if (!outSnappedToExisting)
    {
        bestPosition = ApplyGridSnap(rawPosition);
    }

    bestPosition.y = rawPosition.y;
    return bestPosition;
}

void CastleDesignState::UpdatePlacementPreview(const RegionHeightfield& heightfield)
{
    m_HasPlacementPreview = false;
    m_PlacementSnappedToExisting = false;

    if (!m_PlacementToolActive || m_IsDraggingPlacement)
    {
        return;
    }

    Vector3 terrainHit{};
    if (TryGetTerrainHitUnderMouse(heightfield, terrainHit))
    {
        m_PlacementPreview = SnapCastlePartPosition(
            m_SelectedPartType,
            m_PlacementRotationDegrees,
            terrainHit,
            -1,
            m_PlacementSnappedToExisting);
        m_HasPlacementPreview = true;
    }
}

float CastleDesignState::LayoutPanelBeforeToolbar(float startY, bool draw) const
{
    float panelY = startY;
    if (draw)
    {
        panelY = DrawRegionSidePanelOutlinedParagraph("Castle Design", panelY, g_fontDrawSize, WHITE);
    }
    else
    {
        panelY = MeasureSidePanelParagraph(panelY, "Castle Design", g_fontDrawSize);
    }

    panelY += 2.0f;
    if (m_PlacementToolActive)
    {
        const char* text = "Click terrain to place. Snaps.";
        panelY = draw
            ? DrawRegionSidePanelOutlinedParagraph(text, panelY, g_smallFontDrawSize, Color{ 220, 220, 220, 255 })
            : MeasureSidePanelParagraph(panelY, text, g_smallFontDrawSize);
    }
    else if (m_SelectedPlacementIndex >= 0 && m_SelectedPlacementIndex < static_cast<int>(m_Placements.size()))
    {
        const CastlePartPlacement& selected = m_Placements[static_cast<size_t>(m_SelectedPlacementIndex)];
        const string selectedText = string("Sel: ") + CastlePartTypeButtonLabel(selected.m_Type);
        panelY = draw
            ? DrawRegionSidePanelOutlinedParagraph(selectedText, panelY, g_smallFontDrawSize, Color{ 255, 230, 90, 255 })
            : MeasureSidePanelParagraph(panelY, selectedText, g_smallFontDrawSize);
        const char* helpText = "Drag move. R rot. Del.";
        panelY = draw
            ? DrawRegionSidePanelOutlinedParagraph(helpText, panelY, g_smallFontDrawSize, Color{ 220, 220, 220, 255 })
            : MeasureSidePanelParagraph(panelY, helpText, g_smallFontDrawSize);
    }
    else
    {
        const char* text = "R-click, then click piece.";
        panelY = draw
            ? DrawRegionSidePanelOutlinedParagraph(text, panelY, g_smallFontDrawSize, Color{ 220, 220, 220, 255 })
            : MeasureSidePanelParagraph(panelY, text, g_smallFontDrawSize);
    }

    panelY += 4.0f;
    const char* scaleText = "Scale: blue swordsmen";
    if (draw)
    {
        panelY = DrawRegionSidePanelOutlinedLine(scaleText, panelY, Color{ 140, 175, 230, 255 });
    }
    else
    {
        panelY = MeasureSidePanelLine(panelY);
    }

    panelY += 2.0f;
    return panelY;
}

void CastleDesignState::HandleCastlePartToolbarInput()
{
    const Vector2 mousePosition = GetScaledMousePosition();
    if (!IsPointInRegionSidePanel(mousePosition))
    {
        m_HoveredButtonIndex = -1;
        return;
    }

    const Rectangle minimapBounds = g_RegionMinimap.GetScreenBounds();
    if (CheckCollisionPointRec(mousePosition, minimapBounds))
    {
        m_HoveredButtonIndex = -1;
        return;
    }

    m_HoveredButtonIndex = -1;
    for (int buttonIndex = 0; buttonIndex < kCastlePartButtonCount; ++buttonIndex)
    {
        if (CheckCollisionPointRec(mousePosition, GetCastlePartButtonRect(buttonIndex, m_ToolbarStartY)))
        {
            m_HoveredButtonIndex = buttonIndex;
            break;
        }
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || m_HoveredButtonIndex < 0)
    {
        return;
    }

    m_PlacementToolActive = true;
    m_SelectedPartType = static_cast<CastlePartType>(m_HoveredButtonIndex);
    m_SelectedPlacementIndex = -1;
    m_IsDraggingPlacement = false;
}

void CastleDesignState::HandleCastleDesignInput(const RegionHeightfield& heightfield)
{
    HandleCastlePartToolbarInput();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        m_PlacementToolActive = false;
        m_HasPlacementPreview = false;
        m_IsDraggingPlacement = false;
    }

    const Vector2 mousePosition = GetScaledMousePosition();
    if (IsPointInRegionSidePanel(mousePosition))
    {
        return;
    }

    Vector3 terrainHit{};
    const bool hasTerrainHit = TryGetTerrainHitUnderMouse(heightfield, terrainHit);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (m_PlacementToolActive)
        {
            if (hasTerrainHit && m_HasPlacementPreview)
            {
                CastlePartPlacement placement{};
                placement.m_Type = m_SelectedPartType;
                placement.m_Position = m_PlacementPreview;
                placement.m_RotationDegrees = m_PlacementRotationDegrees;
                m_Placements.push_back(placement);
                m_PlacementSnappedToExisting = false;
            }
        }
        else if (hasTerrainHit)
        {
            const int pickedIndex = PickCastlePartAtTerrainHit(m_Placements, terrainHit);
            if (pickedIndex >= 0)
            {
                m_SelectedPlacementIndex = pickedIndex;
                m_IsDraggingPlacement = true;
            }
            else
            {
                m_SelectedPlacementIndex = -1;
                m_IsDraggingPlacement = false;
            }
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && m_IsDraggingPlacement
        && m_SelectedPlacementIndex >= 0
        && m_SelectedPlacementIndex < static_cast<int>(m_Placements.size())
        && hasTerrainHit)
    {
        CastlePartPlacement& placement = m_Placements[static_cast<size_t>(m_SelectedPlacementIndex)];
        bool snappedToExisting = false;
        placement.m_Position = SnapCastlePartPosition(
            placement.m_Type,
            placement.m_RotationDegrees,
            terrainHit,
            m_SelectedPlacementIndex,
            snappedToExisting);
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        m_IsDraggingPlacement = false;
    }
}

float CastleDesignState::DrawCastlePartToolbar(float startY)
{
    for (int buttonIndex = 0; buttonIndex < kCastlePartButtonCount; ++buttonIndex)
    {
        const auto partType = static_cast<CastlePartType>(buttonIndex);
        const Rectangle buttonRect = GetCastlePartButtonRect(buttonIndex, startY);
        const bool selected = m_PlacementToolActive && (partType == m_SelectedPartType);
        const bool hovered = (buttonIndex == m_HoveredButtonIndex);
        DrawCastlePartButton(buttonRect, CastlePartTypeButtonLabel(partType), selected, hovered);
    }

    return startY + GetCastlePartToolbarHeight();
}

void CastleDesignState::DrawReferenceSwordsmen(const RegionHeightfield& heightfield) const
{
    if (!IsCombatUnitAlive(m_ReferenceSwordsmen))
    {
        return;
    }

    const Camera3D& camera = g_RegionView.GetCamera();
    std::vector<LittlePersonBillboardDrawRequest> billboardDrawRequests;
    billboardDrawRequests.reserve(32);
    AppendCombatUnitBillboardDrawRequests(
        camera,
        heightfield,
        m_ReferenceSwordsmen,
        false,
        billboardDrawRequests);
    DrawCombatUnit(camera, heightfield, m_ReferenceSwordsmen, 0, false);
    DrawLittlePeopleBillboardsSorted(camera, billboardDrawRequests);
}

void CastleDesignState::DrawCastleParts3D(const RegionHeightfield& heightfield) const
{
    for (int index = 0; index < static_cast<int>(m_Placements.size()); ++index)
    {
        const CastlePartPlacement& placement = m_Placements[static_cast<size_t>(index)];
        const bool selected = (!m_PlacementToolActive && index == m_SelectedPlacementIndex);
        Color color = CastlePartTypeColor(placement.m_Type);
        if (selected)
        {
            color.r = static_cast<unsigned char>(std::min(255, static_cast<int>(color.r) + 40));
            color.g = static_cast<unsigned char>(std::min(255, static_cast<int>(color.g) + 40));
            color.b = static_cast<unsigned char>(std::min(255, static_cast<int>(color.b) + 40));
        }

        DrawCastlePartShape(heightfield, placement.m_Type, placement.m_Position, placement.m_RotationDegrees, color);
        if (selected)
        {
            DrawCombatMoveMarker(heightfield, placement.m_Position, Color{ 255, 220, 90, 255 });
        }
    }

    if (m_PlacementToolActive && m_HasPlacementPreview)
    {
        const Color previewColor = CastlePartTypeColor(m_SelectedPartType);
        DrawCastlePartShape(heightfield, m_SelectedPartType, m_PlacementPreview, m_PlacementRotationDegrees,
            previewColor, 0.55f);
        const Color previewMarkerColor = m_PlacementSnappedToExisting
            ? Color{ 90, 255, 130, 220 }
            : Color{ 255, 220, 90, 180 };
        DrawCombatMoveMarker(heightfield, m_PlacementPreview, previewMarkerColor);
    }
}

void CastleDesignState::Update()
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

    m_ToolbarStartY = LayoutPanelBeforeToolbar(GetRegionSidePanelTextBounds().y, false);

    if (heightfield)
    {
        UpdatePlacementPreview(*heightfield);
        HandleCastleDesignInput(*heightfield);
    }

    if (IsKeyPressed(KEY_R))
    {
        if (!m_PlacementToolActive
            && m_SelectedPlacementIndex >= 0
            && m_SelectedPlacementIndex < static_cast<int>(m_Placements.size()))
        {
            CastlePartPlacement& placement = m_Placements[static_cast<size_t>(m_SelectedPlacementIndex)];
            placement.m_RotationDegrees = (placement.m_RotationDegrees + 90) % 360;
        }
        else if (m_PlacementToolActive)
        {
            m_PlacementRotationDegrees = (m_PlacementRotationDegrees + 90) % 360;
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE))
    {
        if (!m_PlacementToolActive
            && m_SelectedPlacementIndex >= 0
            && m_SelectedPlacementIndex < static_cast<int>(m_Placements.size()))
        {
            m_Placements.erase(m_Placements.begin() + m_SelectedPlacementIndex);
            m_SelectedPlacementIndex = -1;
            m_IsDraggingPlacement = false;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
    }
}

void CastleDesignState::Draw()
{
    RegionData* region = g_GameDatabase.GetActiveRegion();
    if (!region || !region->m_Heightfield.m_Generated)
    {
        DrawRegionSidePanelBackground();
        DrawRegionSidePanelOutlinedParagraph("Castle design (no region terrain)", GetRegionSidePanelTextBounds().y,
            g_fontDrawSize, WHITE);
        return;
    }

    const RegionHeightfield& heightfield = region->m_Heightfield;

    g_RegionTerrainMesh.SetHeightfield(&heightfield);
    g_RegionTerrainMesh.SetFlatShaded(true);
    g_RegionTerrainMesh.RebuildIfNeeded();

    g_RegionView.Begin3D();
    g_RegionTerrainMesh.Draw();
    DrawCastleParts3D(heightfield);
    DrawReferenceSwordsmen(heightfield);
    g_RegionView.End3D();

    std::vector<RegionMinimapMarker> minimapMarkers;
    minimapMarkers.reserve(m_Placements.size());
    for (const CastlePartPlacement& placement : m_Placements)
    {
        minimapMarkers.push_back(RegionMinimapMarker{
            placement.m_Position.x,
            placement.m_Position.z,
            CastlePartTypeColor(placement.m_Type)
        });
    }

    if (IsCombatUnitAlive(m_ReferenceSwordsmen))
    {
        minimapMarkers.push_back(RegionMinimapMarker{
            m_ReferenceSwordsmen.m_Anchor.x,
            m_ReferenceSwordsmen.m_Anchor.z,
            Color{ 80, 140, 255, 255 }
        });
    }

    DrawRegionSidePanelBackground();

    g_RegionMinimap.Draw(
        heightfield,
        region->m_HeightfieldSeed,
        g_RegionView.GetCamera(),
        &minimapMarkers);

    float panelY = LayoutPanelBeforeToolbar(GetRegionSidePanelTextBounds().y, true);
    panelY = DrawCastlePartToolbar(panelY);
    panelY += 4.0f;
    DrawRegionSidePanelOutlinedParagraph(
        "R-click: select. WASD pan. Q/E cam. Esc title.",
        panelY,
        g_smallFontDrawSize,
        Color{ 180, 185, 195, 255 });
}