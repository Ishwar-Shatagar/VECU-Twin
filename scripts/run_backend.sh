#!/usr/bin/env bash
set -e

export STANDALONE_MODE=${STANDALONE_MODE:-true}
uvicorn python.api.main:app --reload --host 0.0.0.0 --port 8000
