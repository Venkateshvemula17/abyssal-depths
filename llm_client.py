import anthropic
import os
from typing import Optional

# ─── Anthropic Client ────────────────────────────────────────
_client: Optional[anthropic.Anthropic] = None

def get_client() -> anthropic.Anthropic:
    global _client
    if _client is None:
        api_key = os.getenv("ANTHROPIC_API_KEY")
        if not api_key:
            raise RuntimeError("ANTHROPIC_API_KEY not set in environment")
        _client = anthropic.Anthropic(api_key=api_key)
    return _client


async def ask_claude(
    system_prompt: str,
    user_prompt:   str,
    max_tokens:    int = 300,
    temperature:   float = 0.9
) -> str:
    """
    Core helper — sends a prompt to Claude and returns the text response.
    All AI routes go through this function.
    """
    client = get_client()

    message = client.messages.create(
        model      = "claude-sonnet-4-20250514",
        max_tokens = max_tokens,
        system     = system_prompt,
        messages   = [{"role": "user", "content": user_prompt}]
    )

    # Extract text from response
    for block in message.content:
        if block.type == "text":
            return block.text.strip()

    return ""
