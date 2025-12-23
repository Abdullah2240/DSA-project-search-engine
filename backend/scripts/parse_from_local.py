#!/usr/bin/env python3
"""
parse_from_local.py

Stage 2 of the ingestion pipeline:
  - Reads test_raw.jsonl (or test_raw_renumbered.jsonl) with metadata + pdf_path
  - Parses local PDFs to extract text
  - Tokenizes body text
  - Writes final test.jsonl with body_tokens

Features:
  - Resumable: Can Ctrl+C and resume from where it left off
  - Incremental: Writes results as it processes
  - Tracks progress: Skips already-processed documents

Usage:
    python parse_from_local.py \
        --raw-jsonl backend/data/processed/test_raw_renumbered.jsonl \
        --out-jsonl backend/data/processed/test.jsonl \
        --pdf-dir "C:/Users/Hp/PDFS"
"""

import argparse
import json
import os
import re
import sys
import time
from pathlib import Path
from typing import Optional, Set

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(SCRIPT_DIR, "..", "data")
PROCESSED_DIR = os.path.join(DATA_DIR, "processed")

DEFAULT_RAW_JSONL = os.path.join(PROCESSED_DIR, "test_raw_renumbered.jsonl")
DEFAULT_OUT_JSONL = os.path.join(PROCESSED_DIR, "test.jsonl")

# PDF processing libraries
try:
    import fitz  # PyMuPDF
    PDF_LIB = "PyMuPDF"
except ImportError:
    try:
        import pdfplumber
        PDF_LIB = "pdfplumber"
    except ImportError:
        print("ERROR: No PDF library found. Install one of:")
        print("  pip install PyMuPDF")
        print("  pip install pdfplumber")
        sys.exit(1)


def tokenize(text: Optional[str]):
    """Tokenize text: lowercase, remove non-alphanumeric, split by whitespace."""
    if not text:
        return []
    text = re.sub(r"[^a-z0-9\s]", " ", text.lower())
    return [t for t in text.split() if len(t) > 1]


def extract_text_from_pdf(pdf_path: str) -> Optional[str]:
    """Extract text from PDF file (first 100 pages)."""
    try:
        text = ""
        if PDF_LIB == "PyMuPDF":
            doc = fitz.open(pdf_path)
            if doc.page_count == 0:
                doc.close()
                return None
            for page_num in range(min(doc.page_count, 100)):
                try:
                    page = doc.load_page(page_num)
                    text += page.get_text()
                except Exception:
                    continue
            doc.close()
        else:
            import pdfplumber
            with pdfplumber.open(pdf_path) as pdf:
                for page in pdf.pages[:100]:
                    try:
                        text += page.extract_text() or ""
                    except Exception:
                        continue
        return text.strip() if text else None
    except Exception as e:
        print(f"Error extracting text from {pdf_path}: {e}", file=sys.stderr)
        return None


def load_processed_doc_ids(out_jsonl_path: str) -> Set[int]:
    """Load set of doc_ids that have already been processed."""
    processed = set()
    if not os.path.exists(out_jsonl_path):
        return processed
    
    try:
        with open(out_jsonl_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    record = json.loads(line)
                    doc_id = record.get("doc_id")
                    if doc_id is not None:
                        processed.add(doc_id)
                except json.JSONDecodeError:
                    continue
    except Exception as e:
        print(f"Warning: Could not read existing output file: {e}", file=sys.stderr)
    
    return processed


def process_record(record: dict, pdf_dir: str, max_tokens: Optional[int] = None) -> Optional[dict]:
    """Process a single record: read PDF, extract text, tokenize."""
    pdf_path = record.get("pdf_path")
    
    # If pdf_path is relative or just filename, resolve it
    if pdf_path and not os.path.isabs(pdf_path):
        pdf_path = os.path.join(pdf_dir, os.path.basename(pdf_path))
    elif not pdf_path:
        # Try to construct from doc_id
        doc_id = record.get("doc_id")
        if doc_id is not None:
            pdf_path = os.path.join(pdf_dir, f"{doc_id}.pdf")
        else:
            return None
    
    if not os.path.exists(pdf_path):
        print(f"Warning: PDF not found for doc_id={record.get('doc_id')}: {pdf_path}", file=sys.stderr)
        return None

    text = extract_text_from_pdf(pdf_path)
    if not text:
        return None

    tokens = tokenize(text)
    if not tokens:
        return None

    # Limit tokens if specified (for memory management)
    original_token_count = len(tokens)
    if max_tokens and len(tokens) > max_tokens:
        tokens = tokens[:max_tokens]

    # Enrich record with body_tokens and stats
    result = record.copy()
    result.update({
        "body_tokens": tokens,
        "word_count": len(tokens),
        "original_word_count": original_token_count,  # Track if truncated
        "char_count": len(text),
        "text_preview": text[:200] + ("..." if len(text) > 200 else ""),
        "pdf_path": pdf_path,  # Ensure absolute path is stored
    })
    return result


def parse_args():
    parser = argparse.ArgumentParser(
        description="Parse local PDFs and add body_tokens to create final test.jsonl."
    )
    parser.add_argument(
        "--raw-jsonl",
        type=str,
        default=DEFAULT_RAW_JSONL,
        help=f"Path to input JSONL with metadata and pdf_path (default: {DEFAULT_RAW_JSONL})",
    )
    parser.add_argument(
        "--out-jsonl",
        type=str,
        default=DEFAULT_OUT_JSONL,
        help=f"Path to final JSONL output with body_tokens (default: {DEFAULT_OUT_JSONL})",
    )
    parser.add_argument(
        "--pdf-dir",
        type=str,
        default=r"C:/Users/Hp/PDFS",
        help="Directory containing PDF files (default: C:/Users/Hp/PDFS)",
    )
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=None,
        help="Maximum tokens per document (default: None = no limit). Use 10000 to limit size.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    if not os.path.exists(args.raw_jsonl):
        print(f"ERROR: Raw JSONL file not found: {args.raw_jsonl}", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(args.pdf_dir):
        print(f"ERROR: PDF directory not found: {args.pdf_dir}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(args.out_jsonl), exist_ok=True)

    # Load already-processed doc_ids for resume capability
    processed_doc_ids = load_processed_doc_ids(args.out_jsonl)
    is_resuming = len(processed_doc_ids) > 0
    
    if is_resuming:
        print(f"Resuming: Found {len(processed_doc_ids)} already-processed documents.")
        print(f"Will skip these and continue from remaining documents...")
    
    if args.max_tokens:
        print(f"Token limit: {args.max_tokens} tokens per document (to manage file size)")
    
    # Use append mode if resuming, write mode if starting fresh
    file_mode = "a" if is_resuming else "w"
    
    total_in = 0
    total_out = 0
    skipped = 0
    failed = 0
    start_time = time.time()

    with open(args.raw_jsonl, "r", encoding="utf-8") as infile, \
         open(args.out_jsonl, file_mode, encoding="utf-8") as outfile:
        
        for line in infile:
            line = line.strip()
            if not line:
                continue
            
            total_in += 1
            
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                failed += 1
                continue

            doc_id = record.get("doc_id")
            
            # Skip if already processed
            if doc_id is not None and doc_id in processed_doc_ids:
                skipped += 1
                continue

            # Process PDF
            enriched = process_record(record, args.pdf_dir, max_tokens=args.max_tokens)
            if not enriched:
                failed += 1
                continue

            # Write to output
            outfile.write(json.dumps(enriched, ensure_ascii=False) + "\n")
            outfile.flush()  # Ensure incremental writing
            total_out += 1

            # Progress logging
            if total_out % 100 == 0:
                elapsed = time.time() - start_time
                rate = total_out / elapsed if elapsed > 0 else 0
                remaining = total_in - total_out - skipped
                eta = remaining / rate if rate > 0 else 0
                print(f"Progress: {total_out}/{total_in} processed ({rate:.1f} docs/sec) - "
                      f"Skipped: {skipped}, Failed: {failed}, ETA: {eta/60:.1f} min")

    elapsed = time.time() - start_time
    print(f"\n{'='*60}")
    print(f"Processing complete!")
    print(f"  Total input records: {total_in}")
    print(f"  Successfully processed: {total_out}")
    print(f"  Skipped (already done): {skipped}")
    print(f"  Failed: {failed}")
    print(f"  Time taken: {elapsed:.2f} seconds")
    if total_out > 0:
        print(f"  Average rate: {total_out / elapsed:.2f} docs/sec")
    print(f"  Output saved to: {args.out_jsonl}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
