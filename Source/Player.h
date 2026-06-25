#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "raylib.h"

#include <string>
#include <vector>

enum class DiplomaticRelation : int
{
    War = -2,
    Hostile = -1,
    Neutral = 0,
    Friendly = 1,
    Allied = 2
};

constexpr int kMaxCampaignPlayers = 8;

constexpr int kRegionFoodIncome = 12;
constexpr int kRegionIronIncome = 8;
constexpr int kRegionGoldIncome = 10;
constexpr int kRegionWoodIncome = 10;

struct Player
{
    int m_Id = -1;
    bool m_IsHuman = false;

    int m_Food = 0;
    int m_Iron = 0;
    int m_Gold = 0;
    int m_Wood = 0;

    int m_FoodRegions = 0;
    int m_IronRegions = 0;
    int m_GoldRegions = 0;
    int m_WoodRegions = 0;
    int m_TotalRegions = 0;

    int m_Castles = 0;
    int m_Swordsmen = 0;
    int m_Archers = 0;
    int m_Knights = 0;
    int m_Catapults = 0;

    std::vector<int> m_Relations;

    Color GetColor() const;
    const char* GetColorName() const;
    std::string GetRelationLabel(int otherPlayerId) const;
};

Color PlayerOwnerColor(int ownerId);
const char* PlayerOwnerName(int ownerId);
const char* DiplomaticRelationName(DiplomaticRelation relation);

void InitializeCampaignPlayers(std::vector<Player>& players, int playerCount);
void SyncPlayersFromOverworld(const class OverworldMap& map, std::vector<Player>& players, bool resetAssets);
void CollectTurnIncomeFromRegions(const class OverworldMap& map, std::vector<Player>& players);

#endif