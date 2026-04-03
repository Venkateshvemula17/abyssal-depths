from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import List, Optional
from llm_client import ask_claude
import json

router = APIRouter()

# ─── Models ──────────────────────────────────────────────────
class HintRequest(BaseModel):
    player_id:       str
    player_name:     str
    stuck_seconds:   int          # how long player hasn't progressed
    floor:           int
    dungeon_name:    str
    alive_enemies:   int
    inventory:       List[str]    # item names player has
    player_hp_pct:   float        # 0.0 - 1.0
    context:         Optional[str] = ""

class HintResponse(BaseModel):
    hint:       str     # delivered as a "mysterious voice" in chat
    hint_type:  str     # "combat" | "navigation" | "item" | "strategy"
    urgency:    str     # "low" | "medium" | "high"

@router.post("/hint", response_model=HintResponse)
async def generate_hint(req: HintRequest):
    """
    Generate a subtle, in-world hint for a stuck player.
    Delivered as a cryptic whisper from a "mysterious voice."
    """
    inventory_str = ", ".join(req.inventory) if req.inventory else "nothing useful"
    urgency = "high" if req.player_hp_pct < 0.3 else \
              "medium" if req.stuck_seconds > 60 else "low"

    system = """You are a mysterious oracle whispering hints to struggling adventurers in a dungeon.
Your hints must:
- Sound like cryptic in-world whispers, NOT game tips
- Never break the 4th wall (no "press X" or UI references)
- Be subtle — guide without spoiling
- Be max 1-2 sentences

Respond ONLY in JSON:
{
  "hint": "<cryptic in-world hint>",
  "hint_type": "<combat|navigation|item|strategy>"
}"""

    user = f"""Player: {req.player_name}
Stuck for: {req.stuck_seconds} seconds
Floor: {req.floor} of {req.dungeon_name}
Enemies remaining: {req.alive_enemies}
Inventory: {inventory_str}
HP: {round(req.player_hp_pct * 100)}%
Context: {req.context}

Generate a whispered hint for this player."""

    try:
        raw  = await ask_claude(system, user, max_tokens=120, temperature=0.9)
        raw  = raw.replace("```json", "").replace("```", "").strip()
        data = json.loads(raw)

        return HintResponse(
            hint      = data.get("hint", "The shadows hold the answer..."),
            hint_type = data.get("hint_type", "strategy"),
            urgency   = urgency,
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")


# ─── Lore Hint (world-building flavor) ───────────────────────
class LoreRequest(BaseModel):
    dungeon_name:  str
    dungeon_theme: str
    floor:         int
    trigger:       str   # e.g. "ancient_inscription", "strange_statue", "old_chest"

@router.post("/hint/lore")
async def generate_lore_hint(req: LoreRequest):
    """
    Generate world-building lore text for interactive objects.
    Shown when players interact with dungeon features.
    """
    system = """You write lore inscriptions and flavor text for a dark fantasy dungeon.
Text should feel ancient, weathered, and intriguing.
1-2 sentences only. Make it feel like part of the world's history.
Respond ONLY in JSON: {"lore": "<lore text>"}"""

    user = f"""Dungeon: {req.dungeon_name} ({req.dungeon_theme} theme)
Floor: {req.floor}
Object: {req.trigger}

Write the lore text for this object."""

    try:
        raw  = await ask_claude(system, user, max_tokens=120, temperature=1.0)
        raw  = raw.replace("```json", "").replace("```", "").strip()
        return json.loads(raw)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")
