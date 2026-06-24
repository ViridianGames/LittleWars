#ifndef _REGIONTERRAINMESH_H_
#define _REGIONTERRAINMESH_H_

#include "GameGlobals.h"

class RegionTerrainMesh
{
public:
    static constexpr unsigned int MESH_BUILD_VERSION = 6;

    void SetHeightfield(const RegionHeightfield* heightfield);
    void RebuildIfNeeded();
    void Draw();

private:
    struct TypeMesh
    {
        Model model{};
        bool loaded = false;
        int triangleCount = 0;
    };

    void ClearMeshes();
    void UnloadTypeMesh(int terrainType);

    const RegionHeightfield* m_Heightfield = nullptr;
    unsigned int m_BuiltSeed = 0;
    unsigned int m_BuiltMeshVersion = 0;
    bool m_BuiltGenerated = false;
    TypeMesh m_TypeMeshes[RTT_LASTTERRAINTYPE];
};

extern RegionTerrainMesh g_RegionTerrainMesh;

#endif