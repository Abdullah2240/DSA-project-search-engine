import json
import sys
from collections import defaultdict

if len(sys.argv) != 2 and len(sys.argv) != 3:
    print("Usage: python check.py lexicon.json cleaned.jsonl")
    sys.exit(1)

lex_path = sys.argv[1]
clean_path = sys.argv[2]

print("\n======= VALIDATION REPORT =======\n")

# ----------------------------
# Load Lexicon
# ----------------------------
print("[1] Loading lexicon…")
with open(lex_path, "r", encoding="utf8") as f:
    lex = json.load(f)

index_to_word = lex["index_to_word"]        # LIST
word_to_index = lex["word_to_index"]        # DICT (word → index)
df_map = lex.get("df", {})                  # DICT: index → df
cdf_map = lex.get("cdf", {})                
idf_map = lex.get("idf", {})

lex_size = len(index_to_word)
print(f"   Loaded {lex_size} words.")

# ----------------------------
# Index alignment check
# ----------------------------
print("\n[2] Checking lexicon index alignment…")

indices = list(range(lex_size))

# list version, simply check:
#   index_to_word[i] exists for all i
missing_positions = []

for i in range(lex_size):
    if i >= len(index_to_word):
        missing_positions.append(i)

if missing_positions:
    print("❌ Lexicon index holes:", missing_positions[:20], "…")
else:
    print("   ✓ Lexicon index_to_word list is continuous and correct.")


# ----------------------------
# Forward index scan
# ----------------------------
print("\n[3] Scanning cleaned.jsonl…")

actual_df = defaultdict(int)
doc_count = 0
total_tokens = 0

with open(clean_path, "r", encoding="utf8") as f:
    for line in f:
        if not line.strip():
            continue
        obj = json.loads(line)
        doc_count += 1

        tokens = obj["tokens"]
        total_tokens += len(tokens)

        unique_in_doc = set(tokens)
        for tok in unique_in_doc:
            if tok not in word_to_index:
                print(f"❌ Token '{tok}' in doc {obj['doc_id']} missing in lexicon.")
            else:
                actual_df[tok] += 1

print(f"   Scanned {doc_count} documents.")
print(f"   Total tokens: {total_tokens}")

# ----------------------------
# DF consistency check
# ----------------------------
print("\n[4] Checking DF consistency…")

df_errors = 0

for word, idx in word_to_index.items():
    lex_df = df_map.get(str(idx)) or df_map.get(idx)

    if lex_df is None:
        print(f"❌ DF missing for word '{word}' (index {idx})")
        df_errors += 1
        continue

    actual = actual_df.get(word, 0)

    if lex_df != actual:
        print(f"❌ DF mismatch for '{word}': lex={lex_df}, actual={actual}")
        df_errors += 1

if df_errors == 0:
    print("   ✓ All DF values match.")
else:
    print(f"   Total DF mismatches: {df_errors}")

# ----------------------------
# CDF check
# ----------------------------
print("\n[5] Checking CDF length…")

if len(cdf_map) != lex_size:
    print(f"❌ CDF length mismatch (lex={lex_size}, cdf={len(cdf_map)})")
else:
    print("   ✓ CDF size matches lexicon size")


# ----------------------------
# IDF
# ----------------------------
print("\n[6] Checking IDF…")

missing_idf = [i for i in indices if str(i) not in idf_map]

if missing_idf:
    print("❌ Missing IDF values for indices:", missing_idf[:20], "…")
else:
    print("   ✓ All IDF values present")

print("\n======= VALIDATION COMPLETE =======\n")
