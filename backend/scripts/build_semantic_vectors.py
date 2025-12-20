#!/usr/bin/env python3
"""
build_semantic_vectors.py

Preprocessing script to build semantic vectors for documents.
- Downloads/loads GloVe embeddings (300-dim)
- Computes document vectors (average of word embeddings)
- Saves in binary format for C++ to read

Usage:
    python build_semantic_vectors.py \
        --jsonl backend/data/processed/test.jsonl \
        --output backend/data/processed/document_vectors.bin \
        --word-embeddings backend/data/processed/word_embeddings.bin
"""

import argparse
import json
import os
import sys
import struct
import numpy as np
from collections import defaultdict
from typing import Dict, List, Optional

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(SCRIPT_DIR, "..", "data")
PROCESSED_DIR = os.path.join(DATA_DIR, "processed")

DEFAULT_JSONL = os.path.join(PROCESSED_DIR, "test.jsonl")
DEFAULT_DOC_VECTORS = os.path.join(PROCESSED_DIR, "document_vectors.bin")
DEFAULT_WORD_EMBEDDINGS = os.path.join(PROCESSED_DIR, "word_embeddings.bin")

# GloVe file URL (6B tokens, 300-dim, ~822MB)
GLOVE_URL = "https://nlp.stanford.edu/data/glove.6B.zip"
GLOVE_FILE = "glove.6B.300d.txt"  # After unzipping


def download_glove_if_needed(glove_path: str) -> bool:
    """Download GloVe embeddings if not present."""
    if os.path.exists(glove_path):
        print(f"GloVe embeddings found at {glove_path}")
        return True

    print(f"GloVe embeddings not found. Downloading from {GLOVE_URL}...")
    print("This is a large file (~822MB). Please download manually:")
    print(f"  1. Download: {GLOVE_URL}")
    print(f"  2. Extract glove.6B.300d.txt")
    print(f"  3. Place it at: {glove_path}")
    return False


def load_glove_embeddings(glove_path: str, vocab_limit: Optional[int] = None) -> Dict[str, np.ndarray]:
    """
    Load GloVe embeddings from text file.
    Returns: dict mapping word -> 300-dim numpy array
    """
    print(f"Loading GloVe embeddings from {glove_path}...")
    embeddings = {}
    dim = 300

    try:
        with open(glove_path, "r", encoding="utf-8") as f:
            for i, line in enumerate(f):
                if vocab_limit and i >= vocab_limit:
                    break
                parts = line.strip().split()
                if len(parts) != dim + 1:
                    continue
                word = parts[0]
                vector = np.array([float(x) for x in parts[1:]], dtype=np.float32)
                embeddings[word] = vector

                if (i + 1) % 100000 == 0:
                    print(f"  Loaded {i + 1} word embeddings...")

        print(f"Loaded {len(embeddings)} word embeddings")
        return embeddings
    except FileNotFoundError:
        print(f"ERROR: GloVe file not found: {glove_path}", file=sys.stderr)
        return {}
    except Exception as e:
        print(f"ERROR loading GloVe: {e}", file=sys.stderr)
        return {}


def compute_document_vector(
    tokens: List[str], embeddings: Dict[str, np.ndarray], dim: int = 300
) -> Optional[np.ndarray]:
    """
    Compute document vector as average of word embeddings.
    Returns: 300-dim numpy array or None if no valid tokens
    """
    vectors = []
    for token in tokens:
        if token in embeddings:
            vectors.append(embeddings[token])

    if not vectors:
        return None

    # Average all word embeddings
    doc_vector = np.mean(vectors, axis=0)
    # Normalize to unit vector for cosine similarity
    norm = np.linalg.norm(doc_vector)
    if norm > 0:
        doc_vector = doc_vector / norm
    return doc_vector.astype(np.float32)


def save_binary_vectors(
    doc_vectors: Dict[int, np.ndarray],
    word_embeddings: Dict[str, np.ndarray],
    doc_output_path: str,
    word_output_path: str,
    dim: int = 300,
):
    """
    Save vectors in binary format for C++:
    - Document vectors: [num_docs (int32)] [doc_id (int32)] [vector (300 floats)] ...
    - Word embeddings: [num_words (int32)] [word_len (int32)] [word (bytes)] [vector (300 floats)] ...
    """
    print(f"Saving document vectors to {doc_output_path}...")
    with open(doc_output_path, "wb") as f:
        # Write number of documents
        f.write(struct.pack("i", len(doc_vectors)))
        # Write each document vector
        for doc_id in sorted(doc_vectors.keys()):
            vector = doc_vectors[doc_id]
            f.write(struct.pack("i", doc_id))  # doc_id
            f.write(vector.tobytes())  # 300 floats = 1200 bytes

    print(f"Saved {len(doc_vectors)} document vectors")

    print(f"Saving word embeddings to {word_output_path}...")
    with open(word_output_path, "wb") as f:
        # Write number of words
        f.write(struct.pack("i", len(word_embeddings)))
        # Write each word embedding
        for word, vector in sorted(word_embeddings.items()):
            word_bytes = word.encode("utf-8")
            f.write(struct.pack("i", len(word_bytes)))  # word length
            f.write(word_bytes)  # word
            f.write(vector.tobytes())  # 300 floats

    print(f"Saved {len(word_embeddings)} word embeddings")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Build semantic vectors for documents using GloVe embeddings."
    )
    parser.add_argument(
        "--jsonl",
        type=str,
        default=DEFAULT_JSONL,
        help=f"Path to test.jsonl with body_tokens (default: {DEFAULT_JSONL})",
    )
    parser.add_argument(
        "--output",
        type=str,
        default=DEFAULT_DOC_VECTORS,
        help=f"Path to output binary file for document vectors (default: {DEFAULT_DOC_VECTORS})",
    )
    parser.add_argument(
        "--word-embeddings",
        type=str,
        default=DEFAULT_WORD_EMBEDDINGS,
        help=f"Path to output binary file for word embeddings (default: {DEFAULT_WORD_EMBEDDINGS})",
    )
    parser.add_argument(
        "--glove-path",
        type=str,
        default=os.path.join(PROCESSED_DIR, "glove.6B.300d.txt"),
        help="Path to GloVe embeddings file (glove.6B.300d.txt)",
    )
    parser.add_argument(
        "--vocab-limit",
        type=int,
        default=None,
        help="Limit vocabulary size (for testing, None = use all)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    if not os.path.exists(args.jsonl):
        print(f"ERROR: JSONL file not found: {args.jsonl}", file=sys.stderr)
        sys.exit(1)

    # Load GloVe embeddings
    if not download_glove_if_needed(args.glove_path):
        print("Please download GloVe embeddings first.", file=sys.stderr)
        sys.exit(1)

    embeddings = load_glove_embeddings(args.glove_path, args.vocab_limit)
    if not embeddings:
        print("ERROR: Failed to load embeddings", file=sys.stderr)
        sys.exit(1)

    # Process documents
    print(f"\nProcessing documents from {args.jsonl}...")
    doc_vectors = {}
    total_docs = 0
    skipped_docs = 0

    with open(args.jsonl, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            try:
                record = json.loads(line)
                doc_id = record.get("doc_id")
                if doc_id is None:
                    continue

                # Combine title and body tokens
                title_tokens = record.get("title_tokens", [])
                body_tokens = record.get("body_tokens", [])
                all_tokens = title_tokens + body_tokens

                if not all_tokens:
                    skipped_docs += 1
                    continue

                # Compute document vector
                doc_vector = compute_document_vector(all_tokens, embeddings)
                if doc_vector is not None:
                    doc_vectors[doc_id] = doc_vector
                    total_docs += 1
                else:
                    skipped_docs += 1

                if total_docs % 1000 == 0:
                    print(f"  Processed {total_docs} documents...")

            except json.JSONDecodeError:
                continue

    print(f"\nProcessed {total_docs} documents, skipped {skipped_docs}")

    if not doc_vectors:
        print("ERROR: No document vectors computed", file=sys.stderr)
        sys.exit(1)

    # Save binary files
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    save_binary_vectors(doc_vectors, embeddings, args.output, args.word_embeddings)

    # Calculate file sizes
    doc_size = os.path.getsize(args.output) / (1024 * 1024)
    word_size = os.path.getsize(args.word_embeddings) / (1024 * 1024)
    print(f"\nFile sizes:")
    print(f"  Document vectors: {doc_size:.2f} MB")
    print(f"  Word embeddings: {word_size:.2f} MB")
    print(f"  Total: {doc_size + word_size:.2f} MB")


if __name__ == "__main__":
    main()

