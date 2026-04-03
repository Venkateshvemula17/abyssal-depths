from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
import uvicorn

from routes.npc      import router as npc_router
from routes.enemy    import router as enemy_router
from routes.narrator import router as narrator_router
from routes.hint     import router as hint_router

# ─── App Lifecycle ───────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    print("🤖 AI Service starting...")
    yield
    print("🛑 AI Service shutting down...")

# ─── App Init ────────────────────────────────────────────────
app = FastAPI(
    title="Dungeon Crawler AI Service",
    description="LLM-powered NPCs, enemies, narration & hints",
    version="1.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ─── Routers ─────────────────────────────────────────────────
app.include_router(npc_router,      prefix="/ai", tags=["NPC"])
app.include_router(enemy_router,    prefix="/ai", tags=["Enemy"])
app.include_router(narrator_router, prefix="/ai", tags=["Narrator"])
app.include_router(hint_router,     prefix="/ai", tags=["Hint"])

@app.get("/health")
async def health():
    return {"status": "ok", "service": "dungeon-ai"}

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
