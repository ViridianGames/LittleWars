#include "CombatUnits.h"

#include <algorithm>
#include <cmath>

#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "GameGlobals.h"
#include "raymath.h"

namespace
{
    constexpr float kInfantryRowSpacing = 1.2f;
    constexpr float kInfantryColumnSpacing = 1.0f;
    constexpr float kKnightRowSpacing = 2.5f;
    constexpr float kKnightColumnSpacing = 2.0f;

    const CombatUnitFormation kSwordsmanFormation{
        kSwordsmanFormationRows,
        kSwordsmanFormationColumns,
        kSwordsmanUnitSize,
        kInfantryRowSpacing,
        kInfantryColumnSpacing
    };

    const CombatUnitFormation kArcherFormation{
        kArcherFormationRows,
        kArcherFormationColumns,
        kArcherUnitSize,
        kInfantryRowSpacing,
        kInfantryColumnSpacing
    };

    const CombatUnitFormation kKnightFormation{
        kKnightFormationRows,
        kKnightFormationColumns,
        kKnightUnitSize,
        kKnightRowSpacing,
        kKnightColumnSpacing
    };

    const CombatUnitFormation kCatapultFormation{
        1,
        1,
        kCatapultUnitSize,
        0.0f,
        0.0f
    };

    void DrawCatapultPlaceholder(Vector3 groundPosition)
    {
        const Vector3 bodySize{ 2.4f, 1.4f, 1.8f };
        const Vector3 bodyCenter{
            groundPosition.x,
            groundPosition.y + bodySize.y * 0.5f,
            groundPosition.z
        };

        DrawCube(bodyCenter, bodySize.x, bodySize.y, bodySize.z, Color{ 92, 72, 52, 255 });
        DrawCubeWires(bodyCenter, bodySize.x, bodySize.y, bodySize.z, Color{ 40, 30, 20, 255 });

        const Vector3 armSize{ 3.2f, 0.35f, 0.35f };
        const Vector3 armCenter{
            groundPosition.x,
            groundPosition.y + bodySize.y + armSize.y * 0.5f,
            groundPosition.z - 0.2f
        };
        DrawCube(armCenter, armSize.x, armSize.y, armSize.z, Color{ 70, 55, 38, 255 });
    }
}

const char* CombatUnitTypeName(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Swordsmen:
        return "Swordsmen";
    case CombatUnitType::Archers:
        return "Archers";
    case CombatUnitType::Knights:
        return "Knights";
    case CombatUnitType::Catapult:
        return "Catapult";
    default:
        return "Unknown";
    }
}

const CombatUnitFormation& GetCombatUnitFormation(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Swordsmen:
        return kSwordsmanFormation;
    case CombatUnitType::Archers:
        return kArcherFormation;
    case CombatUnitType::Knights:
        return kKnightFormation;
    case CombatUnitType::Catapult:
        return kCatapultFormation;
    default:
        return kSwordsmanFormation;
    }
}

int GetCombatUnitSoldierCount(CombatUnitType type)
{
    return GetCombatUnitFormation(type).m_SoldierCount;
}

bool GetCombatUnitFormationSlot(CombatUnitType type, int slotIndex, int& row, int& column)
{
    const CombatUnitFormation& formation = GetCombatUnitFormation(type);
    if (slotIndex < 0 || slotIndex >= formation.m_SoldierCount)
    {
        return false;
    }

    row = slotIndex / formation.m_Columns;
    column = slotIndex % formation.m_Columns;
    return true;
}

Vector2 GetCombatUnitFormationOffset(CombatUnitType type, int row, int column)
{
    const CombatUnitFormation& formation = GetCombatUnitFormation(type);
    const float centerRow = (formation.m_Rows - 1) * 0.5f;
    const float centerColumn = (formation.m_Columns - 1) * 0.5f;

    return Vector2{
        (static_cast<float>(column) - centerColumn) * formation.m_ColumnSpacing,
        (static_cast<float>(row) - centerRow) * formation.m_RowSpacing
    };
}

Vector3 TransformCombatFormationOffset(LittlePeopleDirection facing, float localX, float localZ)
{
    const int directionIndex = static_cast<int>(facing) % LITTLEPEOPLE_DIRECTION_COUNT;
    const float angle = static_cast<float>(directionIndex) * (6.28318530718f / static_cast<float>(LITTLEPEOPLE_DIRECTION_COUNT));

    const float cosAngle = std::cosf(angle);
    const float sinAngle = std::sinf(angle);
    return Vector3{
        localX * cosAngle + localZ * sinAngle,
        0.0f,
        -localX * sinAngle + localZ * cosAngle
    };
}

float GetCombatUnitSpriteHeight(CombatUnitType type)
{
    if (type == CombatUnitType::Knights)
    {
        return 1.45f;
    }

    return 1.2f;
}

float GetCombatUnitPickRadiusPixels(CombatUnitType type)
{
    if (type == CombatUnitType::Catapult)
    {
        return 28.0f;
    }

    if (type == CombatUnitType::Knights)
    {
        return 16.0f;
    }

    return 12.0f;
}

float GetCombatUnitSelectionRadius(CombatUnitType type)
{
    const CombatUnitFormation& formation = GetCombatUnitFormation(type);
    if (type == CombatUnitType::Catapult)
    {
        return 2.0f;
    }

    const float halfWidth = formation.m_Columns * formation.m_ColumnSpacing * 0.5f + 0.8f;
    const float halfDepth = formation.m_Rows * formation.m_RowSpacing * 0.5f + 0.8f;
    return std::max(halfWidth, halfDepth);
}

int GetCombatUnitPickSlotCount(const CombatUnitInstance& unit)
{
    return GetCombatUnitSoldierCount(unit.m_Type);
}

Vector3 GetCombatUnitSoldierWorldPosition(const CombatUnitInstance& unit, const RegionHeightfield& heightfield, int slotIndex)
{
    if (unit.m_Type == CombatUnitType::Catapult)
    {
        const float y = heightfield.SampleHeight(unit.m_Anchor.x, unit.m_Anchor.z);
        return Vector3{ unit.m_Anchor.x, y, unit.m_Anchor.z };
    }

    int row = 0;
    int column = 0;
    if (!GetCombatUnitFormationSlot(unit.m_Type, slotIndex, row, column))
    {
        return unit.m_Anchor;
    }

    const Vector2 localOffset = GetCombatUnitFormationOffset(unit.m_Type, row, column);
    const Vector3 worldOffset = TransformCombatFormationOffset(unit.m_Facing, localOffset.x, localOffset.y);
    const float worldX = unit.m_Anchor.x + worldOffset.x;
    const float worldZ = unit.m_Anchor.z + worldOffset.z;
    const float worldY = heightfield.SampleHeight(worldX, worldZ);
    return Vector3{ worldX, worldY, worldZ };
}

Vector3 GetCombatUnitSoldierDrawPosition(const CombatUnitInstance& unit, const RegionHeightfield& heightfield, int slotIndex)
{
    const Vector3 groundPosition = GetCombatUnitSoldierWorldPosition(unit, heightfield, slotIndex);
    if (unit.m_Type == CombatUnitType::Catapult)
    {
        return Vector3{
            groundPosition.x,
            groundPosition.y + 1.1f,
            groundPosition.z
        };
    }

    return Vector3{
        groundPosition.x,
        groundPosition.y + GetCombatUnitSpriteHeight(unit.m_Type) * 0.5f,
        groundPosition.z
    };
}

Vector2 GetCombatScaledMousePosition()
{
    Vector2 mouse = GetMousePosition();
    const float inputScale = g_Engine->GetInputScale();
    mouse.x /= inputScale;
    mouse.y /= inputScale;
    return mouse;
}

Ray GetCombatMouseRay(const Camera3D& camera)
{
    const Vector2 mouse = GetCombatScaledMousePosition();
    if (g_Engine->m_useVirtualResolution)
    {
        return GetScreenToWorldRayEx(
            mouse,
            camera,
            static_cast<int>(g_Engine->m_RenderWidth),
            static_cast<int>(g_Engine->m_RenderHeight));
    }

    return GetScreenToWorldRay(mouse, camera);
}

Vector2 GetCombatWorldToScreen(Vector3 worldPosition, const Camera3D& camera)
{
    if (g_Engine->m_useVirtualResolution)
    {
        return GetWorldToScreenEx(
            worldPosition,
            camera,
            static_cast<int>(g_Engine->m_RenderWidth),
            static_cast<int>(g_Engine->m_RenderHeight));
    }

    return GetWorldToScreen(worldPosition, camera);
}

bool RaycastCombatTerrain(const Ray& ray, const RegionHeightfield& heightfield, Vector3& outHit)
{
    constexpr float kMaxDistance = 300.0f;
    constexpr float kStepDistance = 0.35f;
    constexpr float kFallbackSurfaceDistance = 2.5f;

    const Vector3 direction = Vector3Normalize(ray.direction);
    bool hasPreviousSample = false;
    float previousT = 0.0f;
    float previousHeightDelta = 0.0f;

    float closestSurfaceDistance = kFallbackSurfaceDistance;
    Vector3 closestSurfacePoint{};

    for (float t = 0.0f; t <= kMaxDistance; t += kStepDistance)
    {
        const Vector3 point = Vector3Add(ray.position, Vector3Scale(direction, t));
        if (point.x < 0.0f || point.z < 0.0f
            || point.x > static_cast<float>(REGION_CELLS) || point.z > static_cast<float>(REGION_CELLS))
        {
            continue;
        }

        const float terrainY = heightfield.SampleHeight(point.x, point.z);
        const float heightDelta = point.y - terrainY;
        const float surfaceDistance = std::fabs(heightDelta);
        if (surfaceDistance < closestSurfaceDistance)
        {
            closestSurfaceDistance = surfaceDistance;
            closestSurfacePoint = Vector3{ point.x, terrainY, point.z };
        }

        if (hasPreviousSample && previousHeightDelta > 0.0f && heightDelta <= 0.0f)
        {
            const float blend = previousHeightDelta / (previousHeightDelta - heightDelta + 0.0001f);
            const float hitT = previousT + (t - previousT) * blend;
            const Vector3 hitPoint = Vector3Add(ray.position, Vector3Scale(direction, hitT));
            outHit = Vector3{
                hitPoint.x,
                heightfield.SampleHeight(hitPoint.x, hitPoint.z),
                hitPoint.z
            };
            return true;
        }

        hasPreviousSample = true;
        previousT = t;
        previousHeightDelta = heightDelta;
    }

    if (closestSurfaceDistance < kFallbackSurfaceDistance)
    {
        outHit = closestSurfacePoint;
        return true;
    }

    return false;
}

int PickCombatUnitAtMouse(const Camera3D& camera, const RegionHeightfield& heightfield,
    const std::vector<CombatUnitInstance>& units, Vector2 mousePosition)
{
    int bestUnitIndex = -1;
    float bestDistance = 0.0f;

    for (int unitIndex = 0; unitIndex < static_cast<int>(units.size()); ++unitIndex)
    {
        const CombatUnitInstance& unit = units[static_cast<size_t>(unitIndex)];
        const int slotCount = GetCombatUnitPickSlotCount(unit);
        const float pickRadius = GetCombatUnitPickRadiusPixels(unit.m_Type);

        for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            const Vector3 drawPosition = GetCombatUnitSoldierDrawPosition(unit, heightfield, slotIndex);
            const Vector2 screenPosition = GetCombatWorldToScreen(drawPosition, camera);
            const float distance = Vector2Distance(mousePosition, screenPosition);
            if (distance > pickRadius)
            {
                continue;
            }

            if (bestUnitIndex < 0 || distance < bestDistance)
            {
                bestUnitIndex = unitIndex;
                bestDistance = distance;
            }
        }
    }

    return bestUnitIndex;
}

void FaceCombatUnitToward(CombatUnitInstance& unit, Vector3 worldTarget)
{
    const float dx = worldTarget.x - unit.m_Anchor.x;
    const float dz = worldTarget.z - unit.m_Anchor.z;
    if ((dx * dx + dz * dz) > 0.05f)
    {
        unit.m_Facing = LittlePeopleDirectionFromVector(dx, dz);
    }
}

void MoveCombatUnit(CombatUnitInstance& unit, Vector3 targetAnchor)
{
    FaceCombatUnitToward(unit, targetAnchor);
    unit.m_Anchor.x = std::clamp(targetAnchor.x, 2.0f, static_cast<float>(REGION_CELLS) - 2.0f);
    unit.m_Anchor.z = std::clamp(targetAnchor.z, 2.0f, static_cast<float>(REGION_CELLS) - 2.0f);
}

void DrawCombatUnit(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit,
    int walkFrame, bool selected)
{
    Color tint = WHITE;
    if (selected)
    {
        tint = Color{ 255, 255, 160, 255 };
    }

    if (unit.m_Type == CombatUnitType::Catapult)
    {
        const float y = heightfield.SampleHeight(unit.m_Anchor.x, unit.m_Anchor.z);
        DrawCatapultPlaceholder(Vector3{ unit.m_Anchor.x, y, unit.m_Anchor.z });
        return;
    }

    const int soldierCount = GetCombatUnitSoldierCount(unit.m_Type);
    const float spriteHeight = GetCombatUnitSpriteHeight(unit.m_Type);

    for (int slotIndex = 0; slotIndex < soldierCount; ++slotIndex)
    {
        const Vector3 groundPosition = GetCombatUnitSoldierWorldPosition(unit, heightfield, slotIndex);
        DrawLittlePersonBillboard(
            camera,
            unit.m_Army,
            unit.m_Facing,
            walkFrame,
            groundPosition,
            spriteHeight,
            tint
        );
    }
}

void DrawCombatUnitSelectionIndicator(const RegionHeightfield& heightfield, const CombatUnitInstance& unit)
{
    const float terrainY = heightfield.SampleHeight(unit.m_Anchor.x, unit.m_Anchor.z) + 0.08f;
    const float radius = GetCombatUnitSelectionRadius(unit.m_Type);
    const Vector3 center{ unit.m_Anchor.x, terrainY, unit.m_Anchor.z };
    DrawCircle3D(center, radius, Vector3{ 0.0f, 1.0f, 0.0f }, 32, Color{ 255, 230, 80, 220 });
}

void DrawCombatMoveMarker(const RegionHeightfield& heightfield, Vector3 target, Color color)
{
    const float terrainY = heightfield.SampleHeight(target.x, target.z) + 0.12f;
    const Vector3 center{ target.x, terrainY, target.z };
    constexpr float kMarkerRadius = 0.9f;

    DrawCircle3D(center, kMarkerRadius, Vector3{ 0.0f, 1.0f, 0.0f }, 20, color);
    DrawLine3D(
        Vector3{ center.x - kMarkerRadius, terrainY, center.z },
        Vector3{ center.x + kMarkerRadius, terrainY, center.z },
        color);
    DrawLine3D(
        Vector3{ center.x, terrainY, center.z - kMarkerRadius },
        Vector3{ center.x, terrainY, center.z + kMarkerRadius },
        color);
    DrawLine3D(center, Vector3{ center.x, terrainY + 1.4f, center.z }, color);
}