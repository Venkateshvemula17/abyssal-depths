# 🏰 Dungeon Crawler — C++ Backend

## Dependencies
```bash
# Ubuntu / Debian
sudo apt install libpqxx-dev libhiredis-dev libssl-dev zlib1g-dev

# uWebSockets (manual build)
git clone https://github.com/uNetworking/uWebSockets
cd uWebSockets && make && sudo make install

# nlohmann/json
sudo apt install nlohmann-json3-dev

# cpp-httplib (header-only)
wget https://github.com/yhirose/cpp-httplib/releases/latest/download/httplib.h \
     -O /usr/local/include/httplib.h
```

## Build
```bash
cd backend-cpp
mkdir build && cd build
cmake ..
make -j4
```

## Run
```bash
export DATABASE_URL="postgresql://dungeon:secret@localhost/dungeon_db"
export REDIS_HOST="localhost"
export WS_PORT="9001"
export API_PORT="8080"

./dungeon_server
```

## API Endpoints
| Method | Route | Description |
|--------|-------|-------------|
| POST | /register | Register new player |
| POST | /login | Login + get JWT |
| GET | /dungeons | List all dungeons |
| POST | /session/create | Create game session |
| GET | /leaderboard | Top 50 players |
| GET | /character/:id | Get character info |
| POST | /character/create | Create character |

## WebSocket Messages
| Type | Direction | Description |
|------|-----------|-------------|
| JOIN_ROOM | Client→Server | Join a room by code |
| READY | Client→Server | Set player ready |
| MOVE | Client→Server | Move player {dx,dy} |
| ATTACK | Client→Server | Attack target {targetId} |
| USE_ITEM | Client→Server | Use item {itemId} |
| CHAT | Client→Server | Chat message |
| GAME_STATE | Server→Client | Full game state broadcast |
| PLAYER_MOVED | Server→Client | Player position update |
| ENEMY_ATTACKED | Server→Client | Enemy attacked player |
| FLOOR_CLEAR | Server→Client | All enemies dead |
| VICTORY | Server→Client | Dungeon cleared |
