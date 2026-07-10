#include "CastleParts.h"

#include "GameGlobals.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265359f;

    Vector3 RotateY(Vector3 point, float radians)
    {
        const float cosine = cosf(radians);
        const float sine = sinf(radians);
        return Vector3{
            point.x * cosine + point.z * sine,
            point.y,
            -point.x * sine + point.z * cosine
        };
    }

    bool SegmentIntersectsLocalAabb(
        Vector3 segmentStart,
        Vector3 segmentEnd,
        Vector3 boxMin,
        Vector3 boxMax,
        float& outHitDistance)
    {
        const Vector3 direction = Vector3Subtract(segmentEnd, segmentStart);
        const float segmentLength = Vector3Length(direction);
        if (segmentLength <= 1e-5f)
        {
            return false;
        }

        float tMin = 0.0f;
        float tMax = segmentLength;

        const float axes[3] = { direction.x, direction.y, direction.z };
        const float starts[3] = { segmentStart.x, segmentStart.y, segmentStart.z };
        const float mins[3] = { boxMin.x, boxMin.y, boxMin.z };
        const float maxs[3] = { boxMax.x, boxMax.y, boxMax.z };

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(axes[axis]) < 1e-5f)
            {
                if (starts[axis] < mins[axis] || starts[axis] > maxs[axis])
                {
                    return false;
                }
            }
            else
            {
                const float inv = 1.0f / axes[axis];
                float t1 = (mins[axis] - starts[axis]) * inv;
                float t2 = (maxs[axis] - starts[axis]) * inv;
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                }

                tMin = std::max(tMin, t1);
                tMax = std::min(tMax, t2);
                if (tMin > tMax)
                {
                    return false;
                }
            }
        }

        outHitDistance = tMin;
        return tMin <= segmentLength;
    }

    bool SegmentIntersectsCylinderY(
        Vector3 segmentStart,
        Vector3 segmentEnd,
        Vector3 baseCenter,
        float radius,
        float height,
        float& outHitDistance)
    {
        const Vector3 direction = Vector3Subtract(segmentEnd, segmentStart);
        const float segmentLength = Vector3Length(direction);
        if (segmentLength <= 1e-5f)
        {
            return false;
        }

        const Vector3 localStart{
            segmentStart.x - baseCenter.x,
            segmentStart.y - baseCenter.y,
            segmentStart.z - baseCenter.z
        };
        const Vector3 localEnd{
            segmentEnd.x - baseCenter.x,
            segmentEnd.y - baseCenter.y,
            segmentEnd.z - baseCenter.z
        };
        const Vector3 localDirection = Vector3Subtract(localEnd, localStart);

        float bestDistance = segmentLength + 1.0f;
        bool hit = false;

        if (SegmentIntersectsLocalAabb(
                localStart,
                localEnd,
                Vector3{ -radius, 0.0f, -radius },
                Vector3{ radius, height, radius },
                bestDistance))
        {
            hit = true;
        }

        const float a = localDirection.x * localDirection.x + localDirection.z * localDirection.z;
        if (a > 1e-5f)
        {
            const float b = 2.0f * (localStart.x * localDirection.x + localStart.z * localDirection.z);
            const float c = localStart.x * localStart.x + localStart.z * localStart.z - radius * radius;
            const float discriminant = b * b - 4.0f * a * c;
            if (discriminant >= 0.0f)
            {
                const float sqrtDisc = std::sqrt(discriminant);
                const float inv2a = 0.5f / a;
                const float roots[2] = {
                    (-b - sqrtDisc) * inv2a,
                    (-b + sqrtDisc) * inv2a
                };

                for (float root : roots)
                {
                    if (root < 0.0f || root > 1.0f)
                    {
                        continue;
                    }

                    const float y = localStart.y + localDirection.y * root;
                    if (y < 0.0f || y > height)
                    {
                        continue;
                    }

                    const float distanceAlongSegment = root * segmentLength;
                    if (distanceAlongSegment < bestDistance)
                    {
                        bestDistance = distanceAlongSegment;
                        hit = true;
                    }
                }
            }
        }

        if (hit)
        {
            outHitDistance = bestDistance;
        }

        return hit;
    }
}

const char* CastlePartTypeName(CastlePartType type)
{
    switch (type)
    {
    case CastlePartType::RoundTower:
        return "Round Tower";
    case CastlePartType::SquareTower:
        return "Square Tower";
    case CastlePartType::ShortWall:
        return "Short Wall";
    case CastlePartType::TallWall:
        return "Tall Wall";
    case CastlePartType::Gate:
        return "Gate";
    case CastlePartType::Moat:
        return "Moat";
    default:
        return "Unknown";
    }
}

Color CastlePartTypeColor(CastlePartType type)
{
    switch (type)
    {
    case CastlePartType::RoundTower:
        return Color{ 150, 148, 142, 255 };
    case CastlePartType::SquareTower:
        return Color{ 138, 136, 130, 255 };
    case CastlePartType::ShortWall:
        return Color{ 128, 126, 120, 255 };
    case CastlePartType::TallWall:
        return Color{ 118, 116, 110, 255 };
    case CastlePartType::Gate:
        return Color{ 102, 78, 52, 255 };
    case CastlePartType::Moat:
        return Color{ 48, 72, 110, 255 };
    default:
        return Color{ 140, 140, 140, 255 };
    }
}

void GetCastlePartHalfExtents(CastlePartType type, float& halfX, float& halfZ)
{
    switch (type)
    {
    case CastlePartType::RoundTower:
        halfX = 1.4f;
        halfZ = 1.4f;
        break;
    case CastlePartType::SquareTower:
        halfX = 1.4f;
        halfZ = 1.4f;
        break;
    case CastlePartType::ShortWall:
        halfX = 2.0f;
        halfZ = 0.5f;
        break;
    case CastlePartType::TallWall:
        halfX = 2.0f;
        halfZ = 0.5f;
        break;
    case CastlePartType::Gate:
        halfX = 1.6f;
        halfZ = 0.6f;
        break;
    case CastlePartType::Moat:
        halfX = 2.5f;
        halfZ = 1.5f;
        break;
    default:
        halfX = 1.0f;
        halfZ = 1.0f;
        break;
    }
}

float GetCastlePartPickRadius(CastlePartType type)
{
    switch (type)
    {
    case CastlePartType::RoundTower:
        return 1.6f;
    case CastlePartType::SquareTower:
        return 2.2f;
    case CastlePartType::ShortWall:
        return 2.6f;
    case CastlePartType::TallWall:
        return 2.6f;
    case CastlePartType::Gate:
        return 2.0f;
    case CastlePartType::Moat:
        return 3.0f;
    default:
        return 2.0f;
    }
}

void GetCastlePartCollisionSize(CastlePartType type, Vector3& outSize)
{
    switch (type)
    {
    case CastlePartType::RoundTower:
        outSize = Vector3{ 2.8f, 4.5f, 2.8f };
        break;
    case CastlePartType::SquareTower:
        outSize = Vector3{ 2.8f, 5.0f, 2.8f };
        break;
    case CastlePartType::ShortWall:
        outSize = Vector3{ 4.0f, 1.6f, 1.0f };
        break;
    case CastlePartType::TallWall:
        outSize = Vector3{ 4.0f, 3.6f, 1.0f };
        break;
    case CastlePartType::Gate:
        outSize = Vector3{ 3.2f, 2.8f, 1.2f };
        break;
    default:
        outSize = Vector3{ 2.0f, 2.0f, 2.0f };
        break;
    }
}

bool CastlePartBlocksProjectiles(CastlePartType type)
{
    return type != CastlePartType::Moat;
}

bool SegmentIntersectsCastlePart(
    Vector3 segmentStart,
    Vector3 segmentEnd,
    const CastlePartPlacement& placement,
    const RegionHeightfield& heightfield,
    float& outHitDistance)
{
    if (!CastlePartBlocksProjectiles(placement.m_Type))
    {
        return false;
    }

    const float terrainY = heightfield.SampleHeight(placement.m_Position.x, placement.m_Position.z);
    const float rotationRadians = static_cast<float>(placement.m_RotationDegrees) * (kPi / 180.0f);

    if (placement.m_Type == CastlePartType::RoundTower)
    {
        const Vector3 baseCenter{
            placement.m_Position.x,
            terrainY,
            placement.m_Position.z
        };
        return SegmentIntersectsCylinderY(segmentStart, segmentEnd, baseCenter, 1.4f, 4.5f, outHitDistance);
    }

    Vector3 size{};
    GetCastlePartCollisionSize(placement.m_Type, size);
    const Vector3 localHalf{
        size.x * 0.5f,
        size.y * 0.5f,
        size.z * 0.5f
    };

    const Vector3 worldCenter{
        placement.m_Position.x,
        terrainY + localHalf.y,
        placement.m_Position.z
    };

    const Vector3 localStart = RotateY(Vector3Subtract(segmentStart, worldCenter), -rotationRadians);
    const Vector3 localEnd = RotateY(Vector3Subtract(segmentEnd, worldCenter), -rotationRadians);
    const Vector3 boxMin{
        -localHalf.x,
        -localHalf.y,
        -localHalf.z
    };
    const Vector3 boxMax{
        localHalf.x,
        localHalf.y,
        localHalf.z
    };

    return SegmentIntersectsLocalAabb(localStart, localEnd, boxMin, boxMax, outHitDistance);
}

void DrawCastlePartShape(
    const RegionHeightfield& heightfield,
    CastlePartType type,
    Vector3 position,
    int rotationDegrees,
    Color color,
    float alphaScale)
{
    const float terrainY = heightfield.SampleHeight(position.x, position.z);
    Color drawColor = color;
    drawColor.a = static_cast<unsigned char>(static_cast<float>(drawColor.a) * alphaScale);

    rlPushMatrix();
    rlTranslatef(position.x, terrainY, position.z);
    rlRotatef(static_cast<float>(rotationDegrees), 0.0f, 1.0f, 0.0f);

    switch (type)
    {
    case CastlePartType::RoundTower:
    {
        const float radius = 1.4f;
        const float height = 4.5f;
        DrawCylinder(
            Vector3{ 0.0f, height * 0.5f, 0.0f },
            radius,
            radius,
            height,
            16,
            drawColor);
        break;
    }
    case CastlePartType::SquareTower:
    {
        const Vector3 size{ 2.8f, 5.0f, 2.8f };
        DrawCube(Vector3{ 0.0f, size.y * 0.5f, 0.0f }, size.x, size.y, size.z, drawColor);
        break;
    }
    case CastlePartType::ShortWall:
    {
        const Vector3 size{ 4.0f, 1.6f, 1.0f };
        DrawCube(Vector3{ 0.0f, size.y * 0.5f, 0.0f }, size.x, size.y, size.z, drawColor);
        break;
    }
    case CastlePartType::TallWall:
    {
        const Vector3 size{ 4.0f, 3.6f, 1.0f };
        DrawCube(Vector3{ 0.0f, size.y * 0.5f, 0.0f }, size.x, size.y, size.z, drawColor);
        break;
    }
    case CastlePartType::Gate:
    {
        const Vector3 size{ 3.2f, 2.8f, 1.2f };
        DrawCube(Vector3{ 0.0f, size.y * 0.5f, 0.0f }, size.x, size.y, size.z, drawColor);
        break;
    }
    case CastlePartType::Moat:
    {
        const Vector3 size{ 5.0f, 0.35f, 3.0f };
        DrawCube(Vector3{ 0.0f, -size.y * 0.5f + 0.05f, 0.0f }, size.x, size.y, size.z, drawColor);
        break;
    }
    default:
        break;
    }

    rlPopMatrix();
}

void DrawCastleParts3D(
    const RegionHeightfield& heightfield,
    const std::vector<CastlePartPlacement>& placements)
{
    for (const CastlePartPlacement& placement : placements)
    {
        DrawCastlePartShape(
            heightfield,
            placement.m_Type,
            placement.m_Position,
            placement.m_RotationDegrees,
            CastlePartTypeColor(placement.m_Type));
    }
}