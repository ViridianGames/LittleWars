#include "Player.h"

#include "OverworldMap.h"

#include <algorithm>

Color PlayerOwnerColor(int ownerId)
{
    switch (ownerId)
    {
    case 0:
        return Color{ 80, 140, 255, 255 };
    case 1:
        return Color{ 220, 60, 60, 255 };
    case 2:
        return Color{ 230, 170, 40, 255 };
    case 3:
        return Color{ 180, 80, 220, 255 };
    case 4:
        return Color{ 60, 190, 90, 255 };
    case 5:
        return Color{ 230, 120, 50, 255 };
    case 6:
        return Color{ 70, 210, 210, 255 };
    case 7:
        return Color{ 220, 100, 170, 255 };
    default:
        return Color{ 255, 255, 255, 255 };
    }
}

const char* PlayerOwnerName(int ownerId)
{
    switch (ownerId)
    {
    case 0:
        return "Blue";
    case 1:
        return "Red";
    case 2:
        return "Gold";
    case 3:
        return "Purple";
    case 4:
        return "Green";
    case 5:
        return "Orange";
    case 6:
        return "Cyan";
    case 7:
        return "Pink";
    default:
        return "Unclaimed";
    }
}

const char* DiplomaticRelationName(DiplomaticRelation relation)
{
    switch (relation)
    {
    case DiplomaticRelation::War:
        return "War";
    case DiplomaticRelation::Hostile:
        return "Hostile";
    case DiplomaticRelation::Neutral:
        return "Neutral";
    case DiplomaticRelation::Friendly:
        return "Friendly";
    case DiplomaticRelation::Allied:
        return "Allied";
    default:
        return "Unknown";
    }
}

Color Player::GetColor() const
{
    return PlayerOwnerColor(m_Id);
}

const char* Player::GetColorName() const
{
    return PlayerOwnerName(m_Id);
}

std::string Player::GetRelationLabel(int otherPlayerId) const
{
    if (otherPlayerId < 0 || otherPlayerId >= static_cast<int>(m_Relations.size()))
    {
        return "?";
    }

    if (otherPlayerId == m_Id)
    {
        return "--";
    }

    const auto relation = static_cast<DiplomaticRelation>(m_Relations[static_cast<size_t>(otherPlayerId)]);
    return std::string(PlayerOwnerName(otherPlayerId)) + ": " + DiplomaticRelationName(relation);
}

void InitializeCampaignPlayers(std::vector<Player>& players, int playerCount)
{
    playerCount = std::clamp(playerCount, 1, kMaxCampaignPlayers);
    players.clear();
    players.resize(static_cast<size_t>(playerCount));

    for (int id = 0; id < playerCount; ++id)
    {
        Player& player = players[static_cast<size_t>(id)];
        player.m_Id = id;
        player.m_IsHuman = (id == 0);
        player.m_Relations.assign(static_cast<size_t>(playerCount), static_cast<int>(DiplomaticRelation::Neutral));

        for (int otherId = 0; otherId < playerCount; ++otherId)
        {
            if (otherId == id)
            {
                player.m_Relations[static_cast<size_t>(otherId)] = static_cast<int>(DiplomaticRelation::Allied);
            }
            else if (id == 0 || otherId == 0)
            {
                player.m_Relations[static_cast<size_t>(otherId)] = static_cast<int>(DiplomaticRelation::Neutral);
            }
            else
            {
                player.m_Relations[static_cast<size_t>(otherId)] =
                    static_cast<int>((id + otherId) % 2 == 0 ? DiplomaticRelation::Hostile : DiplomaticRelation::Neutral);
            }
        }
    }
}

void SyncPlayersFromOverworld(const OverworldMap& map, std::vector<Player>& players, bool resetAssets)
{
    if (players.empty())
    {
        InitializeCampaignPlayers(players, kMaxCampaignPlayers);
    }

    for (Player& player : players)
    {
        player.m_FoodRegions = 0;
        player.m_IronRegions = 0;
        player.m_GoldRegions = 0;
        player.m_WoodRegions = 0;
        player.m_TotalRegions = 0;
        player.m_Castles = 0;
    }

    for (const OverworldRegionData& region : map.GetRegions())
    {
        if (region.m_OwnerId < 0 || region.m_OwnerId >= static_cast<int>(players.size()))
        {
            continue;
        }

        Player& player = players[static_cast<size_t>(region.m_OwnerId)];
        ++player.m_TotalRegions;

        switch (region.m_Resource)
        {
        case CountyResource::Food:
            ++player.m_FoodRegions;
            break;
        case CountyResource::Iron:
            ++player.m_IronRegions;
            break;
        case CountyResource::Gold:
            ++player.m_GoldRegions;
            break;
        case CountyResource::Wood:
            ++player.m_WoodRegions;
            break;
        }

        if (region.m_HasCastle)
        {
            ++player.m_Castles;
        }
    }

    for (Player& player : players)
    {
        if (!resetAssets && player.m_TotalRegions == 0)
        {
            continue;
        }

        if (resetAssets || (player.m_Food == 0 && player.m_Gold == 0 && player.m_TotalRegions > 0))
        {
            player.m_Food = player.m_FoodRegions * 12 + 20;
            player.m_Iron = player.m_IronRegions * 8 + 10;
            player.m_Gold = player.m_GoldRegions * 10 + 25;
            player.m_Wood = player.m_WoodRegions * 10 + 15;

            player.m_Swordsmen = std::max(1, player.m_TotalRegions / 2);
            player.m_Archers = std::max(0, player.m_TotalRegions / 3);
            player.m_Knights = player.m_Castles;
            player.m_Catapults = player.m_Castles / 2;
        }
        else if (resetAssets)
        {
            player.m_Food = 0;
            player.m_Iron = 0;
            player.m_Gold = 0;
            player.m_Wood = 0;
            player.m_Swordsmen = 0;
            player.m_Archers = 0;
            player.m_Knights = 0;
            player.m_Catapults = 0;
        }
    }
}

void CollectTurnIncomeFromRegions(const OverworldMap& map, std::vector<Player>& players)
{
    for (const OverworldRegionData& region : map.GetRegions())
    {
        if (region.m_OwnerId < 0 || region.m_OwnerId >= static_cast<int>(players.size()))
        {
            continue;
        }

        Player& player = players[static_cast<size_t>(region.m_OwnerId)];
        switch (region.m_Resource)
        {
        case CountyResource::Food:
            player.m_Food += kRegionFoodIncome;
            break;
        case CountyResource::Iron:
            player.m_Iron += kRegionIronIncome;
            break;
        case CountyResource::Gold:
            player.m_Gold += kRegionGoldIncome;
            break;
        case CountyResource::Wood:
            player.m_Wood += kRegionWoodIncome;
            break;
        }
    }
}