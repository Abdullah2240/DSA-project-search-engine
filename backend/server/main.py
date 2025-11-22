from fastapi import FastAPI
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware
import engine

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)
# for now we will integrate the lexicon working in the main work flow
@app.get("/search")
def do_search(q: str):
    results = engine.search(q)
    return {"results": results}

@app.get("/lexicon/stats")
def get_lexicon_stats():
    """Showcase lexicon feature - get statistics about the lexicon"""
    stats = engine.get_lexicon_stats()
    return {"stats": stats, "message": "Lexicon is working!"}

@app.get("/")
def root():
    return {
        "message": "Search Engine API",
        "endpoints": {
            "/search?q=query": "Search using lexicon",
            "/lexicon/stats": "Get lexicon statistics and showcase"
        }
    }