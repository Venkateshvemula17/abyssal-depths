from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import List, Optional
from llm_client import ask_claude
import json

router = APIRouter()

# ─── Models ──────────────────────────────────────────────────
class NarrateRequest(BaseModel):
    event:        str          # room_entered | boss_killed | player_died |
                               # floor_cleared | item_found | dungeon_start | victory
    dungeon_name: str
    floor:        int
    players:      List[str]    # player names
    context:      Optional[str] = ""   # extra info

class NarrateResponse(BaseModel):
    narration:  str    # shown to ALL players as atmospheric overlay
    mood:       str    # "tense" | "epic" | "tragic" | "mysterious" | "triumphant"
    duration_ms: int   # how long to show it (ms)

# ─── Event Templates ─────────────────────────────────────────
EVENT_MOODS = {
    "room_entered":   ("mysterious", 3000),
    "boss_killed":    ("epic",       5000),
    "player_died":    ("tragic",     4000),
    "floor_cleared":  ("triumphant", 3500),
    "item_found":     ("mysterious", 2500),
    "dungeon_start":  ("tense",      4000),
    "victory":        ("triumphant", 6000),
    "all_players_dead": ("tragic",   5000),
}

@router.post("/narrate", response_model=NarrateResponse)
async def narrate_event(req: NarrateRequest):
    """
    Generate atmospheric dungeon narration for key game events.
    Displayed as cinematic overlay text to all players.
    """
    mood, duration = EVENT_MOODS.get(req.event, ("mysterious", 3000))
    players_str    = ", ".join(req.players) if req.players else "the adventurers"

    system = """You are the omniscient narrator of a dark fantasy dungeon crawler.
Your voice is dramatic, atmospheric, and immersive — like a gothic novel.
Keep narration to 1-2 sentences maximum.
Never use generic phrases like "suddenly" or "out of nowhere".
Respond ONLY in JSON:
{
  "narration": "<atmospheric narration text>",
  "mood": "<tense|epic|tragic|mysterious|triumphant>"
}"""

    event_prompts = {
        "room_entered":    f"{players_str} step into a new chamber of {req.dungeon_name}, floor {req.floor}. {req.context}",
        "boss_killed":     f"{players_str} have slain the boss on floor {req.floor} of {req.dungeon_name}. {req.context}",
        "player_died":     f"A member of the party has fallen in {req.dungeon_name}. {req.context}",
        "floor_cleared":   f"{players_str} cleared floor {req.floor} of {req.dungeon_name}.",
        "item_found":      f"{players_str} discovered something in {req.dungeon_name}. {req.context}",
        "dungeon_start":   f"{players_str} descend into {req.dungeon_name} for the first time.",
        "victory":         f"{players_str} have conquered {req.dungeon_name} in its entirety!",
        "all_players_dead":f"All adventurers have perished in {req.dungeon_name}, floor {req.floor}.",
    }

    user = event_prompts.get(req.event,
        f"Event: {req.event} in {req.dungeon_name}. {req.context}")

    try:
        raw  = await ask_claude(system, user, max_tokens=150, temperature=0.95)
        raw  = raw.replace("```json", "").replace("```", "").strip()
        data = json.loads(raw)

        return NarrateResponse(
            narration   = data.get("narration", "The dungeon holds its breath..."),
            mood        = data.get("mood", mood),
            duration_ms = duration,
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")


# ─── Story Intro Generator ───────────────────────────────────
class StoryIntroRequest(BaseModel):
    dungeon_name:  str
    dungeon_theme: str    # Forest | Volcano | Ice | Shadow | Ancient
    difficulty:    str
    player_names:  List[str]

@router.post("/narrate/intro")
async def generate_story_intro(req: StoryIntroRequest):
    """
    Generate a unique story intro paragraph when players start a new dungeon.
    """
    system = """You are writing the opening narration for a dark fantasy dungeon run.
It should feel like the opening of a gothic novel — atmospheric, foreboding, unique.
2-3 sentences. Mention the dungeon theme and hint at dangers ahead.
Respond ONLY in JSON: {"intro": "<text>"}"""

    user = f"""Dungeon: {req.dungeon_name}
Theme: {req.dungeon_theme}
Difficulty: {req.difficulty}
Party: {', '.join(req.player_names)}

Write the story intro."""

    try:
        raw  = await ask_claude(system, user, max_tokens=200, temperature=1.0)
        raw  = raw.replace("```json", "").replace("```", "").strip()
        return json.loads(raw)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")
