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
    // Human games: you + N opponents. All-AI: same roster size, everyone is AI.
    return std::clamp(1 + setup.m_EnemyCount, 1, kMaxCampaignPlayers);
}

namespace
{
    void AiNote(const Player& player, const std::string& message)
    {
        g_AiObserverLog.Add(player.m_Id, std::string(player.GetColorName()) + " ("
            + AiPersonalityName(player.m_AiPersonality) + "): " + message);
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
                region->m_Income = GetRegionTurnIncome(*ow);
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
            candidate.m_Income = GetRegionTurnIncome(region);

            // Prefer free land, then rich land, avoid castles unless expansionist/hard.
            candidate.m_Score = candidate.m_Income * 10;
            if (candidate.m_IsNeutral)
            {
                candidate.m_Score += 100;
            }
            if (candidate.m_HasCastle)
            {
                candidate.m_Score -= 25;
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

    int EstimateDefendStrength(const OverworldMap& map, const std::vector<Player>& players, int ownerId, bool hasCastle)
    {
        if (ownerId < 0 || ownerId >= static_cast<int>(players.size()))
        {
            return hasCastle ? 4 : 1;
        }

        const Player& defender = players[static_cast<size_t>(ownerId)];
        const int regions = std::max(1, CountOwnedRegions(map, ownerId));
        // Local defense scales with share of army "garrisoned" loosely.
        int strength = std::max(1, GetPlayerArmyStrength(defender) / std::max(1, regions / 2 + 1));
        if (hasCastle)
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
        const int defendPower = EstimateDefendStrength(map, players, target.m_OwnerId, target.m_HasCastle);

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

                std::string message;
                if (ResolveRegionAttack(player, map, players, candidate.m_RegionId, &message))
                {
                    AiNote(player, message);
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
        bool captured = true;
        int captures = 0;
        while (captured && captures < 8)
        {
            captured = false;
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
                    if (player.m_Iron < attackTask->m_Cost.m_Iron)
                    {
                        break;
                    }
                }

                std::string message;
                if (ResolveRegionAttack(player, map, players, candidate.m_RegionId, &message))
                {
                    AiNote(player, message);
                    captured = true;
                    ++captures;
                    break;
                }
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

        if (captures == 0 && recruited == 0)
        {
            AiNote(player, "no expansion this turn (blocked or broke)");
        }
        else if (captures > 0)
        {
            AiNote(player, "expansion wave: " + std::to_string(captures) + " county(ies)");
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

                const int score = 10 - region.m_OutputMultiplier * 2 + GetRegionTurnIncome(region);
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
                            dbRegion->m_Income = GetRegionTurnIncome(*ow);
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

                    const int score = borderThreat * 5 + GetRegionTurnIncome(region);
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

        // 3) Expand when opportunity looks good (1-3 captures).
        int captures = 0;
        {
            bool captured = true;
            while (captured && captures < 3)
            {
                captured = false;
                const auto candidates = CollectAttackCandidates(player, map);
                for (const AttackCandidate& candidate : candidates)
                {
                    if (!ShouldAttemptAttack(player, candidate, map, players, AiPersonality::Balanced))
                    {
                        continue;
                    }

                    std::string message;
                    if (ResolveRegionAttack(player, map, players, candidate.m_RegionId, &message))
                    {
                        AiNote(player, message);
                        captured = true;
                        ++captures;
                        break;
                    }
                }
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
            else if (captures == 0)
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

bool CanPlayerAttackRegion(const Player& attacker, const OverworldMap& map, int targetRegionId)
{
    const OverworldRegionData* target = map.GetRegion(targetRegionId);
    if (!target || target->m_IsWater || target->m_OwnerId == attacker.m_Id)
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
    std::string* outMessage)
{
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

    const int defenderId = target->m_OwnerId;
    const bool wasNeutral = defenderId < 0;
    const int attackPower = GetPlayerArmyStrength(attacker);
    const int defendPower = EstimateDefendStrength(map, players, defenderId, target->m_HasCastle);

    // Light RNG-free auto-resolve: edge of 0 is a hard fight that still favors attacker slightly.
    const bool attackerWins = wasNeutral || (attackPower + 1 >= defendPower);

    auto applyLosses = [](Player& p, int lossPoints)
    {
        lossPoints = std::max(0, lossPoints);
        while (lossPoints > 0 && (p.m_Swordsmen + p.m_Archers + p.m_Knights) > 0)
        {
            if (p.m_Swordsmen > 0)
            {
                --p.m_Swordsmen;
                --lossPoints;
            }
            else if (p.m_Archers > 0)
            {
                --p.m_Archers;
                --lossPoints;
            }
            else if (p.m_Knights > 0)
            {
                --p.m_Knights;
                lossPoints -= 2;
            }
            else
            {
                break;
            }
        }

        // Keep active-task upkeep stacks roughly in sync with living units.
        int infantryStacks = 0;
        int archerStacks = 0;
        int knightStacks = 0;
        for (auto& entry : p.m_ActiveTasks)
        {
            if (entry.first == "recruitInfantry")
            {
                entry.second = std::min(entry.second, std::max(0, p.m_Swordsmen));
                infantryStacks = entry.second;
            }
            else if (entry.first == "recruitArchers")
            {
                entry.second = std::min(entry.second, std::max(0, p.m_Archers));
                archerStacks = entry.second;
            }
            else if (entry.first == "recruitKnights")
            {
                entry.second = std::min(entry.second, std::max(0, p.m_Knights));
                knightStacks = entry.second;
            }
        }
        (void)infantryStacks;
        (void)archerStacks;
        (void)knightStacks;
        p.m_ActiveTasks.erase(
            std::remove_if(p.m_ActiveTasks.begin(), p.m_ActiveTasks.end(),
                [](const std::pair<std::string, int>& e) { return e.second <= 0; }),
            p.m_ActiveTasks.end());
    };

    if (!attackerWins)
    {
        applyLosses(attacker, std::max(1, defendPower / 3));
        if (defenderId >= 0 && defenderId < static_cast<int>(players.size()))
        {
            applyLosses(players[static_cast<size_t>(defenderId)], std::max(1, attackPower / 4));
        }

        if (outMessage)
        {
            *outMessage = "Attack on county " + std::to_string(targetRegionId) + " failed";
        }
        return false;
    }

    // Capture
    applyLosses(attacker, wasNeutral ? 0 : std::max(1, defendPower / 4));
    if (defenderId >= 0 && defenderId < static_cast<int>(players.size()))
    {
        applyLosses(players[static_cast<size_t>(defenderId)], std::max(1, attackPower / 3));
        // Cancel incomplete castle builds on conquered land (finished castles remain).
    }

    target->m_OwnerId = attacker.m_Id;
    if (target->m_CastleBuildTurnsRemaining > 0 && !target->m_HasCastle)
    {
        target->m_CastleBuildTurnsRemaining = 0;
    }

    SyncGameDbOwner(targetRegionId, attacker.m_Id);
    SyncGameDbCastle(targetRegionId, target->m_HasCastle);

    SyncPlayersFromOverworld(map, players, false);

    if (outMessage)
    {
        std::ostringstream oss;
        oss << "Captured county " << targetRegionId;
        if (wasNeutral)
        {
            oss << " (unclaimed)";
        }
        else if (defenderId >= 0 && defenderId < static_cast<int>(players.size()))
        {
            oss << " from " << players[static_cast<size_t>(defenderId)].GetColorName();
        }
        *outMessage = oss.str();
    }

    return true;
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
