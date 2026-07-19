#include "LittlePeopleSprites.h"

#include <algorithm>
#include <cmath>

#include "../Geist/Source/Config.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/ResourceManager.h"
#include "Player.h"
#include "rlgl.h"

namespace
{
    bool g_LittlePeopleSpritesInitialized = false;
    Shader g_LittlePeopleBillboardShader{};
    bool g_LittlePeopleBillboardShaderReady = false;

    const char* kLittlePeopleBillboardVertexShader =
        "#version 330\n"
        "in vec3 vertexPosition;\n"
        "in vec2 vertexTexCoord;\n"
        "in vec4 vertexColor;\n"
        "out vec2 fragTexCoord;\n"
        "out vec4 fragColor;\n"
        "uniform mat4 mvp;\n"
        "void main()\n"
        "{\n"
        "    fragTexCoord = vertexTexCoord;\n"
        "    fragColor = vertexColor;\n"
        "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
        "}\n";

    const char* kLittlePeopleBillboardFragmentShader =
        "#version 330\n"
        "in vec2 fragTexCoord;\n"
        "in vec4 fragColor;\n"
        "out vec4 finalColor;\n"
        "uniform sampler2D texture0;\n"
        "uniform vec4 colDiffuse;\n"
        "void main()\n"
        "{\n"
        "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
        "    if (texelColor.a < 0.5)\n"
        "    {\n"
        "        discard;\n"
        "    }\n"
        "    finalColor = texelColor * colDiffuse * fragColor;\n"
        "}\n";

    void EnsureLittlePeopleBillboardShader()
    {
        if (g_LittlePeopleBillboardShaderReady)
        {
            return;
        }

        g_LittlePeopleBillboardShader = LoadShaderFromMemory(
            kLittlePeopleBillboardVertexShader,
            kLittlePeopleBillboardFragmentShader);
        g_LittlePeopleBillboardShaderReady = g_LittlePeopleBillboardShader.id != 0;
    }

    float GetLittlePersonBillboardSortDepth(const Camera3D& camera, const LittlePersonBillboardDrawRequest& request)
    {
        const Vector3 drawPosition{
            request.m_GroundPosition.x,
            request.m_GroundPosition.y + request.m_WorldHeight * 0.5f,
            request.m_GroundPosition.z
        };
        const float dx = drawPosition.x - camera.position.x;
        const float dy = drawPosition.y - camera.position.y;
        const float dz = drawPosition.z - camera.position.z;
        return dx * dx + dy * dy + dz * dz;
    }

    void DrawLittlePersonBillboardInternal(const Camera3D& camera, const LittlePersonBillboardDrawRequest& request)
    {
        Texture* texture = GetLittlePeopleAtlasTexture();
        if (!texture || texture->id == 0)
        {
            return;
        }

        const LittlePeopleDirection spriteDirection = LittlePeopleDirectionForCamera(
            request.m_WorldDirection,
            camera);
        const Rectangle source = GetLittlePeopleSpriteSourceRect(
            request.m_Army,
            spriteDirection,
            request.m_Frame);
        const float aspect = static_cast<float>(LITTLEPEOPLE_CELL_WIDTH)
            / static_cast<float>(LITTLEPEOPLE_CELL_HEIGHT);
        const Vector2 size{ request.m_WorldHeight * aspect, request.m_WorldHeight };
        const Vector3 drawPosition{
            request.m_GroundPosition.x,
            request.m_GroundPosition.y + request.m_WorldHeight * 0.5f,
            request.m_GroundPosition.z
        };

        DrawBillboardRec(camera, *texture, source, drawPosition, size, request.m_Tint);
    }

    void DrawMaskedBillboardInternal(const Camera3D& camera, const LittlePersonBillboardDrawRequest& request,
        const char* atlasPath, const char* maskPath, int cellWidth, int sourceHeight, int rowHeight,
        int displayWidth, int displayHeight)
    {
        Texture* texture = g_ResourceManager->GetTexture(atlasPath, false);
        Texture* maskTexture = g_ResourceManager->GetTexture(maskPath, false);
        if (!texture || texture->id == 0 || !maskTexture || maskTexture->id == 0)
        {
            return;
        }

        const LittlePeopleDirection spriteDirection = LittlePeopleDirectionForCamera(
            request.m_WorldDirection,
            camera);
        const Rectangle source = GetMaskedUnitSpriteSourceRect(
            spriteDirection,
            request.m_Frame,
            request.m_IsMoving,
            cellWidth,
            sourceHeight,
            rowHeight);
        const float aspect = static_cast<float>(displayWidth) / static_cast<float>(displayHeight);
        const Vector2 size{ request.m_WorldHeight * aspect, request.m_WorldHeight };
        const Vector3 drawPosition{
            request.m_GroundPosition.x,
            request.m_GroundPosition.y + request.m_WorldHeight * 0.5f,
            request.m_GroundPosition.z
        };

        const Color armyColor = GetLittlePeopleArmyColor(request.m_Army);
        const Color maskTint = ColorTint(armyColor, request.m_Tint);

        DrawBillboardRec(camera, *texture, source, drawPosition, size, WHITE);
        DrawBillboardRec(camera, *maskTexture, source, drawPosition, size, maskTint);
    }
}

int GetLittlePeopleArmyOwnerId(LittlePeopleArmy army)
{
    switch (army)
    {
    case LittlePeopleArmy::Blue:
        return 0;
    case LittlePeopleArmy::Red:
        return 1;
    case LittlePeopleArmy::Green:
        return 4;
    case LittlePeopleArmy::White:
    default:
        return -1;
    }
}

Color GetLittlePeopleArmyColor(LittlePeopleArmy army)
{
    return PlayerOwnerColor(GetLittlePeopleArmyOwnerId(army));
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

Rectangle GetMaskedUnitSpriteSourceRect(LittlePeopleDirection direction, int walkFrame, bool isMoving,
    int cellWidth, int sourceHeight, int rowHeight)
{
    const int directionIndex = static_cast<int>(direction);
    const int sheetRow = (LITTLEPEOPLE_DIRECTION_COUNT - 1) - directionIndex;
    const int frameColumn = isMoving
        ? 1 + (walkFrame % LITTLEPEOPLE_WALK_FRAMES)
        : 0;

    return Rectangle{
        static_cast<float>(frameColumn * cellWidth),
        static_cast<float>(sheetRow * rowHeight),
        static_cast<float>(cellWidth),
        static_cast<float>(sourceHeight)
    };
}

LittlePeopleDirection LittlePeopleDirectionFromVector(float dx, float dz)
{
    if (std::fabs(dx) < 0.001f && std::fabs(dz) < 0.001f)
    {
        return LittlePeopleDirection::South;
    }

    float angle = std::atan2(dx, dz);
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
    return std::atan2(dx, dz);
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
    g_ResourceManager->AddTexture(SWORDSMAN_ATLAS_PATH, false);
    g_ResourceManager->AddTexture(SWORDSMAN_MASK_PATH, false);
    g_ResourceManager->AddTexture(ARCHER_ATLAS_PATH, false);
    g_ResourceManager->AddTexture(ARCHER_MASK_PATH, false);
    EnsureLittlePeopleBillboardShader();
    g_LittlePeopleSpritesInitialized = true;
}

void ShutdownLittlePeopleSprites()
{
    if (g_LittlePeopleBillboardShaderReady)
    {
        UnloadShader(g_LittlePeopleBillboardShader);
        g_LittlePeopleBillboardShader = Shader{};
        g_LittlePeopleBillboardShaderReady = false;
    }

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
    const std::vector<LittlePersonBillboardDrawRequest> requests{
        LittlePersonBillboardDrawRequest{
            army,
            worldDirection,
            frame,
            groundPosition,
            worldHeight,
            tint
        }
    };
    DrawLittlePeopleBillboardsSorted(camera, requests);
}

void DrawLittlePeopleBillboardsSorted(const Camera3D& camera,
    const std::vector<LittlePersonBillboardDrawRequest>& requests)
{
    if (requests.empty())
    {
        return;
    }

    if (!g_LittlePeopleSpritesInitialized)
    {
        InitLittlePeopleSprites();
    }

    std::vector<LittlePersonBillboardDrawRequest> sortedRequests = requests;
    std::sort(sortedRequests.begin(), sortedRequests.end(),
        [&camera](const LittlePersonBillboardDrawRequest& left, const LittlePersonBillboardDrawRequest& right)
        {
            return GetLittlePersonBillboardSortDepth(camera, left) > GetLittlePersonBillboardSortDepth(camera, right);
        });

    EnsureLittlePeopleBillboardShader();

    rlDisableDepthMask();

    bool littlePeopleShaderActive = false;
    for (const LittlePersonBillboardDrawRequest& request : sortedRequests)
    {
        if (!littlePeopleShaderActive && g_LittlePeopleBillboardShaderReady)
        {
            BeginShaderMode(g_LittlePeopleBillboardShader);
            littlePeopleShaderActive = true;
        }

        if (request.m_MaskedSprite == LittlePersonMaskedSprite::Swordsman)
        {
            DrawMaskedBillboardInternal(
                camera,
                request,
                SWORDSMAN_ATLAS_PATH,
                SWORDSMAN_MASK_PATH,
                SWORDSMAN_CELL_WIDTH,
                SWORDSMAN_CELL_HEIGHT,
                SWORDSMAN_CELL_HEIGHT,
                SWORDSMAN_CELL_WIDTH,
                SWORDSMAN_CELL_HEIGHT);
        }
        else if (request.m_MaskedSprite == LittlePersonMaskedSprite::Archer)
        {
            DrawMaskedBillboardInternal(
                camera,
                request,
                ARCHER_ATLAS_PATH,
                ARCHER_MASK_PATH,
                ARCHER_CELL_WIDTH,
                ARCHER_ROW_HEIGHT,
                ARCHER_ROW_HEIGHT,
                SWORDSMAN_CELL_WIDTH,
                SWORDSMAN_CELL_HEIGHT);
        }
        else
        {
            DrawLittlePersonBillboardInternal(camera, request);
        }
    }

    if (littlePeopleShaderActive)
    {
        EndShaderMode();
    }

    rlEnableDepthMask();
}