-- ============================================================
-- AI-POWERED MULTIPLAYER DUNGEON CRAWLER
-- DATABASE SCHEMA (PostgreSQL)
-- ============================================================

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- ============================================================
-- PLAYERS TABLE
-- ============================================================
CREATE TABLE players (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username        VARCHAR(50) UNIQUE NOT NULL,
    email           VARCHAR(100) UNIQUE NOT NULL,
    password_hash   TEXT NOT NULL,
    level           INT DEFAULT 1,
    xp              INT DEFAULT 0,
    gold            INT DEFAULT 100,
    created_at      TIMESTAMP DEFAULT NOW(),
    last_login      TIMESTAMP
);

-- ============================================================
-- CHARACTERS TABLE
-- ============================================================
CREATE TABLE characters (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    player_id   UUID REFERENCES players(id) ON DELETE CASCADE,
    name        VARCHAR(50) NOT NULL,
    class       VARCHAR(20) CHECK (class IN ('Warrior', 'Mage', 'Rogue', 'Paladin')),
    health      INT DEFAULT 100,
    max_health  INT DEFAULT 100,
    mana        INT DEFAULT 50,
    max_mana    INT DEFAULT 50,
    strength    INT DEFAULT 10,
    defense     INT DEFAULT 5,
    agility     INT DEFAULT 5,
    inventory   JSONB DEFAULT '[]',
    equipped    JSONB DEFAULT '{}',
    created_at  TIMESTAMP DEFAULT NOW()
);

-- ============================================================
-- DUNGEONS TABLE
-- ============================================================
CREATE TABLE dungeons (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name        VARCHAR(100) NOT NULL,
    theme       VARCHAR(50) CHECK (theme IN ('Forest', 'Volcano', 'Ice', 'Shadow', 'Ancient')),
    difficulty  VARCHAR(20) CHECK (difficulty IN ('Easy', 'Medium', 'Hard', 'Nightmare')),
    floors      INT DEFAULT 5,
    description TEXT,
    created_at  TIMESTAMP DEFAULT NOW()
);

-- ============================================================
-- GAME SESSIONS TABLE
-- ============================================================
CREATE TABLE game_sessions (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    dungeon_id  UUID REFERENCES dungeons(id),
    room_code   VARCHAR(8) UNIQUE NOT NULL,
    status      VARCHAR(20) DEFAULT 'waiting'
                CHECK (status IN ('waiting', 'active', 'completed', 'abandoned')),
    max_players INT DEFAULT 4,
    current_floor INT DEFAULT 1,
    created_at  TIMESTAMP DEFAULT NOW(),
    started_at  TIMESTAMP,
    ended_at    TIMESTAMP
);

-- ============================================================
-- SESSION PLAYERS TABLE (who is in which session)
-- ============================================================
CREATE TABLE session_players (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id      UUID REFERENCES game_sessions(id) ON DELETE CASCADE,
    player_id       UUID REFERENCES players(id) ON DELETE CASCADE,
    character_id    UUID REFERENCES characters(id),
    is_ready        BOOLEAN DEFAULT FALSE,
    score           INT DEFAULT 0,
    kills           INT DEFAULT 0,
    deaths          INT DEFAULT 0,
    joined_at       TIMESTAMP DEFAULT NOW(),
    UNIQUE(session_id, player_id)
);

-- ============================================================
-- ENEMIES TABLE
-- ============================================================
CREATE TABLE enemies (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    dungeon_id      UUID REFERENCES dungeons(id) ON DELETE CASCADE,
    name            VARCHAR(50) NOT NULL,
    type            VARCHAR(20) CHECK (type IN ('Goblin', 'Skeleton', 'Mage', 'Boss', 'Elite')),
    health          INT NOT NULL,
    attack          INT NOT NULL,
    defense         INT DEFAULT 0,
    xp_reward       INT DEFAULT 10,
    gold_reward     INT DEFAULT 5,
    ai_behavior     JSONB DEFAULT '{"style": "aggressive", "flee_threshold": 0.2}',
    sprite_key      VARCHAR(50),
    floor_range     INT[] DEFAULT '{1,5}'
);

-- ============================================================
-- ITEMS TABLE
-- ============================================================
CREATE TABLE items (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name        VARCHAR(100) NOT NULL,
    type        VARCHAR(30) CHECK (type IN ('Weapon', 'Armor', 'Potion', 'Scroll', 'Key')),
    rarity      VARCHAR(20) CHECK (rarity IN ('Common', 'Rare', 'Epic', 'Legendary')),
    stats       JSONB DEFAULT '{}',
    description TEXT,
    icon_key    VARCHAR(50)
);

-- ============================================================
-- CHAT LOGS TABLE (player + AI NPC messages)
-- ============================================================
CREATE TABLE chat_logs (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id  UUID REFERENCES game_sessions(id) ON DELETE CASCADE,
    player_id   UUID REFERENCES players(id) ON DELETE SET NULL,
    message     TEXT NOT NULL,
    is_npc      BOOLEAN DEFAULT FALSE,
    npc_name    VARCHAR(50),
    timestamp   TIMESTAMP DEFAULT NOW()
);

-- ============================================================
-- LEADERBOARD TABLE
-- ============================================================
CREATE TABLE leaderboard (
    id                  UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    player_id           UUID REFERENCES players(id) ON DELETE CASCADE UNIQUE,
    total_score         INT DEFAULT 0,
    dungeons_cleared    INT DEFAULT 0,
    total_kills         INT DEFAULT 0,
    highest_floor       INT DEFAULT 0,
    rank                INT,
    updated_at          TIMESTAMP DEFAULT NOW()
);

-- ============================================================
-- AI NARRATION LOGS TABLE
-- ============================================================
CREATE TABLE narration_logs (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id  UUID REFERENCES game_sessions(id) ON DELETE CASCADE,
    event_type  VARCHAR(50),
    narration   TEXT NOT NULL,
    created_at  TIMESTAMP DEFAULT NOW()
);

-- ============================================================
-- INDEXES FOR PERFORMANCE
-- ============================================================
CREATE INDEX idx_characters_player    ON characters(player_id);
CREATE INDEX idx_session_players_sess ON session_players(session_id);
CREATE INDEX idx_session_players_play ON session_players(player_id);
CREATE INDEX idx_chat_logs_session    ON chat_logs(session_id);
CREATE INDEX idx_chat_logs_timestamp  ON chat_logs(timestamp);
CREATE INDEX idx_leaderboard_score    ON leaderboard(total_score DESC);
CREATE INDEX idx_enemies_dungeon      ON enemies(dungeon_id);
CREATE INDEX idx_sessions_room_code   ON game_sessions(room_code);

-- ============================================================
-- SEED DATA
-- ============================================================

-- Default Dungeons
INSERT INTO dungeons (name, theme, difficulty, floors, description) VALUES
('Whispering Woods',    'Forest',  'Easy',      3, 'A haunted forest filled with restless spirits and wild beasts.'),
('Molten Depths',       'Volcano', 'Medium',    5, 'Deep underground where fire drakes and lava golems roam.'),
('Frostpeak Caverns',   'Ice',     'Hard',      7, 'Frozen tunnels guarded by ice wraiths and frozen undead.'),
('Shadow Realm',        'Shadow',  'Nightmare', 10,'The darkest dungeon — only legends survive here.');

-- Default Items
INSERT INTO items (name, type, rarity, stats, description) VALUES
('Iron Sword',      'Weapon', 'Common',    '{"attack": 5}',          'A basic iron sword.'),
('Health Potion',   'Potion', 'Common',    '{"heal": 30}',           'Restores 30 HP.'),
('Mana Potion',     'Potion', 'Common',    '{"mana": 20}',           'Restores 20 Mana.'),
('Shadow Blade',    'Weapon', 'Rare',      '{"attack": 15, "crit": 10}', 'A blade forged in shadow.'),
('Dragon Armor',    'Armor',  'Epic',      '{"defense": 20}',        'Scales of an ancient dragon.'),
('Dungeon Key',     'Key',    'Common',    '{}',                     'Opens locked dungeon doors.'),
('Fireball Scroll', 'Scroll', 'Rare',      '{"damage": 40, "aoe": true}', 'Unleashes a burst of fire.');

-- Default Enemies
INSERT INTO enemies (dungeon_id, name, type, health, attack, defense, xp_reward, gold_reward, ai_behavior, floor_range)
SELECT d.id, 'Forest Goblin', 'Goblin', 30, 8, 2, 15, 8,
       '{"style": "aggressive", "flee_threshold": 0.15}'::jsonb, '{1,2}'
FROM dungeons d WHERE d.theme = 'Forest';

INSERT INTO enemies (dungeon_id, name, type, health, attack, defense, xp_reward, gold_reward, ai_behavior, floor_range)
SELECT d.id, 'Wood Skeleton', 'Skeleton', 40, 10, 3, 20, 10,
       '{"style": "defensive", "flee_threshold": 0.1}'::jsonb, '{2,3}'
FROM dungeons d WHERE d.theme = 'Forest';

INSERT INTO enemies (dungeon_id, name, type, health, attack, defense, xp_reward, gold_reward, ai_behavior, floor_range)
SELECT d.id, 'Ancient Guardian', 'Boss', 200, 30, 15, 150, 100,
       '{"style": "adaptive", "flee_threshold": 0.0, "phases": 3}'::jsonb, '{3,3}'
FROM dungeons d WHERE d.theme = 'Forest';

-- ============================================================
-- VIEWS
-- ============================================================

-- Top leaderboard view
CREATE OR REPLACE VIEW top_players AS
SELECT
    p.username,
    l.total_score,
    l.dungeons_cleared,
    l.total_kills,
    l.highest_floor,
    RANK() OVER (ORDER BY l.total_score DESC) AS rank
FROM leaderboard l
JOIN players p ON p.id = l.player_id
ORDER BY l.total_score DESC
LIMIT 50;

-- Active sessions view
CREATE OR REPLACE VIEW active_sessions AS
SELECT
    gs.id,
    gs.room_code,
    d.name AS dungeon_name,
    d.difficulty,
    gs.current_floor,
    COUNT(sp.player_id) AS player_count,
    gs.max_players,
    gs.created_at
FROM game_sessions gs
JOIN dungeons d ON d.id = gs.dungeon_id
LEFT JOIN session_players sp ON sp.session_id = gs.id
WHERE gs.status = 'active'
GROUP BY gs.id, d.name, d.difficulty;
