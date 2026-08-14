#include "CampaignAI.h"

#include "GameGlobals.h"
#include "OverworldMap.h"
#include "PlayerTasksConfig.h"

#include "../Geist/Source/Logging.h"
#include "../Geist/Source/RNG.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

AiObserverLog g_AiObserverLog;

void AiObserverLog::Clear()
{
    m_Entries.clear();
    m_CurrentTurn = 0;
}

void AiObserverLog::BeginTurn(int turn)
{
    m_CurrentTurn = turn;
    Add(-1, "--- Turn " + std::to_string(turn) + " ---");
}

void AiObserverLog::Add(int playerId, const std::string& message)
{
    if (message.empty())
    {
        return;
    }

    m_Entries.push_back(AiLogEntry{ m_CurrentTurn, playerId, message });
    while (static_cast<int>(m_Entries.size()) > kMaxEntries)
    {
        m_Entries.erase(m_Entries.begin());
    }

    if (playerId >= 0)
    {
        Log("AI[" + std::to_string(playerId) + "] " + message);
    }
    else
    {
        Log(message);
    }
}

void AiObserverLog::CollectFiltered(int filterPlayerId, int maxCount, std::vector<const AiLogEntry*>& out) const
{
    out.clear();
    for (int i = static_cast<int>(m_Entries.size()) - 1; i >= 0 && static_cast<int>(out.size()) < maxCount; --i)
    {
        const AiLogEntry& entry = m_Entries[static_cast<size_t>(i)];
        if (filterPlayerId >= 0 && entry.m_PlayerId != filterPlayerId && entry.m_PlayerId >= 0)
        {
            continue;
        }
        out.push_back(&entry);
    }
}

int GetCampaignPlayerCount(const CampaignSetup& setup)
{
    // Human games: you + N opponents (m_EnemyCount = 3..7).
    // All-AI observe: m_EnemyCount is total AI players (4..8).
    if (setup.m_AllAi)
    {
        return std::clamp(setup.m_EnemyCount, kMinAiObservePlayers, kMaxAiObservePlayers);
    }

    return std::clamp(1 + setup.m_EnemyCount, 1, kMaxCampaignPlayers);
}

namespace
{
    void AiNote(const Player& player, const std::string& message)
    {
        g_AiObserverLog.Add(player.m_Id, std::string(player.GetColorName()) + ": " + message);
    }

    void SyncGameDbOwner(int regionId, int ownerId)
    {
        if (RegionData* region = g_GameDatabase.GetRegion(regionId))
        {
            region->m_OwnerId = ownerId;
        }
    }

    void SyncGameDbCastle(int regionId, bool hasCastle)
    {
        if (RegionData* region = g_GameDatabase.GetRegion(regionId))
        {
            region->m_HasCastle = hasCastle;
            region->m_Income = 0;
        }

        if (const OverworldRegionData* ow = g_OverworldMap.GetRegion(regionId))
        {
            if (RegionData* region = g_GameDatabase.GetRegion(regionId))
            {
                region->m_Income = GetRegionTurnIncome(g_OverworldMap, *ow);
                region->m_HasCastle = ow->m_HasCastle;
            }
        }
    }

    int CountOwnedRegions(const OverworldMap& map, int ownerId)
    {
        int count = 0;
        for (const OverworldRegionData& region : map.GetRegions())
        {
            if (!region.m_IsWater && region.m_OwnerId == ownerId)
            {
                ++count;
            }
        }
        return count;
    }

    bool TryExecuteTaskOnRegion(
        Player& player,
        const std::string& taskId,
        OverworldMap& map,
        int regionId)
    {
        const PlayerTaskDefinition* task = g_PlayerTasksConfig.FindTaskById(taskId);
        if (!task)
        {
            return false;
        }

        return g_PlayerTasksConfig.ExecuteTask(player, *task, map, regionId);
    }

    bool TryExecuteTask(Player& player, const std::string& taskId, OverworldMap& map)
    {
        return TryExecuteTaskOnRegion(player, taskId, map, -1);
    }

    struct AttackCandidate
    {
        int m_RegionId = -1;
        int m_OwnerId = -1;
        int m_Score = 0;
        bool m_IsNeutral = false;
        bool m_HasCastle = false;
        bool m_IsFortified = false;
        int m_Income = 0;
    };

    std::vector<AttackCandidate> CollectAttackCandidates(const Player& player, const OverworldMap& map)
    {
        std::vector<AttackCandidate> candidates;
        for (const OverworldRegionData& region : map.GetRegions())
        {
            if (region.m_IsWater || region.m_OwnerId == player.m_Id)
            {
                continue;
            }

            if (!CanPlayerAttackRegion(player, map, region.m_Id))
            {
                continue;
            }

            AttackCandidate candidate;
            candidate.m_RegionId = region.m_Id;
            candidate.m_OwnerId = region.m_OwnerId;
            candidate.m_IsNeutral = region.m_OwnerId < 0;
            candidate.m_HasCastle = region.m_HasCastle;
            candidate.m_IsFortified = map.IsRegionFortified(region.m_Id);
            candidate.m_Income = GetRegionTurnIncome(map, region);

            // Prefer free land, then rich land; fortified/castle holdings are harder.
            candidate.m_Score = candidate.m_Income * 10;
            if (candidate.m_IsNeutral)
            {
                candidate.m_Score += 100;
            }
            if (candidate.m_HasCastle)
            {
                candidate.m_Score -= 25;
            }
            else if (candidate.m_IsFortified)
            {
                candidate.m_Score -= 12;
            }

            candidates.push_back(candidate);
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const AttackCandidate& a, const AttackCandidate& b)
            {
                return a.m_Score > b.m_Score;
            });
        return candidates;
    }

    int EstimateDefendStrength(const OverworldMap& map, const std::vector<Player>& players, int ownerId, bool fortified)
    {
        if (ownerId < 0 || ownerId >= static_cast<int>(players.size()))
        {
            // Neutral garrison is 1S+1A (~2) with ~50% chance of +1 → expected ~2.5.
            // Use 3 as AI planning estimate; fortified still gets the usual bonus.
            return fortified ? 3 + 8 : 3;
        }

        const Player& defender = players[static_cast<size_t>(ownerId)];
        const int regions = std::max(1, CountOwnedRegions(map, ownerId));
        // Local defense scales with share of army "garrisoned" loosely.
        int strength = std::max(1, GetPlayerArmyStrength(defender) / std::max(1, regions / 2 + 1));
        if (fortified)
        {
            strength += 8;
        }
        return strength;
    }

    bool ShouldAttemptAttack(
        const Player& attacker,
        const AttackCandidate& target,
        const OverworldMap& map,
        const std::vector<Player>& players,
        AiPersonality personality)
    {
        if (GetPlayerArmyStrength(attacker) <= 0)
        {
            return false;
        }

        const int attackPower = GetPlayerArmyStrength(attacker);
        const int defendPower = EstimateDefendStrength(map, players, target.m_OwnerId, target.m_IsFortified);

        switch (personality)
        {
        case AiPersonality::Turtle:
            // Only claim free land when army is already large and odds are trivial.
            return target.m_IsNeutral && attackPower >= 6 && defendPower <= 2;
        case AiPersonality::Expansionist:
            // Take everything that is not a brutal fight.
            return attackPower + 2 >= defendPower || target.m_IsNeutral;
        case AiPersonality::Balanced:
        default:
            if (target.m_IsNeutral)
            {
                return attackPower >= 2;
            }
            // Need a comfortable edge before picking a fight.
            return attackPower >= defendPower + 3;
        }
    }

    int RecruitGreedily(Player& player, OverworldMap& map, int maxRecruits, bool preferCheap)
    {
        const char* recruitOrderCheap[] = {
            "recruitInfantry", "recruitArchers", "recruitKnights"
        };
        const char* recruitOrderQuality[] = {
            "recruitKnights", "recruitArchers", "recruitInfantry"
        };

        const char** order = preferCheap ? recruitOrderCheap : recruitOrderQuality;
        int recruited = 0;
        bool progressed = true;
        while (progressed && recruited < maxRecruits)
        {
            progressed = false;
            for (int i = 0; i < 3; ++i)
            {
                if (TryExecuteTask(player, order[i], map))
                {
                    ++recruited;
                    progressed = true;
                    break;
                }
            }
        }
        return recruited;
    }

    void RunTurtleAi(Player& player, OverworldMap& map, std::vector<Player>& players, RNG& rng)
    {
        bool didSomething = false;

        // Rare expansion: ~15% of turns, and only if a trivial free county exists.
        if (rng.Random(100) < 15)
        {
            const auto candidates = CollectAttackCandidates(player, map);
            for (const AttackCandidate& candidate : candidates)
            {
                if (!ShouldAttemptAttack(player, candidate, map, players, AiPersonality::Turtle))
                {
                    continue;
                }

                if (ResolveRegionAttack(player, map, players, candidate.m_RegionId, nullptr, &rng))
                {
                    didSomething = true;
                }
                break;
            }
        }

        // Dump resources into troops until broke (upkeep will catch up later).
        const int recruited = RecruitGreedily(player, map, 12, true);
        if (recruited > 0)
        {
            AiNote(player, "recruited " + std::to_string(recruited) + " unit(s) (stockpiling)");
            didSomething = true;
        }

        if (!didSomething)
        {
            AiNote(player, "holds position (no recruits affordable / no easy land)");
        }
    }

    void RunExpansionistAi(Player& player, OverworldMap& map, std::vector<Player>& players, RNG& /*rng*/)
    {
        // One attack per turn for all sides.
        bool attacked = false;
        {
            const auto candidates = CollectAttackCandidates(player, map);
            for (const AttackCandidate& candidate : candidates)
            {
                if (!ShouldAttemptAttack(player, candidate, map, players, AiPersonality::Expansionist))
                {
                    continue;
                }

                const PlayerTaskDefinition* attackTask = g_PlayerTasksConfig.FindTaskById("attack");
                if (attackTask && !attackTask->m_Cost.CanAfford(player))
                {
                    break;
                }

                // Expansionist has no per-turn RNG in signature — use vital/non-vital via default.
                attacked = ResolveRegionAttack(player, map, players, candidate.m_RegionId, nullptr, nullptr)
                    || player.m_AttacksThisTurn > 0;
                break;
            }
        }

        const int regions = std::max(1, player.m_TotalRegions);
        const int army = GetPlayerArmyStrength(player);
        int recruited = 0;
        if (army < regions + 2)
        {
            recruited = RecruitGreedily(player, map, 3, true);
            if (recruited > 0)
            {
                AiNote(player, "recruited " + std::to_string(recruited) + " to cover sprawl");
            }
        }

        if (!attacked && recruited == 0)
        {
            AiNote(player, "no expansion this turn (blocked or broke)");
        }
    }

    void RunBalancedAi(Player& player, OverworldMap& map, std::vector<Player>& players, RNG& rng)
    {
        // 1) Improve a weak owned region (boost income before expanding too hard).
        {
            int bestRegion = -1;
            int bestScore = -1;
            for (const OverworldRegionData& region : map.GetRegions())
            {
                if (region.m_IsWater || region.m_OwnerId != player.m_Id)
                {
                    continue;
                }

                // Prefer low multipliers that still have room to grow.
                if (region.m_OutputMultiplier >= 4)
                {
                    continue;
                }

                const int score = 10 - region.m_OutputMultiplier * 2 + GetRegionTurnIncome(map, region);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestRegion = region.m_Id;
                }
            }

            if (bestRegion >= 0 && rng.Random(100) < 55)
            {
                if (TryExecuteTaskOnRegion(player, "improveRegion", map, bestRegion))
                {
                    AiNote(player, "improved region " + std::to_string(bestRegion));
                    if (RegionData* dbRegion = g_GameDatabase.GetRegion(bestRegion))
                    {
                        if (const OverworldRegionData* ow = map.GetRegion(bestRegion))
                        {
                            dbRegion->m_Income = GetRegionTurnIncome(map, *ow);
                        }
                    }
                }
            }
        }

        // 2) Start a castle on a border / high-value county when stable.
        {
            ResourceAmount projected{};
            ComputePlayerTurnDelta(map, player, projected);
            const bool foodHealthy = projected.m_Food >= 0 && player.m_Food >= 15;
            if (foodHealthy && player.m_Castles < std::max(1, player.m_TotalRegions / 3))
            {
                int bestRegion = -1;
                int bestScore = -1;
                for (const OverworldRegionData& region : map.GetRegions())
                {
                    if (region.m_IsWater || region.m_OwnerId != player.m_Id
                        || region.m_HasCastle || region.m_CastleBuildTurnsRemaining > 0)
                    {
                        continue;
                    }

                    int borderThreat = 0;
                    for (int neighborId : map.GetTraversableAdjacentRegions(region.m_Id))
                    {
                        const OverworldRegionData* neighbor = map.GetRegion(neighborId);
                        if (!neighbor || neighbor->m_IsWater)
                        {
                            continue;
                        }
                        if (neighbor->m_OwnerId != player.m_Id)
                        {
                            ++borderThreat;
                        }
                    }

                    const int score = borderThreat * 5 + GetRegionTurnIncome(map, region);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestRegion = region.m_Id;
                    }
                }

                if (bestRegion >= 0 && bestScore >= 5)
                {
                    if (TryExecuteTaskOnRegion(player, "buildCastle", map, bestRegion))
                    {
                        AiNote(player, "started castle in region " + std::to_string(bestRegion));
                    }
                }
            }
        }

        // 3) Expand when opportunity looks good (one attack per turn).
        bool attacked = false;
        {
            const auto candidates = CollectAttackCandidates(player, map);
            for (const AttackCandidate& candidate : candidates)
            {
                if (!ShouldAttemptAttack(player, candidate, map, players, AiPersonality::Balanced))
                {
                    continue;
                }

                ResolveRegionAttack(player, map, players, candidate.m_RegionId, nullptr, &rng);
                attacked = player.m_AttacksThisTurn > 0;
                break;
            }
        }

        // 4) Recruit carefully — do not dig into a deep food deficit.
        {
            ResourceAmount projected{};
            ComputePlayerTurnDelta(map, player, projected);
            int recruitBudget = 4;
            if (projected.m_Food < 0)
            {
                recruitBudget = 0;
            }
            else if (projected.m_Food < 2)
            {
                recruitBudget = 1;
            }

            const int army = GetPlayerArmyStrength(player);
            const int regions = std::max(1, player.m_TotalRegions);
            int recruited = 0;
            if (army < regions * 2)
            {
                recruited = RecruitGreedily(player, map, recruitBudget, false);
            }
            else if (recruitBudget > 0 && player.m_Gold > 20)
            {
                recruited = RecruitGreedily(player, map, 1, false);
            }

            if (recruited > 0)
            {
                AiNote(player, "recruited " + std::to_string(recruited) + " (balanced)");
            }
            else if (!attacked)
            {
                AiNote(player, "consolidating holdings");
            }
        }
    }
}

const char* AiPersonalityName(AiPersonality personality)
{
    switch (personality)
    {
    case AiPersonality::Turtle:
        return "Turtle";
    case AiPersonality::Expansionist:
        return "Expansionist";
    case AiPersonality::Balanced:
        return "Balanced";
    default:
        return "Unknown";
    }
}

void AssignCampaignAiPersonalities(std::vector<Player>& players, const CampaignSetup& setup)
{
    // Difficulty biases the mix of AI types the player faces.
    // Squire: all turtles. Baron: turtles + expansionists. Viscount: expansionists.
    // Marquis: expansionists + balanced. King: all balanced.
    int aiIndex = 0;
    for (Player& player : players)
    {
        if (player.m_IsHuman)
        {
            player.m_AiPersonality = AiPersonality::Balanced;
            continue;
        }

        AiPersonality personality = AiPersonality::Expansionist;
        switch (setup.m_Difficulty)
        {
        case Difficulty::Squire:
            personality = AiPersonality::Turtle;
            break;
        case Difficulty::Baron:
            personality = (aiIndex % 2 == 0) ? AiPersonality::Turtle : AiPersonality::Expansionist;
            break;
        case Difficulty::Viscount:
            personality = AiPersonality::Expansionist;
            break;
        case Difficulty::Marquis:
            personality = (aiIndex % 2 == 0) ? AiPersonality::Expansionist : AiPersonality::Balanced;
            break;
        case Difficulty::King:
            personality = AiPersonality::Balanced;
            break;
        default:
            personality = AiPersonality::Expansionist;
            break;
        }

        // Ensure variety when there are 3+ opponents: cycle all three types once,
        // then fall back to difficulty bias for the rest.
        if (players.size() >= 4 && aiIndex < 3 && setup.m_Difficulty != Difficulty::Squire
            && setup.m_Difficulty != Difficulty::King)
        {
            personality = static_cast<AiPersonality>(aiIndex % 3);
        }

        player.m_AiPersonality = personality;
        g_AiObserverLog.Add(player.m_Id, std::string(player.GetColorName()) + " assigned "
            + AiPersonalityName(personality));
        ++aiIndex;
    }
}

int GetPlayerArmyStrength(const Player& player)
{
    return player.m_Swordsmen
        + player.m_Archers
        + player.m_Knights * 2
        + player.m_Catapults
        + player.m_SiegeTowers;
}

namespace
{
    struct ArmyCounts
    {
        int m_Swordsmen = 0;
        int m_Archers = 0;
        int m_Knights = 0;
        int m_Catapults = 0;
        int m_SiegeTowers = 0;
    };

    ArmyCounts SnapshotPlayerArmy(const Player& player)
    {
        return ArmyCounts{
            player.m_Swordsmen,
            player.m_Archers,
            player.m_Knights,
            player.m_Catapults,
            player.m_SiegeTowers
        };
    }

    int ArmyCountsStrength(const ArmyCounts& army)
    {
        return army.m_Swordsmen
            + army.m_Archers
            + army.m_Knights * 2
            + army.m_Catapults
            + army.m_SiegeTowers;
    }

    std::string FormatArmyCounts(const ArmyCounts& army)
    {
        std::ostringstream oss;
        oss << "S" << army.m_Swordsmen
            << " A" << army.m_Archers
            << " K" << army.m_Knights
            << " C" << army.m_Catapults;
        if (army.m_SiegeTowers > 0)
        {
            oss << " T" << army.m_SiegeTowers;
        }
        return oss.str();
    }

    void ApplyLossPointsToArmy(ArmyCounts& army, int lossPoints)
    {
        lossPoints = std::max(0, lossPoints);
        while (lossPoints > 0
            && (army.m_Swordsmen + army.m_Archers + army.m_Knights + army.m_Catapults) > 0)
        {
            if (army.m_Swordsmen > 0)
            {
                --army.m_Swordsmen;
                --lossPoints;
            }
            else if (army.m_Archers > 0)
            {
                --army.m_Archers;
                --lossPoints;
            }
            else if (army.m_Knights > 0)
            {
                --army.m_Knights;
                lossPoints -= 2;
            }
            else if (army.m_Catapults > 0)
            {
                --army.m_Catapults;
                --lossPoints;
            }
            else
            {
                break;
            }
        }
    }

    void WriteArmyToPlayer(Player& player, const ArmyCounts& army)
    {
        player.m_Swordsmen = std::max(0, army.m_Swordsmen);
        player.m_Archers = std::max(0, army.m_Archers);
        player.m_Knights = std::max(0, army.m_Knights);
        player.m_Catapults = std::max(0, army.m_Catapults);
        player.m_SiegeTowers = std::max(0, army.m_SiegeTowers);

        for (auto& entry : player.m_ActiveTasks)
        {
            if (entry.first == "recruitInfantry")
            {
                entry.second = std::min(entry.second, player.m_Swordsmen);
            }
            else if (entry.first == "recruitArchers")
            {
                entry.second = std::min(entry.second, player.m_Archers);
            }
            else if (entry.first == "recruitKnights")
            {
                entry.second = std::min(entry.second, player.m_Knights);
            }
        }
        player.m_ActiveTasks.erase(
            std::remove_if(player.m_ActiveTasks.begin(), player.m_ActiveTasks.end(),
                [](const std::pair<std::string, int>& e) { return e.second <= 0; }),
            player.m_ActiveTasks.end());
    }

    RNG* ResolveBattleRng(RNG* preferred)
    {
        if (preferred)
        {
            return preferred;
        }
        if (g_vitalRNG)
        {
            return g_vitalRNG.get();
        }
        if (g_nonVitalRNG)
        {
            return g_nonVitalRNG.get();
        }
        return nullptr;
    }
}

void RollNeutralRegionGarrison(int& outSwordsmen, int& outArchers, RNG* rng)
{
    outSwordsmen = 1;
    outArchers = 1;

    RNG* rollRng = ResolveBattleRng(rng);
    const bool extraUnit = rollRng ? (rollRng->Random(2) == 0) : false;
    if (!extraUnit)
    {
        return;
    }

    // 50/50 swordsmen vs archers for the bonus unit.
    if (rollRng && rollRng->Random(2) == 0)
    {
        ++outSwordsmen;
    }
    else
    {
        ++outArchers;
    }
}

bool CanPlayerAttackRegion(const Player& attacker, const OverworldMap& map, int targetRegionId)
{
    const OverworldRegionData* target = map.GetRegion(targetRegionId);
    if (!target || target->m_IsWater || target->m_OwnerId == attacker.m_Id)
    {
        return false;
    }

    if (attacker.m_AttacksThisTurn >= 1)
    {
        return false;
    }

    if (GetPlayerArmyStrength(attacker) <= 0)
    {
        return false;
    }

    for (int neighborId : map.GetTraversableAdjacentRegions(targetRegionId))
    {
        const OverworldRegionData* neighbor = map.GetRegion(neighborId);
        if (neighbor && !neighbor->m_IsWater && neighbor->m_OwnerId == attacker.m_Id)
        {
            return true;
        }
    }

    return false;
}

bool ResolveRegionAttack(
    Player& attacker,
    OverworldMap& map,
    std::vector<Player>& players,
    int targetRegionId,
    std::string* outMessage,
    RNG* rng)
{
    if (attacker.m_AttacksThisTurn >= 1)
    {
        if (outMessage)
        {
            *outMessage = "Already attacked this turn";
        }
        return false;
    }

    if (!CanPlayerAttackRegion(attacker, map, targetRegionId))
    {
        if (outMessage)
        {
            *outMessage = "Cannot attack that county";
        }
        return false;
    }

    const PlayerTaskDefinition* attackTask = g_PlayerTasksConfig.FindTaskById("attack");
    if (attackTask && !attackTask->m_Cost.CanAfford(attacker))
    {
        if (outMessage)
        {
            *outMessage = "Cannot afford attack";
        }
        return false;
    }

    OverworldRegionData* target = map.GetRegion(targetRegionId);
    if (!target)
    {
        return false;
    }

    if (attackTask)
    {
        attackTask->m_Cost.Deduct(attacker);
    }

    // One attack order per player per turn (win or lose).
    ++attacker.m_AttacksThisTurn;

    const int defenderId = target->m_OwnerId;
    const bool wasNeutral = defenderId < 0;
    const bool fortified = map.IsRegionFortified(targetRegionId);

    ArmyCounts atkBefore = SnapshotPlayerArmy(attacker);
    ArmyCounts defBefore{};
    std::string defenderName = "Neutral";

    if (wasNeutral)
    {
        RollNeutralRegionGarrison(defBefore.m_Swordsmen, defBefore.m_Archers, rng);
        defenderName = "Neutral garrison";
    }
    else if (defenderId >= 0 && defenderId < static_cast<int>(players.size()))
    {
        defBefore = SnapshotPlayerArmy(players[static_cast<size_t>(defenderId)]);
        defenderName = players[static_cast<size_t>(defenderId)].GetColorName();
    }

    int attackPower = ArmyCountsStrength(atkBefore);
    int defendPower = ArmyCountsStrength(defBefore);
    // Owned land: dilute full roster across holdings (same idea as EstimateDefendStrength).
    if (!wasNeutral && defenderId >= 0)
    {
        const int regions = std::max(1, CountOwnedRegions(map, defenderId));
        defendPower = std::max(1, defendPower / std::max(1, regions / 2 + 1));
    }
    if (fortified)
    {
        defendPower += 8;
    }

    // Slight attacker edge on a pure tie (attackPower + 1 >= defendPower).
    const bool attackerWins = (attackPower + 1 >= defendPower);

    ArmyCounts atkAfter = atkBefore;
    ArmyCounts defAfter = defBefore;

    if (!attackerWins)
    {
        ApplyLossPointsToArmy(atkAfter, std::max(1, defendPower / 3));
        ApplyLossPointsToArmy(defAfter, std::max(1, attackPower / 4));
    }
    else
    {
        ApplyLossPointsToArmy(atkAfter, std::max(1, defendPower / 4));
        // Defeated garrison / army is gutted harder.
        ApplyLossPointsToArmy(defAfter, std::max(1, attackPower / 3));
        // If attacker still has any troops, wipe remaining defenders (county falls).
        if (ArmyCountsStrength(atkAfter) > 0)
        {
            defAfter = ArmyCounts{};
        }
    }

    WriteArmyToPlayer(attacker, atkAfter);
    if (!wasNeutral && defenderId >= 0 && defenderId < static_cast<int>(players.size()))
    {
        WriteArmyToPlayer(players[static_cast<size_t>(defenderId)], defAfter);
    }

    if (attackerWins)
    {
        target->m_OwnerId = attacker.m_Id;
        if (target->m_CastleBuildTurnsRemaining > 0 && !target->m_HasCastle)
        {
            target->m_CastleBuildTurnsRemaining = 0;
        }

        SyncGameDbOwner(targetRegionId, attacker.m_Id);
        SyncGameDbCastle(targetRegionId, target->m_HasCastle);
    }

    SyncPlayersFromOverworld(map, players, false);

    // Explicit multi-line turn log report.
    {
        std::ostringstream header;
        header << attacker.GetColorName() << " attacks county " << targetRegionId
               << " (" << defenderName
               << (fortified ? ", fortified" : "")
               << ")";
        g_AiObserverLog.Add(attacker.m_Id, header.str());
        g_AiObserverLog.Add(attacker.m_Id,
            "  " + std::string(attacker.GetColorName()) + " force: " + FormatArmyCounts(atkBefore));
        g_AiObserverLog.Add(attacker.m_Id,
            "  " + defenderName + " force: " + FormatArmyCounts(defBefore));
        g_AiObserverLog.Add(attacker.m_Id,
            attackerWins
                ? ("  Result: " + std::string(attacker.GetColorName()) + " WINS — county captured")
                : ("  Result: " + defenderName + " HOLDS — attack fails"));
        g_AiObserverLog.Add(attacker.m_Id,
            "  " + std::string(attacker.GetColorName()) + " left: " + FormatArmyCounts(atkAfter));
        g_AiObserverLog.Add(attacker.m_Id,
            "  " + defenderName + " left: " + FormatArmyCounts(defAfter));
    }

    if (outMessage)
    {
        std::ostringstream oss;
        if (attackerWins)
        {
            oss << "Captured county " << targetRegionId
                << " (" << FormatArmyCounts(atkBefore) << " vs " << FormatArmyCounts(defBefore)
                << " → left " << FormatArmyCounts(atkAfter) << ")";
        }
        else
        {
            oss << "Failed attack on county " << targetRegionId
                << " (" << FormatArmyCounts(atkBefore) << " vs " << FormatArmyCounts(defBefore)
                << " → left " << FormatArmyCounts(atkAfter) << ")";
        }
        *outMessage = oss.str();
    }

    return attackerWins;
}

void RunCampaignAiTurn(Player& player, OverworldMap& map, std::vector<Player>& players, RNG& rng)
{
    if (player.m_IsHuman || player.m_TotalRegions <= 0)
    {
        return;
    }

    switch (player.m_AiPersonality)
    {
    case AiPersonality::Turtle:
        RunTurtleAi(player, map, players, rng);
        break;
    case AiPersonality::Expansionist:
        RunExpansionistAi(player, map, players, rng);
        break;
    case AiPersonality::Balanced:
        RunBalancedAi(player, map, players, rng);
        break;
    }
}

void RunAllCampaignAiTurns(OverworldMap& map, std::vector<Player>& players, RNG& rng, int turnNumber)
{
    g_AiObserverLog.BeginTurn(turnNumber);

    for (Player& player : players)
    {
        RunCampaignAiTurn(player, map, players, rng);
    }

    // Region counts may have changed during conquests.
    SyncPlayersFromOverworld(map, players, false);
}
