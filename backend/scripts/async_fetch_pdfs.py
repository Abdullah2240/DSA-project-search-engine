#!/usr/bin/env python3
"""
async_fetch_pdfs.py

Stage 1 of the ingestion pipeline:
  - Fetches works from the OpenAlex API
  - Downloads PDFs concurrently using aiohttp
  - Writes metadata (without body_tokens) to test_raw.jsonl
  - Saves PDFs to a configurable directory

Stage 2 (see parse_from_local.py) is responsible for parsing these local PDFs
and producing the final test.jsonl with body_tokens.
"""

import asyncio
import aiohttp
import argparse
import json
import os
import re
import sys
import time
import uuid
from datetime import datetime
from typing import Optional, Dict, Any

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(SCRIPT_DIR, "..", "data")
PROCESSED_DIR = os.path.join(DATA_DIR, "processed")

# Default paths; can be overridden via CLI
DEFAULT_RAW_JSONL = os.path.join(PROCESSED_DIR, "test_raw.jsonl")
DEFAULT_FINAL_DIR = os.path.join(DATA_DIR, "temp_pdfs")

BASE_URL = "https://api.openalex.org/works"
TITLE_MAX_LEN = 500


def tokenize(text: Optional[str]):
    if not text:
        return []
    text = re.sub(r"[^a-z0-9\s]", " ", text.lower())
    return [t for t in text.split() if len(t) > 1]


def extract_year_from_date(date_str: Any) -> Optional[int]:
    if not date_str:
        return None
    date_str = str(date_str)[:10]
    for fmt in ("%Y-%m-%d", "%Y-%m", "%Y"):
        try:
            return datetime.strptime(date_str, fmt).year
        except Exception:
            continue
    return None


def get_page_rank(work: Dict[str, Any]) -> Optional[Dict[str, int]]:
    try:
        return {"cited_by_count": int(work.get("cited_by_count"))}
    except Exception:
        return None


def safe_pdf_url(work: Dict[str, Any]) -> Optional[str]:
    loc = work.get("best_oa_location") or {}
    url = loc.get("pdf_url")
    if isinstance(url, str) and url.startswith("http"):
        return url
    return None


def extract_metadata(work: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    title = work.get("title")
    if not title or len(title) > TITLE_MAX_LEN:
        return None

    pdf_url = safe_pdf_url(work)
    page_rank = get_page_rank(work)
    pub_year = extract_year_from_date(
        work.get("publication_date") or work.get("publication_year")
    )

    if not all([pdf_url, page_rank, pub_year]):
        return None

    return {
        "title": title,
        "url": pdf_url,
        "publication_year": pub_year,
        "page_rank": page_rank,
        "openalex_id": work.get("id"),
        "title_tokens": tokenize(title),
    }


async def fetch_pdf(
    session: aiohttp.ClientSession,
    url: str,
    dest_path: str,
    max_retries: int = 2,
    doc_id: Optional[int] = None,
    verify_ssl: bool = True,
) -> bool:
    """
    Download a single PDF to dest_path.
    Returns True on success, False otherwise.
    """
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)

    # Use unique temp filename to avoid Windows file locking issues
    tmp_path = dest_path + f".{uuid.uuid4().hex}.part"

    for attempt in range(max_retries):
        try:
            # For publishers that require a landing page (e.g. Wiley),
            # hit the landing page first if needed.
            if "wiley.com" in url and "pdf" in url:
                landing = url.split("/pdf")[0]
                try:
                    async with session.get(landing, timeout=30, ssl=verify_ssl) as _:
                        await _.read()
                except Exception:
                    # Best-effort; continue to main request
                    pass

            async with session.get(
                url, timeout=25, allow_redirects=True, ssl=verify_ssl
            ) as resp:
                if resp.status != 200:
                    await resp.read()
                    if attempt < max_retries - 1:
                        await asyncio.sleep(2**attempt)
                        continue
                    return False

                # Check content length (skip very large PDFs > 50MB)
                content_length = resp.headers.get("Content-Length") or resp.headers.get(
                    "content-length"
                )
                if content_length is not None:
                    try:
                        size_bytes = int(content_length)
                        if size_bytes > 50 * 1024 * 1024:
                            print(
                                f"[doc {doc_id}] Skipping PDF larger than 50MB "
                                f"({size_bytes / (1024*1024):.1f} MB)"
                            )
                            await resp.read()
                            return False
                    except ValueError:
                        pass

                # Stream to file
                try:
                    with open(tmp_path, "wb") as f:
                        async for chunk in resp.content.iter_chunked(8192):
                            if chunk:
                                f.write(chunk)
                except OSError as e:
                    # Windows file locking issue - skip this download
                    if e.winerror == 32:
                        print(f"[doc {doc_id}] WinError 32 (file in use), skipping.")
                        try:
                            if os.path.exists(tmp_path):
                                os.remove(tmp_path)
                        except OSError:
                            pass
                        return False
                    raise

                # Ensure response is fully consumed/closed
                await resp.release()

                # Verify PDF magic bytes
                try:
                    with open(tmp_path, "rb") as f:
                        if f.read(4) != b"%PDF":
                            os.remove(tmp_path)
                            print(f"[doc {doc_id}] Response is not a PDF (bad magic bytes), skipping.")
                            return False
                except OSError as e:
                    if e.winerror == 32:
                        print(f"[doc {doc_id}] WinError 32 reading temp file, skipping.")
                        return False
                    raise

                # Atomic rename
                try:
                    os.replace(tmp_path, dest_path)
                    print(f"[doc {doc_id}] Downloaded PDF to {dest_path}")
                    return True
                except OSError as e:
                    if e.winerror == 32:
                        print(f"[doc {doc_id}] WinError 32 renaming file, skipping.")
                        try:
                            if os.path.exists(tmp_path):
                                os.remove(tmp_path)
                        except OSError:
                            pass
                        return False
                    raise

        except (aiohttp.ClientError, asyncio.TimeoutError, OSError) as e:
            print(f"[doc {doc_id}] Error downloading PDF (attempt {attempt+1}): {e}")
            # Clean up temp file on error
            try:
                if os.path.exists(tmp_path):
                    os.remove(tmp_path)
            except OSError:
                pass
            if attempt < max_retries - 1:
                await asyncio.sleep(2**attempt)
                continue
            return False

    return False


async def worker(
    name: int,
    session: aiohttp.ClientSession,
    queue: "asyncio.Queue[Dict[str, Any]]",
    raw_file,
    failed_log,
    result_queue: "asyncio.Queue[bool]",
    verify_ssl: bool = True,
):
    while True:
        item = await queue.get()
        if item is None:
            queue.task_done()
            break

        metadata = item["metadata"]
        doc_id = item["doc_id"]
        pdf_dir = item["pdf_dir"]
        pdf_path = os.path.join(pdf_dir, f"{doc_id}.pdf")

        print(f"[doc {doc_id}] Starting download: {metadata['url']}")
        success = await fetch_pdf(
            session, metadata["url"], pdf_path, doc_id=doc_id, verify_ssl=verify_ssl
        )

        # Report result back to the main task
        await result_queue.put(success)

        if success:
            record = {
                "doc_id": doc_id,
                **metadata,
                "pdf_path": pdf_path,
            }
            raw_file.write(json.dumps(record, ensure_ascii=False) + "\n")
            raw_file.flush()
        else:
            failed_log.write(
                json.dumps(
                    {
                        "doc_id": doc_id,
                        "url": metadata["url"],
                        "reason": "download_failed",
                    },
                    ensure_ascii=False,
                )
                + "\n"
            )
            failed_log.flush()

        queue.task_done()


def load_resume_state(raw_jsonl_path: str) -> tuple[int, str, int]:
    """
    Load resume state from existing files.
    Returns: (success_count, cursor, next_doc_id)
    """
    cursor_file = raw_jsonl_path + ".cursor"
    success_count = 0
    cursor = "*"
    next_doc_id = 0

    # Count existing successful downloads
    if os.path.exists(raw_jsonl_path):
        with open(raw_jsonl_path, "r", encoding="utf-8") as f:
            for line in f:
                if line.strip():
                    try:
                        rec = json.loads(line)
                        success_count += 1
                        # Track highest doc_id seen
                        doc_id = rec.get("doc_id", 0)
                        if isinstance(doc_id, int) and doc_id >= next_doc_id:
                            next_doc_id = doc_id + 1
                    except json.JSONDecodeError:
                        continue

    # Load cursor if available
    if os.path.exists(cursor_file):
        try:
            with open(cursor_file, "r", encoding="utf-8") as f:
                cursor = f.read().strip()
                if not cursor:
                    cursor = "*"
        except Exception:
            cursor = "*"

    return success_count, cursor, next_doc_id


def save_cursor(raw_jsonl_path: str, cursor: str):
    """Save cursor to file for resume capability."""
    cursor_file = raw_jsonl_path + ".cursor"
    try:
        with open(cursor_file, "w", encoding="utf-8") as f:
            f.write(cursor)
    except Exception:
        pass  # Best effort


async def main_async(
    target_count: int,
    concurrency: int,
    raw_jsonl_path: str,
    pdf_dir: str,
    test_limit: Optional[int] = None,
    verify_ssl: bool = True,
):
    os.makedirs(os.path.dirname(raw_jsonl_path), exist_ok=True)
    os.makedirs(pdf_dir, exist_ok=True)

    # Clean up any stale .part files from previous runs
    for name in os.listdir(pdf_dir):
        if name.endswith(".part"):
            try:
                os.remove(os.path.join(pdf_dir, name))
            except OSError:
                pass

    # Load resume state
    success_count, cursor, next_doc_id = load_resume_state(raw_jsonl_path)
    is_resuming = success_count > 0

    if is_resuming:
        print(
            f"Resuming: Found {success_count} existing successful downloads. "
            f"Starting from doc_id {next_doc_id}, cursor: {cursor[:50]}..."
        )

    connector = aiohttp.TCPConnector(limit_per_host=concurrency)
    headers = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
        "Accept": "application/pdf,text/html;q=0.9,*/*;q=0.8",
    }

    timeout = aiohttp.ClientTimeout(total=40, sock_read=15)
    job_queue: "asyncio.Queue[Dict[str, Any]]" = asyncio.Queue()
    result_queue: "asyncio.Queue[bool]" = asyncio.Queue()

    # Use append mode if resuming, write mode if starting fresh
    file_mode = "a" if is_resuming else "w"
    failed_log_mode = "a" if is_resuming else "w"

    async with aiohttp.ClientSession(
        connector=connector, headers=headers, timeout=timeout
    ) as session:
        with open(raw_jsonl_path, file_mode, encoding="utf-8") as raw_file, open(
            raw_jsonl_path + ".failed.log", failed_log_mode, encoding="utf-8"
        ) as failed_log:
            workers = [
                asyncio.create_task(
                    worker(
                        i, session, job_queue, raw_file, failed_log, result_queue, verify_ssl
                    )
                )
                for i in range(concurrency)
            ]

            doc_id = next_doc_id
            start_time = time.time()

            while success_count < target_count:
                per_page = 25
                params = {
                    # Prefer open-access articles with PDFs to avoid paywalled/HTML-only URLs
                    "filter": "type:article,is_oa:true,has_fulltext:true",
                    "cursor": cursor,
                    "per-page": per_page,
                    "select": "id,title,publication_date,publication_year,best_oa_location,cited_by_count",
                }

                try:
                    async with session.get(BASE_URL, params=params, timeout=60) as resp:
                        resp.raise_for_status()
                        data = await resp.json()
                except Exception as e:
                    print(f"Error fetching OpenAlex data: {e}", file=sys.stderr)
                    await asyncio.sleep(5)
                    continue

                new_cursor = (data.get("meta") or {}).get("next_cursor")
                if not new_cursor:
                    print("No more results from OpenAlex API.")
                    break

                cursor = new_cursor
                save_cursor(raw_jsonl_path, cursor)  # Persist cursor after each page

                for work in data.get("results", []):
                    if success_count >= target_count:
                        break

                    metadata = extract_metadata(work)
                    if not metadata:
                        continue

                    await job_queue.put(
                        {
                            "metadata": metadata,
                            "doc_id": doc_id,
                            "pdf_dir": pdf_dir,
                        }
                    )
                    doc_id += 1

                # Drain result_queue for any completed downloads
                while not result_queue.empty():
                    success = await result_queue.get()
                    if success:
                        success_count += 1
                        if success_count % 10 == 0:
                            print(f"Progress: {success_count}/{target_count} successful downloads")
                    result_queue.task_done()

                if test_limit is not None and success_count >= test_limit:
                    break

            # After enqueueing all jobs, tell workers to exit and wait for them
            for _ in workers:
                await job_queue.put(None)

            await job_queue.join()

            # Drain any remaining results
            while not result_queue.empty():
                success = await result_queue.get()
                if success:
                    success_count += 1
                result_queue.task_done()

            for w in workers:
                await w

            elapsed = time.time() - start_time
            print(
                f"\nSuccessfully downloaded PDFs for {success_count} documents "
                f"(enqueued {doc_id - next_doc_id} new metadata records) in {elapsed:.2f} seconds"
            )
            print(f"Total documents in {raw_jsonl_path}: {success_count}")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Asynchronously fetch PDFs and metadata from OpenAlex."
    )
    parser.add_argument(
        "--target-count",
        type=int,
        default=45000,
        help="Number of valid PDF documents to fetch (default: 45000)",
    )
    parser.add_argument(
        "--concurrency",
        type=int,
        default=20,
        help="Maximum number of concurrent PDF downloads (default: 3)",
    )
    parser.add_argument(
        "--raw-jsonl",
        type=str,
        default=DEFAULT_RAW_JSONL,
        help=f"Path to intermediate JSONL output (default: {DEFAULT_RAW_JSONL})",
    )
    parser.add_argument(
        "--pdf-dir",
        type=str,
        default=DEFAULT_FINAL_DIR,
        help=f"Directory to store downloaded PDFs (default: {DEFAULT_FINAL_DIR})",
    )
    parser.add_argument(
        "--test-20",
        action="store_true",
        help="Only fetch 20 PDFs and print timing for a quick test run.",
    )
    parser.add_argument(
        "--insecure-ssl",
        action="store_true",
        help="Disable SSL certificate verification (fixes some broken sites, less secure).",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    # Allow overriding with your specific absolute path if desired.
    # For example, to use C:/Users/Hp/PDFS, run:
    #   python async_fetch_pdfs.py --pdf-dir "C:/Users/Hp/PDFS"

    target = args.target_count
    test_limit = 20 if args.test_20 else None
    if args.test_20:
        target = 20

    verify_ssl = not args.insecure_ssl
    if args.insecure_ssl:
        print("Warning: SSL verification disabled. This is less secure but may fix some sites.")

    asyncio.run(
        main_async(
            target_count=target,
            concurrency=args.concurrency,
            raw_jsonl_path=args.raw_jsonl,
            pdf_dir=args.pdf_dir,
            test_limit=test_limit,
            verify_ssl=verify_ssl,
        )
    )


if __name__ == "__main__":
    main()


