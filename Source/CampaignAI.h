#ifndef _CAMPAIGNAI_H_
#define _CAMPAIGNAI_H_

#include "Player.h"

#include <string>
#include <vector>

class OverworldMap;
class RNG;
struct CampaignSetup;

const char* AiPersonalityName(AiPersonality personality);

void AssignCampaignAiPersonalities(std::vector<Player>& players, const CampaignSetup& setup);

// Military power used for auto-resolve conquest.
int GetPlayerArmyStrength(const Player& player);

// True if target is land, not owned by attacker, and adjacent (traversable) to an owned county.
bool CanPlayerAttackRegion(const Player& attacker, const OverworldMap& map, int targetRegionId);

// Neutral county garrison: 1 swordsmen + 1 archers, plus 50% chance of one extra S or A.
// Writes counts into outSwordsmen / outArchers. Uses rng when non-null, else g_vitalRNG / g_nonVitalRNG.
void RollNeutralRegionGarrison(int& outSwordsmen, int& outArchers, RNG* rng = nullptr);

// Auto-resolve conquest. Returns true if the attacker captured the region.
// Writes a multi-line battle report into the turn log; outMessage gets a one-line summary when non-null.
bool ResolveRegionAttack(
    Player& attacker,
    OverworldMap& map,
    std::vector<Player>& players,
    int targetRegionId,
    std::string* outMessage = nullptr,
    RNG* rng = nullptr);

// Run one overworld decision pass for every non-human living player.
void RunAllCampaignAiTurns(OverworldMap& map, std::vector<Player>& players, RNG& rng, int turnNumber);

// Run one decision pass for a single AI player.
void RunCampaignAiTurn(Player& player, OverworldMap& map, std::vector<Player>& players, RNG& rng);

// ---------------------------------------------------------------------------
// AI observer log — recent actions for the MainState observer pane
// ---------------------------------------------------------------------------

struct AiLogEntry
{
    int m_Turn = 0;
    int m_PlayerId = -1;       // Primary actor (-1 = system)
    int m_OtherPlayerId = -1;  // Secondary party (e.g. battle defender); -1 = none
    std::string m_Message;
};

class AiObserverLog
{
public:
    static constexpr int kMaxEntries = 250;

    void Clear();
    void BeginTurn(int turn);
    void Add(int playerId, const std::string& message);
    // otherPlayerId marks a second involved seat (defender, spy target, etc.).
    void Add(int playerId, int otherPlayerId, const std::string& message);
    int GetCurrentTurn() const { return m_CurrentTurn; }
    const std::vector<AiLogEntry>& GetEntries() const { return m_Entries; }

    // Newest-first filtered view. filterPlayerId < 0 means all players.
    // relevantPlayerId >= 0: only system lines + entries involving that seat
    // (primary or other). Used in single-player to hide AI-vs-AI chatter.
    void CollectFiltered(
        int filterPlayerId,
        int maxCount,
        std::vector<const AiLogEntry*>& out,
        int relevantPlayerId = -1) const;

private:
    std::vector<AiLogEntry> m_Entries;
    int m_CurrentTurn = 0;
};

extern AiObserverLog g_AiObserverLog;

// Campaign player-count helper (human + opponents, or all-AI roster size).
int GetCampaignPlayerCount(const CampaignSetup& setup);

#endif
