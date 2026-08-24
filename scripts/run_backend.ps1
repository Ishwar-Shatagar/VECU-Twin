$env:STANDALONE_MODE = if ($env:STANDALONE_MODE) { $env:STANDALONE_MODE } else { "true" }
uvicorn python.api.main:app --reload --host 0.0.0.0 --port 8000
