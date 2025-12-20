# DSA Search Engine

A high-performance document search engine built with C++ backend and React frontend. Designed for fast, relevant search across thousands of research papers.

## ✨ Features

- 🚀 **Fast Search**: Sub-100ms query response time
- 🔍 **Smart Autocomplete**: Real-time suggestions as you type
- 📊 **Ranked Results**: BM25 ranking with title boost
- 📅 **Rich Metadata**: Publication dates, citation counts, keywords
- 📤 **PDF Upload**: Add new documents to the index
- 🎨 **Modern UI**: Responsive React interface with smooth animations

## 🏗️ Architecture

```
Frontend (React)  →  HTTP  →  Backend (C++)  →  Search Engine
                                    ↓
                           Data Files (JSON/Binary)
                                    ↑
                           Data Pipeline (Python)
```

**Key Design Principles:**
- **Python**: Handles all data ingestion, PDF parsing, and tokenization
- **C++**: Handles only searching and ranking (reads pre-built indices)
- **Separation**: Never mix responsibilities

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed system design.

## 📁 Project Structure

```
├── backend/
│   ├── src/                 # C++ source files
│   │   ├── main.cpp         # HTTP server
│   │   ├── SearchService.cpp # Search engine core
│   │   └── ...
│   ├── include/             # Header files
│   ├── scripts/             # Python data pipeline
│   │   ├── async_fetch_pdfs.py    # Stage 1: Download
│   │   ├── renumber_docs.py       # Stage 2: Renumber
│   │   ├── parse_from_local.py    # Stage 3: Parse & Tokenize
│   │   └── ...
│   ├── data/
│   │   └── processed/       # Generated indices
│   └── static/              # Built frontend (served by backend)
│
├── frontend/
│   ├── src/
│   │   ├── Components/      # React components
│   │   ├── hooks/           # Custom hooks
│   │   └── config/          # API configuration
│   └── ...
│
├── run-app.ps1             # PowerShell launcher
├── run-app.bat             # Batch launcher
├── README.md               # This file
├── ARCHITECTURE.md         # System design
├── DATA_PIPELINE.md        # Data processing guide
└── QUICK_START.md          # Getting started
```

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: MSVC, GCC, or Clang
- **CMake** 3.10+
- **Node.js** 18+ and npm
- **Python** 3.8+ (for data pipeline)

### Installation

**1. Clone the repository:**
```bash
git clone <repository-url>
cd dsa-search-engine
```

**2. Build the backend:**
```bash
cd backend
cmake -S . -B build
cmake --build build
cd ..
```

**3. Install frontend dependencies:**
```bash
cd frontend
npm install
cd ..
```

### Running the Application

**Option 1: Use the launcher script (recommended):**
```powershell
# Windows PowerShell
.\run-app.ps1

# Or Windows Command Prompt
run-app.bat
```

This will:
1. Build the frontend → `backend/static/`
2. Start the C++ backend on port 8080
3. Serve both API and frontend from the same server

**Option 2: Manual start:**
```bash
# Build frontend
cd frontend
npm run build
cd ..

# Run backend
cd backend/build
./search_engine
```

**3. Open your browser:**
```
http://localhost:8080
```

## 📊 Data Pipeline

To add documents to the search engine, run the data pipeline:

### Step-by-Step Process

**1. Download PDFs from OpenAlex:**
```bash
cd backend/scripts
python async_fetch_pdfs.py --target-count 5000 --pdf-dir "C:/Users/DELL/PDFS"
```

**2. Renumber documents (make IDs contiguous):**
```bash
python renumber_docs.py \
    --raw-jsonl ../data/processed/test_raw.jsonl \
    --pdf-dir "C:/Users/DELL/PDFS"
```

**3. Parse PDFs and tokenize (CRITICAL STEP):**
```bash
python parse_from_local.py \
    --raw-jsonl ../data/processed/test_raw_renumbered.jsonl \
    --out-jsonl ../data/processed/test.jsonl \
    --pdf-dir "C:/Users/DELL/PDFS"
```

**4. Extract metadata:**
```bash
python extract_metadata.py
```

**5. Build indices (C++):**
```bash
cd ../build
./build_lexicon
./build_forward_index
./build_inverted_index
```

**6. (Optional) Build semantic vectors:**
```bash
cd ../scripts
python build_semantic_vectors.py
```

**See [DATA_PIPELINE.md](DATA_PIPELINE.md) for complete pipeline documentation.**

## 🎯 Usage

### Search
1. Type your query in the search bar
2. See autocomplete suggestions
3. Press Enter or click search
4. View ranked results with metadata

### Upload PDFs
1. Click "Upload PDFs" tab
2. Drag and drop files or browse
3. Files are saved to backend
4. Run pipeline to index them

## 🔧 Configuration

### API Endpoint (Development)
Edit `frontend/src/config/api.js`:
```javascript
export const API_BASE_URL = 'http://localhost:8080';
```

### Backend Port
Edit `backend/src/main.cpp`:
```cpp
svr.listen("127.0.0.1", 8080);  // Change port here
```

## 🛠️ Technology Stack

**Backend:**
- C++17
- httplib (HTTP server)
- nlohmann/json (JSON parsing)
- Custom data structures (Trie, Inverted Index)

**Frontend:**
- React 19
- Vite (build tool)
- CSS3 (animations)

**Data Pipeline:**
- Python 3.8+
- PyMuPDF (PDF parsing)
- requests (API calls)

## 📈 Performance

- **Search Speed**: < 100ms for most queries
- **Autocomplete**: < 50ms with Trie
- **Index Size**: ~500MB for 5,000 documents
- **Memory Usage**: ~200MB at runtime

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📝 License

This project is for academic purposes at the National University of Sciences & Technology (NUST).

## 👥 Authors

DSA Project Team - NUST

## 🙏 Acknowledgments

- NUST DSA Course Instructors
- OpenAlex for research paper data
- Open-source library contributors

## 📚 Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture and design decisions
- [DATA_PIPELINE.md](DATA_PIPELINE.md) - Complete data processing pipeline
- [QUICK_START.md](QUICK_START.md) - Step-by-step getting started guide

## 🐛 Troubleshooting

### Backend won't start
```bash
# Check if port 8080 is in use
netstat -ano | findstr :8080

# Kill the process if needed
taskkill /PID <process_id> /F
```

### Frontend build fails
```bash
cd frontend
rm -rf node_modules package-lock.json
npm cache clean --force
npm install
```

### Search returns no results
1. Verify indices exist: `backend/data/processed/`
2. Check lexicon: `backend/data/processed/lexicon.json`
3. Try a simple query like "computer"

### Corrupted index files
```bash
cd backend/scripts
python fix_inverted_delta.py
```

---

**Need help? Check the [QUICK_START.md](QUICK_START.md) guide or create an issue.**
