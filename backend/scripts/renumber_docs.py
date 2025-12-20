#!/usr/bin/env python3
"""
renumber_docs.py

Renumbers PDFs and doc_ids to be contiguous (0, 1, 2, 3, ...).
This makes the dataset cleaner and ensures doc_ids match filenames.

Usage:
    python renumber_docs.py \
        --raw-jsonl backend/data/processed/test_raw.jsonl \
        --pdf-dir "C:/Users/Hp/PDFS" \
        --out-jsonl backend/data/processed/test_raw_renumbered.jsonl
"""

import argparse
import json
import os
import shutil
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(SCRIPT_DIR, "..", "data")
PROCESSED_DIR = os.path.join(DATA_DIR, "processed")

DEFAULT_RAW_JSONL = os.path.join(PROCESSED_DIR, "test_raw.jsonl")
DEFAULT_OUT_JSONL = os.path.join(PROCESSED_DIR, "test_raw_renumbered.jsonl")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Renumber PDFs and doc_ids to be contiguous (0, 1, 2, ...)."
    )
    parser.add_argument(
        "--raw-jsonl",
        type=str,
        default=DEFAULT_RAW_JSONL,
        help=f"Path to input JSONL (default: {DEFAULT_RAW_JSONL})",
    )
    parser.add_argument(
        "--out-jsonl",
        type=str,
        default=DEFAULT_OUT_JSONL,
        help=f"Path to output JSONL (default: {DEFAULT_OUT_JSONL})",
    )
    parser.add_argument(
        "--pdf-dir",
        type=str,
        required=True,
        help="Directory containing PDFs (e.g., C:/Users/Hp/PDFS)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without actually renaming files.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    if not os.path.exists(args.raw_jsonl):
        print(f"ERROR: Input JSONL file not found: {args.raw_jsonl}", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(args.pdf_dir):
        print(f"ERROR: PDF directory not found: {args.pdf_dir}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(args.out_jsonl), exist_ok=True)

    # First pass: collect all valid records
    valid_records = []
    with open(args.raw_jsonl, "r", encoding="utf-8") as infile:
        for line in infile:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
                pdf_path = record.get("pdf_path")
                if pdf_path and os.path.exists(pdf_path):
                    valid_records.append(record)
                else:
                    print(
                        f"Warning: Skipping doc_id {record.get('doc_id')} - PDF not found: {pdf_path}",
                        file=sys.stderr,
                    )
            except json.JSONDecodeError:
                continue

    print(f"Found {len(valid_records)} valid records with existing PDFs.")

    if args.dry_run:
        print("\nDRY RUN - Would rename:")
        for i, rec in enumerate(valid_records[:10]):  # Show first 10
            old_path = rec.get("pdf_path")
            new_path = os.path.join(args.pdf_dir, f"{i}.pdf")
            print(f"  {old_path} -> {new_path}")
        if len(valid_records) > 10:
            print(f"  ... and {len(valid_records) - 10} more")
        return

    # Second pass: rename files and write new JSONL
    new_id = 0
    renamed_count = 0
    skipped_count = 0

    with open(args.out_jsonl, "w", encoding="utf-8") as outfile:
        for record in valid_records:
            old_path = record.get("pdf_path")
            if not old_path or not os.path.exists(old_path):
                skipped_count += 1
                continue

            new_path = os.path.join(args.pdf_dir, f"{new_id}.pdf")

            # Only rename if different
            if old_path != new_path:
                try:
                    # If target already exists and is different, skip
                    if os.path.exists(new_path) and os.path.abspath(old_path) != os.path.abspath(new_path):
                        print(
                            f"Warning: Target {new_path} already exists, skipping doc_id {record.get('doc_id')}",
                            file=sys.stderr,
                        )
                        skipped_count += 1
                        continue

                    shutil.move(old_path, new_path)
                    renamed_count += 1
                except Exception as e:
                    print(
                        f"Error renaming {old_path} to {new_path}: {e}",
                        file=sys.stderr,
                    )
                    skipped_count += 1
                    continue

            # Update record with new doc_id and path
            record["doc_id"] = new_id
            record["pdf_path"] = new_path

            outfile.write(json.dumps(record, ensure_ascii=False) + "\n")
            new_id += 1

            if new_id % 100 == 0:
                print(f"Processed {new_id} documents...")

    print(f"\nRenumbering complete:")
    print(f"  - Renamed {renamed_count} PDF files")
    print(f"  - Skipped {skipped_count} records")
    print(f"  - Wrote {new_id} records to {args.out_jsonl}")
    print(f"\nNext step: Run parse_from_local.py with --raw-jsonl {args.out_jsonl}")


if __name__ == "__main__":
    main()

