import json

# Load your lexicon file
with open("lexicon.json", "r", encoding="utf-8") as f:
    lexicon = json.load(f)

word_to_index = lexicon.get("word_to_index", {})
index_to_word = lexicon.get("index_to_word", [])

mismatch_count = 0

for word, idx in word_to_index.items():
    # Check if index is within bounds of index_to_word
    if idx >= len(index_to_word):
        print(f"Index out of range: word='{word}', index={idx}")
        mismatch_count += 1
    elif index_to_word[idx] != word:
        print(f"Mismatch: word='{word}', index={idx}, index_to_word={index_to_word[idx]}")
        mismatch_count += 1

print(f"\nTotal mismatches found: {mismatch_count}")
