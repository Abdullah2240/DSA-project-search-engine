# Run Both Frontend and Backend
# This script starts both the backend server and frontend dev server

Write-Host "================================" -ForegroundColor Cyan
Write-Host "DSA Search Engine Launcher" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Check if backend executable exists
$backendExe = "backend\build\search_engine.exe"
if (-not (Test-Path $backendExe)) {
    Write-Host "ERROR: Backend executable not found!" -ForegroundColor Red
    Write-Host "Building backend..." -ForegroundColor Yellow
    Push-Location backend
    cmake -S . -B build
    cmake --build build
    Pop-Location
    
    if (-not (Test-Path $backendExe)) {
        Write-Host "Backend build failed!" -ForegroundColor Red
        exit 1
    }
}

# Check if frontend dependencies are installed
if (-not (Test-Path "frontend\node_modules")) {
    Write-Host "Installing frontend dependencies..." -ForegroundColor Yellow
    Push-Location frontend
    npm install
    if ($LASTEXITCODE -ne 0) {
        Write-Host "npm install failed!" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
    Write-Host ""
}

# Build frontend
Write-Host "Building frontend..." -ForegroundColor Green
Push-Location frontend
npm run build
if ($LASTEXITCODE -ne 0) {
    Write-Host "Frontend build failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

Write-Host "Frontend built successfully to backend/static/" -ForegroundColor Green
Write-Host ""

# Start backend (which now serves the frontend)
Write-Host "Starting backend server..." -ForegroundColor Green
Write-Host "This serves both the API and the frontend." -ForegroundColor Cyan
Write-Host ""
Write-Host "Open http://localhost:8080 in your browser" -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop the server" -ForegroundColor Cyan
Write-Host ""

Push-Location backend
& ".\build\search_engine.exe"
Pop-Location
