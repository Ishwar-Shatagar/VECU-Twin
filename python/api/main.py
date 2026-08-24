"""
VECU-Twin FastAPI Application Entry Point.

Start with:
    uvicorn python.api.main:app --reload --port 8000

The API will automatically use STANDALONE_MODE if the C++ simulator
is not running (controlled by STANDALONE_MODE env variable in .env).
"""

import os
import time
from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

# Load .env file if present
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass  # python-dotenv optional; use system env vars

from contextlib import asynccontextmanager

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    init_db()
    standalone = os.getenv("STANDALONE_MODE", "true").lower() == "true"
    if standalone:
        from python.simulation.simulator import get_simulator
        sim = get_simulator()
        sim.set_scenario("NORMAL_DRIVE")
    yield
    # Shutdown
    if standalone:
        from python.simulation.simulator import _simulator
        if _simulator:
            _simulator.stop()

from python.api.routes import router
from python.storage.database import init_db

# ─── App creation ─────────────────────────────────────────────────────────────
app = FastAPI(
    title="VECU-Twin API",
    description="""
## Virtual ECU & Digital Twin Vehicle Simulator API

This API exposes real-time vehicle state, ECU status, CAN bus data,
Digital Twin synchronization status, and fault injection control.
    """,
    version="1.0.0",
    lifespan=lifespan,
    contact={"name": "VECU-Twin Project"},
    license_info={"name": "MIT"},
)

# ─── CORS ─────────────────────────────────────────────────────────────────────
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://localhost:3000", "http://localhost:8080", "*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(router, prefix="/api")


# ─── Root redirect ────────────────────────────────────────────────────────────
@app.get("/", include_in_schema=False)
def root():
    return {
        "name":    "VECU-Twin API",
        "version": "1.0.0",
        "docs":    "/docs",
        "health":  "/api/health",
    }
