#include "RegionMinimap.h"

#include <algorithm>
#include <cmath>

#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "RegionUILayout.h"
#include "raymath.h"

RegionMinimap g_RegionMinimap;

namespace
{
    constexpr int kMapTextureSize = REGION_CELLS;

    Color MinimapTerrainColor(RegionTerrainType type)
    {
        switch (type)
        {
        case RTT_GRASS:
            return Color{ 72, 140, 62, 255 };
        case RTT_BEACH:
            return Color{ 210, 195, 140, 255 };
        case RTT_SWAMP:
            return Color{ 55, 85, 55, 255 };
        case RTT_MARKED_PATH:
            return Color{ 150, 130, 90, 255 };
        case RTT_WATER:
            return Color{ 45, 90, 150, 255 };
        case RTT_LAVA:
            return Color{ 180, 50, 30, 255 };
        case RTT_FARM:
            return Color{ 100, 160, 55, 255 };
        case RTT_STONE:
            return Color{ 120, 120, 125, 255 };
        default:
            return Color{ 80, 80, 80, 255 };
        }
    }

    Color ShadeMinimapColor(Color color, float height)
    {
        const float shade = std::clamp(0.82f + (height * 0.04f), 0.65f, 1.2f);
        color.r = static_cast<unsigned char>(std::min(255.0f, color.r * shade));
        color.g = static_cast<unsigned char>(std::min(255.0f, color.g * shade));
        color.b = static_cast<unsigned char>(std::min(255.0f, color.b * shade));
        return color;
    }

    bool RayIntersectGroundPlane(const Ray& ray, Vector3& outHit)
    {
        if (std::fabs(ray.direction.y) < 1e-5f)
        {
            return false;
        }

        const float t = -ray.position.y / ray.direction.y;
        if (t < 0.0f)
        {
            return false;
        }

        outHit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        return true;
    }

    Ray GetMinimapScreenRay(Vector2 screenPosition, const Camera3D& camera)
    {
        if (g_Engine->m_useVirtualResolution)
        {
            return GetScreenToWorldRayEx(
                screenPosition,
                camera,
                GetRegionWorldViewWidth(),
                GetRegionWorldViewHeight());
        }

        return GetScreenToWorldRay(screenPosition, camera);
    }

    Vector2 WorldToMinimapPoint(Vector3 worldPosition, int minimapX, int minimapY, int minimapSize)
    {
        const float normalizedX = worldPosition.x / static_cast<float>(REGION_CELLS);
        const float normalizedZ = worldPosition.z / static_cast<float>(REGION_CELLS);
        return Vector2{
            static_cast<float>(minimapX) + (normalizedX * static_cast<float>(minimapSize)),
            static_cast<float>(minimapY) + (normalizedZ * static_cast<float>(minimapSize))
        };
    }

    void DrawMinimapViewport(const Camera3D& camera, int minimapX, int minimapY, int minimapSize)
    {
        const Rectangle worldView = GetRegionWorldViewBounds();
        const Vector2 screenCorners[4] = {
            Vector2{ worldView.x, worldView.y },
            Vector2{ worldView.x + worldView.width, worldView.y },
            Vector2{ worldView.x + worldView.width, worldView.y + worldView.height },
            Vector2{ worldView.x, worldView.y + worldView.height }
        };

        Vector2 minimapCorners[4]{};
        int validCornerCount = 0;
        for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
        {
            const Ray ray = GetMinimapScreenRay(screenCorners[cornerIndex], camera);
            Vector3 groundHit{};
            if (!RayIntersectGroundPlane(ray, groundHit))
            {
                continue;
            }

            minimapCorners[validCornerCount] = WorldToMinimapPoint(groundHit, minimapX, minimapY, minimapSize);
            ++validCornerCount;
        }

        if (validCornerCount < 3)
        {
            return;
        }

        const Color viewportOutline = Color{ 255, 255, 255, 255 };

        for (int cornerIndex = 0; cornerIndex < validCornerCount; ++cornerIndex)
        {
            const int nextCorner = (cornerIndex + 1) % validCornerCount;
            DrawLineEx(minimapCorners[cornerIndex], minimapCorners[nextCorner], 1.0f, viewportOutline);
        }
    }

}

Vector2 MinimapPointToWorldPosition(Vector2 minimapPoint, int minimapX, int minimapY, int minimapSize)
{
    const float normalizedX = (minimapPoint.x - static_cast<float>(minimapX)) / static_cast<float>(minimapSize);
    const float normalizedZ = (minimapPoint.y - static_cast<float>(minimapY)) / static_cast<float>(minimapSize);
    return Vector2{
        normalizedX * static_cast<float>(REGION_CELLS),
        normalizedZ * static_cast<float>(REGION_CELLS)
    };
}

void RegionMinimap::Shutdown()
{
    if (m_HasTexture)
    {
        UnloadTexture(m_Texture);
        m_Texture = Texture2D{};
        m_HasTexture = false;
    }

    m_CachedSeed = 0;
}

void RegionMinimap::RebuildTexture(const RegionHeightfield& heightfield)
{
    Image image = GenImageColor(kMapTextureSize, kMapTextureSize, Color{ 24, 28, 34, 255 });
    for (int cellY = 0; cellY < REGION_CELLS; ++cellY)
    {
        for (int cellX = 0; cellX < REGION_CELLS; ++cellX)
        {
            const auto terrainType = static_cast<RegionTerrainType>(heightfield.GetTerrainType(cellX, cellY));
            Color color = ShadeMinimapColor(MinimapTerrainColor(terrainType), heightfield.GetHeight(cellX, cellY));
            ImageDrawPixel(&image, cellX, cellY, color);
        }
    }

    if (m_HasTexture)
    {
        UnloadTexture(m_Texture);
        m_HasTexture = false;
    }

    m_Texture = LoadTextureFromImage(image);
    UnloadImage(image);
    m_HasTexture = m_Texture.id != 0;
}

void RegionMinimap::Draw(const RegionHeightfield& heightfield, unsigned int heightfieldSeed, const Camera3D& camera,
    const std::vector<RegionMinimapMarker>* markers)
{
    if (!heightfield.m_Generated)
    {
        return;
    }

    if (!m_HasTexture || m_CachedSeed != heightfieldSeed)
    {
        RebuildTexture(heightfield);
        m_CachedSeed = heightfieldSeed;
    }

    if (!m_HasTexture)
    {
        return;
    }

    const Rectangle minimapBounds = GetRegionMinimapBounds();
    const int minimapX = static_cast<int>(minimapBounds.x);
    const int minimapY = static_cast<int>(minimapBounds.y);
    const int minimapSize = static_cast<int>(minimapBounds.width);

    DrawRectangle(minimapX - 1, minimapY - 1, minimapSize + 2, minimapSize + 2, Color{ 16, 18, 24, 255 });
    DrawTexturePro(
        m_Texture,
        Rectangle{ 0.0f, 0.0f, static_cast<float>(m_Texture.width), static_cast<float>(m_Texture.height) },
        minimapBounds,
        Vector2{ 0.0f, 0.0f },
        0.0f,
        WHITE);

    DrawMinimapViewport(camera, minimapX, minimapY, minimapSize);

    if (markers)
    {
        for (const RegionMinimapMarker& marker : *markers)
        {
            const Vector2 point = WorldToMinimapPoint(
                Vector3{ marker.m_X, 0.0f, marker.m_Z },
                minimapX,
                minimapY,
                minimapSize);
            DrawCircleV(point, 2.0f, marker.m_Color);
            DrawCircleLinesV(point, 2.0f, Color{ 0, 0, 0, 180 });
        }
    }

    DrawRectangleLines(minimapX, minimapY, minimapSize, minimapSize, Color{ 220, 220, 230, 255 });
}

Rectangle RegionMinimap::GetScreenBounds() const
{
    return GetRegionMinimapBounds();
}

bool RegionMinimap::TryGetClickWorldPosition(Vector2 scaledMousePosition, float& outWorldX, float& outWorldZ) const
{
    const Rectangle bounds = GetScreenBounds();
    if (!CheckCollisionPointRec(scaledMousePosition, bounds))
    {
        return false;
    }

    const Vector2 worldPosition = MinimapPointToWorldPosition(
        scaledMousePosition,
        static_cast<int>(bounds.x),
        static_cast<int>(bounds.y),
        static_cast<int>(bounds.width));
    outWorldX = worldPosition.x;
    outWorldZ = worldPosition.y;
    return true;
}

void RegionMinimap::HandleInput(Vector2 scaledMousePosition, float& outWorldX, float& outWorldZ, bool& outClicked) const
{
    outClicked = false;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    if (TryGetClickWorldPosition(scaledMousePosition, outWorldX, outWorldZ))
    {
        outClicked = true;
    }
}