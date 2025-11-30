import requests
import json
import time
import re
import os

# Get the directory of this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_FILE = os.path.join(SCRIPT_DIR, "..", "processed", "cleaned.jsonl")
TARGET_COUNT = 2000
TITLE_MAX_LEN = 500  # skip crazy long titles

BASE_URL = "https://api.openalex.org/works"
PER_PAGE = 200  # max per page

def safe_pdf_url(work):
    """
    Returns PDF URL if available, else None
    """
    loc = work.get("best_oa_location")
    if not loc or not isinstance(loc, dict):
        return None
    pdf_url = loc.get("pdf_url")
    if not pdf_url or not isinstance(pdf_url, str) or len(pdf_url) < 10:
        return None
    if not (pdf_url.startswith("http://") or pdf_url.startswith("https://")):
        return None
    return pdf_url

def tokenize(text):
    """
    Lowercase, remove non-alphanumeric, split by whitespace
    """
    text = text.lower()
    text = re.sub(r"[^a-z0-9\s]", " ", text)
    tokens = [t for t in text.split() if t]
    return tokens

def main():
    collected = 0
    cursor = "*"
    
    # Ensure output directory exists
    output_dir = os.path.dirname(os.path.abspath(OUTPUT_FILE))
    os.makedirs(output_dir, exist_ok=True)

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        while collected < TARGET_COUNT:
            params = {
                "select": "id,title,best_oa_location",
                "filter": "type:article",
                "per-page": PER_PAGE,
                "cursor": cursor
            }

            try:
                res = requests.get(BASE_URL, params=params, timeout=30)
                res.raise_for_status()  # Raise an exception for bad status codes
                data = res.json()
            except requests.exceptions.RequestException as e:
                print(f"Request failed: {e}, retrying in 1 seconds...")
                time.sleep(1)
                continue
            except json.JSONDecodeError as e:
                print(f"Failed to parse JSON response: {e}, retrying in 1 seconds...")
                time.sleep(1)
                continue
            except Exception as e:
                print(f"Unexpected error: {e}, retrying in 1 seconds...")
                time.sleep(1)
                continue

            cursor = data.get("meta", {}).get("next_cursor")
            if not cursor:
                print("No more pages.")
                break

            results = data.get("results", [])
            if not results:
                print("No results on this page, moving on...")
                time.sleep(0.2)
                continue

            for w in results:
                if collected >= TARGET_COUNT:
                    break

                pdf_url = safe_pdf_url(w)
                if not pdf_url:  # skip entries without PDF URL
                    continue

                title = w.get("title", "") or ""
                if len(title) > TITLE_MAX_LEN:
                    continue

                tokens = tokenize(title)

                entry = {
                    "doc_id": collected,
                    "url": pdf_url,
                    "tokens": tokens
                }

                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
                collected += 1

                if collected % 100 == 0:
                    print(f"Collected {collected} articles with PDFs...")

            # polite rate limiting
            time.sleep(0.2)

    print(f"Done! Collected {collected} articles in {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
