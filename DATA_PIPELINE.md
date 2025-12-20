# Data Ingestion Pipeline

## Overview
The DSA Search Engine uses a multi-stage pipeline to process research papers from OpenAlex into a searchable index. **Python handles all data ingestion and preprocessing**, while **C++ handles only searching**.

## Architecture Principle

```
┌─────────────────────────────────────────────────────────────┐
│                    DATA INGESTION (Python)                   │
│  Download → Parse → Tokenize → Index → Build Vectors         │
└─────────────────────────────────────────────────────────────┘
                              ↓
                    ┌─────────────────┐
                    │   Data Files    │
                    │   (JSON/Binary) │
                    └─────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   SEARCH ENGINE (C++)                        │
│         Load Indices → Search → Rank → Return Results        │
└─────────────────────────────────────────────────────────────┘
```

**Key Rule**: C++ NEVER parses PDFs or tokenizes text. That's Python's job.

---

## Pipeline Stages

### Stage 1: Download PDFs & Metadata
**Script**: `backend/scripts/async_fetch_pdfs.py`

**What it does**:
- Queries OpenAlex API for research papers
- Downloads metadata (title, URL, citations, date)
- Downloads PDF files to local directory
- Creates initial JSONL file with metadata

**Usage**:
```bash
cd backend/scripts
python async_fetch_pdfs.py \
    --target-count 5000 \
    --pdf-dir "C:/Users/DELL/PDFS" \
    --output ../data/processed/test_raw.jsonl
```

**Output**: `test_raw.jsonl`
```json
{
  "doc_id": 8500,
  "title": "Some Paper",
  "url": "https://...",
  "pdf_path": "C:/Users/DELL/PDFS/8500.pdf",
  "cited_by_count": 1234,
  "publication_year": 2023,
  "publication_month": 6
}
```

---

### Stage 2: Renumber Documents
**Script**: `backend/scripts/renumber_docs.py`

**What it does**:
- Makes doc_ids contiguous (0, 1, 2, 3, ...)
- Renames PDF files to match doc_ids
- Skips documents with missing PDFs
- Creates clean, ordered dataset

**Usage**:
```bash
python renumber_docs.py \
    --raw-jsonl ../data/processed/test_raw.jsonl \
    --pdf-dir "C:/Users/DELL/PDFS" \
    --out-jsonl ../data/processed/test_raw_renumbered.jsonl
```

**Why?**: 
- Original doc_ids from OpenAlex are sparse (8500, 8691, ...)
- C++ arrays work best with contiguous indices
- Makes debugging easier

**Output**: `test_raw_renumbered.jsonl` + renamed PDFs
```
C:/Users/DELL/PDFS/0.pdf
C:/Users/DELL/PDFS/1.pdf
C:/Users/DELL/PDFS/2.pdf
...
```

---

### Stage 3: Parse PDFs & Tokenize (CRITICAL STAGE)
**Script**: `backend/scripts/parse_from_local.py`

**What it does**:
- Reads each PDF file
- Extracts text using PyMuPDF
- Tokenizes body text (lowercase, remove punctuation, split)
- Adds `body_tokens` field to each document
- Creates the **MAIN DATASET FILE** used by all subsequent stages

**Usage**:
```bash
python parse_from_local.py \
    --raw-jsonl ../data/processed/test_raw_renumbered.jsonl \
    --out-jsonl ../data/processed/test.jsonl \
    --pdf-dir "C:/Users/DELL/PDFS" \
    --max-tokens 10000
```

**Features**:
- ✅ Resumable (can Ctrl+C and restart)
- ✅ Incremental (writes as it processes)
- ✅ Progress tracking
- ✅ Error handling (skips corrupted PDFs)

**Output**: `test.jsonl` ← **THIS IS THE MAIN DATASET**
```json
{
  "doc_id": 0,
  "title": "Deep Learning for Computer Vision",
  "url": "https://...",
  "pdf_path": "C:/Users/DELL/PDFS/0.pdf",
  "cited_by_count": 5000,
  "publication_year": 2023,
  "publication_month": 3,
  "body_tokens": ["deep", "learning", "neural", "network", ...]
}
```

**⚠️ IMPORTANT**: This file is **sacred**. All other processing depends on it.

---

### Stage 4: Extract Metadata
**Script**: `backend/scripts/extract_metadata.py`

**What it does**:
- Reads `test.jsonl`
- Extracts structured metadata for fast C++ loading
- Creates lookup table: doc_id → metadata

**Usage**:
```bash
python extract_metadata.py \
    --input ../data/processed/test.jsonl \
    --output ../data/processed/document_metadata.json
```

**Output**: `document_metadata.json`
```json
{
  "0": {
    "title": "Deep Learning for Computer Vision",
    "url": "https://...",
    "publication_year": 2023,
    "publication_month": 3,
    "cited_by_count": 5000,
    "keywords": ["deep learning", "computer vision"]
  }
}
```

---

### Stage 5: Build Lexicon (C++)
**Executable**: `backend/build/build_lexicon`

**What it does**:
- Reads `test.jsonl`
- Counts word frequencies across all documents
- Filters out stopwords and rare words
- Creates frozen vocabulary: word ↔ word_id mapping

**Usage**:
```bash
cd backend/build
./build_lexicon
```

**Output**: `data/processed/lexicon.json`
```json
{
  "vocabulary": {
    "learning": 0,
    "neural": 1,
    "network": 2
  },
  "stats": {
    "total_words": 50000,
    "doc_count": 5000
  }
}
```

**⚠️ IMPORTANT**: After this stage, the vocabulary is **frozen**. Word IDs never change.

---

### Stage 6: Build Forward Index (C++)
**Executable**: `backend/build/build_forward_index`

**What it does**:
- Reads `test.jsonl` and `lexicon.json`
- Converts body_tokens to word_ids
- Creates doc_id → {word_id: frequency} mapping

**Usage**:
```bash
cd backend/build
./build_forward_index
```

**Output**: `data/processed/forward_index.jsonl`
```json
{"doc_id": 0, "terms": {"0": 15, "1": 8, "2": 12}}
{"doc_id": 1, "terms": {"5": 20, "8": 3}}
```

---

### Stage 7: Build Inverted Index (C++)
**Executable**: `backend/build/build_inverted_index`

**What it does**:
- Reads `forward_index.jsonl`
- Inverts the mapping: word_id → list of doc_ids
- Distributes into 100 barrel files for efficient loading
- Creates delta barrel for dynamic updates

**Usage**:
```bash
cd backend/build
./build_inverted_index
```

**Output**: 
- `data/processed/barrels/inverted_barrel_0.json`
- `data/processed/barrels/inverted_barrel_1.json`
- ...
- `data/processed/barrels/inverted_barrel_99.json`
- `data/processed/barrels/inverted_delta.json`

**Format**:
```json
{
  "word_id": [
    [doc_id, frequency, [positions]],
    [doc_id, frequency, [positions]]
  ]
}
```

---

### Stage 8: Build Semantic Vectors (Optional)
**Script**: `backend/scripts/build_semantic_vectors.py`

**What it does**:
- Downloads GloVe 300-dimensional embeddings
- Computes document vectors (average of word embeddings)
- Saves in binary format for fast C++ loading

**Usage**:
```bash
python build_semantic_vectors.py \
    --jsonl ../data/processed/test.jsonl \
    --output ../data/processed/document_vectors.bin \
    --word-embeddings ../data/processed/word_embeddings.bin
```

**Output**: 
- `document_vectors.bin` (doc_id → 300D vector)
- `word_embeddings.bin` (word_id → 300D vector)

---

## Search Engine (C++)

### What It Does:
1. **Loads** pre-built indices into memory
2. **Searches** using inverted index + BM25 ranking
3. **Returns** JSON results to frontend

### What It DOES NOT Do:
❌ Parse PDFs
❌ Tokenize text
❌ Build indices
❌ Modify data files

### Files:
- `backend/src/main.cpp` - HTTP server
- `backend/src/SearchService.cpp` - Search logic
- `backend/include/SearchService.hpp` - Interface

---

## File Dependency Graph

```
test_raw.jsonl
    ↓
test_raw_renumbered.jsonl + PDFs (0.pdf, 1.pdf, ...)
    ↓
test.jsonl ← MAIN DATASET (with body_tokens)
    ↓
    ├→ document_metadata.json
    ├→ lexicon.json (frozen vocabulary)
    │     ↓
    ├→ forward_index.jsonl
    │     ↓
    └→ inverted_barrel_*.json (100 barrels)
        └→ inverted_delta.json
```

---

## Quick Commands

**Full Pipeline (from scratch)**:
```bash
# 1. Download
cd backend/scripts
python async_fetch_pdfs.py --target-count 5000 --pdf-dir "C:/Users/DELL/PDFS"

# 2. Renumber
python renumber_docs.py \
    --raw-jsonl ../data/processed/test_raw.jsonl \
    --pdf-dir "C:/Users/DELL/PDFS"

# 3. Parse & Tokenize
python parse_from_local.py \
    --raw-jsonl ../data/processed/test_raw_renumbered.jsonl \
    --out-jsonl ../data/processed/test.jsonl \
    --pdf-dir "C:/Users/DELL/PDFS"

# 4. Extract metadata
python extract_metadata.py

# 5. Build indices (C++)
cd ../build
./build_lexicon
./build_forward_index
./build_inverted_index

# 6. Optional: Build semantic vectors
cd ../scripts
python build_semantic_vectors.py
```

**Run the search engine**:
```bash
cd backend/build
./search_engine
```

Then open `http://localhost:8080` in your browser.

---

## Troubleshooting

### "No module named 'fitz'"
```bash
pip install pymupdf
```

### "Corrupted inverted_delta.json"
```bash
python backend/scripts/fix_inverted_delta.py
```

### "Frontend can't connect to backend"
1. Make sure backend is running: `cd backend/build && ./search_engine`
2. Check `http://localhost:8080/api` shows API documentation
3. Build frontend: `cd frontend && npm run build`

### "Search returns no results"
1. Check lexicon exists: `backend/data/processed/lexicon.json`
2. Check indices exist: `backend/data/processed/barrels/`
3. Try a simple query like "computer"

---

## Key Principles

1. **Separation of Concerns**
   - Python: Data ingestion & preprocessing
   - C++: Fast searching & ranking

2. **test.jsonl is Sacred**
   - This is the source of truth
   - Never delete or modify manually
   - All indices can be rebuilt from this file

3. **Lexicon is Frozen**
   - After building, word IDs are permanent
   - Adding new documents requires rebuilding lexicon

4. **Incremental Updates**
   - Use delta barrel for new documents
   - Merge to main barrels periodically

5. **Never Mix Pipelines**
   - Don't let C++ tokenize text
   - Don't let Python do searching
