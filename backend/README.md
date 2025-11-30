# Search Engine API (CROW Framework)

A search engine API built with CROW framework that uses inverted index for fast document retrieval.

## Setup

### 1. Install CROW

**Option 1: Using vcpkg (Recommended)**
```bash
vcpkg install crow
```

Then configure CMake with vcpkg toolchain:
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/Users/Hp/vcpkg//scripts/buildsystems/vcpkg.cmake
```

### 2. Build

```bash
cd backend
mkdir -p build
cd build
cmake ..
make
# Or on Windows with Visual Studio:
# cmake .. -G "Visual Studio 17 2022"
# Then open the .sln file
```

### 3. Build Indices (if not already built)

```bash
# Build lexicon
./build_lexicon

# Build forward index
./build_forward_index

# Build inverted index
./build_inverted_index
```

### 4. Run

```bash
./search_engine
```

Server will start on `http://localhost:8080`

## API Endpoints

### GET /search
Search using inverted index.

**Query Parameters:**
- `q` or `query` (required): Search query string
- `page` (optional): Page number (default: 0)
- `page_size` (optional): Results per page (default: 10, max: 100)

**Example:**
```
GET /search?q=machine learning&page=0&page_size=10
```

**Response:**
```json
{
  "query": "machine learning",
  "total_results": 150,
  "page": 0,
  "page_size": 10,
  "query_time_ms": 5,
  "results": [
    {
      "doc_id": 123,
      "score": 45
    },
    {
      "doc_id": 456,
      "score": 32
    }
  ]
}
```

### GET /health
Health check endpoint.

**Response:**
```json
{
  "status": "ok",
  "lexicon_loaded": true,
  "lexicon_size": 50000
}
```

## Project Structure

```
backend/
├── src/
│   └── main.cpp              # CROW API server
├── cpp/
│   ├── include/              # Core library headers
│   └── src/                   # Core library sources
├── data/
│   └── processed/             # Index files
│       ├── lexicon.json
│       ├── forward_index.json
│       └── barrels/           # Inverted index barrels
└── CMakeLists.txt
```

## Notes

- The search uses inverted index for fast retrieval
- Results are ranked by term frequency (TF) score
- Lexicon and barrels are cached in memory for performance
- CORS is enabled for all origins

