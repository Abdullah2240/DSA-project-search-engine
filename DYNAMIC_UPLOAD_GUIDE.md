# Dynamic PDF Upload Feature - Usage Guide

## 🎉 What's New

PDFs uploaded through the browser are now **automatically indexed and searchable** within seconds!

## ✅ What Was Added

### New Files
1. **`backend/include/PDFProcessor.hpp`** - PDF processing header
2. **`backend/src/PDFProcessor.cpp`** - PDF processing implementation  
3. **`backend/scripts/tokenize_single_pdf.py`** - Python tokenizer for single PDFs

### Modified Files
1. **`backend/src/main.cpp`** - Updated upload endpoint with automatic indexing
2. **`backend/include/SearchService.hpp`** - Added reload methods
3. **`backend/src/SearchService.cpp`** - Implemented reload methods
4. **`backend/CMakeLists.txt`** - Added PDFProcessor to build

## 🔧 Build Instructions

```bash
# Navigate to backend
cd backend

# Rebuild with CMake
cmake --build build --config Release

# Or on Windows with Ninja
cd build
ninja
```

## 🚀 How It Works Now

### Upload Flow
```
1. User uploads PDF via browser
   ↓
2. C++ saves file to data/temp_pdfs/
   ↓
3. PDFProcessor calls Python tokenizer
   ↓
4. Lexicon updated (new words added)
   ↓
5. Forward index appended
   ↓
6. Inverted delta barrel updated
   ↓
7. Metadata & URL mapping added
   ↓
8. test.jsonl updated (persistence)
   ↓
9. SearchService reloads indices
   ↓
10. Document IMMEDIATELY searchable! ✅
```

## 📝 Testing

1. **Start the server:**
   ```bash
   cd backend/build
   ./search_engine
   ```

2. **Upload a PDF:**
   - Go to http://localhost:8080
   - Click "Upload PDF"
   - Select a PDF file
   - Click Upload

3. **Watch the console:**
   You'll see:
   ```
   [Upload] Saved: my_paper.pdf
   [PDFProcessor] Starting processing...
   [PDFProcessor] Assigned doc_id: 5001
   [PDFProcessor] Tokenizing...
   SUCCESS: Tokenized 2345 tokens from data/temp_pdfs/my_paper.pdf
   [PDFProcessor] Extracted 2345 tokens
   [Lexicon] Added 12 new words.
   [PDFProcessor] Lexicon updated
   [PDFProcessor] Built stats for 1234 unique words
   [PDFProcessor] Forward index updated
   [PDFProcessor] Delta barrel updated
   [Metadata] Added doc_id 5001
   [PDFProcessor] Metadata added
   [PDFProcessor] URL mapping added
   [PDFProcessor] Added to test.jsonl
   [PDFProcessor] ✅ Document 5001 is now searchable!
   [Upload] ✅ Indexed doc_id 5001
   [Upload] Reloading search engine indices...
   [Engine] Reloading delta index...
   [Engine] Delta index reloaded successfully
   [Engine] Reloading metadata...
   [Metadata] Loaded metadata for 5001 documents
   [Engine] Metadata reloaded: 5001 documents
   ```

4. **Search for the document:**
   - Type keywords from the PDF in the search box
   - Your uploaded document should appear in results!

## 🛠️ Technical Details

### Incremental Indexing
- **Lexicon**: Only new words are added (no full rebuild)
- **Forward Index**: Single line appended to JSONL
- **Inverted Index**: Updates only delta barrel
- **No server restart needed**: Indices reloaded in-memory

### Files Updated
- `data/processed/lexicon.json` - New words added
- `data/processed/forward_index.jsonl` - New doc appended
- `data/processed/barrels/inverted_delta.json` - New postings added
- `data/processed/document_metadata.json` - New metadata added
- `data/processed/docid_to_url.json` - URL mapping updated
- `data/processed/test.jsonl` - Main dataset updated

### Python Dependencies
Make sure you have PDF parsing libraries:
```bash
pip install PyPDF2
# OR
pip install pdfplumber
```

## ⚠️ Notes

1. **First PDF**: May take 5-10 seconds to process
2. **Subsequent PDFs**: Usually 2-3 seconds each
3. **Delta Barrel**: Periodically merge to main barrels for optimal performance
4. **Doc IDs**: Automatically assigned sequentially
5. **Metadata**: Currently sets year=2024, month=1, citations=0 for uploads

## 🔄 Periodic Maintenance (Optional)

After many uploads, merge delta barrel to main:
```bash
cd backend/build
./build_inverted_index  # Merges delta into 100 barrels
```

## 🎯 Success Criteria

✅ Upload PDF via browser
✅ No manual scripts to run
✅ Document searchable within seconds
✅ Metadata displayed correctly
✅ No server restart required
✅ Persistent across restarts (saved to test.jsonl)

---

**Enjoy your fully automated dynamic PDF indexing!** 🚀
