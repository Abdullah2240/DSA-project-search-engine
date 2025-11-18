from fastapi import FastAPI
from pydantic import BaseModel
import engine

app =  FastAPI()

@app.get("/search")
def do_search(q: str):
    return {"results": engine.search(q)}