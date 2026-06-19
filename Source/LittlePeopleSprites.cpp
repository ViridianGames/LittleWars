#include "LittlePeopleSprites.h"

#include <cmath>

#include "Geist/Config.h"
#include "Geist/Globals.h"
#include "Geist/ResourceManager.h"

namespace
{
    bool g_LittlePeopleSpritesInitialized = false;
}

Rectangle GetLittlePeopleSpriteSourceRect(LittlePeopleArmy army, LittlePeopleDirection direction, int frame)
{
    const int column = static_cast<int>(army);
    const int directionIndex = static_cast<int>(direction);
    const int clampedFrame = frame % LITTLEPEOPLE_WALK_FRAMES;
    const int row = directionIndex * LITTLEPEOPLE_WALK_FRAMES + clampedFrame;

    return GetLittlePeopleAtlasSourceRect(column, row);
}

Rectangle GetLittlePeopleAtlasSourceRect(int column, int row)
{
    return Rectangle{
        static_cast<float>(column * LITTLEPEOPLE_CELL_WIDTH),
        static_cast<float>(row * LITTLEPEOPLE_CELL_HEIGHT),
        static_cast<float>(LITTLEPEOPLE_CELL_WIDTH),
        static_cast<float>(LITTLEPEOPLE_CELL_HEIGHT)
    };
}

LittlePeopleDirection LittlePeopleDirectionFromVector(float dx, float dz)
{
    if (std::fabs(dx) < 0.001f && std::fabs(dz) < 0.001f)
    {
        return LittlePeopleDirection::South;
    }

    float angle = std::atan2f(dx, dz);
    if (angle < 0.0f)
    {
        angle += 6.28318530718f;
    }

    const float slice = 6.28318530718f / static_cast<float>(LITTLEPEOPLE_DIRECTION_COUNT);
    int direction = static_cast<int>((angle + slice * 0.5f) / slice) % LITTLEPEOPLE_DIRECTION_COUNT;
    return static_cast<LittlePeopleDirection>(direction);
}

float LittlePeopleCameraYawFromTarget(const Camera3D& camera)
{
    const float dx = camera.position.x - camera.target.x;
    const float dz = camera.position.z - camera.target.z;
    return std::atan2f(dx, dz);
}

LittlePeopleDirection LittlePeopleDirectionForCamera(LittlePeopleDirection worldDirection, const Camera3D& camera)
{
    const float cameraDx = camera.position.x - camera.target.x;
    const float cameraDz = camera.position.z - camera.target.z;
    const int cameraDirection = static_cast<int>(LittlePeopleDirectionFromVector(cameraDx, cameraDz));
    const int worldDirectionIndex = static_cast<int>(worldDirection);
    const int spriteDirection = (worldDirectionIndex - cameraDirection + LITTLEPEOPLE_DIRECTION_COUNT)
        % LITTLEPEOPLE_DIRECTION_COUNT;

    return static_cast<LittlePeopleDirection>(spriteDirection);
}

int LittlePeopleWalkFrameFromTime(double timeSeconds, double stepsPerSecond)
{
    const int frame = static_cast<int>(timeSeconds * stepsPerSecond) % LITTLEPEOPLE_WALK_FRAMES;
    return frame;
}

void InitLittlePeopleSprites()
{
    if (g_LittlePeopleSpritesInitialized)
    {
        return;
    }

    g_ResourceManager->AddTexture(LITTLEPEOPLE_ATLAS_PATH, false);
    g_LittlePeopleSpritesInitialized = true;
}

void ShutdownLittlePeopleSprites()
{
    g_LittlePeopleSpritesInitialized = false;
}

Texture* GetLittlePeopleAtlasTexture()
{
    if (!g_LittlePeopleSpritesInitialized)
    {
        InitLittlePeopleSprites();
    }

    return g_ResourceManager->GetTexture(LITTLEPEOPLE_ATLAS_PATH, false);
}

void DrawLittlePersonBillboard(const Camera3D& camera, LittlePeopleArmy army, LittlePeopleDirection worldDirection,
    int frame, Vector3 groundPosition, float worldHeight, Color tint)
{
    Texture* texture = GetLittlePeopleAtlasTexture();
    if (!texture || texture->id == 0)
    {
        return;
    }

    const LittlePeopleDirection spriteDirection = LittlePeopleDirectionForCamera(worldDirection, camera);
    const Rectangle source = GetLittlePeopleSpriteSourceRect(army, spriteDirection, frame);
    const float aspect = static_cast<float>(LITTLEPEOPLE_CELL_WIDTH) / static_cast<float>(LITTLEPEOPLE_CELL_HEIGHT);
    const Vector2 size = { worldHeight * aspect, worldHeight };
    const Vector3 drawPosition = {
        groundPosition.x,
        groundPosition.y + worldHeight * 0.5f,
        groundPosition.z
    };

    DrawBillboardRec(camera, *texture, source, drawPosition, size, tint);
}