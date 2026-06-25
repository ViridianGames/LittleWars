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

    float GetCombatFormationLineExtent(CombatUnitType type)
    {
        const CombatUnitFormation& formation = GetCombatUnitFormation(type);
        return ((formation.m_Rows - 1) * 0.5f) * formation.m_RowSpacing;
    }

    int PickRandomLivingFigureIndexInRange(const CombatUnitInstance& attacker, const CombatFigure& attackerFigure,
        const CombatUnitInstance& target, const RegionHeightfield& heightfield)
    {
        const float attackRange = GetCombatUnitAttackRange(attacker.m_Type);
        std::vector<int> inRangeFigureIndices;
        inRangeFigureIndices.reserve(target.m_Figures.size());

        for (int figureIndex = 0; figureIndex < static_cast<int>(target.m_Figures.size()); ++figureIndex)
        {
            const CombatFigure& targetFigure = target.m_Figures[static_cast<size_t>(figureIndex)];
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
                inRangeFigureIndices.push_back(figureIndex);
            }
        }

        if (inRangeFigureIndices.empty())
        {
            return -1;
        }

        const int choice = GetRandomValue(0, static_cast<int>(inRangeFigureIndices.size()) - 1);
        return inRangeFigureIndices[static_cast<size_t>(choice)];
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

void ApplyCombatFigureDamage(CombatUnitInstance& unit, int figureIndex, int damage)
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

    if (!IsCombatFigureAlive(figure))
    {
        CompactCombatUnitFormation(unit);
    }
}

void ClearCombatUnitAttackTarget(CombatUnitInstance& unit)
{
    unit.m_AttackTargetUnitIndex = -1;
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

        FaceCombatUnitToward(unit, target.m_Anchor);

        if (CanCombatFiguresEngage(unit, target, heightfield))
        {
            unit.m_IsMoving = false;
            continue;
        }

        const Vector3 approachPosition = GetCombatApproachPosition(unit, target);
        const float approachDx = approachPosition.x - unit.m_MoveTargetAnchor.x;
        const float approachDz = approachPosition.z - unit.m_MoveTargetAnchor.z;
        if (!unit.m_IsMoving || (approachDx * approachDx + approachDz * approachDz) > 1.0f)
        {
            BeginCombatUnitMove(unit, approachPosition, false);
        }
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
        FaceCombatUnitToward(attacker, targetUnit.m_Anchor);

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

            const int targetFigureIndex = PickRandomLivingFigureIndexInRange(
                attacker,
                attackerFigure,
                targetUnit,
                heightfield);
            if (targetFigureIndex < 0)
            {
                continue;
            }

            const CombatFigure& targetFigure = targetUnit.m_Figures[static_cast<size_t>(targetFigureIndex)];

            if (attacker.m_Type == CombatUnitType::Archers)
            {
                const Vector3 startPosition = GetCombatFigureDrawPosition(attacker, attackerFigure, heightfield);
                const Vector3 endPosition = GetCombatFigureDrawPosition(targetUnit, targetFigure, heightfield);
                const float figureDistance = GetCombatFigureDistance(
                    attacker,
                    attackerFigure,
                    targetUnit,
                    targetFigure,
                    heightfield);

                CombatProjectile projectile{};
                projectile.m_StartPosition = startPosition;
                projectile.m_EndPosition = endPosition;
                projectile.m_TravelTime = 0.24f + figureDistance * 0.03f;
                projectile.m_TargetUnitIndex = targetUnitIndex;
                projectile.m_TargetFigureIndex = targetFigureIndex;
                projectile.m_Damage = damagePerFigure;
                projectile.m_Active = true;
                projectiles.push_back(projectile);
            }
            else
            {
                ApplyCombatFigureDamage(targetUnit, targetFigureIndex, damagePerFigure);
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
            ApplyCombatFigureDamage(targetUnit, projectile.m_TargetFigureIndex, projectile.m_Damage);
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
    return GetCombatUnitLivingSoldierCount(unit);
}

Vector3 GetCombatFigureWorldPosition(const CombatUnitInstance& unit, const CombatFigure& figure, const RegionHeightfield& heightfield)
{
    if (unit.m_Type == CombatUnitType::Catapult)
    {
        const float y = heightfield.SampleHeight(unit.m_Anchor.x, unit.m_Anchor.z);
        return Vector3{ unit.m_Anchor.x, y, unit.m_Anchor.z };
    }

    const Vector2 localOffset = GetCombatUnitFormationOffset(
        unit.m_Type,
        figure.m_FormationRow,
        figure.m_FormationColumn);
    const Vector3 worldOffset = TransformCombatFormationOffset(unit.m_Facing, localOffset.x, localOffset.y);
    const float worldX = unit.m_Anchor.x + worldOffset.x;
    const float worldZ = unit.m_Anchor.z + worldOffset.z;
    const float worldY = heightfield.SampleHeight(worldX, worldZ);
    return Vector3{ worldX, worldY, worldZ };
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
    return unit.m_IsMoving;
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

void BeginCombatUnitMove(CombatUnitInstance& unit, Vector3 targetAnchor, bool clearAttackTarget)
{
    if (clearAttackTarget)
    {
        ClearCombatUnitAttackTarget(unit);
    }

    FaceCombatUnitToward(unit, targetAnchor);

    unit.m_MoveTargetAnchor.x = std::clamp(targetAnchor.x, 2.0f, static_cast<float>(REGION_CELLS) - 2.0f);
    unit.m_MoveTargetAnchor.z = std::clamp(targetAnchor.z, 2.0f, static_cast<float>(REGION_CELLS) - 2.0f);
    unit.m_MoveTargetAnchor.y = 0.0f;

    const float dx = unit.m_MoveTargetAnchor.x - unit.m_Anchor.x;
    const float dz = unit.m_MoveTargetAnchor.z - unit.m_Anchor.z;
    if ((dx * dx + dz * dz) <= 0.05f)
    {
        unit.m_IsMoving = false;
        return;
    }

    unit.m_IsMoving = true;
}

void UpdateCombatUnitMovement(CombatUnitInstance& unit, float deltaTime)
{
    if (!unit.m_IsMoving)
    {
        return;
    }

    const float dx = unit.m_MoveTargetAnchor.x - unit.m_Anchor.x;
    const float dz = unit.m_MoveTargetAnchor.z - unit.m_Anchor.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.05f)
    {
        unit.m_Anchor.x = unit.m_MoveTargetAnchor.x;
        unit.m_Anchor.z = unit.m_MoveTargetAnchor.z;
        unit.m_IsMoving = false;
        return;
    }

    const float moveStep = GetCombatUnitMoveSpeed(unit.m_Type) * deltaTime;
    if (moveStep >= distance)
    {
        unit.m_Anchor.x = unit.m_MoveTargetAnchor.x;
        unit.m_Anchor.z = unit.m_MoveTargetAnchor.z;
        unit.m_IsMoving = false;
        return;
    }

    const float invDistance = 1.0f / distance;
    unit.m_Anchor.x += dx * invDistance * moveStep;
    unit.m_Anchor.z += dz * invDistance * moveStep;
}

void UpdateCombatUnitsMovement(std::vector<CombatUnitInstance>& units, float deltaTime)
{
    for (CombatUnitInstance& unit : units)
    {
        UpdateCombatUnitMovement(unit, deltaTime);
    }
}

void DrawCombatUnit(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit,
    int walkFrame, bool selected)
{
    Color tint = WHITE;
    if (selected)
    {
        tint = Color{ 255, 255, 160, 255 };
    }

    if (!IsCombatUnitAlive(unit))
    {
        return;
    }

    if (unit.m_Type == CombatUnitType::Catapult)
    {
        const float y = heightfield.SampleHeight(unit.m_Anchor.x, unit.m_Anchor.z);
        DrawCatapultPlaceholder(Vector3{ unit.m_Anchor.x, y, unit.m_Anchor.z });
        return;
    }

    const float spriteHeight = GetCombatUnitSpriteHeight(unit.m_Type);
    for (const CombatFigure& figure : unit.m_Figures)
    {
        if (!IsCombatFigureAlive(figure))
        {
            continue;
        }

        const Vector3 groundPosition = GetCombatFigureWorldPosition(unit, figure, heightfield);
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
    if (!IsCombatUnitAlive(unit))
    {
        return;
    }

    const float terrainY = heightfield.SampleHeight(unit.m_Anchor.x, unit.m_Anchor.z) + 0.08f;
    const float radius = GetCombatUnitSelectionRadius(unit.m_Type);
    const Vector3 center{ unit.m_Anchor.x, terrainY, unit.m_Anchor.z };
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