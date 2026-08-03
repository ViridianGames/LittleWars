#ifndef _LITTLEPEOPLESPRITES_H_
#define _LITTLEPEOPLESPRITES_H_

#include "raylib.h"

#include <vector>

constexpr int LITTLEPEOPLE_CELL_WIDTH = 8;
constexpr int LITTLEPEOPLE_CELL_HEIGHT = 11;
constexpr int LITTLEPEOPLE_ARMY_COUNT = 4;
constexpr int LITTLEPEOPLE_DIRECTION_COUNT = 8;
constexpr int LITTLEPEOPLE_WALK_FRAMES = 2;
constexpr int LITTLEPEOPLE_ANIMATION_ROWS = 16;
constexpr const char* LITTLEPEOPLE_ATLAS_PATH = "Images/littlepeople.png";

// swordsmen4-fixed.png: 32x32 cells, 8 direction rows (S..SW),
// 8 columns per row (col 0 = idle, cols 1-7 = walk cycle).
// Mask recolors pants + shield with army color (same layout as atlas).
constexpr int SWORDSMAN_CELL_WIDTH = 32;
constexpr int SWORDSMAN_CELL_HEIGHT = 32;
constexpr int SWORDSMAN_WALK_FRAMES = 7; // walking columns only (idle is separate)
constexpr int SWORDSMAN_COLUMNS = 8;     // idle + 7 walk
constexpr const char* SWORDSMAN_ATLAS_PATH = "Images/swordsmen4-fixed.png";
constexpr const char* SWORDSMAN_MASK_PATH = "Images/swordsmen4-fixed-mask.png";

// archers-final-shrunk.png: 32x32 cells, 8 direction rows (S..SW),
// 5 columns per row (col 0 = idle, cols 1-4 = walk cycle).
// Mask recolors pants with army color (same layout as atlas).
constexpr int ARCHER_CELL_WIDTH = 32;
constexpr int ARCHER_CELL_HEIGHT = 32;
constexpr int ARCHER_WALK_FRAMES = 4; // walking columns only (idle is separate)
constexpr int ARCHER_COLUMNS = 5;     // idle + 4 walk
constexpr const char* ARCHER_ATLAS_PATH = "Images/archers-final-shrunk.png";
constexpr const char* ARCHER_MASK_PATH = "Images/archers-final-shrunk-mask.png";

enum class LittlePeopleArmy : int
{
    White = 0,
    Blue,
    Red,
    Green
};

// Eight compass directions. Each uses two consecutive rows in the army column.
enum class LittlePeopleDirection : int
{
    South = 0,
    SouthEast,
    East,
    NorthEast,
    North,
    NorthWest,
    West,
    SouthWest
};

enum class LittlePersonMaskedSprite : int
{
    None = 0,
    Swordsman,
    Archer
};

struct LittlePersonBillboardDrawRequest
{
    LittlePeopleArmy m_Army = LittlePeopleArmy::Blue;
    LittlePeopleDirection m_WorldDirection = LittlePeopleDirection::South;
    int m_Frame = 0;
    Vector3 m_GroundPosition{ 0.0f, 0.0f, 0.0f };
    float m_WorldHeight = 1.2f;
    Color m_Tint = WHITE;
    LittlePersonMaskedSprite m_MaskedSprite = LittlePersonMaskedSprite::None;
    bool m_IsMoving = false;
};

int GetLittlePeopleArmyOwnerId(LittlePeopleArmy army);

Color GetLittlePeopleArmyColor(LittlePeopleArmy army);

Rectangle GetLittlePeopleSpriteSourceRect(LittlePeopleArmy army, LittlePeopleDirection direction, int frame);

// Legacy row-reversed layout (unused by new 32x32 sheets; kept for tools/compat).
Rectangle GetMaskedUnitSpriteSourceRect(LittlePeopleDirection direction, int walkFrame, bool isMoving,
    int cellWidth, int sourceHeight, int rowHeight, int walkFrameCount = LITTLEPEOPLE_WALK_FRAMES);

// New unit sheets: row = direction (S=0 .. SW=7), col 0 idle, cols 1+ walk.
Rectangle GetDirectionalUnitSpriteSourceRect(LittlePeopleDirection direction, int walkFrame, bool isMoving,
    int cellWidth, int cellHeight, int walkFrameCount);

Rectangle GetArcherSpriteSourceRect(LittlePeopleDirection direction, int walkFrame, bool isMoving);
Rectangle GetSwordsmanSpriteSourceRect(LittlePeopleDirection direction, int walkFrame, bool isMoving);

Rectangle GetLittlePeopleAtlasSourceRect(int column, int row);

LittlePeopleDirection LittlePeopleDirectionFromVector(float dx, float dz);

// Maps a world-facing direction to the sprite row for the current camera yaw.
LittlePeopleDirection LittlePeopleDirectionForCamera(LittlePeopleDirection worldDirection, const Camera3D& camera);

float LittlePeopleCameraYawFromTarget(const Camera3D& camera);

int LittlePeopleWalkFrameFromTime(double timeSeconds, double stepsPerSecond = 6.0);
int WalkFrameFromTime(double timeSeconds, int frameCount, double stepsPerSecond = 6.0);

void InitLittlePeopleSprites();
void ShutdownLittlePeopleSprites();
Texture* GetLittlePeopleAtlasTexture();

// worldDirection is the unit's facing on the map; sprite row is chosen relative to camera yaw.
void DrawLittlePersonBillboard(const Camera3D& camera, LittlePeopleArmy army, LittlePeopleDirection worldDirection,
    int frame, Vector3 groundPosition, float worldHeight, Color tint = WHITE);

void DrawLittlePeopleBillboardsSorted(const Camera3D& camera,
    const std::vector<LittlePersonBillboardDrawRequest>& requests);

#endif