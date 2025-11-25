import json
import re

LEXICON_PATH = "../data/processed/lexicon.json"   # adjust this if needed

# -----------------------------
# Load lexicon
# -----------------------------
def load_lexicon(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data["word_to_index"], data["index_to_word"]


# -----------------------------
# Tokenize SAME AS C++ CODE
# lowercase + remove non-alnum
# -----------------------------
def tokenize(text):
    clean = ""
    for c in text:
        if c.isalnum() or c.isspace():
            clean += c.lower()
        else:
            clean += " "
    return clean.split()


# -----------------------------
# Search just the lexicon
# -----------------------------
def search(query, word_to_index, index_to_word):
    words = tokenize(query)
    indices = []

    for w in words:
        if w in word_to_index:
            indices.append(word_to_index[w])

    if not indices:
        return ["No matching words found in lexicon."]

    results = [
        f"Query processed: {len(indices)} words found in lexicon",
        "Word indices:"
    ]

    for idx in indices[:10]:  # first 10
        results.append(f"  [{idx}] {index_to_word[idx]}")   # <-- FIXED

    return results

# -----------------------------
# Main interactive REPL
# -----------------------------
if __name__ == "__main__":
    print("Loading lexicon...")
    word_to_index, index_to_word = load_lexicon(LEXICON_PATH)
    print(f"Lexicon loaded with {len(word_to_index)} words.")

    while True:
        q = input("\nEnter search query (or 'exit'): ")
        if q == "exit":
            break

        result = search(q, word_to_index, index_to_word)
        print("\n".join(result))
