#ifndef _COMBATUNITS_H_
#define _COMBATUNITS_H_

#include "LittlePeopleSprites.h"
#include "raylib.h"

#include <vector>

class RegionHeightfield;

enum class CombatUnitType : int
{
    Swordsmen = 0,
    Archers,
    Knights,
    Catapult
};

struct CombatUnitFormation
{
    int m_Rows = 1;
    int m_Columns = 1;
    int m_SoldierCount = 1;
    float m_RowSpacing = 1.2f;
    float m_ColumnSpacing = 1.0f;
};

constexpr int kSwordsmanUnitSize = 20;
constexpr int kSwordsmanFormationRows = 5;
constexpr int kSwordsmanFormationColumns = 4;

constexpr int kArcherUnitSize = 20;
constexpr int kArcherFormationRows = 2;
constexpr int kArcherFormationColumns = 10;

constexpr int kKnightUnitSize = 4;
constexpr int kKnightFormationRows = 1;
constexpr int kKnightFormationColumns = 4;

constexpr int kCatapultUnitSize = 1;

struct CombatFigure
{
    int m_CurrentHP = 0;
    int m_FormationRow = 0;
    int m_FormationColumn = 0;
    float m_AttackCooldownRemaining = 0.0f;
    float m_WorldX = 0.0f;
    float m_WorldZ = 0.0f;
    float m_FacingAngle = 0.0f;
    float m_TargetFacingAngle = 0.0f;
    bool m_IsMoving = false;
    float m_MoveTargetX = 0.0f;
    float m_MoveTargetZ = 0.0f;
};

struct CombatUnitInstance
{
    int m_Id = -1;
    CombatUnitType m_Type = CombatUnitType::Swordsmen;
    Vector3 m_Anchor{ 0.0f, 0.0f, 0.0f };
    LittlePeopleArmy m_Army = LittlePeopleArmy::Blue;
    LittlePeopleDirection m_Facing = LittlePeopleDirection::South;
    float m_FacingAngle = 0.0f;
    float m_TargetFacingAngle = 0.0f;
    bool m_IsMoving = false;
    Vector3 m_MoveTargetAnchor{ 0.0f, 0.0f, 0.0f };
    int m_AttackTargetUnitIndex = -1;
    std::vector<CombatFigure> m_Figures;
};

struct CombatProjectile
{
    Vector3 m_StartPosition{};
    Vector3 m_EndPosition{};
    float m_TravelTime = 0.4f;
    float m_Elapsed = 0.0f;
    int m_SourceUnitIndex = -1;
    int m_TargetUnitIndex = -1;
    int m_TargetFigureIndex = -1;
    int m_Damage = 0;
    bool m_Active = false;
};

const char* CombatUnitTypeName(CombatUnitType type);
const CombatUnitFormation& GetCombatUnitFormation(CombatUnitType type);
int GetCombatUnitSoldierCount(CombatUnitType type);
int GetCombatUnitHitPointsPerFigure(CombatUnitType type);
int GetCombatFigureAttackDamage(CombatUnitType type);
int GetCombatUnitMaxHitPoints(const CombatUnitInstance& unit);
int GetCombatUnitCurrentHitPoints(const CombatUnitInstance& unit);
int GetCombatUnitLivingSoldierCount(const CombatUnitInstance& unit);
int GetCombatUnitAttackDamage(const CombatUnitInstance& unit);
float GetCombatUnitAttackRange(CombatUnitType type);
float GetCombatUnitAttackCooldown(CombatUnitType type);
bool IsCombatFigureAlive(const CombatFigure& figure);
bool IsCombatUnitAlive(const CombatUnitInstance& unit);
bool IsCombatUnitPlayerControlled(const CombatUnitInstance& unit);
bool CanCombatUnitAttack(const CombatUnitInstance& unit);
bool AreCombatUnitsHostile(const CombatUnitInstance& a, const CombatUnitInstance& b);
float GetCombatUnitDistance(const CombatUnitInstance& a, const CombatUnitInstance& b);
float GetCombatFigureDistance(const CombatUnitInstance& a, const CombatFigure& aFigure,
    const CombatUnitInstance& b, const CombatFigure& bFigure, const RegionHeightfield& heightfield);
bool CanCombatFiguresEngage(const CombatUnitInstance& attacker, const CombatUnitInstance& target,
    const RegionHeightfield& heightfield);
Vector3 GetCombatApproachPosition(const CombatUnitInstance& attacker, const CombatUnitInstance& target);
void InitCombatUnitHealth(CombatUnitInstance& unit);
void CompactCombatUnitFormation(CombatUnitInstance& unit);
void ApplyCombatFigureDamage(CombatUnitInstance& unit, int figureIndex, int damage,
    int attackerUnitIndex, const std::vector<CombatUnitInstance>& units);
void ClearCombatUnitAttackTarget(CombatUnitInstance& unit);
void BeginCombatUnitAttackMove(CombatUnitInstance& attacker, int targetUnitIndex, const std::vector<CombatUnitInstance>& units);
void UpdateCombatUnitsAttackOrders(std::vector<CombatUnitInstance>& units, const RegionHeightfield& heightfield);
void UpdateCombatUnitsCombat(std::vector<CombatUnitInstance>& units, std::vector<CombatProjectile>& projectiles,
    const RegionHeightfield& heightfield, float deltaTime);
void UpdateCombatProjectiles(std::vector<CombatProjectile>& projectiles, std::vector<CombatUnitInstance>& units, float deltaTime);
bool GetCombatUnitFormationSlot(CombatUnitType type, int slotIndex, int& row, int& column);
Vector2 GetCombatUnitFormationOffset(CombatUnitType type, int row, int column);
Vector3 TransformCombatFormationOffset(float facingAngleRadians, float localX, float localZ);
Vector3 TransformCombatFormationOffset(LittlePeopleDirection facing, float localX, float localZ);

float GetCombatUnitSpriteHeight(CombatUnitType type);
float GetCombatUnitPickRadiusPixels(CombatUnitType type);
float GetCombatUnitSelectionRadius(CombatUnitType type);
int GetCombatUnitPickSlotCount(const CombatUnitInstance& unit);
Vector3 GetCombatFigureWorldPosition(const CombatUnitInstance& unit, const CombatFigure& figure, const RegionHeightfield& heightfield);
Vector3 GetCombatFigureDrawPosition(const CombatUnitInstance& unit, const CombatFigure& figure, const RegionHeightfield& heightfield);

Vector2 GetCombatScaledMousePosition();
Ray GetCombatMouseRay(const Camera3D& camera);
Vector2 GetCombatWorldToScreen(Vector3 worldPosition, const Camera3D& camera);
bool RaycastCombatTerrain(const Ray& ray, const RegionHeightfield& heightfield, Vector3& outHit);

int PickCombatUnitAtMouse(const Camera3D& camera, const RegionHeightfield& heightfield,
    const std::vector<CombatUnitInstance>& units, Vector2 mousePosition);
float GetCombatUnitMoveSpeed(CombatUnitType type);
bool IsCombatUnitMoving(const CombatUnitInstance& unit);
bool IsCombatFigureMoving(const CombatFigure& figure);
void FaceCombatUnitToward(CombatUnitInstance& unit, Vector3 worldTarget);
void BeginCombatUnitMove(CombatUnitInstance& unit, Vector3 targetAnchor, bool clearAttackTarget = true);
void UpdateCombatUnitsFormationRecovery(std::vector<CombatUnitInstance>& units);
void UpdateCombatUnitsMovement(std::vector<CombatUnitInstance>& units, float deltaTime);

void AppendCombatUnitBillboardDrawRequests(const Camera3D& camera, const RegionHeightfield& heightfield,
    const CombatUnitInstance& unit, bool selected, std::vector<LittlePersonBillboardDrawRequest>& outRequests);
void DrawCombatUnit(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit,
    int walkFrame, bool selected = false);
void DrawCombatUnitSelectionIndicator(const RegionHeightfield& heightfield, const CombatUnitInstance& unit);
void DrawCombatUnitHealthMarkers(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit);
void DrawCombatMoveMarker(const RegionHeightfield& heightfield, Vector3 target, Color color);
void DrawCombatProjectiles(const std::vector<CombatProjectile>& projectiles);

#endif