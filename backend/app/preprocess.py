# This script will read a compressed JSONL dataset of papers and rebuild abstracts 
#it will tokenize the text and save the processed tokens to a new JSONL file.
import gzip
import json
import re
import os
#This is a path to our raw data 
RAW_PATH = "backend/data/raw/openalex_50k.jsonl.gz"
#we will store our cleaned files in this path
OUT_PATH = "backend/data/processed/cleaned.jsonl"

#We will only be considering the title and abstract in lexicon
def rebuild_abstract(inv_idx):
    if not inv_idx:
        return ""
    max_pos = max(pos for positions in inv_idx.values() for pos in positions)
    words = [""] * (max_pos + 1)
    for word, positions in inv_idx.items():
        for p in positions:
            words[p] = word
    return " ".join(words)

#Tokenize data:make it lower case and remove punctuations to get a array of tokens
def tokenize(text):
    text = text.lower()
    return re.findall(r"[a-z0-9]+", text)


def main():
    os.makedirs("backend/data/processed", exist_ok=True)

    with gzip.open(RAW_PATH, "rt", encoding="utf-8") as f_in, \
         open(OUT_PATH, "w", encoding="utf-8") as f_out:

        doc_id = 0

        for line in f_in:
            obj = json.loads(line)

            title = obj.get("title", "")

            if obj.get("abstract_inverted_index"):
                abstract = rebuild_abstract(obj["abstract_inverted_index"])
            else:
                abstract = obj.get("abstract", "")

            full_text = f"{title} {abstract}"
            tokens = tokenize(full_text)

            out_obj = {
                "doc_id": doc_id,
                "tokens": tokens
            }

            f_out.write(json.dumps(out_obj) + "\n")
            doc_id += 1

        print(f"Done. Processed {doc_id} documents.")
        print(f"Saved to {OUT_PATH}")


if __name__ == "__main__":
    main()
