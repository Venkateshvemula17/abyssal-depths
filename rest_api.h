#pragma once
#include "db_client.h"
#include "game_engine.h"
#include <httplib.h>
#include <string>

class RestAPI {
public:
    RestAPI(DBClient& db, GameEngine& engine, int port);
    void start();
    void stop();

private:
    DBClient&    _db;
    GameEngine&  _engine;
    int          _port;
    httplib::Server _server;

    // ── Auth Routes ──────────────────────────────────────────
    void _registerRoutes();
    void _handleRegister(const httplib::Request& req,
                          httplib::Response& res);
    void _handleLogin   (const httplib::Request& req,
                          httplib::Response& res);

    // ── Game Routes ──────────────────────────────────────────
    void _handleCreateSession (const httplib::Request& req,
                                httplib::Response& res);
    void _handleGetDungeons   (const httplib::Request& req,
                                httplib::Response& res);
    void _handleLeaderboard   (const httplib::Request& req,
                                httplib::Response& res);
    void _handleGetCharacter  (const httplib::Request& req,
                                httplib::Response& res);
    void _handleCreateCharacter(const httplib::Request& req,
                                 httplib::Response& res);
    void _handleActiveSessions(const httplib::Request& req,
                                httplib::Response& res);

    // ── Middleware ───────────────────────────────────────────
    std::string _verifyJWT    (const std::string& token);
    bool        _requireAuth  (const httplib::Request& req,
                                httplib::Response& res,
                                std::string& playerId);
    void        _setCORSHeaders(httplib::Response& res);

    // ── Helpers ───────────────────────────────────────────────
    std::string _generateJWT  (const std::string& playerId);
    std::string _hashPassword (const std::string& password);
    std::string _jsonError    (const std::string& msg);
    std::string _jsonOk       (const std::string& data);
};
