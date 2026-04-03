#include "game_engine.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <random>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────
GameEngine::GameEngine(DBClient& db, RedisClient& redis)
    : _db(db), _redis(redis) {}

GameEngine::~GameEngine() { stopGameLoop(); }

// ─── Session Management ───────────────────────────────────────
std::string GameEngine::createSession(const std::string& dungeonId,
                                       int maxPlayers) {
    std::lock_guard<std::mutex> lock(_mutex);

    GameSession sess;
    sess.id         = "sess_" + std::to_string(
                        std::chrono::steady_clock::now()
                        .time_since_epoch().count());
    sess.roomCode   = _generateRoomCode();
    sess.dungeonId  = dungeonId;
    sess.maxPlayers = maxPlayers;
    sess.status     = SessionStatus::Waiting;
    sess.createdAt  = std::chrono::steady_clock::now();

    // Persist to DB
    _db.createSession(dungeonId, sess.roomCode, maxPlayers);

    // Cache in Redis
    _redis.setSessionState(sess.id, _serializeState(sess));

    _sessions[sess.id] = std::move(sess);
    return sess.roomCode;
}

bool GameEngine::joinSession(const std::string& roomCode,
                              const std::string& playerId,
                              const std::string& characterId) {
    std::lock_guard<std::mutex> lock(_mutex);

    // Find session by room code
    for (auto& [id, sess] : _sessions) {
        if (sess.roomCode == roomCode) {
            if (sess.status != SessionStatus::Waiting) return false;
            if ((int)sess.players.size() >= sess.maxPlayers) return false;

            // Load character from DB
            auto charOpt = _db.getCharacter(characterId);
            if (!charOpt) return false;

            Player p = *charOpt;
            p.isReady = false;

            sess.players[playerId] = std::move(p);
            _playerSession[playerId] = id;
            _db.addPlayerToSession(id, playerId, characterId);
            _redis.addPlayerToRoom(id, playerId);

            // Broadcast updated lobby state
            if (onBroadcast)
                onBroadcast(id, _serializeState(sess));

            return true;
        }
    }
    return false;
}

void GameEngine::leaveSession(const std::string& playerId,
                               const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) return;

    it->second.players.erase(playerId);
    _playerSession.erase(playerId);
    _redis.removePlayerFromRoom(sessionId, playerId);

    if (onBroadcast)
        onBroadcast(sessionId, _serializeState(it->second));
}

bool GameEngine::setReady(const std::string& playerId,
                           const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) return false;

    auto& sess = it->second;
    if (sess.players.count(playerId))
        sess.players[playerId].isReady = true;

    // Check if all players ready → start game
    bool allReady = !sess.players.empty() &&
        std::all_of(sess.players.begin(), sess.players.end(),
            [](const auto& p){ return p.second.isReady; });

    if (allReady) _startSession(sess);

    if (onBroadcast)
        onBroadcast(sessionId, _serializeState(sess));

    return true;
}

// ─── Game Actions ─────────────────────────────────────────────
void GameEngine::processMove(const std::string& sessionId,
                              const std::string& playerId,
                              int dx, int dy) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) return;

    auto& sess = it->second;
    if (sess.status != SessionStatus::Active) return;

    auto pit = sess.players.find(playerId);
    if (pit == sess.players.end() || !pit->second.isAlive) return;

    int nx = pit->second.pos.x + dx;
    int ny = pit->second.pos.y + dy;

    if (_isValidMove(sess, nx, ny)) {
        pit->second.pos.x = nx;
        pit->second.pos.y = ny;
        _redis.setPlayerPosition(playerId, nx, ny);

        if (onBroadcast) {
            json msg;
            msg["type"]     = "PLAYER_MOVED";
            msg["playerId"] = playerId;
            msg["x"]        = nx;
            msg["y"]        = ny;
            onBroadcast(sessionId, msg.dump());
        }
    }
}

void GameEngine::processAttack(const std::string& sessionId,
                                const std::string& playerId,
                                const std::string& targetId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) return;

    auto& sess   = it->second;
    auto  pit    = sess.players.find(playerId);
    if (pit == sess.players.end() || !pit->second.isAlive) return;

    int dmg = 0;

    // Check if target is enemy
    for (auto& enemy : sess.enemies) {
        if (enemy.id == targetId && enemy.isAlive) {
            dmg = _calcDamage(pit->second.stats, enemy.stats);
            _applyDamage(enemy, dmg);

            if (!enemy.isAlive) {
                pit->second.kills++;
                pit->second.score += 50;
                _dropLoot(sess, enemy);
            }

            json msg;
            msg["type"]     = "ATTACK_RESULT";
            msg["attackerId"] = playerId;
            msg["targetId"]   = targetId;
            msg["damage"]     = dmg;
            msg["targetAlive"]= enemy.isAlive;
            if (onBroadcast) onBroadcast(sessionId, msg.dump());
            break;
        }
    }

    _checkFloorClear(sess);
}

void GameEngine::processUseItem(const std::string& sessionId,
                                 const std::string& playerId,
                                 const std::string& itemId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) return;

    auto& sess = it->second;
    auto  pit  = sess.players.find(playerId);
    if (pit == sess.players.end()) return;

    // Simple potion logic (extend for full inventory)
    if (itemId == "health_potion") {
        auto& p = pit->second;
        int healed = std::min(30, p.stats.maxHealth - p.stats.health);
        p.stats.health += healed;

        json msg;
        msg["type"]     = "ITEM_USED";
        msg["playerId"] = playerId;
        msg["itemId"]   = itemId;
        msg["effect"]   = "heal";
        msg["value"]    = healed;
        if (onBroadcast) onBroadcast(sessionId, msg.dump());
    }
}

// ─── Game Loop ────────────────────────────────────────────────
void GameEngine::startGameLoop() {
    _running = true;
    _gameLoopThread = std::thread([this]() {
        while (_running) {
            _tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 ticks/sec
        }
    });
}

void GameEngine::stopGameLoop() {
    _running = false;
    if (_gameLoopThread.joinable())
        _gameLoopThread.join();
}

// ─── Internal Helpers ────────────────────────────────────────
void GameEngine::_tick() {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& [id, sess] : _sessions) {
        if (sess.status != SessionStatus::Active) continue;
        _processEnemyAI(sess);
        _syncStateToRedis(sess);
    }
}

void GameEngine::_startSession(GameSession& sess) {
    sess.status       = SessionStatus::Active;
    sess.currentFloor = 1;
    _generateMap(sess);
    _spawnEnemies(sess);
    _db.updateSessionStatus(sess.id, "active");

    json msg;
    msg["type"]    = "GAME_STARTED";
    msg["floor"]   = sess.currentFloor;
    msg["map"]     = _serializeState(sess);
    if (onBroadcast) onBroadcast(sess.id, msg.dump());
}

void GameEngine::_processEnemyAI(GameSession& sess) {
    if (sess.players.empty()) return;

    for (auto& enemy : sess.enemies) {
        if (!enemy.isAlive) continue;

        // Find closest player
        Player* closest = nullptr;
        int minDist = INT_MAX;
        for (auto& [pid, player] : sess.players) {
            if (!player.isAlive) continue;
            int dist = abs(enemy.pos.x - player.pos.x) +
                       abs(enemy.pos.y - player.pos.y);
            if (dist < minDist) { minDist = dist; closest = &player; }
        }
        if (!closest) continue;

        float hpRatio = (float)enemy.stats.health / enemy.stats.maxHealth;
        bool shouldFlee = hpRatio < enemy.fleeThreshold &&
                          enemy.aiBehavior == "defensive";

        if (minDist == 1 && !shouldFlee) {
            // Attack player
            int dmg = _calcDamage(enemy.stats, closest->stats);
            _applyDamage(*closest, dmg);

            json msg;
            msg["type"]      = "ENEMY_ATTACKED";
            msg["enemyId"]   = enemy.id;
            msg["playerId"]  = closest->id;
            msg["damage"]    = dmg;
            msg["playerAlive"] = closest->isAlive;
            if (onBroadcast) onBroadcast(sess.id, msg.dump());
        } else {
            // Move toward (or away from) player
            int dx = (shouldFlee ? -1 : 1) *
                     (closest->pos.x > enemy.pos.x ? 1 : -1);
            int dy = (shouldFlee ? -1 : 1) *
                     (closest->pos.y > enemy.pos.y ? 1 : -1);
            int nx = enemy.pos.x + (abs(dx) > abs(dy) ? dx : 0);
            int ny = enemy.pos.y + (abs(dy) >= abs(dx) ? dy : 0);
            if (_isValidMove(sess, nx, ny)) {
                enemy.pos.x = nx;
                enemy.pos.y = ny;
            }
        }
    }
}

void GameEngine::_checkFloorClear(GameSession& sess) {
    bool allDead = std::all_of(sess.enemies.begin(), sess.enemies.end(),
                               [](const Enemy& e){ return !e.isAlive; });
    if (!allDead) return;

    if (sess.currentFloor >= sess.totalFloors) {
        sess.status = SessionStatus::Completed;
        _db.endSession(sess.id);
        json msg; msg["type"] = "VICTORY";
        if (onBroadcast) onBroadcast(sess.id, msg.dump());
    } else {
        _nextFloor(sess);
    }
}

void GameEngine::_nextFloor(GameSession& sess) {
    sess.currentFloor++;
    sess.enemies.clear();
    _generateMap(sess);
    _spawnEnemies(sess);

    json msg;
    msg["type"]  = "FLOOR_CLEAR";
    msg["floor"] = sess.currentFloor;
    if (onBroadcast) onBroadcast(sess.id, msg.dump());
}

void GameEngine::_generateMap(GameSession& sess) {
    int W = 20, H = 20;
    sess.map.width  = W;
    sess.map.height = H;
    sess.map.grid.assign(H, std::vector<int>(W, 1)); // fill walls

    // Simple room carver
    std::mt19937 rng(std::random_device{}());
    auto carve = [&](int x1, int y1, int x2, int y2) {
        for (int y = y1; y <= y2; y++)
            for (int x = x1; x <= x2; x++)
                sess.map.grid[y][x] = 0;
    };
    carve(2, 2, 8, 8);    // room 1
    carve(11, 2, 17, 8);   // room 2
    carve(2, 11, 8, 17);   // room 3
    carve(11, 11, 17, 17); // room 4
    carve(8, 5, 11, 5);    // corridor H top
    carve(5, 8, 5, 11);    // corridor V left
    carve(14, 8, 14, 11);  // corridor V right
    carve(8, 14, 11, 14);  // corridor H bottom

    // Place players at start
    int i = 0;
    for (auto& [pid, player] : sess.players) {
        player.pos = {3 + i, 3};
        i++;
    }
}

void GameEngine::_spawnEnemies(GameSession& sess) {
    auto dbEnemies = _db.getEnemiesForDungeon(sess.dungeonId,
                                               sess.currentFloor);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> px(11, 16), py(11, 16);

    for (auto& e : dbEnemies) {
        e.pos = {px(rng), py(rng)};
        sess.enemies.push_back(e);
    }
}

bool GameEngine::_isValidMove(const GameSession& sess, int x, int y) {
    if (x < 0 || y < 0 ||
        x >= sess.map.width ||
        y >= sess.map.height) return false;
    return sess.map.grid[y][x] == 0;
}

int GameEngine::_calcDamage(const Stats& atk, const Stats& def) {
    int base = atk.strength - def.defense / 2;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> var(-2, 3);
    return std::max(1, base + var(rng));
}

void GameEngine::_applyDamage(Player& p, int dmg) {
    p.stats.health = std::max(0, p.stats.health - dmg);
    if (p.stats.health == 0) p.isAlive = false;
}

void GameEngine::_applyDamage(Enemy& e, int dmg) {
    e.stats.health = std::max(0, e.stats.health - dmg);
    if (e.stats.health == 0) e.isAlive = false;
}

void GameEngine::_dropLoot(GameSession& sess, const Enemy& enemy) {
    // Broadcast loot drop event (client picks up)
    json msg;
    msg["type"]   = "LOOT_DROP";
    msg["x"]      = enemy.pos.x;
    msg["y"]      = enemy.pos.y;
    msg["gold"]   = 10 + (sess.currentFloor * 5);
    msg["item"]   = (enemy.type == "Boss") ? "shadow_blade" : "health_potion";
    if (onBroadcast) onBroadcast(sess.id, msg.dump());
}

void GameEngine::_syncStateToRedis(const GameSession& sess) {
    _redis.setSessionState(sess.id, _serializeState(sess));
}

std::string GameEngine::_serializeState(const GameSession& sess) {
    json j;
    j["sessionId"]    = sess.id;
    j["roomCode"]     = sess.roomCode;
    j["status"]       = (int)sess.status;
    j["floor"]        = sess.currentFloor;
    j["totalFloors"]  = sess.totalFloors;

    json players = json::array();
    for (const auto& [pid, p] : sess.players) {
        json pj;
        pj["id"]      = p.id;
        pj["name"]    = p.characterName;
        pj["health"]  = p.stats.health;
        pj["maxHp"]   = p.stats.maxHealth;
        pj["mana"]    = p.stats.mana;
        pj["x"]       = p.pos.x;
        pj["y"]       = p.pos.y;
        pj["isAlive"] = p.isAlive;
        pj["isReady"] = p.isReady;
        pj["score"]   = p.score;
        players.push_back(pj);
    }
    j["players"] = players;

    json enemies = json::array();
    for (const auto& e : sess.enemies) {
        json ej;
        ej["id"]     = e.id;
        ej["name"]   = e.name;
        ej["type"]   = e.type;
        ej["health"] = e.stats.health;
        ej["maxHp"]  = e.stats.maxHealth;
        ej["x"]      = e.pos.x;
        ej["y"]      = e.pos.y;
        ej["alive"]  = e.isAlive;
        enemies.push_back(ej);
    }
    j["enemies"] = enemies;

    // Map grid
    j["map"] = sess.map.grid;

    return j.dump();
}

std::string GameEngine::_generateRoomCode() {
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    std::string code(6, ' ');
    for (auto& c : code) c = chars[dist(rng)];
    return code;
}

GameSession* GameEngine::getSession(const std::string& sessionId) {
    auto it = _sessions.find(sessionId);
    return (it != _sessions.end()) ? &it->second : nullptr;
}

std::string GameEngine::getSessionByRoomCode(const std::string& roomCode) {
    for (const auto& [id, sess] : _sessions)
        if (sess.roomCode == roomCode) return id;
    return "";
}
