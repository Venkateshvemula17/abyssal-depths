#include "websocket_server.h"
#include "rest_api.h"
#include "game_engine.h"
#include "db_client.h"
#include "redis_client.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>

using json = nlohmann::json;

// ─── Global shutdown flag ────────────────────────────────────
std::atomic<bool> g_running{true};
void signalHandler(int) { g_running = false; }

// ─────────────────────────────────────────────────────────────
int main() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Config from environment ──────────────────────────────
    const char* pgConn    = std::getenv("DATABASE_URL");
    const char* redisHost = std::getenv("REDIS_HOST");
    const char* wsPortStr = std::getenv("WS_PORT");
    const char* apiPortStr= std::getenv("API_PORT");

    std::string dbConn   = pgConn    ? pgConn    : "postgresql://dungeon:secret@localhost/dungeon_db";
    std::string rHost    = redisHost ? redisHost  : "localhost";
    int wsPort           = wsPortStr  ? std::stoi(wsPortStr)  : 9001;
    int apiPort          = apiPortStr ? std::stoi(apiPortStr) : 8080;

    std::cout << "🏰 Dungeon Crawler Server starting...\n";

    // ── Init DB & Redis ──────────────────────────────────────
    DBClient    db(dbConn);
    RedisClient redis(rHost, 6379);
    std::cout << "✅ Database connected\n";
    std::cout << "✅ Redis connected\n";

    // ── Init Game Engine ─────────────────────────────────────
    GameEngine engine(db, redis);
    engine.startGameLoop();
    std::cout << "✅ Game engine running\n";

    // ── Init REST API ─────────────────────────────────────────
    RestAPI api(db, engine, apiPort);
    std::thread apiThread([&api]() { api.start(); });
    std::cout << "✅ REST API on port " << apiPort << "\n";

    // ── Init WebSocket Server ────────────────────────────────
    WebSocketServer wsServer(wsPort);

    // Wire WebSocket → GameEngine
    wsServer.onMessage = [&](const std::string& playerId,
                              const std::string& sessionId,
                              const std::string& msgStr) {
        try {
            auto msg = json::parse(msgStr);
            std::string type = msg["type"];

            if (type == "JOIN_ROOM") {
                engine.joinSession(msg["roomCode"],
                                   playerId,
                                   msg["characterId"]);
            }
            else if (type == "READY") {
                engine.setReady(playerId, sessionId);
            }
            else if (type == "MOVE") {
                engine.processMove(sessionId, playerId,
                                   msg["dx"], msg["dy"]);
            }
            else if (type == "ATTACK") {
                engine.processAttack(sessionId, playerId,
                                     msg["targetId"]);
            }
            else if (type == "USE_ITEM") {
                engine.processUseItem(sessionId, playerId,
                                      msg["itemId"]);
            }
            else if (type == "CHAT") {
                db.saveChatMessage(sessionId, playerId,
                                   msg["message"]);
                // Broadcast chat
                json broadcast;
                broadcast["type"]    = "CHAT";
                broadcast["from"]    = playerId;
                broadcast["message"] = msg["message"];
                wsServer.broadcastToSession(sessionId,
                                            broadcast.dump());
            }
        } catch (const std::exception& e) {
            std::cerr << "Message parse error: " << e.what() << "\n";
        }
    };

    wsServer.onDisconnect = [&](const std::string& playerId) {
        // Find and leave session
        auto sessId = engine.getSessionByRoomCode(playerId);
        if (!sessId.empty())
            engine.leaveSession(playerId, sessId);
        std::cout << "Player disconnected: " << playerId << "\n";
    };

    // Wire GameEngine → WebSocket (for broadcasting)
    engine.onBroadcast = [&](const std::string& sessionId,
                              const std::string& msg) {
        wsServer.broadcastToSession(sessionId, msg);
    };
    engine.onSendPlayer = [&](const std::string& playerId,
                               const std::string& msg) {
        wsServer.sendToPlayer(playerId, msg);
    };

    // Start WebSocket server (blocking)
    std::thread wsThread([&wsServer]() { wsServer.start(); });
    std::cout << "✅ WebSocket server on port " << wsPort << "\n";
    std::cout << "🚀 Server is READY!\n\n";

    // ── Main loop ────────────────────────────────────────────
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n🛑 Shutting down...\n";
    engine.stopGameLoop();
    wsServer.stop();
    api.stop();

    if (wsThread.joinable())  wsThread.join();
    if (apiThread.joinable()) apiThread.join();

    std::cout << "✅ Server stopped cleanly.\n";
    return 0;
}
