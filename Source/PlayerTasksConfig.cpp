#include "PlayerTasksConfig.h"

#include "OverworldMap.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "../Geist/Source/Config.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/Logging.h"
#include "../Geist/Source/ResourceManager.h"
#include <json.hpp>

using json = nlohmann::json;

PlayerTasksConfig g_PlayerTasksConfig;

namespace
{
    constexpr const char* kPlayerTasksConfigPath = "Data/player_tasks.json";

    bool ParseResourceAmount(const json& resourceJson, ResourceAmount& outAmount)
    {
        if (!resourceJson.is_object())
        {
            return false;
        }

        outAmount = ResourceAmount{};
        if (resourceJson.contains("food"))
        {
            outAmount.m_Food = resourceJson["food"].get<int>();
        }
        if (resourceJson.contains("iron"))
        {
            outAmount.m_Iron = resourceJson["iron"].get<int>();
        }
        if (resourceJson.contains("gold"))
        {
            outAmount.m_Gold = resourceJson["gold"].get<int>();
        }
        if (resourceJson.contains("wood"))
        {
            outAmount.m_Wood = resourceJson["wood"].get<int>();
        }
        if (resourceJson.contains("grain"))
        {
            outAmount.m_Food += resourceJson["grain"].get<int>();
        }

        return true;
    }

    bool ParseTaskIcon(const json& iconJson, PlayerTaskIcon& outIcon)
    {
        if (!iconJson.is_object())
        {
            return false;
        }

        outIcon = PlayerTaskIcon{};
        if (iconJson.contains("atlas"))
        {
            outIcon.m_Atlas = iconJson["atlas"].get<std::string>();
        }
        if (iconJson.contains("preset"))
        {
            outIcon.m_Preset = iconJson["preset"].get<std::string>();
        }
        if (iconJson.contains("x"))
        {
            outIcon.m_X = iconJson["x"].get<int>();
        }
        if (iconJson.contains("y"))
        {
            outIcon.m_Y = iconJson["y"].get<int>();
        }
        if (iconJson.contains("width"))
        {
            outIcon.m_Width = iconJson["width"].get<int>();
        }
        if (iconJson.contains("height"))
        {
            outIcon.m_Height = iconJson["height"].get<int>();
        }

        return !outIcon.m_Preset.empty() || outIcon.m_Width > 0;
    }

    bool ParseTask(const json& taskJson, PlayerTaskDefinition& outTask)
    {
        if (!taskJson.contains("id") || !taskJson.contains("name")
            || !taskJson.contains("cost") || !taskJson.contains("effect"))
        {
            return false;
        }

        outTask = PlayerTaskDefinition{};
        outTask.m_Id = taskJson["id"].get<std::string>();
        outTask.m_Name = taskJson["name"].get<std::string>();

        if (!ParseTaskIcon(taskJson.value("icon", json::object()), outTask.m_Icon)
            || !ParseResourceAmount(taskJson["cost"], outTask.m_Cost))
        {
            return false;
        }

        if (taskJson.contains("maintenance"))
        {
            ParseResourceAmount(taskJson["maintenance"], outTask.m_Maintenance);
        }

        const json& effectJson = taskJson["effect"];
        if (!effectJson.contains("type"))
        {
            return false;
        }

        outTask.m_Effect.m_Type = effectJson["type"].get<std::string>();
        outTask.m_Effect.m_Amount = effectJson.value("amount", 0);
        outTask.m_RequiresRegion = taskJson.value("requiresRegion", false);
        outTask.m_BuildTurns = taskJson.value("buildTurns", 0);

        if (taskJson.contains("constructionMaintenance"))
        {
            ParseResourceAmount(taskJson["constructionMaintenance"], outTask.m_ConstructionMaintenance);
        }

        return true;
    }

    MapTilesSpriteSpec MakeSpriteSpecFromIcon(const PlayerTaskIcon& icon)
    {
        MapTilesSpriteSpec spec{};
        spec.m_Source = Rectangle{
            static_cast<float>(icon.m_X),
            static_cast<float>(icon.m_Y),
            static_cast<float>(icon.m_Width),
            static_cast<float>(icon.m_Height)
        };
        spec.m_Width = icon.m_Width;
        spec.m_Height = icon.m_Height;
        return spec;
    }
}

bool ResourceAmount::IsEmpty() const
{
    return m_Food == 0 && m_Iron == 0 && m_Gold == 0 && m_Wood == 0;
}

bool ResourceAmount::CanAfford(const Player& player) const
{
    return player.m_Food >= m_Food
        && player.m_Iron >= m_Iron
        && player.m_Gold >= m_Gold
        && player.m_Wood >= m_Wood;
}

void ResourceAmount::Deduct(Player& player) const
{
    player.m_Food -= m_Food;
    player.m_Iron -= m_Iron;
    player.m_Gold -= m_Gold;
    player.m_Wood -= m_Wood;
}

void ResourceAmount::Add(Player& player) const
{
    player.m_Food += m_Food;
    player.m_Iron += m_Iron;
    player.m_Gold += m_Gold;
    player.m_Wood += m_Wood;
}

int ResourceAmount::GetAmount(CountyResource resource) const
{
    switch (resource)
    {
    case CountyResource::Food:
        return m_Food;
    case CountyResource::Iron:
        return m_Iron;
    case CountyResource::Gold:
        return m_Gold;
    case CountyResource::Wood:
        return m_Wood;
    default:
        return 0;
    }
}

std::string ResourceAmount::ToShortLabel() const
{
    std::ostringstream label;
    bool first = true;
    auto appendPart = [&](const char* prefix, int value)
    {
        if (value <= 0)
        {
            return;
        }

        if (!first)
        {
            label << ' ';
        }

        label << prefix << value;
        first = false;
    };

    appendPart("F", m_Food);
    appendPart("I", m_Iron);
    appendPart("G", m_Gold);
    appendPart("W", m_Wood);
    return label.str();
}

void PlayerTasksConfig::LoadDefaults()
{
    if (LoadFromFile(kPlayerTasksConfigPath))
    {
        return;
    }

    Log("PlayerTasksConfig::LoadDefaults - using built-in fallback definitions");
    m_Title = "Tasks";
    m_HelpText = "Click a task to spend resources";
    m_Layout = PlayerTaskLayout{};
    m_Tasks = {
        PlayerTaskDefinition{
            "recruitSwordsmen",
            "Recruit Swordsmen",
            PlayerTaskIcon{ MAP_TILES_ATLAS_PATH, "iron", 0, 0, 7, 6 },
            ResourceAmount{ 20, 15, 25, 0 },
            ResourceAmount{ 4, 0, 0, 0 },
            ResourceAmount{},
            PlayerTaskEffect{ "addSwordsmen", 5 }
        },
        PlayerTaskDefinition{
            "trainArchers",
            "Train Archers",
            PlayerTaskIcon{ MAP_TILES_ATLAS_PATH, "wood", 0, 0, 7, 7 },
            ResourceAmount{ 15, 0, 15, 20 },
            ResourceAmount{ 3, 0, 0, 0 },
            ResourceAmount{},
            PlayerTaskEffect{ "addArchers", 5 }
        }
    };
}

bool PlayerTasksConfig::LoadFromFile(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream)
    {
        Log("PlayerTasksConfig::LoadFromFile - failed to open " + path);
        return false;
    }

    json root;
    try
    {
        stream >> root;
    }
    catch (const std::exception& exception)
    {
        Log(std::string("PlayerTasksConfig::LoadFromFile - parse error: ") + exception.what());
        return false;
    }

    if (!root.contains("tasks") || !root["tasks"].is_array() || root["tasks"].empty())
    {
        Log("PlayerTasksConfig::LoadFromFile - missing tasks array in " + path);
        return false;
    }

    PlayerTasksConfig parsed{};
    parsed.m_Title = root.value("title", "Tasks");
    parsed.m_HelpText = root.value("helpText", "Click a task to spend resources");

    if (root.contains("layout") && root["layout"].is_object())
    {
        const json& layoutJson = root["layout"];
        if (layoutJson.contains("rowHeight"))
        {
            parsed.m_Layout.m_RowHeight = layoutJson["rowHeight"].get<int>();
        }
        if (layoutJson.contains("iconSize"))
        {
            parsed.m_Layout.m_IconSize = layoutJson["iconSize"].get<int>();
        }
        if (layoutJson.contains("iconX"))
        {
            parsed.m_Layout.m_IconX = layoutJson["iconX"].get<int>();
        }
        if (layoutJson.contains("nameX"))
        {
            parsed.m_Layout.m_NameX = layoutJson["nameX"].get<int>();
        }
        if (layoutJson.contains("costX"))
        {
            parsed.m_Layout.m_CostX = layoutJson["costX"].get<int>();
        }
    }

    for (const json& taskJson : root["tasks"])
    {
        PlayerTaskDefinition task{};
        if (!ParseTask(taskJson, task))
        {
            Log("PlayerTasksConfig::LoadFromFile - invalid task in " + path);
            return false;
        }

        parsed.m_Tasks.push_back(task);
    }

    *this = std::move(parsed);
    Log("PlayerTasksConfig::LoadFromFile - loaded " + path);
    return true;
}

const PlayerTaskDefinition& PlayerTasksConfig::GetTask(int taskIndex) const
{
    static const PlayerTaskDefinition kEmptyTask{};
    if (taskIndex < 0 || taskIndex >= static_cast<int>(m_Tasks.size()))
    {
        return kEmptyTask;
    }

    return m_Tasks[static_cast<size_t>(taskIndex)];
}

const PlayerTaskDefinition* PlayerTasksConfig::FindTaskById(const std::string& taskId) const
{
    for (const PlayerTaskDefinition& task : m_Tasks)
    {
        if (task.m_Id == taskId)
        {
            return &task;
        }
    }

    return nullptr;
}

MapTilesSpriteSpec PlayerTasksConfig::ResolveTaskIcon(const PlayerTaskDefinition& task) const
{
    if (!task.m_Icon.m_Preset.empty())
    {
        if (task.m_Icon.m_Preset == "food")
        {
            return GetMapTilesResourceSpec(CountyResource::Food);
        }
        if (task.m_Icon.m_Preset == "iron")
        {
            return GetMapTilesResourceSpec(CountyResource::Iron);
        }
        if (task.m_Icon.m_Preset == "gold")
        {
            return GetMapTilesResourceSpec(CountyResource::Gold);
        }
        if (task.m_Icon.m_Preset == "wood")
        {
            return GetMapTilesResourceSpec(CountyResource::Wood);
        }
        if (task.m_Icon.m_Preset == "flag")
        {
            return MapTilesSpriteSpec{
                GetMapTilesFlagSourceRect(),
                MAP_TILES_FLAG_WIDTH,
                MAP_TILES_FLAG_HEIGHT
            };
        }
    }

    return MakeSpriteSpecFromIcon(task.m_Icon);
}

bool PlayerTasksConfig::IsInstantEffect(const std::string& effectType) const
{
    return effectType == "scouting"
        || effectType == "attack"
        || effectType == "saboteur"
        || effectType == "spying"
        || effectType == "diplomat"
        || effectType == "sendDiplomat"
        || effectType == "sendSpy"
        || effectType == "merchant"
        || effectType == "startCastleBuild"
        || effectType == "improveRegion"
        || effectType == "throwFestival";
}

bool PlayerTasksConfig::IsRegionValidForTask(
    const Player& player,
    const PlayerTaskDefinition& task,
    const OverworldMap& map,
    int selectedRegionId) const
{
    if (!task.m_RequiresRegion)
    {
        return true;
    }

    if (selectedRegionId < 0)
    {
        return false;
    }

    const OverworldRegionData* region = map.GetRegion(selectedRegionId);
    if (!region || region->m_IsWater || region->m_OwnerId != player.m_Id)
    {
        return false;
    }

    if (task.m_Effect.m_Type == "startCastleBuild")
    {
        return !region->m_HasCastle && region->m_CastleBuildTurnsRemaining <= 0;
    }

    return true;
}

bool PlayerTasksConfig::CanPlayerPerformTask(
    const Player& player,
    const PlayerTaskDefinition& task,
    const OverworldMap& map,
    int selectedRegionId) const
{
    const bool hasValidEffect = task.m_Effect.m_Amount > 0 || IsInstantEffect(task.m_Effect.m_Type);
    return hasValidEffect
        && task.m_Cost.CanAfford(player)
        && IsRegionValidForTask(player, task, map, selectedRegionId);
}

std::string PlayerTasksConfig::GetTaskFailureReason(
    const Player& player,
    const PlayerTaskDefinition& task,
    const OverworldMap& map,
    int selectedRegionId) const
{
    const bool hasValidEffect = task.m_Effect.m_Amount > 0 || IsInstantEffect(task.m_Effect.m_Type);
    if (!hasValidEffect)
    {
        return "Task has no effect: " + task.m_Name;
    }

    if (!task.m_Cost.CanAfford(player))
    {
        return "Cannot afford: " + task.m_Name;
    }

    if (task.m_RequiresRegion && selectedRegionId < 0)
    {
        return "Select a county: " + task.m_Name;
    }

    const OverworldRegionData* region = map.GetRegion(selectedRegionId);
    if (task.m_RequiresRegion)
    {
        if (!region || region->m_IsWater)
        {
            return "Invalid county: " + task.m_Name;
        }

        if (region->m_OwnerId != player.m_Id)
        {
            return "You do not own this county: " + task.m_Name;
        }

        if (task.m_Effect.m_Type == "startCastleBuild"
            && (region->m_HasCastle || region->m_CastleBuildTurnsRemaining > 0))
        {
            return "Castle already built or under construction";
        }
    }

    return "Cannot perform: " + task.m_Name;
}

bool PlayerTasksConfig::ApplyEffect(
    Player& player,
    const PlayerTaskEffect& effect,
    OverworldMap& map,
    int selectedRegionId,
    const PlayerTaskDefinition& task) const
{
    if (effect.m_Type == "addSwordsmen" && effect.m_Amount > 0)
    {
        player.m_Swordsmen += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addArchers" && effect.m_Amount > 0)
    {
        player.m_Archers += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addKnights" && effect.m_Amount > 0)
    {
        player.m_Knights += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addCatapults" && effect.m_Amount > 0)
    {
        player.m_Catapults += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addSiegeTowers" && effect.m_Amount > 0)
    {
        player.m_SiegeTowers += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addFood" && effect.m_Amount > 0)
    {
        player.m_Food += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addIron" && effect.m_Amount > 0)
    {
        player.m_Iron += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addGold" && effect.m_Amount > 0)
    {
        player.m_Gold += effect.m_Amount;
        return true;
    }
    if (effect.m_Type == "addWood" && effect.m_Amount > 0)
    {
        player.m_Wood += effect.m_Amount;
        return true;
    }

    if (effect.m_Type == "startCastleBuild")
    {
        OverworldRegionData* region = map.GetRegion(selectedRegionId);
        if (!region || region->m_HasCastle || region->m_CastleBuildTurnsRemaining > 0)
        {
            return false;
        }

        const int buildTurns = std::max(1, task.m_BuildTurns);
        region->m_CastleBuildTurnsRemaining = buildTurns;
        return true;
    }

    if (effect.m_Type == "improveRegion")
    {
        OverworldRegionData* region = map.GetRegion(selectedRegionId);
        if (!region)
        {
            return false;
        }

        region->m_OutputMultiplier = std::max(1, region->m_OutputMultiplier) * 2;
        return true;
    }

    if (effect.m_Type == "throwFestival")
    {
        player.m_Happiness = std::min(100, player.m_Happiness + 10);
        return true;
    }

    if (effect.m_Type == "scouting"
        || effect.m_Type == "attack"
        || effect.m_Type == "saboteur"
        || effect.m_Type == "spying"
        || effect.m_Type == "diplomat"
        || effect.m_Type == "sendDiplomat"
        || effect.m_Type == "sendSpy"
        || effect.m_Type == "merchant")
    {
        return true;
    }

    return false;
}

void PlayerTasksConfig::RegisterActiveTask(Player& player, const PlayerTaskDefinition& task) const
{
    if (task.m_Maintenance.IsEmpty())
    {
        return;
    }

    for (const auto& activeTask : player.m_ActiveTasks)
    {
        if (activeTask.first == task.m_Id)
        {
            return;
        }
    }

    player.m_ActiveTasks.emplace_back(task.m_Id, 1);
}

bool PlayerTasksConfig::ExecuteTask(
    Player& player,
    const PlayerTaskDefinition& task,
    OverworldMap& map,
    int selectedRegionId)
{
    if (!CanPlayerPerformTask(player, task, map, selectedRegionId))
    {
        return false;
    }

    task.m_Cost.Deduct(player);
    if (!ApplyEffect(player, task.m_Effect, map, selectedRegionId, task))
    {
        task.m_Cost.Add(player);
        return false;
    }

    if (!task.m_Maintenance.IsEmpty()
        && task.m_Effect.m_Type != "startCastleBuild")
    {
        RegisterActiveTask(player, task);
    }

    return true;
}

void PlayerTasksConfig::ApplyMaintenance(Player& player) const
{
    for (const auto& activeTaskEntry : player.m_ActiveTasks)
    {
        const PlayerTaskDefinition* task = FindTaskById(activeTaskEntry.first);
        if (!task || task->m_Maintenance.IsEmpty())
        {
            continue;
        }

        const ResourceAmount& upkeep = task->m_Maintenance;
        if (upkeep.CanAfford(player))
        {
            upkeep.Deduct(player);
            continue;
        }

        player.m_Food = std::max(0, player.m_Food - upkeep.m_Food);
        player.m_Iron = std::max(0, player.m_Iron - upkeep.m_Iron);
        player.m_Gold = std::max(0, player.m_Gold - upkeep.m_Gold);
        player.m_Wood = std::max(0, player.m_Wood - upkeep.m_Wood);
    }
}

void ComputePlayerResourceBreakdown(
    const OverworldMap& map,
    const Player& player,
    CountyResource resource,
    std::vector<ResourceTurnLine>& outLines)
{
    outLines.clear();

    int countyIncome = 0;
    for (const OverworldRegionData& region : map.GetRegions())
    {
        if (region.m_IsWater || region.m_OwnerId != player.m_Id || region.m_Resource != resource)
        {
            continue;
        }

        countyIncome += GetRegionTurnIncome(region);
    }

    if (countyIncome > 0)
    {
        outLines.push_back(ResourceTurnLine{ "From Counties", countyIncome });
    }

    for (const auto& activeTaskEntry : player.m_ActiveTasks)
    {
        const PlayerTaskDefinition* task = g_PlayerTasksConfig.FindTaskById(activeTaskEntry.first);
        if (!task || task->m_Maintenance.IsEmpty())
        {
            continue;
        }

        const int upkeep = task->m_Maintenance.GetAmount(resource);
        if (upkeep > 0)
        {
            outLines.push_back(ResourceTurnLine{ "For " + task->m_Name, -upkeep });
        }
    }

    const PlayerTaskDefinition* buildCastleTask = g_PlayerTasksConfig.FindTaskById("buildCastle");
    if (buildCastleTask)
    {
        int castleUpkeep = 0;
        const int perCastle = buildCastleTask->m_ConstructionMaintenance.GetAmount(resource);
        if (perCastle > 0)
        {
            for (const OverworldRegionData& region : map.GetRegions())
            {
                if (region.m_IsWater || region.m_CastleBuildTurnsRemaining <= 0 || region.m_OwnerId != player.m_Id)
                {
                    continue;
                }

                castleUpkeep += perCastle;
            }
        }

        if (castleUpkeep > 0)
        {
            outLines.push_back(ResourceTurnLine{ "For Castle Construction", -castleUpkeep });
        }
    }
}

Player* GetHumanPlayer(std::vector<Player>& players)
{
    for (Player& player : players)
    {
        if (player.m_IsHuman)
        {
            return &player;
        }
    }

    return players.empty() ? nullptr : &players.front();
}

const Player* GetHumanPlayer(const std::vector<Player>& players)
{
    for (const Player& player : players)
    {
        if (player.m_IsHuman)
        {
            return &player;
        }
    }

    return players.empty() ? nullptr : &players.front();
}

int GetPlayerActiveTaskCount(const Player& player, const std::string& taskId)
{
    for (const auto& activeTask : player.m_ActiveTasks)
    {
        if (activeTask.first == taskId)
        {
            return activeTask.second;
        }
    }

    return 0;
}

void DrawPlayerTaskIcon(const PlayerTaskDefinition& task, Vector2 center, Color tint)
{
    const MapTilesSpriteSpec spec = g_PlayerTasksConfig.ResolveTaskIcon(task);
    if (!task.m_Icon.m_Preset.empty() || task.m_Icon.m_Atlas == MAP_TILES_ATLAS_PATH)
    {
        DrawMapTilesSprite(spec, center, tint);
        return;
    }

    Texture* texture = g_ResourceManager->GetTexture(task.m_Icon.m_Atlas.c_str(), false);
    if (!texture || texture->id == 0 || spec.m_Width <= 0 || spec.m_Height <= 0)
    {
        return;
    }

    const int drawX = static_cast<int>(center.x) - (spec.m_Width / 2);
    const int drawY = static_cast<int>(center.y) - (spec.m_Height / 2);
    const Rectangle dest{
        static_cast<float>(drawX),
        static_cast<float>(drawY),
        static_cast<float>(spec.m_Width),
        static_cast<float>(spec.m_Height)
    };
    DrawTexturePro(*texture, spec.m_Source, dest, Vector2{ 0.0f, 0.0f }, 0.0f, tint);
}