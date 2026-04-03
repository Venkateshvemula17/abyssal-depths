#pragma once
#include "types.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>

// We use uWebSockets for high-performance WebSocket handling
// Include path depends on your installation
#include <uwebsockets/App.h>

// ─── Per-socket user data ────────────────────────────────────
struct SocketData {
    std::string playerId;
    std::string sessionId;
    bool        authenticated = false;
};

// ─── WebSocket Server ────────────────────────────────────────
class WebSocketServer {
public:
    WebSocketServer(int port);

    void start();
    void stop();

    // Broadcast to all players in a session
    void broadcastToSession(const std::string& sessionId,
                            const std::string& message);

    // Send to a single player
    void sendToPlayer(const std::string& playerId,
                      const std::string& message);

    // Callbacks (set by GameEngine)
    std::function<void(const std::string& playerId,
                       const std::string& sessionId,
                       const std::string& msg)> onMessage;

    std::function<void(const std::string& playerId)> onDisconnect;

private:
    int  _port;
    bool _running = false;

    // Map playerId → WebSocket pointer (raw, managed carefully)
    std::mutex _mutex;
    std::unordered_map<std::string,
        uWS::WebSocket<false, true, SocketData>*> _sockets;

    // Map sessionId → set of playerIds
    std::unordered_map<std::string,
        std::vector<std::string>> _sessionPlayers;

    void _handleOpen  (uWS::WebSocket<false, true, SocketData>* ws);
    void _handleMessage(uWS::WebSocket<false, true, SocketData>* ws,
                        std::string_view msg, uWS::OpCode opCode);
    void _handleClose (uWS::WebSocket<false, true, SocketData>* ws,
                        int code, std::string_view msg);

    std::string _verifyJWT(const std::string& token);
};
