#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>

// ─── Forward declarations ───────────────────────────────────
struct Player;
struct Character;
struct GameSession;
struct Enemy;

// ─── Enums ──────────────────────────────────────────────────
enum class CharacterClass { Warrior, Mage, Rogue, Paladin };
enum class SessionStatus  { Waiting, Active, Completed, Abandoned };
enum class MessageType {
    JOIN_ROOM, LEAVE_ROOM, MOVE, ATTACK,
    USE_ITEM, CHAT, READY, GAME_STATE,
    ENEMY_ACTION, PLAYER_DIED, FLOOR_CLEAR,
    GAME_OVER, VICTORY, ERROR
};

// ─── Structs ─────────────────────────────────────────────────
struct Position { int x = 0, y = 0; };

struct Stats {
    int health, maxHealth;
    int mana,   maxMana;
    int strength, defense, agility;
};

struct Player {
    std::string id;
    std::string username;
    std::string characterId;
    std::string characterName;
    CharacterClass charClass;
    Stats stats;
    Position pos;
    bool isReady   = false;
    bool isAlive   = true;
    int  score     = 0;
    int  kills     = 0;
};

struct Enemy {
    std::string id;
    std::string name;
    std::string type;
    Stats stats;
    Position pos;
    bool isAlive   = true;
    std::string aiBehavior;  // "aggressive" | "defensive" | "adaptive"
    float fleeThreshold = 0.2f;
};

struct TileMap {
    int width  = 20;
    int height = 20;
    std::vector<std::vector<int>> grid; // 0=floor, 1=wall, 2=door, 3=chest
};

struct GameSession {
    std::string id;
    std::string roomCode;
    std::string dungeonId;
    std::string dungeonName;
    std::string difficulty;
    SessionStatus status = SessionStatus::Waiting;
    int maxPlayers    = 4;
    int currentFloor  = 1;
    int totalFloors   = 5;
    TileMap map;
    std::unordered_map<std::string, Player>  players;
    std::vector<Enemy>                        enemies;
    std::chrono::steady_clock::time_point     createdAt;
};

// ─── Message structure ───────────────────────────────────────
struct GameMessage {
    MessageType type;
    std::string sessionId;
    std::string playerId;
    std::string payload;  // JSON string
};
