#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <optional>
#include <vector>

class RedisClient {
public:
    RedisClient(const std::string& host, int port);
    ~RedisClient();

    // ── Key/Value ────────────────────────────────────────────
    bool        set   (const std::string& key,
                       const std::string& value,
                       int ttlSeconds = -1);
    std::optional<std::string>
                get   (const std::string& key);
    bool        del   (const std::string& key);
    bool        exists(const std::string& key);

    // ── Session State ────────────────────────────────────────
    void setSessionState (const std::string& sessionId,
                          const std::string& stateJson);
    std::optional<std::string>
         getSessionState (const std::string& sessionId);
    void deleteSession   (const std::string& sessionId);

    // ── Player Position ──────────────────────────────────────
    void setPlayerPosition(const std::string& playerId,
                            int x, int y);
    std::pair<int,int>
         getPlayerPosition(const std::string& playerId);

    // ── Room Players List ────────────────────────────────────
    void        addPlayerToRoom   (const std::string& roomId,
                                   const std::string& playerId);
    void        removePlayerFromRoom(const std::string& roomId,
                                     const std::string& playerId);
    std::vector<std::string>
                getPlayersInRoom  (const std::string& roomId);

    // ── Chat Buffer ──────────────────────────────────────────
    void        pushChatMessage(const std::string& roomId,
                                 const std::string& message);
    std::vector<std::string>
                getChatHistory (const std::string& roomId,
                                 int limit = 50);

private:
    redisContext* _ctx;
    std::string   _host;
    int           _port;
    void          _reconnect();
};
