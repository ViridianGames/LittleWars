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

struct LittlePersonBillboardDrawRequest
{
    LittlePeopleArmy m_Army = LittlePeopleArmy::Blue;
    LittlePeopleDirection m_WorldDirection = LittlePeopleDirection::South;
    int m_Frame = 0;
    Vector3 m_GroundPosition{ 0.0f, 0.0f, 0.0f };
    float m_WorldHeight = 1.2f;
    Color m_Tint = WHITE;
};

Rectangle GetLittlePeopleSpriteSourceRect(LittlePeopleArmy army, LittlePeopleDirection direction, int frame);

Rectangle GetLittlePeopleAtlasSourceRect(int column, int row);

LittlePeopleDirection LittlePeopleDirectionFromVector(float dx, float dz);

// Maps a world-facing direction to the sprite row for the current camera yaw.
LittlePeopleDirection LittlePeopleDirectionForCamera(LittlePeopleDirection worldDirection, const Camera3D& camera);

float LittlePeopleCameraYawFromTarget(const Camera3D& camera);

int LittlePeopleWalkFrameFromTime(double timeSeconds, double stepsPerSecond = 6.0);

void InitLittlePeopleSprites();
void ShutdownLittlePeopleSprites();
Texture* GetLittlePeopleAtlasTexture();

// worldDirection is the unit's facing on the map; sprite row is chosen relative to camera yaw.
void DrawLittlePersonBillboard(const Camera3D& camera, LittlePeopleArmy army, LittlePeopleDirection worldDirection,
    int frame, Vector3 groundPosition, float worldHeight, Color tint = WHITE);

void DrawLittlePeopleBillboardsSorted(const Camera3D& camera,
    const std::vector<LittlePersonBillboardDrawRequest>& requests);

#endif