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

constexpr int kRegionBaseIncome = 1;

// Overworld AI personalities (see CampaignAI for behavior).
enum class AiPersonality : int
{
    Turtle = 0,       // Rarely expands; recruits until upkeep bankrupts it
    Expansionist = 1, // Grabs every available territory; never fortifies/improves
    Balanced = 2      // Expands when sensible; fortifies and improves holdings
};

struct Player
{
    int m_Id = -1;
    bool m_IsHuman = false;
    // Display / map color (0..kMaxCampaignPlayers-1). Independent of seat m_Id.
    int m_ColorIndex = 0;
    AiPersonality m_AiPersonality = AiPersonality::Turtle;

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
    int m_SiegeTowers = 0;
    int m_Happiness = 50;

    // Overworld conquests: max one attack order per player per turn.
    int m_AttacksThisTurn = 0;

    std::vector<int> m_Relations;
    std::vector<std::pair<std::string, int>> m_ActiveTasks;

    Color GetColor() const;
    const char* GetColorName() const;
    std::string GetRelationLabel(int otherPlayerId) const;
};

// Palette entry by color index (0=Blue … 7=Pink).
Color ColorFromPlayerIndex(int colorIndex);
const char* ColorNameFromPlayerIndex(int colorIndex);

// Seat/owner id → display color/name (uses Player::m_ColorIndex when roster is live).
Color PlayerOwnerColor(int ownerId);
const char* PlayerOwnerName(int ownerId);
const char* DiplomaticRelationName(DiplomaticRelation relation);

// hasHumanPlayer: when false, every seat is AI (observer / all-AI games).
// humanColorIndex: preferred palette slot for the human seat (ignored if all-AI).
void InitializeCampaignPlayers(
    std::vector<Player>& players,
    int playerCount,
    bool hasHumanPlayer = true,
    int humanColorIndex = 0);
void SyncPlayersFromOverworld(const class OverworldMap& map, std::vector<Player>& players, bool resetAssets);
void CollectTurnIncomeFromRegions(const class OverworldMap& map, std::vector<Player>& players);
void ProcessCastleConstruction(class OverworldMap& map, std::vector<Player>& players);
void ComputePlayerTurnDelta(const class OverworldMap& map, const Player& player, struct ResourceAmount& outDelta);

#endif