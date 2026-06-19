#ifndef _GAMEGLOBALS_H_
#define _GAMEGLOBALS_H_

#include "CastleDesignState.h"
#include "CombatState.h"
#include "MainState.h"
#include "OptionsState.h"
#include "SetupGameState.h"
#include "TitleState.h"
#include "raylib.h"
#include "Geist/RNG.h"

#include <memory>
#include <string>
#include <vector>

enum GameStates
{
    STATE_TITLESTATE = 0,
    STATE_MAINSTATE,
    STATE_OPTIONSSTATE,
    STATE_COMBATSTATE,
    STATE_CASTLEDESIGNSTATE,
    STATE_SETUPGAMESTATE,
    STATE_LASTSTATE
};

struct ConsoleString
{
    std::string m_String;
    Color m_Color;
    unsigned int m_StartTime;
};

inline float g_drawScale = 1.0f;

inline std::unique_ptr<RNG> g_vitalRNG;
inline std::unique_ptr<RNG> g_nonVitalRNG;

inline std::shared_ptr<Font> g_font;
inline std::shared_ptr<Font> g_smallFont;

void DrawOutlinedText(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float fontSize, int spacing, Color color);

void DrawParagraph(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float maxwidth, float fontSize, int spacing, Color color, bool outlined = false);

void DebugPrint(std::string text);

// ---------------------------------------------------------------------------
// Campaign / region database (accessible from any state)
// ---------------------------------------------------------------------------

constexpr int REGION_CELLS = 64;
constexpr int REGION_VERTICES = REGION_CELLS + 1;

constexpr float REGION_WATER_HEIGHT = 1.0f;

enum class MapSize
{
    Small = 0,
    Medium,
    Large
};

// Matches terrain.png layout: row-major, top-left to bottom-right.
enum RegionTerrainType : unsigned char
{
    RTT_GRASS = 0,
    RTT_BEACH,
    RTT_SWAMP,
    RTT_MARKED_PATH,
    RTT_WATER,
    RTT_LAVA,
    RTT_FARM,
    RTT_STONE,

    RTT_LASTTERRAINTYPE
};

struct CampaignSetup
{
    unsigned int m_Seed = 0;
    int m_Difficulty = 1;
    int m_EnemyCount = 3;
    MapSize m_MapSize = MapSize::Medium;
    int m_RegionColumns = 4;
    int m_RegionRows = 4;
};

struct PlayerData
{
    int m_Id = 0;
    int m_Gold = 100;
    int m_Food = 50;
};

struct RegionHeightfield
{
    bool m_Generated = false;
    unsigned int m_Seed = 0;
    std::vector<float> m_Heights;
    std::vector<unsigned char> m_TerrainTypes;

    void Clear();

    float GetHeight(int x, int y) const;
    float SampleHeight(float x, float z) const;
    void SetHeight(int x, int y, float height);
    unsigned char GetTerrainType(int x, int y) const;
    void SetTerrainType(int x, int y, unsigned char type);
};

struct RegionData
{
    int m_Id = -1;
    int m_MapX = 0;
    int m_MapY = 0;
    int m_OwnerId = -1;
    int m_Income = 0;
    bool m_HasCastle = false;
    unsigned int m_HeightfieldSeed = 0;
    RegionHeightfield m_Heightfield;
};

class GameDatabase
{
public:
    static constexpr const char* SAVE_MAGIC = "LWAR";
    static constexpr int SAVE_VERSION = 2;

    CampaignSetup m_Setup;
    int m_Turn = 0;
    int m_ActiveRegionId = -1;
    PlayerData m_Player;
    std::vector<RegionData> m_Regions;

    void Clear();
    void InitNewCampaign(const CampaignSetup& setup);

    RegionData* GetRegion(int regionId);
    const RegionData* GetRegion(int regionId) const;
    RegionData* GetRegionAtMapPos(int mapX, int mapY);
    const RegionData* GetRegionAtMapPos(int mapX, int mapY) const;

    RegionHeightfield* EnsureRegionHeightfield(int regionId);
    void SetActiveRegion(int regionId);
    RegionData* GetActiveRegion();
    const RegionData* GetActiveRegion() const;

    bool SaveCampaign(const std::string& path) const;
    bool LoadCampaign(const std::string& path);

private:
    void GenerateOverworldRegions();
    void GenerateRegionHeightfield(RegionData& region);
};

extern GameDatabase g_GameDatabase;

#endif