import requests
import json
import time
import re
import os

# Get the directory of this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_FILE = os.path.join(SCRIPT_DIR, "..", "data", "processed", "cleaned.jsonl")
TARGET_COUNT = 15000  # 15k articles
TITLE_MAX_LEN = 500  # skip crazy long titles

BASE_URL = "https://api.openalex.org/works"
PER_PAGE = 200  # max per page

def safe_pdf_url(work):
    """
    Returns PDF URL if available and valid, else None
    Skips if PDF URL is null or missing
    """
    loc = work.get("best_oa_location")
    if not loc or not isinstance(loc, dict):
        return None
    
    pdf_url = loc.get("pdf_url")
    # Skip if null, empty, or invalid
    if not pdf_url or pdf_url is None:
        return None
    if not isinstance(pdf_url, str) or len(pdf_url) < 10:
        return None
    if not (pdf_url.startswith("http://") or pdf_url.startswith("https://")):
        return None
    
    return pdf_url

def get_page_rank(work):
    """
    Extracts page rank data from work object
    Returns None if page rank data is null or missing
    """
    # OpenAlex provides cited_by_count directly
    # We use citation count as a proxy for page rank
    cited_by_count = work.get("cited_by_count")
    
    # Strict validation: cited_by_count must be present and not None
    # We accept 0 citations (new papers), but reject null/missing values
    if cited_by_count is None:
        return None
    
    # Ensure it's a valid number (not a string or other type)
    try:
        cited_by_count = int(cited_by_count)
    except (ValueError, TypeError):
        return None
    
    # Build page rank object (cited_by_count is the main metric for ranking)
    page_rank = {
        "cited_by_count": cited_by_count
    }
    
    # Try to get additional metrics if available (may not be in select response)
    metrics = work.get("metrics")
    if metrics and isinstance(metrics, dict):
        if "i10_index" in metrics and metrics.get("i10_index") is not None:
            try:
                page_rank["i10_index"] = int(metrics.get("i10_index"))
            except (ValueError, TypeError):
                pass
        if "h_index" in metrics and metrics.get("h_index") is not None:
            try:
                page_rank["h_index"] = int(metrics.get("h_index"))
            except (ValueError, TypeError):
                pass
    
    return page_rank

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
    
    # Always start fresh - overwrite any existing file
    # This ensures doc_id always starts from 0
    if os.path.exists(OUTPUT_FILE):
        print(f"Warning: Overwriting existing file: {OUTPUT_FILE}")
        print("Starting fresh with doc_id = 0\n")

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        while collected < TARGET_COUNT:
            params = {
                "select": "id,title,best_oa_location,cited_by_count",
                "filter": "type:article",  # We'll validate page rank data in code (allows 0 citations for new papers)
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

                # Get PDF URL - skip if null or invalid
                pdf_url = safe_pdf_url(w)
                if not pdf_url:
                    continue

                # Get page rank data - skip if null or invalid
                # This is critical for ranking, so we must have valid page rank data
                page_rank = get_page_rank(w)
                if page_rank is None or page_rank.get("cited_by_count") is None:
                    continue

                # Get title - skip if null or too long
                title = w.get("title")
                if not title or title is None or len(title) > TITLE_MAX_LEN:
                    continue

                title_tokens = tokenize(title)

                entry = {
                    "doc_id": collected,
                    "url": pdf_url,
                    "title": title,
                    "title_tokens": title_tokens,
                    "page_rank": page_rank
                    # body_tokens will be added by crawl_pdf.py
                }

                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
                collected += 1

                if collected % 100 == 0:
                    print(f"Collected {collected}/{TARGET_COUNT} articles with PDFs and page rank...")

            # polite rate limiting
            time.sleep(0.2)

    print(f"\nDone! Collected {collected} articles with valid PDF URLs and page rank data")
    print(f"Output saved to: {OUTPUT_FILE}")
    print(f"\nNext step: Run crawl_pdf.py to extract text from PDFs")

if __name__ == "__main__":
    main()
