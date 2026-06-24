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

struct CombatUnitInstance
{
    int m_Id = -1;
    CombatUnitType m_Type = CombatUnitType::Swordsmen;
    Vector3 m_Anchor{ 0.0f, 0.0f, 0.0f };
    LittlePeopleArmy m_Army = LittlePeopleArmy::Blue;
    LittlePeopleDirection m_Facing = LittlePeopleDirection::South;
    bool m_IsMoving = false;
    Vector3 m_MoveTargetAnchor{ 0.0f, 0.0f, 0.0f };
};

const char* CombatUnitTypeName(CombatUnitType type);
const CombatUnitFormation& GetCombatUnitFormation(CombatUnitType type);
int GetCombatUnitSoldierCount(CombatUnitType type);
bool GetCombatUnitFormationSlot(CombatUnitType type, int slotIndex, int& row, int& column);
Vector2 GetCombatUnitFormationOffset(CombatUnitType type, int row, int column);
Vector3 TransformCombatFormationOffset(LittlePeopleDirection facing, float localX, float localZ);

float GetCombatUnitSpriteHeight(CombatUnitType type);
float GetCombatUnitPickRadiusPixels(CombatUnitType type);
float GetCombatUnitSelectionRadius(CombatUnitType type);
int GetCombatUnitPickSlotCount(const CombatUnitInstance& unit);
Vector3 GetCombatUnitSoldierWorldPosition(const CombatUnitInstance& unit, const RegionHeightfield& heightfield, int slotIndex);
Vector3 GetCombatUnitSoldierDrawPosition(const CombatUnitInstance& unit, const RegionHeightfield& heightfield, int slotIndex);

Vector2 GetCombatScaledMousePosition();
Ray GetCombatMouseRay(const Camera3D& camera);
Vector2 GetCombatWorldToScreen(Vector3 worldPosition, const Camera3D& camera);
bool RaycastCombatTerrain(const Ray& ray, const RegionHeightfield& heightfield, Vector3& outHit);

int PickCombatUnitAtMouse(const Camera3D& camera, const RegionHeightfield& heightfield,
    const std::vector<CombatUnitInstance>& units, Vector2 mousePosition);
float GetCombatUnitMoveSpeed(CombatUnitType type);
bool IsCombatUnitMoving(const CombatUnitInstance& unit);
void FaceCombatUnitToward(CombatUnitInstance& unit, Vector3 worldTarget);
void BeginCombatUnitMove(CombatUnitInstance& unit, Vector3 targetAnchor);
void UpdateCombatUnitMovement(CombatUnitInstance& unit, float deltaTime);
void UpdateCombatUnitsMovement(std::vector<CombatUnitInstance>& units, float deltaTime);

void DrawCombatUnit(const Camera3D& camera, const RegionHeightfield& heightfield, const CombatUnitInstance& unit,
    int walkFrame, bool selected = false);
void DrawCombatUnitSelectionIndicator(const RegionHeightfield& heightfield, const CombatUnitInstance& unit);
void DrawCombatMoveMarker(const RegionHeightfield& heightfield, Vector3 target, Color color);

#endif