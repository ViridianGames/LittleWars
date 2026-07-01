#ifndef _OVERWORLDMAP_H_
#define _OVERWORLDMAP_H_

#include "../Geist/Source/RNG.h"

#include <unordered_map>
#include <vector>

struct CampaignSetup;

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

enum class RegionBorderType : unsigned char
{
    Open = 0,
    Mountain
};

struct OverworldRegionData
{
    int m_Id = -1;
    int m_OwnerId = -1;
    CountyResource m_Resource = CountyResource::Food;
    bool m_HasCastle = false;
    int m_OutputMultiplier = 1;
    int m_CastleBuildTurnsRemaining = 0;
    int m_SeedX = 0;
    int m_SeedY = 0;
    int m_CellCount = 0;
    bool m_IsWater = false;
    std::vector<int> m_AdjacentRegionIds;
};

constexpr int kCastleOutputMultiplier = 2;

int GetRegionIncomeMultiplier(const OverworldRegionData& region);
int GetRegionTurnIncome(const OverworldRegionData& region);

const char* CountyResourceName(CountyResource resource);
char CountyResourceMarker(CountyResource resource);

class OverworldMap
{
public:
    void Clear();
    void Generate(unsigned int seed, const CampaignSetup& setup);

    OverworldCellType GetCell(int x, int y) const;
    int GetRegionId(int x, int y) const;

    const OverworldRegionData* GetRegion(int regionId) const;
    OverworldRegionData* GetRegion(int regionId);
    const std::vector<OverworldRegionData>& GetRegions() const { return m_Regions; }
    void ApplyRegionCampaignOverlay(int regionId, int outputMultiplier, int castleBuildTurnsRemaining, bool hasCastle);

    bool IsGenerated() const { return m_Generated; }
    unsigned int GetSeed() const { return m_Seed; }
    int GetTargetConquerableRegions() const { return m_TargetConquerableRegions; }
    int GetConquerableRegionCount() const;
    int GetLakeRegionCount() const;

    void Draw(int x, int y, int pixelsPerCell, int selectedRegionId = -1) const;
    void DrawRegionHighlight(int x, int y, int pixelsPerCell, int regionId) const;
    void DrawAdjacencyGraph(int x, int y, int width, int height, int selectedRegionId = -1) const;
    void DrawAccessibilityGrid(int x, int y, int width, int height, int selectedRegionId = -1) const;

    RegionBorderType GetBorderType(int regionA, int regionB) const;
    std::vector<int> GetTraversableAdjacentRegions(int regionId) const;

private:
    bool IsInBounds(int x, int y) const;
    bool IsLandCell(OverworldCellType type) const;
    int GetCellIndex(int x, int y) const;

    void GenerateTerrain(RNG& rng);
    void SculptCoastline(RNG& rng);
    void PartitionIntoRegions(RNG& rng, int targetRegionCount, int minKeptRegions);
    void DesignateWaterRegions(RNG& rng, int minConquerableRegions);
    void ClearLandRegionInteriors();
    void CarveInterRegionMountains();
    void RecalculateRegionCellCounts();
    void BuildAdjacency();
    void AssignRegionBorders(RNG& rng);
    void AssignRegionResources(RNG& rng, int resourceDistribution);
    void AssignRegionCampaignState(RNG& rng, int enemyCount, int startingRegionsPerPlayer);

    bool m_Generated = false;
    unsigned int m_Seed = 0;
    int m_TargetConquerableRegions = 0;
    std::vector<OverworldCellType> m_Cells;
    std::vector<int> m_RegionIds;
    std::vector<OverworldRegionData> m_Regions;
    std::unordered_map<unsigned long long, RegionBorderType> m_BorderTypes;
};

extern OverworldMap g_OverworldMap;

#endif