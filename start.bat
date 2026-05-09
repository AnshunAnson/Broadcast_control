@echo off
chcp 65001 >nul
title 播控中台 - 一键启动

set "SCRIPT_DIR=%~dp0"
set "FRONTEND_DIR=%SCRIPT_DIR%webview_ui"
set "BACKEND_PORT=8000"
set "FRONTEND_PORT=5173"
set "BACKEND_URL=http://127.0.0.1:%BACKEND_PORT%"
set "FRONTEND_URL=http://localhost:%FRONTEND_PORT%"

echo ========================================
echo   虚拟制片播控中台 - 一键启动
echo ========================================
echo.

REM ---------- 检查依赖 ----------
echo [1/4] 检查依赖...

where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo   [ERROR] 未找到 Python，请先安装 Python 3.10+
    pause
    exit /b 1
)
for /f "tokens=*" %%i in ('python --version 2^>^&1') do echo   Python: %%i

where node >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo   [ERROR] 未找到 Node.js，请先安装 Node.js
    pause
    exit /b 1
)
for /f "tokens=*" %%i in ('node --version 2^>^&1') do echo   Node.js: %%i

if not exist "%FRONTEND_DIR%\node_modules" (
    echo   [WARN] 前端依赖未安装，正在安装...
    cd /d "%FRONTEND_DIR%"
    call npm install
    cd /d "%SCRIPT_DIR%"
    echo   前端依赖安装完成
)

REM ---------- 启动后端 ----------
echo.
echo [2/4] 启动后端服务 (FastAPI)...

start "播控后端 - FastAPI" /min cmd /c "cd /d "%SCRIPT_DIR%" && uvicorn broadcast_control.main:app --host 0.0.0.0 --port %BACKEND_PORT% --log-level info"
echo   后端已启动，端口: %BACKEND_PORT%

REM 等待后端就绪
echo   等待后端就绪...
set RETRY=0
:wait_backend
timeout /t 2 /nobreak >nul
set /a RETRY+=1
powershell -Command "try { (Invoke-WebRequest '%BACKEND_URL%/health' -UseBasicParsing -TimeoutSec 2).StatusCode } catch { exit 1 }" >nul 2>&1
if %ERRORLEVEL% EQU 0 goto backend_ready
if %RETRY% LSS 15 goto wait_backend
echo   [WARN] 后端启动超时，继续...
goto frontend_start
:backend_ready
echo   后端已就绪!

:frontend_start
REM ---------- 启动前端 ----------
echo.
echo [3/4] 启动前端服务 (Vite + React)...

start "播控前端 - Vite" /min cmd /c "cd /d "%FRONTEND_DIR%" && npx vite --host"
echo   前端已启动，端口: %FRONTEND_PORT%

REM 等待前端就绪
echo   等待前端就绪...
set RETRY=0
:wait_frontend
timeout /t 2 /nobreak >nul
set /a RETRY+=1
powershell -Command "try { (Invoke-WebRequest '%FRONTEND_URL%' -UseBasicParsing -TimeoutSec 2).StatusCode } catch { exit 1 }" >nul 2>&1
if %ERRORLEVEL% EQU 0 goto frontend_ready
if %RETRY% LSS 15 goto wait_frontend
echo   [WARN] 前端启动超时
goto summary
:frontend_ready
echo   前端已就绪!

:summary
REM ---------- 打开浏览器 ----------
echo.
echo [4/4] 打开浏览器...
start "" "%FRONTEND_URL%"

REM ---------- 汇总信息 ----------
echo.
echo ========================================
echo   全部服务已启动!
echo ========================================
echo   后端 API:     %BACKEND_URL%
echo   健康检查:     %BACKEND_URL%/health
echo   API 文档:     %BACKEND_URL%/docs
echo   前端页面:     %FRONTEND_URL%
echo ========================================
echo.
echo 关闭本窗口不会停止服务，请手动关闭后端和前端命令行窗口。
echo.
pause