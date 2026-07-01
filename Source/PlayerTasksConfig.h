#ifndef _PLAYERTASKSCONFIG_H_
#define _PLAYERTASKSCONFIG_H_

#include "MapTilesSprites.h"
#include "Player.h"

#include <string>
#include <vector>

struct ResourceAmount
{
    int m_Food = 0;
    int m_Iron = 0;
    int m_Gold = 0;
    int m_Wood = 0;

    bool IsEmpty() const;
    bool CanAfford(const Player& player) const;
    void Deduct(Player& player) const;
    void Add(Player& player) const;
    std::string ToShortLabel() const;
    int GetAmount(CountyResource resource) const;
};

struct ResourceTurnLine
{
    std::string m_Label;
    int m_Amount = 0;
};

struct PlayerTaskIcon
{
    std::string m_Atlas = MAP_TILES_ATLAS_PATH;
    std::string m_Preset;
    int m_X = 0;
    int m_Y = 0;
    int m_Width = 8;
    int m_Height = 8;
};

struct PlayerTaskEffect
{
    std::string m_Type;
    int m_Amount = 0;
};

struct PlayerTaskDefinition
{
    std::string m_Id;
    std::string m_Name;
    PlayerTaskIcon m_Icon;
    ResourceAmount m_Cost;
    ResourceAmount m_Maintenance;
    ResourceAmount m_ConstructionMaintenance;
    PlayerTaskEffect m_Effect;
    bool m_RequiresRegion = false;
    int m_BuildTurns = 0;
};

struct PlayerTaskLayout
{
    int m_RowHeight = 18;
    int m_IconSize = 10;
    int m_IconX = 6;
    int m_NameX = 20;
    int m_CostX = 132;
};

class PlayerTasksConfig
{
public:
    bool LoadFromFile(const std::string& path);
    void LoadDefaults();

    const std::string& GetTitle() const { return m_Title; }
    const std::string& GetHelpText() const { return m_HelpText; }
    const PlayerTaskLayout& GetLayout() const { return m_Layout; }
    int GetTaskCount() const { return static_cast<int>(m_Tasks.size()); }
    const PlayerTaskDefinition& GetTask(int taskIndex) const;
    const PlayerTaskDefinition* FindTaskById(const std::string& taskId) const;

    MapTilesSpriteSpec ResolveTaskIcon(const PlayerTaskDefinition& task) const;
    bool CanPlayerPerformTask(
        const Player& player,
        const PlayerTaskDefinition& task,
        const class OverworldMap& map,
        int selectedRegionId) const;
    bool ExecuteTask(
        Player& player,
        const PlayerTaskDefinition& task,
        class OverworldMap& map,
        int selectedRegionId);
    void ApplyMaintenance(Player& player) const;
    std::string GetTaskFailureReason(
        const Player& player,
        const PlayerTaskDefinition& task,
        const class OverworldMap& map,
        int selectedRegionId) const;

private:
    std::string m_Title;
    std::string m_HelpText;
    PlayerTaskLayout m_Layout;
    std::vector<PlayerTaskDefinition> m_Tasks;

    bool IsInstantEffect(const std::string& effectType) const;
    bool IsRegionValidForTask(
        const Player& player,
        const PlayerTaskDefinition& task,
        const class OverworldMap& map,
        int selectedRegionId) const;
    bool ApplyEffect(
        Player& player,
        const PlayerTaskEffect& effect,
        class OverworldMap& map,
        int selectedRegionId,
        const PlayerTaskDefinition& task) const;
    void RegisterActiveTask(Player& player, const PlayerTaskDefinition& task) const;
};

extern PlayerTasksConfig g_PlayerTasksConfig;

void ComputePlayerResourceBreakdown(
    const class OverworldMap& map,
    const Player& player,
    CountyResource resource,
    std::vector<ResourceTurnLine>& outLines);

Player* GetHumanPlayer(std::vector<Player>& players);
const Player* GetHumanPlayer(const std::vector<Player>& players);
int GetPlayerActiveTaskCount(const Player& player, const std::string& taskId);
void DrawPlayerTaskIcon(const PlayerTaskDefinition& task, Vector2 center, Color tint = WHITE);

#endif