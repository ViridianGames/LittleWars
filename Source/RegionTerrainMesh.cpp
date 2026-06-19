#include "RegionTerrainMesh.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "Geist/Primitives.h"
#include "TerrainTextures.h"
#include "raymath.h"

namespace
{
    constexpr float TERRAIN_AMBIENT_LIGHT = 128.0f;
    constexpr float TERRAIN_DIRECT_LIGHT_SCALE = 127.0f;

    const Vector3 g_TerrainSunDirection = Vector3Normalize(Vector3{ 1.0f, 1.0f, 1.0f });

    int GetTerrainVertexLight(int vertexX, int vertexZ, const RegionHeightfield& heightfield)
    {
        const float heightA = heightfield.GetHeight(vertexX, vertexZ);
        const float heightB = heightfield.GetHeight(vertexX + 1, vertexZ);
        const float heightC = heightfield.GetHeight(vertexX, vertexZ + 1);

        const Vector3 tangentU = { 1.0f, heightB - heightA, 0.0f };
        const Vector3 tangentV = { 0.0f, heightC - heightA, -1.0f };
        Vector3 normal = Vector3CrossProduct(tangentU, tangentV);
        normal = Vector3Normalize(normal);

        float cosine = Vector3DotProduct(normal, g_TerrainSunDirection);
        if (cosine < 0.0f)
        {
            cosine = 0.0f;
        }

        const int lightComponent = static_cast<int>(cosine * TERRAIN_DIRECT_LIGHT_SCALE);
        const int colorComponent = static_cast<int>(TERRAIN_AMBIENT_LIGHT) + lightComponent;
        return std::min(colorComponent, 255);
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

    Vertex MakeTerrainVertex(float x, float y, float z, float u, float v, int light)
    {
        const float shade = static_cast<float>(light) / 255.0f;
        return CreateVertex(x, y, z, shade, shade, shade, 1.0f, u, v);
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
        }
        return;
    }

    if (m_BuiltGenerated
        && m_BuiltSeed == m_Heightfield->m_Seed
        && m_BuiltMeshVersion == MESH_BUILD_VERSION)
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

            const int lightUL = GetTerrainVertexLight(cellX, cellZ, *m_Heightfield);
            const int lightUR = GetTerrainVertexLight(cellX + 1, cellZ, *m_Heightfield);
            const int lightLR = GetTerrainVertexLight(cellX + 1, cellZ + 1, *m_Heightfield);
            const int lightLL = GetTerrainVertexLight(cellX, cellZ + 1, *m_Heightfield);

            const unsigned short baseIndex = static_cast<unsigned short>(typeVertices[terrainType].size());
            typeVertices[terrainType].push_back(MakeTerrainVertex(
                static_cast<float>(cellX), heightUL, static_cast<float>(cellZ), u0, v0, lightUL));
            typeVertices[terrainType].push_back(MakeTerrainVertex(
                static_cast<float>(cellX + 1), heightUR, static_cast<float>(cellZ), u1, v0, lightUR));
            typeVertices[terrainType].push_back(MakeTerrainVertex(
                static_cast<float>(cellX + 1), heightLR, static_cast<float>(cellZ + 1), u1, v1, lightLR));
            typeVertices[terrainType].push_back(MakeTerrainVertex(
                static_cast<float>(cellX), heightLL, static_cast<float>(cellZ + 1), u0, v1, lightLL));

            const float diffA = std::fabs(heightUL - heightLR);
            const float diffB = std::fabs(heightUR - heightLL);
            const bool flipDiagonal = diffA > diffB;

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

    for (int terrainType = 0; terrainType < RTT_LASTTERRAINTYPE; ++terrainType)
    {
        if (typeIndices[terrainType].empty())
        {
            continue;
        }

        Mesh mesh = BuildMeshFromVerticesAndIndices(typeVertices[terrainType], typeIndices[terrainType]);
        m_TypeMeshes[terrainType].model = LoadModelFromMesh(mesh);
        m_TypeMeshes[terrainType].loaded = true;
        m_TypeMeshes[terrainType].triangleCount = static_cast<int>(typeIndices[terrainType].size()) / 3;
    }

    m_BuiltGenerated = true;
    m_BuiltSeed = m_Heightfield->m_Seed;
    m_BuiltMeshVersion = MESH_BUILD_VERSION;
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