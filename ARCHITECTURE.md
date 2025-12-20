# DSA Search Engine - System Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         USER INTERFACE                           │
│                  (React Frontend - Port 5173 dev)                │
│                  Search Bar | Results | Upload                   │
└─────────────────────────────────────────────────────────────────┘
                              ↓ HTTP/JSON
┌─────────────────────────────────────────────────────────────────┐
│                      HTTP SERVER (C++)                           │
│                       Port 8080 (httplib)                        │
│   Routes: /search | /autocomplete | /upload | / (static)        │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                    SEARCH ENGINE (C++)                           │
│    SearchService | Lexicon+Trie | BM25 Ranker | Metadata        │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                      DATA LAYER (Files)                          │
│  Lexicon | Inverted Index (100 barrels) | Metadata | Vectors    │
└─────────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────────┐
│                  DATA PIPELINE (Python)                          │
│   Download → Parse → Tokenize → Index → Build Vectors           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Component Breakdown

### 1. Frontend (React + Vite)

**Location**: `frontend/`

**Key Files**:
- `src/App.jsx` - Main application component
- `src/Components/SearchBar.jsx` - Search interface with autocomplete
- `src/Components/SearchResults.jsx` - Results display
- `src/Components/PDFUpload.jsx` - File upload
- `src/config/api.js` - API endpoint configuration
- `src/hooks/useAutocomplete.js` - Autocomplete logic

**Build Output**: `backend/static/` (served by C++ backend)

**Features**:
- Real-time autocomplete
- Debounced search
- Pagination
- File upload
- Responsive design

---

### 2. Backend Server (C++ httplib)

**Location**: `backend/src/main.cpp`

**Responsibilities**:
- HTTP server on port 8080
- Serve static frontend files
- Handle API requests
- CORS middleware

**Endpoints**:
- `GET /` - Serve React app
- `GET /search?q=<query>` - Search documents
- `GET /autocomplete?q=<prefix>&limit=<n>` - Autocomplete suggestions
- `POST /upload` - Upload PDFs
- `GET /api` - API documentation

---

### 3. Search Engine Core (C++)

**Location**: `backend/src/SearchService.cpp`

**Components**:

#### a) LexiconWithTrie
- **File**: `backend/src/LexiconWithTrie.cpp`
- **Purpose**: Fast word lookup and autocomplete
- **Data Structure**: Trie (prefix tree)
- **Operations**:
  - `get_word_index(word)` → word_id
  - `get_word(word_id)` → word
  - `autocomplete(prefix, limit)` → list of words

#### b) DocumentMetadata
- **File**: `backend/src/DocumentMetadata.cpp`
- **Purpose**: Fast metadata lookup
- **Data Structure**: Hash map (doc_id → metadata)
- **Fields**: title, URL, publication date, citations

#### c) InvertedIndex
- **Files**: `backend/src/inverted_index.cpp`
- **Purpose**: Fast document retrieval
- **Data Structure**: 100 barrel files + 1 delta
- **Format**: word_id → [(doc_id, freq, positions), ...]

#### d) BM25 Ranker
- **Location**: Integrated in `SearchService`
- **Purpose**: Relevance scoring
- **Factors**:
  - Term frequency (TF)
  - Inverse document frequency (IDF)
  - Document length normalization
  - Title boost (2x weight)

---

### 4. Data Layer

**Location**: `backend/data/processed/`

**Files**:

| File | Purpose | Format | Size |
|------|---------|--------|------|
| `test.jsonl` | Main dataset | JSONL | ~500MB |
| `lexicon.json` | Vocabulary | JSON | ~5MB |
| `document_metadata.json` | Metadata lookup | JSON | ~10MB |
| `forward_index.jsonl` | Doc → words | JSONL | ~200MB |
| `inverted_barrel_*.json` | Word → docs | JSON | ~100MB total |
| `inverted_delta.json` | New docs | JSON | <1MB |
| `document_vectors.bin` | Semantic vectors | Binary | ~60MB |

---

### 5. Data Pipeline (Python)

**Location**: `backend/scripts/`

**Scripts**:

| Script | Stage | Input | Output |
|--------|-------|-------|--------|
| `async_fetch_pdfs.py` | 1 | OpenAlex API | `test_raw.jsonl` + PDFs |
| `renumber_docs.py` | 2 | `test_raw.jsonl` | `test_raw_renumbered.jsonl` |
| `parse_from_local.py` | 3 | PDFs | `test.jsonl` (with tokens) |
| `extract_metadata.py` | 4 | `test.jsonl` | `document_metadata.json` |
| `build_lexicon.cpp` | 5 | `test.jsonl` | `lexicon.json` |
| `build_forward_index.cpp` | 6 | `test.jsonl` + lexicon | `forward_index.jsonl` |
| `build_inverted_index.cpp` | 7 | `forward_index.jsonl` | barrel files |
| `build_semantic_vectors.py` | 8 | `test.jsonl` + GloVe | vector files |

**See [DATA_PIPELINE.md](DATA_PIPELINE.md) for detailed workflow.**

---

## Data Structures

### Lexicon (Trie)
```
         root
        /    \
      [c]    [d]
      /        \
    [o]        [a]
    /            \
  [m]            [t]
  /                \
[p]uter            [a]
  \                  \
  [u]te              [base]
```

### Inverted Index (Barrels)
```
Barrel 0 (word_ids 0-499):
  0 → [(5, 3, [10,20,30]), (12, 2, [5,15]), ...]
  1 → [(2, 1, [100]), (5, 5, [20,25,30,35,40]), ...]
  ...

Barrel 1 (word_ids 500-999):
  500 → [(1, 2, [50,100]), ...]
  ...

Delta Barrel (new documents):
  123 → [(5001, 1, [10]), (5002, 3, [5,10,15]), ...]
```

### Document Metadata (Hash Map)
```cpp
unordered_map<int, Metadata> {
  0 → {title: "...", url: "...", year: 2023, ...},
  1 → {title: "...", url: "...", year: 2022, ...},
  ...
}
```

---

## Search Algorithm

```
User Query: "machine learning neural"
    ↓
1. Tokenize: ["machine", "learning", "neural"]
    ↓
2. Convert to word_ids: [15, 42, 108]
    ↓
3. Load barrels: barrel[15], barrel[42], barrel[108]
    ↓
4. Get posting lists:
   15 → [(1, 5), (3, 2), (7, 8), ...]
   42 → [(1, 3), (5, 1), (7, 6), ...]
   108 → [(1, 2), (3, 1), (7, 4), ...]
    ↓
5. Compute BM25 scores:
   doc_1 → score = 8.5
   doc_3 → score = 6.2
   doc_7 → score = 12.8
    ↓
6. Sort by score: [7, 1, 3, ...]
    ↓
7. Return top K results with metadata
```

---

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|-----------------|------------------|
| Autocomplete | O(p) | O(1) |
| Word lookup | O(1) | O(V) |
| Search | O(Q × D_q) | O(D) |
| Add document | O(T) | O(T) |
| Merge delta | O(N × log N) | O(N) |

Where:
- p = prefix length
- V = vocabulary size
- Q = query length
- D_q = docs containing query term
- T = tokens in document
- N = total postings
- D = total documents

---

## Scalability

**Current Capacity**:
- Documents: 5,000
- Vocabulary: ~50,000 words
- Search time: <100ms
- Index size: ~500MB

**Scaling Options**:
1. **Horizontal**: Shard barrels across multiple servers
2. **Vertical**: Load frequently-accessed barrels into memory
3. **Caching**: Cache popular queries
4. **Compression**: Use variable-byte encoding for postings

---

## Technology Stack

**Backend**:
- C++17
- httplib (HTTP server)
- nlohmann/json (JSON parsing)
- Custom data structures

**Frontend**:
- React 19
- Vite (bundler)
- CSS3 (styling)

**Data Pipeline**:
- Python 3.8+
- PyMuPDF (PDF parsing)
- requests (API calls)
- asyncio (concurrent downloads)

**Build Tools**:
- CMake
- Ninja
- npm

---

## Design Decisions

### Why C++ for Search?
- ✅ Fast in-memory operations
- ✅ Low latency (< 100ms)
- ✅ Efficient memory management
- ✅ Good for data structures

### Why Python for Pipeline?
- ✅ Rich library ecosystem (PyMuPDF, requests)
- ✅ Easy to iterate and debug
- ✅ Good for I/O-heavy tasks
- ✅ Simpler error handling

### Why Barrels?
- ✅ Lazy loading (load only needed barrels)
- ✅ Parallel processing
- ✅ Easy to update (delta barrel)
- ✅ Cache-friendly

### Why Trie for Autocomplete?
- ✅ O(p) prefix search
- ✅ Memory-efficient
- ✅ Natural ordering

---

## Future Enhancements

1. **Query Understanding**
   - Spell correction
   - Synonym expansion
   - Phrase detection

2. **Ranking Improvements**
   - Learning to rank (ML)
   - Personalization
   - Click-through data

3. **Semantic Search**
   - Use document vectors
   - Cosine similarity
   - Hybrid ranking (BM25 + semantic)

4. **Distributed System**
   - Shard by barrel ID
   - Load balancer
   - Replication

5. **Real-time Updates**
   - Stream new documents
   - Incremental indexing
   - Background merges

---

## See Also

- [DATA_PIPELINE.md](DATA_PIPELINE.md) - Detailed pipeline documentation
- [QUICK_START.md](QUICK_START.md) - Getting started guide
- [README.md](README.md) - Project overview
