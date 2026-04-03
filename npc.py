from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from llm_client import ask_claude

router = APIRouter()

# ─── Request / Response Models ───────────────────────────────
class NPCDialogueRequest(BaseModel):
    npc_name:      str           # e.g. "Grimbold the Blacksmith"
    personality:   str           # e.g. "gruff, battle-hardened, secretly kind"
    player_action: str           # e.g. "asked about the dungeon entrance"
    context:       str           # e.g. "player is level 3, dungeon floor 1"
    player_name:   str = "Adventurer"

class NPCDialogueResponse(BaseModel):
    npc_name:   str
    dialogue:   str
    emotion:    str   # e.g. "neutral", "angry", "friendly"
    offer:      str   # e.g. "buy_items", "give_quest", "none"

# ─── NPC Personalities ───────────────────────────────────────
NPC_PERSONAS = {
    "blacksmith": "a gruff, battle-hardened dwarven blacksmith who has seen countless adventurers die. Secretly kind but hides it with gruffness. Speaks in short sentences.",
    "dungeon_keeper": "an ancient, mysterious keeper of the dungeon. Knows all its secrets. Speaks in cryptic riddles and old-world prose.",
    "mysterious_stranger": "a hooded figure of unknown origin. Speaks very little. Every word drips with hidden meaning. Never reveals their true purpose.",
    "healer": "a warm, motherly elf healer. Worried about adventurers' safety. Gives advice freely but mourns those who don't listen.",
    "merchant": "a cheerful, fast-talking halfling merchant. Always trying to make a deal. Exaggerates wildly about the quality of their wares.",
}

# ─── Route ───────────────────────────────────────────────────
@router.post("/npc-dialogue", response_model=NPCDialogueResponse)
async def generate_npc_dialogue(req: NPCDialogueRequest):
    """
    Generate dynamic, context-aware NPC dialogue using Claude.
    """
    persona = NPC_PERSONAS.get(
        req.npc_name.lower().replace(" ", "_"),
        req.personality
    )

    system = """You are a game master generating NPC dialogue for a dark fantasy dungeon crawler.
Your responses must be:
- In character at all times
- 1-3 sentences maximum
- Atmospheric and immersive
- Reflect the NPC's personality perfectly

Respond in JSON format ONLY:
{
  "dialogue": "<what the NPC says>",
  "emotion": "<neutral|friendly|angry|fearful|mysterious|sad>",
  "offer": "<buy_items|sell_items|give_quest|give_hint|none>"
}"""

    user = f"""NPC: {req.npc_name}
Personality: {persona}
Player name: {req.player_name}
Player action: {req.player_action}
Context: {req.context}

Generate dialogue for this NPC responding to the player's action."""

    try:
        import json
        raw = await ask_claude(system, user, max_tokens=200)
        # Strip markdown fences if present
        raw = raw.replace("```json", "").replace("```", "").strip()
        data = json.loads(raw)
        return NPCDialogueResponse(
            npc_name = req.npc_name,
            dialogue = data.get("dialogue", "..."),
            emotion  = data.get("emotion",  "neutral"),
            offer    = data.get("offer",    "none"),
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")


# ─── Batch Dialogue (for cutscenes) ──────────────────────────
class BatchDialogueRequest(BaseModel):
    scene:   str   # description of the scene
    npcs:    list  # list of {name, personality}
    trigger: str   # what triggered this scene

@router.post("/npc-dialogue/batch")
async def generate_batch_dialogue(req: BatchDialogueRequest):
    """
    Generate a short back-and-forth dialogue between multiple NPCs.
    Used for cutscenes and story moments.
    """
    npc_list = "\n".join([f"- {n['name']}: {n['personality']}" for n in req.npcs])

    system = """You are a game master writing a short cutscene dialogue for a dark fantasy dungeon crawler.
Write natural, atmospheric conversation between NPCs.
Respond ONLY in JSON:
{
  "lines": [
    {"npc": "<name>", "text": "<dialogue line>"},
    ...
  ]
}
Maximum 6 lines total."""

    user = f"""Scene: {req.scene}
Trigger: {req.trigger}
NPCs present:
{npc_list}

Write their conversation."""

    try:
        import json
        raw = await ask_claude(system, user, max_tokens=400)
        raw = raw.replace("```json", "").replace("```", "").strip()
        data = json.loads(raw)
        return data
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI error: {str(e)}")
