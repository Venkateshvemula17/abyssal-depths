#pragma once
#include "types.h"
#include "db_client.h"
#include "redis_client.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

class GameEngine {
public:
    GameEngine(DBClient& db, RedisClient& redis);
    ~GameEngine();

    // Session Management
    std::string createSession(const std::string& dungeonId,
                               int maxPlayers = 4);
    bool        joinSession  (const std::string& roomCode,
                               const std::string& playerId,
                               const std::string& characterId);
    void        leaveSession (const std::string& playerId,
                               const std::string& sessionId);
    bool        setReady     (const std::string& playerId,
                               const std::string& sessionId);

    // Game Actions
    void processMove   (const std::string& sessionId,
                        const std::string& playerId,
                        int dx, int dy);
    void processAttack (const std::string& sessionId,
                        const std::string& playerId,
                        const std::string& targetId);
    void processUseItem(const std::string& sessionId,
                        const std::string& playerId,
                        const std::string& itemId);

    // Game Loop
    void startGameLoop();
    void stopGameLoop();

    // Callbacks (wired to WebSocket server)
    std::function<void(const std::string& sessionId,
                       const std::string& msg)> onBroadcast;
    std::function<void(const std::string& playerId,
                       const std::string& msg)> onSendPlayer;

    // Getters
    GameSession* getSession(const std::string& sessionId);
    std::string  getSessionByRoomCode(const std::string& roomCode);

private:
    DBClient&    _db;
    RedisClient& _redis;

    std::unordered_map<std::string, GameSession> _sessions; // sessionId → session
    std::unordered_map<std::string, std::string> _playerSession; // playerId → sessionId
    std::mutex   _mutex;

    std::thread  _gameLoopThread;
    std::atomic<bool> _running{false};

    // Internal helpers
    void   _tick();                          // called 20x/sec
    void   _startSession(GameSession& sess);
    void   _processEnemyAI(GameSession& sess);
    void   _checkFloorClear(GameSession& sess);
    void   _nextFloor(GameSession& sess);
    void   _spawnEnemies(GameSession& sess);
    void   _generateMap(GameSession& sess);
    bool   _isValidMove(const GameSession& sess, int x, int y);
    int    _calcDamage(const Stats& attacker, const Stats& defender);
    void   _applyDamage(Player& target, int dmg);
    void   _applyDamage(Enemy& target, int dmg);
    void   _dropLoot(GameSession& sess, const Enemy& enemy);
    void   _syncStateToRedis(const GameSession& sess);
    std::string _serializeState(const GameSession& sess);
    std::string _generateRoomCode();
};
