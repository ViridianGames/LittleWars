#ifndef _OVERWORLDMAP_H_
#define _OVERWORLDMAP_H_

#include "../Geist/Source/RNG.h"

#include <vector>

constexpr int OVERWORLD_MAP_SIZE = 128;

enum OverworldCellType : unsigned char
{
    OW_CLEAR = 0,
    OW_MOUNTAIN,
    OW_WATER,
    OW_TREES,
    OW_MARSH
};

enum class CountyResource : unsigned char
{
    Food = 0,
    Gold,
    Iron,
    Wood
};

struct OverworldRegionData
{
    int m_Id = -1;
    int m_OwnerId = -1;
    CountyResource m_Resource = CountyResource::Food;
    bool m_HasCastle = false;
    int m_SeedX = 0;
    int m_SeedY = 0;
    int m_CellCount = 0;
    std::vector<int> m_AdjacentRegionIds;
};

const char* CountyResourceName(CountyResource resource);
char CountyResourceMarker(CountyResource resource);

class OverworldMap
{
public:
    void Clear();
    void Generate(unsigned int seed);

    OverworldCellType GetCell(int x, int y) const;
    int GetRegionId(int x, int y) const;

    const OverworldRegionData* GetRegion(int regionId) const;
    OverworldRegionData* GetRegion(int regionId);
    const std::vector<OverworldRegionData>& GetRegions() const { return m_Regions; }

    bool IsGenerated() const { return m_Generated; }
    unsigned int GetSeed() const { return m_Seed; }

    void Draw(int x, int y, int pixelsPerCell) const;
    void DrawRegionHighlight(int x, int y, int pixelsPerCell, int regionId) const;
    void DrawAdjacencyGraph(int x, int y, int width, int height, int selectedRegionId = -1) const;

private:
    bool IsInBounds(int x, int y) const;
    bool IsLandCell(OverworldCellType type) const;
    int GetCellIndex(int x, int y) const;

    void GenerateTerrain(RNG& rng);
    void PartitionIntoRegions(RNG& rng);
    void BuildAdjacency();
    void AssignRegionResources(RNG& rng);
    void AssignRegionCampaignState(RNG& rng);

    bool m_Generated = false;
    unsigned int m_Seed = 0;
    std::vector<OverworldCellType> m_Cells;
    std::vector<int> m_RegionIds;
    std::vector<OverworldRegionData> m_Regions;
};

extern OverworldMap g_OverworldMap;

#endif