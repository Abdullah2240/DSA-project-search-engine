@echo off
echo ================================
echo DSA Search Engine Launcher
echo ================================
echo.

REM Check if backend executable exists
if not exist "backend\build\search_engine.exe" (
    echo ERROR: Backend executable not found!
    echo Building backend...
    cd backend
    cmake -S . -B build
    cmake --build build
    cd ..
    
    if not exist "backend\build\search_engine.exe" (
        echo Backend build failed!
        pause
        exit /b 1
    )
)

REM Check if frontend dependencies are installed
if not exist "frontend\node_modules" (
    echo Installing frontend dependencies...
    cd frontend
    call npm install
    if errorlevel 1 (
        echo npm install failed!
        cd ..
        pause
        exit /b 1
    )
    cd ..
    echo.
)

REM Build frontend
echo Building frontend...
cd frontend
call npm run build
if errorlevel 1 (
    echo Frontend build failed!
    cd ..
    pause
    exit /b 1
)
cd ..

echo Frontend built successfully to backend/static/
echo.

REM Start backend
echo Starting backend server...
echo This serves both the API and the frontend.
echo.
echo Open http://localhost:8080 in your browser
echo Press Ctrl+C to stop the server
echo.

cd backend
.\build\search_engine.exe
cd ..
