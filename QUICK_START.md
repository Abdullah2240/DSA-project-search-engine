# Quick Start Guide

## Running the Application

### Option 1: Use the Launcher Scripts (Easiest)

**Windows (PowerShell):**
```powershell
.\run-app.ps1
```

**Windows (Command Prompt):**
```cmd
run-app.bat
```

This will automatically:
1. Check if the backend is built
2. Install frontend dependencies if needed
3. Start both servers in separate windows
4. Open the application at http://localhost:5173

### Option 2: Manual Start

**Terminal 1 - Backend:**
```bash
cd backend/build/Release
./main.exe
```

**Terminal 2 - Frontend:**
```bash
cd frontend
npm run dev
```

Then open http://localhost:5173 in your browser.

## First Time Setup

If this is your first time running the application:

1. **Build the Backend:**
   ```bash
   cd backend
   cmake -S . -B build
   cmake --build build --config Release
   ```

2. **Install Frontend Dependencies:**
   ```bash
   cd frontend
   npm install
   ```

3. **Build Search Index (if not done yet):**
   ```bash
   cd backend
   # Run your indexing scripts
   ```

## Using the Application

### Search
1. Type your query in the search bar
2. See autocomplete suggestions as you type
3. Press Enter or click the search icon
4. View results with scores and document links

### Upload PDFs
1. Click the "Upload PDFs" tab
2. Drag and drop PDF files or click to browse
3. Files will be uploaded to the backend
4. Run indexing to add them to the search engine

## Troubleshooting

**Backend won't start:**
- Make sure port 8080 is not in use
- Check that the backend was built successfully

**Frontend won't start:**
- Run `npm install` in the frontend folder
- Make sure port 5173 is available

**Can't connect to backend:**
- Ensure backend is running on port 8080
- Check console for errors

**No search results:**
- Make sure the search index has been built
- Check that data files exist in `backend/data/processed/`

## Ports

- Backend: http://localhost:8080
- Frontend: http://localhost:5173

## API Endpoints

- `GET /search?q=<query>` - Search for documents
- `GET /autocomplete?q=<prefix>&limit=<num>` - Get autocomplete suggestions
- `POST /upload` - Upload PDF files

For more details, see [FRONTEND_BACKEND_CONNECTION.md](FRONTEND_BACKEND_CONNECTION.md)
