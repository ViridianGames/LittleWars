#include "RegionTerrainMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "../Geist/Source/Primitives.h"
#include "TerrainTextures.h"
#include "raymath.h"

namespace
{
    constexpr float TERRAIN_AMBIENT_LIGHT = 128.0f;
    constexpr float TERRAIN_DIRECT_LIGHT_SCALE = 127.0f;
    constexpr int TERRAIN_LIGHT_BANDS = 5;
    constexpr int TERRAIN_LIGHT_MIN = 128;
    constexpr int TERRAIN_LIGHT_MAX = 255;

    const Vector3 g_TerrainSunDirection = Vector3Normalize(Vector3{ 1.0f, 1.0f, 1.0f });

    int QuantizeTerrainLight(int light)
    {
        light = std::clamp(light, TERRAIN_LIGHT_MIN, TERRAIN_LIGHT_MAX);
        const float normalized =
            static_cast<float>(light - TERRAIN_LIGHT_MIN)
            / static_cast<float>(TERRAIN_LIGHT_MAX - TERRAIN_LIGHT_MIN);
        const int band = std::min(
            static_cast<int>(normalized * static_cast<float>(TERRAIN_LIGHT_BANDS)),
            TERRAIN_LIGHT_BANDS - 1);
        return TERRAIN_LIGHT_MIN
            + (band * (TERRAIN_LIGHT_MAX - TERRAIN_LIGHT_MIN)) / (TERRAIN_LIGHT_BANDS - 1);
    }

    int ComputeTerrainLightFromNormal(Vector3 normal, bool quantize)
    {
        normal = Vector3Normalize(normal);
        float cosine = Vector3DotProduct(normal, g_TerrainSunDirection);
        if (cosine < 0.0f)
        {
            cosine = 0.0f;
        }

        const int lightComponent = static_cast<int>(cosine * TERRAIN_DIRECT_LIGHT_SCALE);
        const int colorComponent = static_cast<int>(TERRAIN_AMBIENT_LIGHT) + lightComponent;
        const int light = std::min(colorComponent, 255);
        return quantize ? QuantizeTerrainLight(light) : light;
    }

    int GetTerrainVertexLight(int vertexX, int vertexZ, const RegionHeightfield& heightfield)
    {
        const float heightA = heightfield.GetHeight(vertexX, vertexZ);
        const float heightB = heightfield.GetHeight(vertexX + 1, vertexZ);
        const float heightC = heightfield.GetHeight(vertexX, vertexZ + 1);

        const Vector3 tangentU = { 1.0f, heightB - heightA, 0.0f };
        const Vector3 tangentV = { 0.0f, heightC - heightA, -1.0f };
        const Vector3 normal = Vector3CrossProduct(tangentU, tangentV);
        return ComputeTerrainLightFromNormal(normal, false);
    }

    int GetTerrainTriangleLight(Vector3 p0, Vector3 p1, Vector3 p2)
    {
        const Vector3 edgeA = Vector3Subtract(p1, p0);
        const Vector3 edgeB = Vector3Subtract(p2, p0);
        const Vector3 normal = Vector3CrossProduct(edgeA, edgeB);
        return ComputeTerrainLightFromNormal(normal, true);
    }

    Mesh BuildMeshFromVerticesAndIndices(const std::vector<Vertex>& vertices, const std::vector<unsigned short>& indices)
    {
        Mesh mesh{};
        const int vertexCount = static_cast<int>(vertices.size());
        const int indexCount = static_cast<int>(indices.size());
        mesh.vertexCount = vertexCount;
        mesh.triangleCount = indexCount / 3;

        if (vertexCount > 0)
        {
            mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertexCount * 3 * sizeof(float))));
            mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertexCount * 2 * sizeof(float))));
            mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(vertexCount * 4)));

            for (int i = 0; i < vertexCount; ++i)
            {
                const Vertex& vertex = vertices[static_cast<size_t>(i)];
                mesh.vertices[i * 3 + 0] = vertex.x;
                mesh.vertices[i * 3 + 1] = vertex.y;
                mesh.vertices[i * 3 + 2] = vertex.z;
                mesh.texcoords[i * 2 + 0] = vertex.u;
                mesh.texcoords[i * 2 + 1] = vertex.v;
                mesh.colors[i * 4 + 0] = static_cast<unsigned char>(vertex.r * 255.0f);
                mesh.colors[i * 4 + 1] = static_cast<unsigned char>(vertex.g * 255.0f);
                mesh.colors[i * 4 + 2] = static_cast<unsigned char>(vertex.b * 255.0f);
                mesh.colors[i * 4 + 3] = static_cast<unsigned char>(vertex.a * 255.0f);
            }
        }

        if (indexCount > 0)
        {
            mesh.indices = static_cast<unsigned short*>(
                MemAlloc(static_cast<unsigned int>(indexCount * sizeof(unsigned short))));
            memcpy(mesh.indices, indices.data(), static_cast<size_t>(indexCount) * sizeof(unsigned short));
        }

        UploadMesh(&mesh, true);
        return mesh;
    }

    Mesh BuildMeshFromTriangleSoup(const std::vector<Vertex>& vertices)
    {
        Mesh mesh{};
        const int vertexCount = static_cast<int>(vertices.size());
        mesh.vertexCount = vertexCount;
        mesh.triangleCount = vertexCount / 3;

        if (vertexCount <= 0)
        {
            return mesh;
        }

        mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertexCount * 3 * sizeof(float))));
        mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertexCount * 2 * sizeof(float))));
        mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(vertexCount * 4)));

        for (int i = 0; i < vertexCount; ++i)
        {
            const Vertex& vertex = vertices[static_cast<size_t>(i)];
            mesh.vertices[i * 3 + 0] = vertex.x;
            mesh.vertices[i * 3 + 1] = vertex.y;
            mesh.vertices[i * 3 + 2] = vertex.z;
            mesh.texcoords[i * 2 + 0] = vertex.u;
            mesh.texcoords[i * 2 + 1] = vertex.v;
            mesh.colors[i * 4 + 0] = static_cast<unsigned char>(vertex.r * 255.0f);
            mesh.colors[i * 4 + 1] = static_cast<unsigned char>(vertex.g * 255.0f);
            mesh.colors[i * 4 + 2] = static_cast<unsigned char>(vertex.b * 255.0f);
            mesh.colors[i * 4 + 3] = static_cast<unsigned char>(vertex.a * 255.0f);
        }

        UploadMesh(&mesh, true);
        return mesh;
    }

    Vertex MakeTerrainVertex(float x, float y, float z, float u, float v, int light)
    {
        const float shade = static_cast<float>(light) / 255.0f;
        return CreateVertex(x, y, z, shade, shade, shade, 1.0f, u, v);
    }

    void AppendFlatShadedTriangle(
        std::vector<Vertex>& vertices,
        Vector3 p0,
        Vector3 p1,
        Vector3 p2,
        float u0,
        float v0,
        float u1,
        float v1,
        float u2,
        float v2)
    {
        const int light = GetTerrainTriangleLight(p0, p1, p2);
        vertices.push_back(MakeTerrainVertex(p0.x, p0.y, p0.z, u0, v0, light));
        vertices.push_back(MakeTerrainVertex(p1.x, p1.y, p1.z, u1, v1, light));
        vertices.push_back(MakeTerrainVertex(p2.x, p2.y, p2.z, u2, v2, light));
    }

}

RegionTerrainMesh g_RegionTerrainMesh;

void RegionTerrainMesh::ClearMeshes()
{
    for (int terrainType = 0; terrainType < RTT_LASTTERRAINTYPE; ++terrainType)
    {
        UnloadTypeMesh(terrainType);
    }
}

void RegionTerrainMesh::UnloadTypeMesh(int terrainType)
{
    if (terrainType < 0 || terrainType >= RTT_LASTTERRAINTYPE)
    {
        return;
    }

    TypeMesh& drawMesh = m_TypeMeshes[terrainType];
    if (drawMesh.loaded)
    {
        UnloadModel(drawMesh.model);
        drawMesh.loaded = false;
    }
    drawMesh.triangleCount = 0;
}

void RegionTerrainMesh::SetHeightfield(const RegionHeightfield* heightfield)
{
    m_Heightfield = heightfield;
}

void RegionTerrainMesh::SetFlatShaded(bool flatShaded)
{
    m_FlatShaded = flatShaded;
}

void RegionTerrainMesh::RebuildIfNeeded()
{
    if (!m_Heightfield || !m_Heightfield->m_Generated)
    {
        if (m_BuiltGenerated)
        {
            ClearMeshes();
            m_BuiltGenerated = false;
            m_BuiltSeed = 0;
            m_BuiltMeshVersion = 0;
            m_BuiltFlatShaded = false;
        }
        return;
    }

    if (m_BuiltGenerated
        && m_BuiltSeed == m_Heightfield->m_Seed
        && m_BuiltMeshVersion == MESH_BUILD_VERSION
        && m_BuiltFlatShaded == m_FlatShaded)
    {
        return;
    }

    ClearMeshes();

    std::vector<Vertex> typeVertices[RTT_LASTTERRAINTYPE];
    std::vector<unsigned short> typeIndices[RTT_LASTTERRAINTYPE];

    for (int cellX = 0; cellX < REGION_CELLS; ++cellX)
    {
        for (int cellZ = 0; cellZ < REGION_CELLS; ++cellZ)
        {
            const unsigned char terrainTypeValue = m_Heightfield->GetTerrainType(cellX, cellZ);
            if (terrainTypeValue >= RTT_LASTTERRAINTYPE)
            {
                continue;
            }

            const int terrainType = static_cast<int>(terrainTypeValue);
            float u0 = 0.0f;
            float v0 = 0.0f;
            float u1 = 0.0f;
            float v1 = 0.0f;
            GetTerrainSpriteUV(static_cast<RegionTerrainType>(terrainType), u0, v0, u1, v1);

            const float heightUL = m_Heightfield->GetHeight(cellX, cellZ);
            const float heightUR = m_Heightfield->GetHeight(cellX + 1, cellZ);
            const float heightLR = m_Heightfield->GetHeight(cellX + 1, cellZ + 1);
            const float heightLL = m_Heightfield->GetHeight(cellX, cellZ + 1);

            const Vector3 cornerUL = { static_cast<float>(cellX), heightUL, static_cast<float>(cellZ) };
            const Vector3 cornerUR = { static_cast<float>(cellX + 1), heightUR, static_cast<float>(cellZ) };
            const Vector3 cornerLR = { static_cast<float>(cellX + 1), heightLR, static_cast<float>(cellZ + 1) };
            const Vector3 cornerLL = { static_cast<float>(cellX), heightLL, static_cast<float>(cellZ + 1) };

            const float diffA = std::fabs(heightUL - heightLR);
            const float diffB = std::fabs(heightUR - heightLL);
            const bool flipDiagonal = diffA > diffB;

            if (m_FlatShaded)
            {
                if (!flipDiagonal)
                {
                    AppendFlatShadedTriangle(
                        typeVertices[terrainType], cornerUL, cornerLR, cornerUR, u0, v0, u1, v1, u1, v0);
                    AppendFlatShadedTriangle(
                        typeVertices[terrainType], cornerUL, cornerLL, cornerLR, u0, v0, u0, v1, u1, v1);
                }
                else
                {
                    AppendFlatShadedTriangle(
                        typeVertices[terrainType], cornerUL, cornerLL, cornerUR, u0, v0, u0, v1, u1, v0);
                    AppendFlatShadedTriangle(
                        typeVertices[terrainType], cornerUR, cornerLL, cornerLR, u1, v0, u0, v1, u1, v1);
                }
            }
            else
            {
                const int lightUL = GetTerrainVertexLight(cellX, cellZ, *m_Heightfield);
                const int lightUR = GetTerrainVertexLight(cellX + 1, cellZ, *m_Heightfield);
                const int lightLR = GetTerrainVertexLight(cellX + 1, cellZ + 1, *m_Heightfield);
                const int lightLL = GetTerrainVertexLight(cellX, cellZ + 1, *m_Heightfield);

                const unsigned short baseIndex = static_cast<unsigned short>(typeVertices[terrainType].size());
                typeVertices[terrainType].push_back(MakeTerrainVertex(
                    cornerUL.x, cornerUL.y, cornerUL.z, u0, v0, lightUL));
                typeVertices[terrainType].push_back(MakeTerrainVertex(
                    cornerUR.x, cornerUR.y, cornerUR.z, u1, v0, lightUR));
                typeVertices[terrainType].push_back(MakeTerrainVertex(
                    cornerLR.x, cornerLR.y, cornerLR.z, u1, v1, lightLR));
                typeVertices[terrainType].push_back(MakeTerrainVertex(
                    cornerLL.x, cornerLL.y, cornerLL.z, u0, v1, lightLL));

                // Wound so both triangles face upward (+Y). Mixed winding caused polys to
                // vanish as the camera orbited with backface culling enabled.
                if (!flipDiagonal)
                {
                    typeIndices[terrainType].push_back(baseIndex + 0);
                    typeIndices[terrainType].push_back(baseIndex + 2);
                    typeIndices[terrainType].push_back(baseIndex + 1);
                    typeIndices[terrainType].push_back(baseIndex + 0);
                    typeIndices[terrainType].push_back(baseIndex + 3);
                    typeIndices[terrainType].push_back(baseIndex + 2);
                }
                else
                {
                    typeIndices[terrainType].push_back(baseIndex + 0);
                    typeIndices[terrainType].push_back(baseIndex + 3);
                    typeIndices[terrainType].push_back(baseIndex + 1);
                    typeIndices[terrainType].push_back(baseIndex + 1);
                    typeIndices[terrainType].push_back(baseIndex + 3);
                    typeIndices[terrainType].push_back(baseIndex + 2);
                }
            }
        }
    }

    for (int terrainType = 0; terrainType < RTT_LASTTERRAINTYPE; ++terrainType)
    {
        if (typeVertices[terrainType].empty())
        {
            continue;
        }

        Mesh mesh{};
        if (m_FlatShaded)
        {
            mesh = BuildMeshFromTriangleSoup(typeVertices[terrainType]);
            m_TypeMeshes[terrainType].triangleCount = mesh.triangleCount;
        }
        else
        {
            if (typeIndices[terrainType].empty())
            {
                continue;
            }

            mesh = BuildMeshFromVerticesAndIndices(typeVertices[terrainType], typeIndices[terrainType]);
            m_TypeMeshes[terrainType].triangleCount = static_cast<int>(typeIndices[terrainType].size()) / 3;
        }

        m_TypeMeshes[terrainType].model = LoadModelFromMesh(mesh);
        m_TypeMeshes[terrainType].loaded = true;
    }

    m_BuiltGenerated = true;
    m_BuiltSeed = m_Heightfield->m_Seed;
    m_BuiltMeshVersion = MESH_BUILD_VERSION;
    m_BuiltFlatShaded = m_FlatShaded;
}

void RegionTerrainMesh::Draw()
{
    Texture* terrainAtlas = GetTerrainAtlasTexture();
    if (!terrainAtlas)
    {
        return;
    }

    for (int terrainType = 0; terrainType < RTT_LASTTERRAINTYPE; ++terrainType)
    {
        TypeMesh& drawMesh = m_TypeMeshes[terrainType];
        if (!drawMesh.loaded || drawMesh.triangleCount <= 0)
        {
            continue;
        }

        SetMaterialTexture(&drawMesh.model.materials[0], MATERIAL_MAP_DIFFUSE, *terrainAtlas);
        DrawModel(drawMesh.model, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    }
}