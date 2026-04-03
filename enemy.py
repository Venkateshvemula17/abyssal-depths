from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import List, Optional
from llm_client import ask_claude
import json

router = APIRouter()

# ─── Models ──────────────────────────────────────────────────
class PlayerInfo(BaseModel):
    id:     str
    name:   str
    health: int
    maxHp:  int
    x:      int
    y:      int
    class_: str = "Warrior"

class EnemyActionRequest(BaseModel):
    enemy_id:      str
    enemy_name:    str
    enemy_type:    str        # Goblin | Skeleton | Mage | Boss | Elite
    enemy_health:  int
    enemy_max_hp:  int
    enemy_x:       int
    enemy_y:       int
    floor:         int
    players:       List[PlayerInfo]
    phase:         int = 1    # Boss phase (1-3)

class EnemyActionResponse(BaseModel):
    enemy_id:    str
    action:      str          # attack | move | taunt | flee | special
    target_id:   Optional[str]
    move_dx:     int = 0
    move_dy:     int = 0
    dialogue:    str          # what the enemy "says" (shown in chat)
    ability:     Optional[str] = None  # special ability name if used

# ─── Enemy Behavior Profiles ─────────────────────────────────
ENEMY_PROFILES = {
    "Goblin":   "aggressive and cowardly. Attacks the weakest player. Flees when below 20% HP. Taunts loudly.",
    "Skeleton": "methodical undead. Always attacks the nearest player. Never flees. Silent except for bone rattling.",
    "Mage":     "strategic and cunning. Stays at distance. Targets the player with highest attack. Uses spells.",
    "Elite":    "disciplined veteran. Focuses one player until dead. Uses abilities when at 50% HP.",
    "Boss":     "terrifying apex predator. Adapts strategy each phase. Taunts players dramatically. Never flees.",
}

@router.post("/enemy-action", response_model=EnemyActionResponse)
async def get_enemy_action(req: EnemyActionRequest):
    """
    Claude decides what the enemy does this turn based on game state.
    Returns action, target, movement, and dialogue.
    """
    profile = ENEMY_PROFILES.get(req.enemy_type, "aggressive fighter")
    hp_pct  = round((req.enemy_health / req.enemy_max_hp) * 100)

    players_desc = "\n".join([
        f"  - {p.name} ({p.class_}): HP {p.health}/{p.maxHp}, pos ({p.x},{p.y})"
        for p in req.players if p.health > 0
    ])

    system = """You are an AI game master controlling an enemy in a dungeon crawler.
Decide the enemy's action this turn based on the situation.
Respond ONLY in JSON:
{
  "action": "<attack|move|taunt|flee|special>",
  "target_id": "<player id or null>",
  "move_dx": <-1, 0, or 1>,
  "move_dy": <-1, 0, or 1>,
  "dialogue": "<short battle cry or taunt, max 10 words>",
  "ability": "<ability name or null>"
}"""

    user = f"""Enemy: {req.enemy_name} ({req.enemy_type})
Behavior: {profile}
HP: {req.enemy_health}/{req.enemy_max_hp} ({hp_pct}%)
Position: ({req.enemy_x}, {req.enemy_y})
Floor: {req.floor}
Boss Phase: {req.phase}

Players alive:
{players_desc}

What does {req.enemy_name} do this turn?"""

    try:
        raw  = await ask_claude(system, user, max_tokens=150, temperature=0.8)
        raw  = raw.replace("```json", "").replace("```", "").strip()
        data = json.loads(raw)

        return EnemyActionResponse(
            enemy_id  = req.enemy_id,
            action    = data.get("action", "move"),
            target_id = data.get("target_id"),
            move_dx   = int(data.get("move_dx", 0)),
            move_dy   = int(data.get("move_dy", 0)),
            dialogue  = data.get("dialogue", "..."),
            ability   = data.get("ability"),
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")


# ─── Boss Phase Transition ────────────────────────────────────
class BossPhaseRequest(BaseModel):
    boss_name:   str
    phase:       int
    floor:       int
    players:     List[PlayerInfo]

@router.post("/enemy-action/boss-phase")
async def boss_phase_transition(req: BossPhaseRequest):
    """
    Generate a dramatic boss phase transition speech + new behavior.
    Called when boss crosses 66% and 33% HP thresholds.
    """
    system = """You are writing a dramatic boss phase transition for a dungeon crawler.
The boss has reached a new phase and transforms, becoming more dangerous.
Respond ONLY in JSON:
{
  "speech": "<dramatic boss speech, 2-3 sentences>",
  "new_ability": "<name of new ability unlocked>",
  "ability_desc": "<short description of the ability>",
  "behavior_change": "<how the boss now fights differently>"
}"""

    user = f"""Boss: {req.boss_name}
Entering Phase: {req.phase}
Floor: {req.floor}
Players remaining: {len([p for p in req.players if p.health > 0])}

Generate the phase {req.phase} transformation."""

    try:
        raw  = await ask_claude(system, user, max_tokens=250, temperature=1.0)
        raw  = raw.replace("```json", "").replace("```", "").strip()
        return json.loads(raw)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")
