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

    constexpr float kTwoPi = 6.28318530718f;
    constexpr float kPi = 3.14159265359f;
    constexpr float kFacingAngleSlice = kTwoPi / static_cast<float>(LITTLEPEOPLE_DIRECTION_COUNT);
    constexpr float kUnitRotateSpeed = 5.5f;
    constexpr float kFigureMinSeparation = 0.85f;

    float FacingDirectionToAngle(LittlePeopleDirection facing)
    {
        const int directionIndex = static_cast<int>(facing) % LITTLEPEOPLE_DIRECTION_COUNT;
        return static_cast<float>(directionIndex) * kFacingAngleSlice;
    }

    float NormalizeAngleRadians(float angle)
    {
        while (angle >= kTwoPi)
        {
            angle -= kTwoPi;
        }

        while (angle < 0.0f)
        {
            angle += kTwoPi;
        }

        return angle;
    }

    float ShortestAngleDelta(float currentAngle, float targetAngle)
    {
        float delta = targetAngle - currentAngle;
        while (delta > kPi)
        {
            delta -= kTwoPi;
        }

        while (delta < -kPi)
        {
            delta += kTwoPi;
        }

        return delta;
    }

    float FacingAngleFromVector(float dx, float dz)
    {
        return NormalizeAngleRadians(std::atan2f(dx, dz));
    }

    LittlePeopleDirection VisualFacingFromAngle(float facingAngle)
    {
        return LittlePeopleDirectionFromVector(std::sinf(facingAngle), std::cosf(facingAngle));
    }

    float ClampWorldCoord(float value)
    {
        return std::clamp(value, 2.0f, static_cast<float>(REGION_CELLS) - 2.0f);
    }

    void SyncCombatUnitFacingAngles(CombatUnitInstance& unit)
    {
        unit.m_FacingAngle = FacingDirectionToAngle(unit.m_Facing);
        unit.m_TargetFacingAngle = unit.m_FacingAngle;
    }

    void InitializeFigureWorldState(CombatUnitInstance& unit, CombatFigure& figure)
    {
        const Vector2 localOffset = GetCombatUnitFormationOffset(
            unit.m_Type,
            figure.m_FormationRow,
            figure.m_FormationColumn);
        const Vector3 worldOffset = TransformCombatFormationOffset(unit.m_FacingAngle, localOffset.x, localOffset.y);
        figure.m_WorldX = unit.m_Anchor.x + worldOffset.x;
        figure.m_WorldZ = unit.m_Anchor.z + worldOffset.z;
        figure.m_FacingAngle = unit.m_FacingAngle;
        figure.m_TargetFacingAngle = unit.m_FacingAngle;
        figure.m_MoveTargetX = figure.m_WorldX;
        figure.m_MoveTargetZ = figure.m_WorldZ;
        figure.m_IsMoving = false;
    }

    void UpdateCombatFigureFacing(CombatFigure& figure, float deltaTime)
    {
        const float delta = ShortestAngleDelta(figure.m_FacingAngle, figure.m_TargetFacingAngle);
        if (std::fabs(delta) < 0.005f)
        {
            figure.m_FacingAngle = figure.m_TargetFacingAngle;
            return;
        }

        const float step = kUnitRotateSpeed * deltaTime;
        if (std::fabs(delta) <= step)
        {
            figure.m_FacingAngle = figure.m_TargetFacingAngle;
        }
        else
        {
            figure.m_FacingAngle += (delta > 0.0f ? step : -step);
        }

        figure.m_FacingAngle = NormalizeAngleRadians(figure.m_FacingAngle);
    }

    void FaceCombatFigureToward(CombatFigure& figure, float targetX, float targetZ)
    {
        const float dx = targetX - figure.m_WorldX;
        const float dz = targetZ - figure.m_WorldZ;
        if ((dx * dx + dz * dz) > 0.05f)
        {
            figure.m_TargetFacingAngle = FacingAngleFromVector(dx, dz);
        }
    }

    void SyncCombatUnitAnchorFromFigures(CombatUnitInstance& unit)
    {
        float sumX = 0.0f;
        float sumZ = 0.0f;
        int livingCount = 0;

        for (const CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            sumX += figure.m_WorldX;
            sumZ += figure.m_WorldZ;
            ++livingCount;
        }

        if (livingCount <= 0)
        {
            return;
        }

        const float invCount = 1.0f / static_cast<float>(livingCount);
        unit.m_Anchor.x = sumX * invCount;
        unit.m_Anchor.z = sumZ * invCount;
    }

    Vector2 GetFigureFormationWorldXZ(const CombatUnitInstance& unit, const CombatFigure& figure,
        float anchorX, float anchorZ, float facingAngle)
    {
        const Vector2 localOffset = GetCombatUnitFormationOffset(
            unit.m_Type,
            figure.m_FormationRow,
            figure.m_FormationColumn);
        const Vector3 worldOffset = TransformCombatFormationOffset(facingAngle, localOffset.x, localOffset.y);
        return Vector2{
            anchorX + worldOffset.x,
            anchorZ + worldOffset.z
        };
    }

    float ResolveFormationFacingForMove(const CombatUnitInstance& unit, Vector3 orderCenter)
    {
        const float moveDx = orderCenter.x - unit.m_Anchor.x;
        const float moveDz = orderCenter.z - unit.m_Anchor.z;
        const float moveDistSq = moveDx * moveDx + moveDz * moveDz;
        if (moveDistSq <= 0.05f)
        {
            return unit.m_FacingAngle;
        }

        const float moveDist = std::sqrt(moveDistSq);
        const float forwardX = std::sinf(unit.m_FacingAngle);
        const float forwardZ = std::cosf(unit.m_FacingAngle);
        const float dot = (moveDx * forwardX + moveDz * forwardZ) / moveDist;

        if (dot < -0.15f)
        {
            return unit.m_FacingAngle;
        }

        return FacingAngleFromVector(moveDx, moveDz);
    }

    bool IsCombatFigureNearFormationSlot(const CombatUnitInstance& unit, const CombatFigure& figure,
        float anchorX, float anchorZ, float facingAngle)
    {
        const Vector2 slotPosition = GetFigureFormationWorldXZ(unit, figure, anchorX, anchorZ, facingAngle);
        const float dx = figure.m_WorldX - slotPosition.x;
        const float dz = figure.m_WorldZ - slotPosition.y;
        return (dx * dx + dz * dz) <= 0.1f;
    }

    bool IsUnitInFormationAtAnchor(const CombatUnitInstance& unit, float anchorX, float anchorZ, float facingAngle)
    {
        for (const CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            if (!IsCombatFigureNearFormationSlot(unit, figure, anchorX, anchorZ, facingAngle))
            {
                return false;
            }
        }

        return true;
    }

    bool IsUnitExecutingFormationMove(const CombatUnitInstance& unit)
    {
        if (!unit.m_IsMoving)
        {
            return false;
        }

        int movingFigureCount = 0;
        int formationMoveFigureCount = 0;

        for (const CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure) || !figure.m_IsMoving)
            {
                continue;
            }

            ++movingFigureCount;

            const Vector2 slotAtMoveAnchor = GetFigureFormationWorldXZ(
                unit,
                figure,
                unit.m_MoveTargetAnchor.x,
                unit.m_MoveTargetAnchor.z,
                unit.m_FacingAngle);

            const float slotDx = figure.m_MoveTargetX - slotAtMoveAnchor.x;
            const float slotDz = figure.m_MoveTargetZ - slotAtMoveAnchor.y;
            if ((slotDx * slotDx + slotDz * slotDz) <= 0.1f)
            {
                ++formationMoveFigureCount;
            }
        }

        return movingFigureCount > 0 && formationMoveFigureCount == movingFigureCount;
    }

    Vector3 ResolveFormationMoveAnchor(const CombatUnitInstance& unit, Vector3 orderCenter)
    {
        const float moveDx = orderCenter.x - unit.m_Anchor.x;
        const float moveDz = orderCenter.z - unit.m_Anchor.z;
        const float moveDistSq = moveDx * moveDx + moveDz * moveDz;
        if (moveDistSq <= 0.05f)
        {
            return orderCenter;
        }

        const float moveDist = std::sqrt(moveDistSq);
        const float forwardX = std::sinf(unit.m_FacingAngle);
        const float forwardZ = std::cosf(unit.m_FacingAngle);
        const float dot = (moveDx * forwardX + moveDz * forwardZ) / moveDist;
        if (dot >= -0.15f)
        {
            return orderCenter;
        }

        // Backward march: the clicked point is the rear edge, not formation center.
        const CombatUnitFormation& formation = GetCombatUnitFormation(unit.m_Type);
        const float rearExtent = ((formation.m_Rows - 1) * 0.5f) * formation.m_RowSpacing;
        return Vector3{
            orderCenter.x + forwardX * rearExtent,
            orderCenter.y,
            orderCenter.z + forwardZ * rearExtent
        };
    }

    void AssignFigureFormationTargets(CombatUnitInstance& unit, float anchorX, float anchorZ,
        float facingAngle, bool& anyMoving)
    {
        anyMoving = false;
        for (CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            const Vector2 slotPosition = GetFigureFormationWorldXZ(unit, figure, anchorX, anchorZ, facingAngle);
            figure.m_MoveTargetX = ClampWorldCoord(slotPosition.x);
            figure.m_MoveTargetZ = ClampWorldCoord(slotPosition.y);

            const float dx = figure.m_MoveTargetX - figure.m_WorldX;
            const float dz = figure.m_MoveTargetZ - figure.m_WorldZ;
            figure.m_IsMoving = (dx * dx + dz * dz) > 0.05f;
            anyMoving = anyMoving || figure.m_IsMoving;
        }
    }

    void AssignFigureMoveOrder(CombatUnitInstance& unit, Vector3 orderCenter, float formationFacingAngle)
    {
        unit.m_MoveTargetAnchor.x = ClampWorldCoord(orderCenter.x);
        unit.m_MoveTargetAnchor.z = ClampWorldCoord(orderCenter.z);
        unit.m_MoveTargetAnchor.y = 0.0f;

        bool anyMoving = false;
        AssignFigureFormationTargets(
            unit,
            unit.m_MoveTargetAnchor.x,
            unit.m_MoveTargetAnchor.z,
            formationFacingAngle,
            anyMoving);
        unit.m_IsMoving = anyMoving;
    }

    bool AreCombatFiguresFriendly(const CombatUnitInstance& a, const CombatUnitInstance& b)
    {
        return IsCombatUnitAlive(a) && IsCombatUnitAlive(b) && a.m_Army == b.m_Army;
    }

    bool WouldFigureOverlapFriendly(float worldX, float worldZ,
        const std::vector<CombatUnitInstance>& units,
        int selfUnitIndex, int selfFigureIndex)
    {
        const CombatUnitInstance& selfUnit = units[static_cast<size_t>(selfUnitIndex)];
        const float minDistSq = kFigureMinSeparation * kFigureMinSeparation;

        for (int unitIndex = 0; unitIndex < static_cast<int>(units.size()); ++unitIndex)
        {
            const CombatUnitInstance& unit = units[static_cast<size_t>(unitIndex)];
            if (!AreCombatFiguresFriendly(selfUnit, unit))
            {
                continue;
            }

            for (int figureIndex = 0; figureIndex < static_cast<int>(unit.m_Figures.size()); ++figureIndex)
            {
                if (unitIndex == selfUnitIndex && figureIndex == selfFigureIndex)
                {
                    continue;
                }

                const CombatFigure& other = unit.m_Figures[static_cast<size_t>(figureIndex)];
                if (!IsCombatFigureAlive(other))
                {
                    continue;
                }

                const float dx = worldX - other.m_WorldX;
                const float dz = worldZ - other.m_WorldZ;
                if ((dx * dx + dz * dz) < minDistSq)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void ResolveFriendlyFigureOverlap(CombatFigure& figureA, CombatFigure& figureB,
        float dx, float dz, float dist, float minDistance)
    {
        if (dist < 0.001f)
        {
            dx = 1.0f;
            dz = 0.0f;
            dist = 1.0f;
        }

        const float overlap = minDistance - dist;
        const float pushX = (dx / dist) * overlap;
        const float pushZ = (dz / dist) * overlap;
        const float mobilityA = figureA.m_IsMoving ? 1.0f : 0.2f;
        const float mobilityB = figureB.m_IsMoving ? 1.0f : 0.2f;
        const float invMobilitySum = 1.0f / (mobilityA + mobilityB);

        figureA.m_WorldX = ClampWorldCoord(figureA.m_WorldX - pushX * mobilityA * invMobilitySum);
        figureA.m_WorldZ = ClampWorldCoord(figureA.m_WorldZ - pushZ * mobilityA * invMobilitySum);
        figureB.m_WorldX = ClampWorldCoord(figureB.m_WorldX + pushX * mobilityB * invMobilitySum);
        figureB.m_WorldZ = ClampWorldCoord(figureB.m_WorldZ + pushZ * mobilityB * invMobilitySum);
    }

    void ApplyFriendlyArmySeparation(std::vector<CombatUnitInstance>& units)
    {
        const float minDistSq = kFigureMinSeparation * kFigureMinSeparation;

        for (int unitIndexA = 0; unitIndexA < static_cast<int>(units.size()); ++unitIndexA)
        {
            CombatUnitInstance& unitA = units[static_cast<size_t>(unitIndexA)];
            if (!IsCombatUnitAlive(unitA))
            {
                continue;
            }

            for (int figureIndexA = 0; figureIndexA < static_cast<int>(unitA.m_Figures.size()); ++figureIndexA)
            {
                CombatFigure& figureA = unitA.m_Figures[static_cast<size_t>(figureIndexA)];
                if (!IsCombatFigureAlive(figureA))
                {
                    continue;
                }

                for (int unitIndexB = unitIndexA; unitIndexB < static_cast<int>(units.size()); ++unitIndexB)
                {
                    CombatUnitInstance& unitB = units[static_cast<size_t>(unitIndexB)];
                    if (!AreCombatFiguresFriendly(unitA, unitB))
                    {
                        continue;
                    }

                    const int startFigureIndexB = (unitIndexA == unitIndexB)
                        ? figureIndexA + 1
                        : 0;

                    for (int figureIndexB = startFigureIndexB;
                        figureIndexB < static_cast<int>(unitB.m_Figures.size());
                        ++figureIndexB)
                    {
                        CombatFigure& figureB = unitB.m_Figures[static_cast<size_t>(figureIndexB)];
                        if (!IsCombatFigureAlive(figureB))
                        {
                            continue;
                        }

                        float dx = figureB.m_WorldX - figureA.m_WorldX;
                        float dz = figureB.m_WorldZ - figureA.m_WorldZ;
                        const float distSq = dx * dx + dz * dz;
                        if (distSq >= minDistSq)
                        {
                            continue;
                        }

                        if (figureA.m_IsMoving && figureB.m_IsMoving)
                        {
                            continue;
                        }

                        const float dist = std::sqrt(distSq);
                        ResolveFriendlyFigureOverlap(figureA, figureB, dx, dz, dist, kFigureMinSeparation);
                    }
                }
            }
        }
    }

    int ResolveNearestHostileUnitIndex(const CombatUnitInstance& unit, const std::vector<CombatUnitInstance>& units)
    {
        int bestUnitIndex = -1;
        float bestDistance = 0.0f;

        for (int unitIndex = 0; unitIndex < static_cast<int>(units.size()); ++unitIndex)
        {
            const CombatUnitInstance& candidateUnit = units[static_cast<size_t>(unitIndex)];
            if (!AreCombatUnitsHostile(unit, candidateUnit))
            {
                continue;
            }

            const float distance = GetCombatUnitDistance(unit, candidateUnit);
            if (bestUnitIndex < 0 || distance < bestDistance)
            {
                bestUnitIndex = unitIndex;
                bestDistance = distance;
            }
        }

        return bestUnitIndex;
    }

    int ResolveFigureHostileUnitIndex(const CombatUnitInstance& unit, int preferredTargetUnitIndex,
        const std::vector<CombatUnitInstance>& units)
    {
        if (preferredTargetUnitIndex >= 0
            && preferredTargetUnitIndex < static_cast<int>(units.size())
            && IsCombatUnitAlive(units[static_cast<size_t>(preferredTargetUnitIndex)])
            && AreCombatUnitsHostile(unit, units[static_cast<size_t>(preferredTargetUnitIndex)]))
        {
            return preferredTargetUnitIndex;
        }

        return ResolveNearestHostileUnitIndex(unit, units);
    }

    bool FindNearestHostileFigure(const CombatUnitInstance& unit, const CombatFigure& figure,
        int preferredTargetUnitIndex, const std::vector<CombatUnitInstance>& units,
        int& outUnitIndex, int& outFigureIndex, float& outDistance)
    {
        outUnitIndex = -1;
        outFigureIndex = -1;
        outDistance = 0.0f;

        const int targetUnitIndex = ResolveFigureHostileUnitIndex(unit, preferredTargetUnitIndex, units);
        if (targetUnitIndex < 0)
        {
            return false;
        }

        const CombatUnitInstance& targetUnit = units[static_cast<size_t>(targetUnitIndex)];
        for (int figureIndex = 0; figureIndex < static_cast<int>(targetUnit.m_Figures.size()); ++figureIndex)
        {
            const CombatFigure& candidate = targetUnit.m_Figures[static_cast<size_t>(figureIndex)];
            if (!IsCombatFigureAlive(candidate))
            {
                continue;
            }

            const float dx = candidate.m_WorldX - figure.m_WorldX;
            const float dz = candidate.m_WorldZ - figure.m_WorldZ;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (outUnitIndex < 0 || distance < outDistance)
            {
                outUnitIndex = targetUnitIndex;
                outFigureIndex = figureIndex;
                outDistance = distance;
            }
        }

        return outUnitIndex >= 0;
    }

    bool IsCombatUnitInCombatRange(const CombatUnitInstance& unit, const std::vector<CombatUnitInstance>& units)
    {
        if (!CanCombatUnitAttack(unit))
        {
            return false;
        }

        const float attackRange = GetCombatUnitAttackRange(unit.m_Type);
        for (const CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            int hostileUnitIndex = -1;
            int hostileFigureIndex = -1;
            float hostileDistance = 0.0f;
            if (FindNearestHostileFigure(
                    unit,
                    figure,
                    -1,
                    units,
                    hostileUnitIndex,
                    hostileFigureIndex,
                    hostileDistance)
                && hostileDistance <= attackRange)
            {
                return true;
            }
        }

        return false;
    }

    void UpdateCombatUnitFormationRecovery(CombatUnitInstance& unit, const std::vector<CombatUnitInstance>& units)
    {
        if (!IsCombatUnitAlive(unit) || unit.m_Type == CombatUnitType::Catapult)
        {
            return;
        }

        if (unit.m_AttackTargetUnitIndex >= 0)
        {
            return;
        }

        if (!IsCombatUnitPlayerControlled(unit) && IsCombatUnitInCombatRange(unit, units))
        {
            return;
        }

        if (IsUnitExecutingFormationMove(unit))
        {
            return;
        }

        const float reformAnchorX = unit.m_MoveTargetAnchor.x;
        const float reformAnchorZ = unit.m_MoveTargetAnchor.z;
        const float facingAngle = unit.m_FacingAngle;

        if (IsUnitInFormationAtAnchor(unit, reformAnchorX, reformAnchorZ, facingAngle))
        {
            unit.m_Anchor.x = reformAnchorX;
            unit.m_Anchor.z = reformAnchorZ;
            unit.m_IsMoving = false;
            return;
        }

        bool anyMoving = false;
        for (CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            if (IsCombatFigureNearFormationSlot(unit, figure, reformAnchorX, reformAnchorZ, facingAngle))
            {
                figure.m_IsMoving = false;
                figure.m_MoveTargetX = figure.m_WorldX;
                figure.m_MoveTargetZ = figure.m_WorldZ;
                continue;
            }

            const Vector2 slotPosition = GetFigureFormationWorldXZ(
                unit,
                figure,
                reformAnchorX,
                reformAnchorZ,
                facingAngle);
            figure.m_MoveTargetX = ClampWorldCoord(slotPosition.x);
            figure.m_MoveTargetZ = ClampWorldCoord(slotPosition.y);

            const float dx = figure.m_MoveTargetX - figure.m_WorldX;
            const float dz = figure.m_MoveTargetZ - figure.m_WorldZ;
            figure.m_IsMoving = (dx * dx + dz * dz) > 0.05f;
            anyMoving = anyMoving || figure.m_IsMoving;
        }

        unit.m_IsMoving = anyMoving;
    }

    bool CanFigureStopAt(float worldX, float worldZ,
        const std::vector<CombatUnitInstance>& units,
        int selfUnitIndex, int selfFigureIndex)
    {
        return !WouldFigureOverlapFriendly(worldX, worldZ, units, selfUnitIndex, selfFigureIndex);
    }

    void UpdateCombatFigureMovement(const std::vector<CombatUnitInstance>& units,
        int unitIndex, CombatUnitInstance& unit, int figureIndex, CombatFigure& figure, float deltaTime)
    {
        if (!figure.m_IsMoving || !IsCombatFigureAlive(figure))
        {
            return;
        }

        const float dx = figure.m_MoveTargetX - figure.m_WorldX;
        const float dz = figure.m_MoveTargetZ - figure.m_WorldZ;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= 0.05f)
        {
            if (CanFigureStopAt(figure.m_MoveTargetX, figure.m_MoveTargetZ, units, unitIndex, figureIndex))
            {
                figure.m_WorldX = figure.m_MoveTargetX;
                figure.m_WorldZ = figure.m_MoveTargetZ;
            }

            figure.m_IsMoving = false;
            return;
        }

        figure.m_TargetFacingAngle = FacingAngleFromVector(dx, dz);

        const float moveStep = GetCombatUnitMoveSpeed(unit.m_Type) * deltaTime;
        if (moveStep >= distance)
        {
            if (CanFigureStopAt(figure.m_MoveTargetX, figure.m_MoveTargetZ, units, unitIndex, figureIndex))
            {
                figure.m_WorldX = figure.m_MoveTargetX;
                figure.m_WorldZ = figure.m_MoveTargetZ;
            }

            figure.m_IsMoving = false;
            return;
        }

        const float invDistance = 1.0f / distance;
        figure.m_WorldX += dx * invDistance * moveStep;
        figure.m_WorldZ += dz * invDistance * moveStep;
    }

    float GetCombatFormationLineExtent(CombatUnitType type)
    {
        const CombatUnitFormation& formation = GetCombatUnitFormation(type);
        return ((formation.m_Rows - 1) * 0.5f) * formation.m_RowSpacing;
    }

    int ResolveCombatTargetUnitIndex(const CombatUnitInstance& attacker, const std::vector<CombatUnitInstance>& units,
        const RegionHeightfield& heightfield)
    {
        if (!CanCombatUnitAttack(attacker))
        {
            return -1;
        }

        if (attacker.m_AttackTargetUnitIndex >= 0
            && attacker.m_AttackTargetUnitIndex < static_cast<int>(units.size()))
        {
            const CombatUnitInstance& preferredTarget = units[static_cast<size_t>(attacker.m_AttackTargetUnitIndex)];
            if (AreCombatUnitsHostile(attacker, preferredTarget)
                && CanCombatFiguresEngage(attacker, preferredTarget, heightfield))
            {
                return attacker.m_AttackTargetUnitIndex;
            }

            if (IsCombatUnitPlayerControlled(attacker))
            {
                return -1;
            }
        }

        if (IsCombatUnitPlayerControlled(attacker))
        {
            return -1;
        }

        int bestTargetIndex = -1;
        float bestDistance = 0.0f;
        for (int targetIndex = 0; targetIndex < static_cast<int>(units.size()); ++targetIndex)
        {
            const CombatUnitInstance& target = units[static_cast<size_t>(targetIndex)];
            if (!AreCombatUnitsHostile(attacker, target)
                || !CanCombatFiguresEngage(attacker, target, heightfield))
            {
                continue;
            }

            const float distance = GetCombatUnitDistance(attacker, target);
            if (bestTargetIndex < 0 || distance < bestDistance)
            {
                bestTargetIndex = targetIndex;
                bestDistance = distance;
            }
        }

        return bestTargetIndex;
    }

    Vector3 GetProjectilePosition(const CombatProjectile& projectile)
    {
        const float t = projectile.m_TravelTime > 0.0f
            ? std::clamp(projectile.m_Elapsed / projectile.m_TravelTime, 0.0f, 1.0f)
            : 1.0f;

        Vector3 position = Vector3Lerp(projectile.m_StartPosition, projectile.m_EndPosition, t);
        const float arcHeight = 2.2f + Vector3Distance(projectile.m_StartPosition, projectile.m_EndPosition) * 0.08f;
        position.y += std::sinf(t * 3.14159265f) * arcHeight;
        return position;
    }

    void DrawFigureHealthBar(const Camera3D& camera, Vector3 worldPosition, float spriteHeight, int currentHP, int maxHP)
    {
        const Vector3 markerPosition{
            worldPosition.x,
            worldPosition.y + spriteHeight + 0.35f,
            worldPosition.z
        };
        const Vector2 screenPosition = GetCombatWorldToScreen(markerPosition, camera);
        if (screenPosition.x < -20.0f || screenPosition.y < -20.0f
            || screenPosition.x > static_cast<float>(g_Engine->m_RenderWidth) + 20.0f
            || screenPosition.y > static_cast<float>(g_Engine->m_RenderHeight) + 20.0f)
        {
            return;
        }

        constexpr int kBarWidth = 14;
        constexpr int kBarHeight = 3;
        const int barX = static_cast<int>(screenPosition.x - (kBarWidth * 0.5f));
        const int barY = static_cast<int>(screenPosition.y - 6.0f);
        const float healthFraction = maxHP > 0
            ? static_cast<float>(currentHP) / static_cast<float>(maxHP)
            : 0.0f;
        const int fillWidth = static_cast<int>(kBarWidth * healthFraction);

        Color fillColor = Color{ 80, 200, 90, 255 };
        if (healthFraction <= 0.25f)
        {
            fillColor = Color{ 220, 70, 60, 255 };
        }
        else if (healthFraction <= 0.5f)
        {
            fillColor = Color{ 220, 180, 50, 255 };
        }

        DrawRectangle(barX, barY, kBarWidth, kBarHeight, Color{ 24, 26, 32, 200 });
        if (fillWidth > 0)
        {
            DrawRectangle(barX, barY, fillWidth, kBarHeight, fillColor);
        }
        DrawRectangleLines(barX, barY, kBarWidth, kBarHeight, Color{ 0, 0, 0, 180 });
    }

    void MaybeAssignCombatRetaliationTarget(CombatUnitInstance& victim, int attackerUnitIndex,
        const std::vector<CombatUnitInstance>& units)
    {
        if (!CanCombatUnitAttack(victim) || victim.m_AttackTargetUnitIndex >= 0)
        {
            return;
        }

        if (attackerUnitIndex < 0 || attackerUnitIndex >= static_cast<int>(units.size()))
        {
            return;
        }

        const CombatUnitInstance& attacker = units[static_cast<size_t>(attackerUnitIndex)];
        if (!IsCombatUnitAlive(attacker) || !AreCombatUnitsHostile(victim, attacker))
        {
            return;
        }

        victim.m_AttackTargetUnitIndex = attackerUnitIndex;
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

int GetCombatUnitHitPointsPerFigure(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Archers:
        return 3;
    case CombatUnitType::Swordsmen:
        return 5;
    case CombatUnitType::Knights:
        return 20;
    case CombatUnitType::Catapult:
        return 30;
    default:
        return 5;
    }
}

int GetCombatFigureAttackDamage(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Archers:
        return 1;
    case CombatUnitType::Swordsmen:
        return 2;
    case CombatUnitType::Knights:
        return 8;
    case CombatUnitType::Catapult:
    default:
        return 0;
    }
}

int GetCombatUnitMaxHitPoints(const CombatUnitInstance& unit)
{
    return GetCombatUnitHitPointsPerFigure(unit.m_Type) * GetCombatUnitSoldierCount(unit.m_Type);
}

int GetCombatUnitCurrentHitPoints(const CombatUnitInstance& unit)
{
    int totalHP = 0;
    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (IsCombatFigureAlive(figure))
        {
            totalHP += figure.m_CurrentHP;
        }
    }

    return totalHP;
}

int GetCombatUnitLivingSoldierCount(const CombatUnitInstance& unit)
{
    int livingCount = 0;
    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (IsCombatFigureAlive(figure))
        {
            ++livingCount;
        }
    }

    return livingCount;
}

int GetCombatUnitAttackDamage(const CombatUnitInstance& unit)
{
    return GetCombatFigureAttackDamage(unit.m_Type) * GetCombatUnitLivingSoldierCount(unit);
}

float GetCombatUnitAttackRange(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Archers:
        return 18.0f;
    case CombatUnitType::Swordsmen:
        return 2.8f;
    case CombatUnitType::Knights:
        return 3.5f;
    case CombatUnitType::Catapult:
        return 36.0f;
    default:
        return 2.5f;
    }
}

float GetCombatUnitAttackCooldown(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Archers:
        return 2.0f;
    case CombatUnitType::Swordsmen:
        return 1.2f;
    case CombatUnitType::Knights:
        return 1.5f;
    case CombatUnitType::Catapult:
        return 4.0f;
    default:
        return 1.5f;
    }
}

bool IsCombatFigureAlive(const CombatFigure& figure)
{
    return figure.m_CurrentHP > 0;
}

bool IsCombatUnitAlive(const CombatUnitInstance& unit)
{
    return GetCombatUnitLivingSoldierCount(unit) > 0;
}

bool IsCombatUnitPlayerControlled(const CombatUnitInstance& unit)
{
    return unit.m_Army == LittlePeopleArmy::Red;
}

bool CanCombatUnitAttack(const CombatUnitInstance& unit)
{
    return IsCombatUnitAlive(unit) && unit.m_Type != CombatUnitType::Catapult;
}

bool AreCombatUnitsHostile(const CombatUnitInstance& a, const CombatUnitInstance& b)
{
    return IsCombatUnitAlive(a) && IsCombatUnitAlive(b) && a.m_Army != b.m_Army;
}

float GetCombatUnitDistance(const CombatUnitInstance& a, const CombatUnitInstance& b)
{
    const float dx = a.m_Anchor.x - b.m_Anchor.x;
    const float dz = a.m_Anchor.z - b.m_Anchor.z;
    return std::sqrt(dx * dx + dz * dz);
}

float GetCombatFigureDistance(const CombatUnitInstance& a, const CombatFigure& aFigure,
    const CombatUnitInstance& b, const CombatFigure& bFigure, const RegionHeightfield& heightfield)
{
    const Vector3 aPosition = GetCombatFigureWorldPosition(a, aFigure, heightfield);
    const Vector3 bPosition = GetCombatFigureWorldPosition(b, bFigure, heightfield);
    const float dx = aPosition.x - bPosition.x;
    const float dz = aPosition.z - bPosition.z;
    return std::sqrt(dx * dx + dz * dz);
}

bool CanCombatFiguresEngage(const CombatUnitInstance& attacker, const CombatUnitInstance& target,
    const RegionHeightfield& heightfield)
{
    if (!AreCombatUnitsHostile(attacker, target))
    {
        return false;
    }

    const float attackRange = GetCombatUnitAttackRange(attacker.m_Type);
    for (const CombatFigure& attackerFigure : attacker.m_Figures)
    {
        if (!IsCombatFigureAlive(attackerFigure))
        {
            continue;
        }

        for (const CombatFigure& targetFigure : target.m_Figures)
        {
            if (!IsCombatFigureAlive(targetFigure))
            {
                continue;
            }

            const float distance = GetCombatFigureDistance(
                attacker,
                attackerFigure,
                target,
                targetFigure,
                heightfield);
            if (distance <= attackRange)
            {
                return true;
            }
        }
    }

    return false;
}

Vector3 GetCombatApproachPosition(const CombatUnitInstance& attacker, const CombatUnitInstance& target)
{
    const float dx = target.m_Anchor.x - attacker.m_Anchor.x;
    const float dz = target.m_Anchor.z - attacker.m_Anchor.z;
    const float distance = std::sqrt(dx * dx + dz * dz);

    const float attackRange = GetCombatUnitAttackRange(attacker.m_Type);
    const float attackerExtent = GetCombatFormationLineExtent(attacker.m_Type);
    const float targetExtent = GetCombatFormationLineExtent(target.m_Type);
    const float desiredRange = std::max(
        attackRange * 0.5f,
        attackRange - attackerExtent - targetExtent - 0.5f);

    if (distance <= desiredRange || distance < 0.01f)
    {
        return attacker.m_Anchor;
    }

    const float invDistance = 1.0f / distance;
    return Vector3{
        target.m_Anchor.x - dx * invDistance * desiredRange,
        0.0f,
        target.m_Anchor.z - dz * invDistance * desiredRange
    };
}

void InitCombatUnitHealth(CombatUnitInstance& unit)
{
    unit.m_AttackTargetUnitIndex = -1;
    unit.m_Figures.clear();

    const int soldierCount = GetCombatUnitSoldierCount(unit.m_Type);
    const int hpPerFigure = GetCombatUnitHitPointsPerFigure(unit.m_Type);
    unit.m_Figures.reserve(static_cast<size_t>(soldierCount));

    for (int slotIndex = 0; slotIndex < soldierCount; ++slotIndex)
    {
        int row = 0;
        int column = 0;
        GetCombatUnitFormationSlot(unit.m_Type, slotIndex, row, column);

        CombatFigure figure{};
        figure.m_CurrentHP = hpPerFigure;
        figure.m_FormationRow = row;
        figure.m_FormationColumn = column;
        figure.m_AttackCooldownRemaining = GetRandomValue(0, 500) / 1000.0f;
        unit.m_Figures.push_back(figure);
    }

    SyncCombatUnitFacingAngles(unit);
    for (CombatFigure& figure : unit.m_Figures)
    {
        InitializeFigureWorldState(unit, figure);
    }

    unit.m_MoveTargetAnchor.x = unit.m_Anchor.x;
    unit.m_MoveTargetAnchor.y = 0.0f;
    unit.m_MoveTargetAnchor.z = unit.m_Anchor.z;
}

void CompactCombatUnitFormation(CombatUnitInstance& unit)
{
    if (unit.m_Type == CombatUnitType::Catapult)
    {
        return;
    }

    const CombatUnitFormation& formation = GetCombatUnitFormation(unit.m_Type);
    for (int column = 0; column < formation.m_Columns; ++column)
    {
        std::vector<CombatFigure*> livingFiguresInColumn;
        livingFiguresInColumn.reserve(static_cast<size_t>(formation.m_Rows));

        for (CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure) || figure.m_FormationColumn != column)
            {
                continue;
            }

            livingFiguresInColumn.push_back(&figure);
        }

        std::sort(livingFiguresInColumn.begin(), livingFiguresInColumn.end(),
            [](const CombatFigure* left, const CombatFigure* right)
            {
                return left->m_FormationRow < right->m_FormationRow;
            });

        for (int newRow = 0; newRow < static_cast<int>(livingFiguresInColumn.size()); ++newRow)
        {
            livingFiguresInColumn[static_cast<size_t>(newRow)]->m_FormationRow = newRow;
        }
    }
}

void ApplyCombatFigureDamage(CombatUnitInstance& unit, int figureIndex, int damage,
    int attackerUnitIndex, const std::vector<CombatUnitInstance>& units)
{
    if (damage <= 0 || figureIndex < 0 || figureIndex >= static_cast<int>(unit.m_Figures.size()))
    {
        return;
    }

    CombatFigure& figure = unit.m_Figures[static_cast<size_t>(figureIndex)];
    if (!IsCombatFigureAlive(figure))
    {
        return;
    }

    figure.m_CurrentHP -= damage;
    if (figure.m_CurrentHP < 0)
    {
        figure.m_CurrentHP = 0;
    }

    MaybeAssignCombatRetaliationTarget(unit, attackerUnitIndex, units);
}

void ClearCombatUnitAttackTarget(CombatUnitInstance& unit)
{
    unit.m_AttackTargetUnitIndex = -1;

    for (CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        figure.m_IsMoving = false;
        figure.m_MoveTargetX = figure.m_WorldX;
        figure.m_MoveTargetZ = figure.m_WorldZ;
    }

    unit.m_IsMoving = false;
}

void BeginCombatUnitAttackMove(CombatUnitInstance& attacker, int targetUnitIndex, const std::vector<CombatUnitInstance>& units)
{
    if (!CanCombatUnitAttack(attacker))
    {
        return;
    }

    if (targetUnitIndex < 0 || targetUnitIndex >= static_cast<int>(units.size()))
    {
        return;
    }

    const CombatUnitInstance& target = units[static_cast<size_t>(targetUnitIndex)];
    if (!AreCombatUnitsHostile(attacker, target))
    {
        return;
    }

    attacker.m_AttackTargetUnitIndex = targetUnitIndex;
    const Vector3 approachPosition = GetCombatApproachPosition(attacker, target);
    BeginCombatUnitMove(attacker, approachPosition, false);
}

void UpdateCombatUnitsAttackOrders(std::vector<CombatUnitInstance>& units, const RegionHeightfield& heightfield)
{
    for (CombatUnitInstance& unit : units)
    {
        if (!CanCombatUnitAttack(unit) || unit.m_AttackTargetUnitIndex < 0)
        {
            continue;
        }

        if (unit.m_AttackTargetUnitIndex >= static_cast<int>(units.size())
            || !IsCombatUnitAlive(units[static_cast<size_t>(unit.m_AttackTargetUnitIndex)]))
        {
            ClearCombatUnitAttackTarget(unit);
            continue;
        }

        const CombatUnitInstance& target = units[static_cast<size_t>(unit.m_AttackTargetUnitIndex)];
        if (!AreCombatUnitsHostile(unit, target))
        {
            ClearCombatUnitAttackTarget(unit);
            continue;
        }

        const float attackRange = GetCombatUnitAttackRange(unit.m_Type);
        bool anyMoving = false;

        for (CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            int hostileUnitIndex = -1;
            int hostileFigureIndex = -1;
            float hostileDistance = 0.0f;
            if (!FindNearestHostileFigure(
                    unit,
                    figure,
                    unit.m_AttackTargetUnitIndex,
                    units,
                    hostileUnitIndex,
                    hostileFigureIndex,
                    hostileDistance))
            {
                figure.m_IsMoving = false;
                continue;
            }

            const CombatFigure& hostileFigure = units[static_cast<size_t>(hostileUnitIndex)]
                .m_Figures[static_cast<size_t>(hostileFigureIndex)];
            FaceCombatFigureToward(figure, hostileFigure.m_WorldX, hostileFigure.m_WorldZ);

            if (hostileDistance <= attackRange)
            {
                figure.m_IsMoving = false;
                continue;
            }

            const float advanceDistance = std::max(0.0f, hostileDistance - (attackRange * 0.85f));
            if (advanceDistance <= 0.05f)
            {
                figure.m_IsMoving = false;
                continue;
            }

            const float dx = hostileFigure.m_WorldX - figure.m_WorldX;
            const float dz = hostileFigure.m_WorldZ - figure.m_WorldZ;
            const float invDistance = 1.0f / hostileDistance;
            figure.m_MoveTargetX = ClampWorldCoord(figure.m_WorldX + (dx * invDistance * advanceDistance));
            figure.m_MoveTargetZ = ClampWorldCoord(figure.m_WorldZ + (dz * invDistance * advanceDistance));
            figure.m_IsMoving = true;
            anyMoving = true;
        }

        unit.m_IsMoving = anyMoving;
        SyncCombatUnitAnchorFromFigures(unit);
    }
}

void UpdateCombatUnitsCombat(std::vector<CombatUnitInstance>& units, std::vector<CombatProjectile>& projectiles,
    const RegionHeightfield& heightfield, float deltaTime)
{
    for (int attackerUnitIndex = 0; attackerUnitIndex < static_cast<int>(units.size()); ++attackerUnitIndex)
    {
        CombatUnitInstance& attacker = units[static_cast<size_t>(attackerUnitIndex)];
        if (!CanCombatUnitAttack(attacker))
        {
            continue;
        }

        const int targetUnitIndex = ResolveCombatTargetUnitIndex(attacker, units, heightfield);
        if (targetUnitIndex < 0)
        {
            continue;
        }

        CombatUnitInstance& targetUnit = units[static_cast<size_t>(targetUnitIndex)];

        const int damagePerFigure = GetCombatFigureAttackDamage(attacker.m_Type);
        if (damagePerFigure <= 0)
        {
            continue;
        }

        for (CombatFigure& attackerFigure : attacker.m_Figures)
        {
            if (!IsCombatFigureAlive(attackerFigure))
            {
                continue;
            }

            if (attackerFigure.m_AttackCooldownRemaining > 0.0f)
            {
                attackerFigure.m_AttackCooldownRemaining -= deltaTime;
                if (attackerFigure.m_AttackCooldownRemaining < 0.0f)
                {
                    attackerFigure.m_AttackCooldownRemaining = 0.0f;
                }
            }

            if (attackerFigure.m_AttackCooldownRemaining > 0.0f)
            {
                continue;
            }

            int hostileUnitIndex = -1;
            int targetFigureIndex = -1;
            float hostileDistance = 0.0f;
            if (!FindNearestHostileFigure(
                    attacker,
                    attackerFigure,
                    attacker.m_AttackTargetUnitIndex >= 0 ? attacker.m_AttackTargetUnitIndex : targetUnitIndex,
                    units,
                    hostileUnitIndex,
                    targetFigureIndex,
                    hostileDistance))
            {
                continue;
            }

            if (hostileDistance > GetCombatUnitAttackRange(attacker.m_Type))
            {
                continue;
            }

            CombatUnitInstance& resolvedTargetUnit = units[static_cast<size_t>(hostileUnitIndex)];
            const CombatFigure& targetFigure = resolvedTargetUnit.m_Figures[static_cast<size_t>(targetFigureIndex)];
            FaceCombatFigureToward(attackerFigure, targetFigure.m_WorldX, targetFigure.m_WorldZ);

            if (attacker.m_Type == CombatUnitType::Archers)
            {
                const Vector3 startPosition = GetCombatFigureDrawPosition(attacker, attackerFigure, heightfield);
                const Vector3 endPosition = GetCombatFigureDrawPosition(resolvedTargetUnit, targetFigure, heightfield);

                CombatProjectile projectile{};
                projectile.m_StartPosition = startPosition;
                projectile.m_EndPosition = endPosition;
                projectile.m_TravelTime = 0.24f + hostileDistance * 0.03f;
                projectile.m_SourceUnitIndex = attackerUnitIndex;
                projectile.m_TargetUnitIndex = hostileUnitIndex;
                projectile.m_TargetFigureIndex = targetFigureIndex;
                projectile.m_Damage = damagePerFigure;
                projectile.m_Active = true;
                projectiles.push_back(projectile);
            }
            else
            {
                ApplyCombatFigureDamage(
                    resolvedTargetUnit,
                    targetFigureIndex,
                    damagePerFigure,
                    attackerUnitIndex,
                    units);
            }

            const float cooldownJitter = static_cast<float>(GetRandomValue(0, 250)) / 1000.0f;
            attackerFigure.m_AttackCooldownRemaining = GetCombatUnitAttackCooldown(attacker.m_Type) + cooldownJitter;
        }
    }
}

void UpdateCombatProjectiles(std::vector<CombatProjectile>& projectiles, std::vector<CombatUnitInstance>& units, float deltaTime)
{
    for (CombatProjectile& projectile : projectiles)
    {
        if (!projectile.m_Active)
        {
            continue;
        }

        projectile.m_Elapsed += deltaTime;
        if (projectile.m_Elapsed < projectile.m_TravelTime)
        {
            continue;
        }

        projectile.m_Active = false;
        if (projectile.m_TargetUnitIndex < 0
            || projectile.m_TargetUnitIndex >= static_cast<int>(units.size()))
        {
            continue;
        }

        CombatUnitInstance& targetUnit = units[static_cast<size_t>(projectile.m_TargetUnitIndex)];
        if (IsCombatUnitAlive(targetUnit)
            && projectile.m_TargetFigureIndex >= 0
            && projectile.m_TargetFigureIndex < static_cast<int>(targetUnit.m_Figures.size()))
        {
            ApplyCombatFigureDamage(
                targetUnit,
                projectile.m_TargetFigureIndex,
                projectile.m_Damage,
                projectile.m_SourceUnitIndex,
                units);
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const CombatProjectile& projectile) { return !projectile.m_Active; }),
        projectiles.end());
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

Vector3 TransformCombatFormationOffset(float facingAngleRadians, float localX, float localZ)
{
    const float cosAngle = std::cosf(facingAngleRadians);
    const float sinAngle = std::sinf(facingAngleRadians);
    return Vector3{
        localX * cosAngle + localZ * sinAngle,
        0.0f,
        -localX * sinAngle + localZ * cosAngle
    };
}

Vector3 TransformCombatFormationOffset(LittlePeopleDirection facing, float localX, float localZ)
{
    return TransformCombatFormationOffset(FacingDirectionToAngle(facing), localX, localZ);
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
    return GetCombatUnitLivingSoldierCount(unit);
}

Vector3 GetCombatFigureWorldPosition(const CombatUnitInstance& unit, const CombatFigure& figure, const RegionHeightfield& heightfield)
{
    (void)unit;
    const float worldY = heightfield.SampleHeight(figure.m_WorldX, figure.m_WorldZ);
    return Vector3{ figure.m_WorldX, worldY, figure.m_WorldZ };
}

Vector3 GetCombatFigureDrawPosition(const CombatUnitInstance& unit, const CombatFigure& figure, const RegionHeightfield& heightfield)
{
    const Vector3 groundPosition = GetCombatFigureWorldPosition(unit, figure, heightfield);
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
        if (!IsCombatUnitAlive(unit))
        {
            continue;
        }

        const float pickRadius = GetCombatUnitPickRadiusPixels(unit.m_Type);

        for (const CombatFigure& figure : unit.m_Figures)
        {
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            const Vector3 drawPosition = GetCombatFigureDrawPosition(unit, figure, heightfield);
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

float GetCombatUnitMoveSpeed(CombatUnitType type)
{
    switch (type)
    {
    case CombatUnitType::Knights:
        return 11.0f;
    case CombatUnitType::Catapult:
        return 4.5f;
    case CombatUnitType::Archers:
        return 8.5f;
    case CombatUnitType::Swordsmen:
    default:
        return 9.0f;
    }
}

bool IsCombatUnitMoving(const CombatUnitInstance& unit)
{
    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (IsCombatFigureAlive(figure) && figure.m_IsMoving)
        {
            return true;
        }
    }

    return false;
}

void FaceCombatUnitToward(CombatUnitInstance& unit, Vector3 worldTarget)
{
    const float dx = worldTarget.x - unit.m_Anchor.x;
    const float dz = worldTarget.z - unit.m_Anchor.z;
    if ((dx * dx + dz * dz) > 0.05f)
    {
        unit.m_Facing = LittlePeopleDirectionFromVector(dx, dz);
        unit.m_TargetFacingAngle = FacingAngleFromVector(dx, dz);
        unit.m_FacingAngle = unit.m_TargetFacingAngle;
    }

    for (CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        FaceCombatFigureToward(figure, worldTarget.x, worldTarget.z);
    }
}

void BeginCombatUnitMove(CombatUnitInstance& unit, Vector3 targetAnchor, bool clearAttackTarget)
{
    if (clearAttackTarget)
    {
        ClearCombatUnitAttackTarget(unit);
    }

    SyncCombatUnitAnchorFromFigures(unit);
    const float formationFacing = ResolveFormationFacingForMove(unit, targetAnchor);
    const Vector3 formationAnchor = ResolveFormationMoveAnchor(unit, targetAnchor);

    if (std::fabs(ShortestAngleDelta(formationFacing, unit.m_FacingAngle)) > 0.1f)
    {
        unit.m_FacingAngle = formationFacing;
        unit.m_TargetFacingAngle = formationFacing;
        unit.m_Facing = VisualFacingFromAngle(formationFacing);
    }

    for (CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        figure.m_FacingAngle = unit.m_FacingAngle;
        figure.m_TargetFacingAngle = unit.m_FacingAngle;
    }

    AssignFigureMoveOrder(unit, formationAnchor, formationFacing);
}

void UpdateCombatUnitsFormationRecovery(std::vector<CombatUnitInstance>& units)
{
    for (CombatUnitInstance& unit : units)
    {
        UpdateCombatUnitFormationRecovery(unit, units);
    }
}

bool IsCombatFigureMoving(const CombatFigure& figure)
{
    return figure.m_IsMoving;
}

void UpdateCombatUnitsMovement(std::vector<CombatUnitInstance>& units, float deltaTime)
{
    for (int unitIndex = 0; unitIndex < static_cast<int>(units.size()); ++unitIndex)
    {
        CombatUnitInstance& unit = units[static_cast<size_t>(unitIndex)];
        if (!IsCombatUnitAlive(unit))
        {
            continue;
        }

        bool anyMoving = false;
        for (int figureIndex = 0; figureIndex < static_cast<int>(unit.m_Figures.size()); ++figureIndex)
        {
            CombatFigure& figure = unit.m_Figures[static_cast<size_t>(figureIndex)];
            if (!IsCombatFigureAlive(figure))
            {
                continue;
            }

            UpdateCombatFigureFacing(figure, deltaTime);
            UpdateCombatFigureMovement(units, unitIndex, unit, figureIndex, figure, deltaTime);
            anyMoving = anyMoving || figure.m_IsMoving;
        }

        unit.m_IsMoving = anyMoving;
    }

    ApplyFriendlyArmySeparation(units);
    ApplyFriendlyArmySeparation(units);

    for (CombatUnitInstance& unit : units)
    {
        if (!IsCombatUnitAlive(unit))
        {
            continue;
        }

        if (IsUnitInFormationAtAnchor(
                unit,
                unit.m_MoveTargetAnchor.x,
                unit.m_MoveTargetAnchor.z,
                unit.m_FacingAngle))
        {
            unit.m_Anchor.x = unit.m_MoveTargetAnchor.x;
            unit.m_Anchor.z = unit.m_MoveTargetAnchor.z;
        }
        else if (unit.m_IsMoving)
        {
            SyncCombatUnitAnchorFromFigures(unit);
        }
    }
}

void AppendCombatUnitBillboardDrawRequests(const Camera3D& camera, const RegionHeightfield& heightfield,
    const CombatUnitInstance& unit, bool selected, std::vector<LittlePersonBillboardDrawRequest>& outRequests)
{
    (void)camera;

    if (!IsCombatUnitAlive(unit) || unit.m_Type == CombatUnitType::Catapult)
    {
        return;
    }

    Color tint = WHITE;
    if (selected)
    {
        tint = Color{ 255, 255, 160, 255 };
    }

    const float spriteHeight = GetCombatUnitSpriteHeight(unit.m_Type);
    const double currentTime = GetTime();
    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        const int figureWalkFrame = figure.m_IsMoving
            ? LittlePeopleWalkFrameFromTime(currentTime)
            : 0;
        const Vector3 groundPosition = GetCombatFigureWorldPosition(unit, figure, heightfield);
        outRequests.push_back(LittlePersonBillboardDrawRequest{
            unit.m_Army,
            VisualFacingFromAngle(figure.m_FacingAngle),
            figureWalkFrame,
            groundPosition,
            spriteHeight,
            tint
        });
    }
}

void DrawCombatUnit(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit,
    int walkFrame, bool selected)
{
    (void)camera;
    (void)walkFrame;
    (void)selected;

    if (!IsCombatUnitAlive(unit) || unit.m_Type != CombatUnitType::Catapult)
    {
        return;
    }

    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        const Vector3 groundPosition = GetCombatFigureWorldPosition(unit, figure, heightfield);
        DrawCatapultPlaceholder(groundPosition);
        break;
    }
}

void DrawCombatUnitSelectionIndicator(const RegionHeightfield& heightfield, const CombatUnitInstance& unit)
{
    if (!IsCombatUnitAlive(unit))
    {
        return;
    }

    float minX = unit.m_Anchor.x;
    float maxX = unit.m_Anchor.x;
    float minZ = unit.m_Anchor.z;
    float maxZ = unit.m_Anchor.z;
    bool hasFigure = false;

    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        minX = std::min(minX, figure.m_WorldX);
        maxX = std::max(maxX, figure.m_WorldX);
        minZ = std::min(minZ, figure.m_WorldZ);
        maxZ = std::max(maxZ, figure.m_WorldZ);
        hasFigure = true;
    }

    if (!hasFigure)
    {
        return;
    }

    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;
    const float halfSpanX = (maxX - minX) * 0.5f + 0.8f;
    const float halfSpanZ = (maxZ - minZ) * 0.5f + 0.8f;
    const float radius = std::max(halfSpanX, halfSpanZ);
    const float terrainY = heightfield.SampleHeight(centerX, centerZ) + 0.08f;
    const Vector3 center{ centerX, terrainY, centerZ };
    DrawCircle3D(center, radius, Vector3{ 0.0f, 1.0f, 0.0f }, 32, Color{ 255, 230, 80, 220 });
}

void DrawCombatUnitHealthMarkers(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit)
{
    if (!IsCombatUnitAlive(unit))
    {
        return;
    }

    const int maxFigureHP = GetCombatUnitHitPointsPerFigure(unit.m_Type);
    const float spriteHeight = GetCombatUnitSpriteHeight(unit.m_Type);
    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        const Vector3 worldPosition = GetCombatFigureWorldPosition(unit, figure, heightfield);
        DrawFigureHealthBar(camera, worldPosition, spriteHeight, figure.m_CurrentHP, maxFigureHP);
    }
}

void DrawCombatProjectiles(const std::vector<CombatProjectile>& projectiles)
{
    for (const CombatProjectile& projectile : projectiles)
    {
        if (!projectile.m_Active)
        {
            continue;
        }

        const Vector3 position = GetProjectilePosition(projectile);
        DrawCube(position, 0.22f, 0.22f, 0.45f, Color{ 120, 82, 48, 255 });
        DrawCubeWires(position, 0.22f, 0.22f, 0.45f, Color{ 60, 40, 24, 255 });
    }
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