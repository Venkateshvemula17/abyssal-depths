#pragma once
#include "types.h"
#include <pqxx/pqxx>
#include <string>
#include <optional>
#include <vector>

class DBClient {
public:
    DBClient(const std::string& connStr);

    // ── Auth ────────────────────────────────────────────────
    bool        registerPlayer (const std::string& username,
                                 const std::string& email,
                                 const std::string& passwordHash);
    std::optional<std::string>
                loginPlayer    (const std::string& username,
                                 const std::string& passwordHash);

    // ── Characters ──────────────────────────────────────────
    std::string createCharacter(const std::string& playerId,
                                 const std::string& name,
                                 const std::string& charClass);
    std::optional<Player>
                getCharacter   (const std::string& characterId);

    // ── Sessions ─────────────────────────────────────────────
    std::string createSession  (const std::string& dungeonId,
                                 const std::string& roomCode,
                                 int maxPlayers);
    bool        addPlayerToSession(const std::string& sessionId,
                                   const std::string& playerId,
                                   const std::string& characterId);
    void        updateSessionStatus(const std::string& sessionId,
                                     const std::string& status);
    void        endSession     (const std::string& sessionId);

    // ── Dungeons ─────────────────────────────────────────────
    std::vector<std::string> getDungeons();
    std::string getDungeonInfo(const std::string& dungeonId);

    // ── Enemies ──────────────────────────────────────────────
    std::vector<Enemy> getEnemiesForDungeon(const std::string& dungeonId,
                                             int floor);

    // ── Leaderboard ──────────────────────────────────────────
    std::string getLeaderboard(int limit = 50);
    void        updateLeaderboard(const std::string& playerId,
                                   int score, int kills, int floor);

    // ── Chat ─────────────────────────────────────────────────
    void saveChatMessage(const std::string& sessionId,
                          const std::string& playerId,
                          const std::string& message,
                          bool isNpc = false,
                          const std::string& npcName = "");

private:
    pqxx::connection _conn;
    std::string      _connStr;
    void             _reconnect();
};
