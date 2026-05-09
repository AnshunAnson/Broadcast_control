$ErrorActionPreference = "Stop"
$Host.UI.RawUI.WindowTitle = "播控中台 - 一键启动"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FrontendDir = Join-Path $ScriptDir "webview_ui"
$BackendDir = Join-Path $ScriptDir "broadcast_control"

$BackendPort = 8000
$FrontendPort = 5173
$BackendUrl = "http://127.0.0.1:${BackendPort}"
$FrontendUrl = "http://localhost:${FrontendPort}"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  虚拟制片播控中台 - 一键启动" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ---------- 检查依赖 ----------
Write-Host "[1/4] 检查依赖..." -ForegroundColor Yellow

$pythonCmd = $null
if (Get-Command python -ErrorAction SilentlyContinue) {
    $pythonCmd = "python"
} elseif (Get-Command python3 -ErrorAction SilentlyContinue) {
    $pythonCmd = "python3"
} else {
    Write-Host "  [ERROR] 未找到 Python，请先安装 Python 3.10+" -ForegroundColor Red
    Read-Host "按 Enter 退出"
    exit 1
}
Write-Host "  Python: $(& $pythonCmd --version)" -ForegroundColor Green

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    Write-Host "  [ERROR] 未找到 Node.js，请先安装 Node.js" -ForegroundColor Red
    Read-Host "按 Enter 退出"
    exit 1
}
Write-Host "  Node.js: $(& node --version)" -ForegroundColor Green

# 检查前端 node_modules
if (-not (Test-Path (Join-Path $FrontendDir "node_modules"))) {
    Write-Host "  [WARN] 前端依赖未安装，正在安装..." -ForegroundColor Yellow
    Push-Location $FrontendDir
    npm install
    Pop-Location
    Write-Host "  前端依赖安装完成" -ForegroundColor Green
}

# ---------- 清理旧进程 ----------
Write-Host ""
Write-Host "[2/4] 清理已有进程..." -ForegroundColor Yellow

$oldBackend = Get-Process -Name "python" -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -match "broadcast_control" }
$oldFrontend = Get-Process -Name "node" -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -match "vite" }

if ($oldBackend) {
    Write-Host "  终止旧后端进程..." -ForegroundColor Gray
    $oldBackend | Stop-Process -Force -ErrorAction SilentlyContinue
}
if ($oldFrontend) {
    Write-Host "  终止旧前端进程..." -ForegroundColor Gray
    $oldFrontend | Stop-Process -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 1
Write-Host "  清理完成" -ForegroundColor Green

# ---------- 启动后端 ----------
Write-Host ""
Write-Host "[3/4] 启动后端服务 (FastAPI)..." -ForegroundColor Yellow

$backendProcess = Start-Process -FilePath $pythonCmd `
    -ArgumentList "-m", "broadcast_control.main" `
    -WorkingDirectory $ScriptDir `
    -WindowStyle Minimized `
    -PassThru

Write-Host "  后端 PID: $($backendProcess.Id), 端口: $BackendPort" -ForegroundColor Green

# 等待后端就绪
Write-Host "  等待后端就绪..." -ForegroundColor Gray
$retryCount = 0
$maxRetries = 30
do {
    Start-Sleep -Seconds 1
    $retryCount++
    try {
        $response = Invoke-WebRequest -Uri "$BackendUrl/health" -UseBasicParsing -TimeoutSec 2 -ErrorAction SilentlyContinue
        if ($response.StatusCode -eq 200) {
            break
        }
    } catch {
        # 继续等待
    }
} while ($retryCount -lt $maxRetries)

if ($retryCount -ge $maxRetries) {
    Write-Host "  [WARN] 后端启动超时，继续启动前端..." -ForegroundColor Yellow
} else {
    Write-Host "  后端已就绪!" -ForegroundColor Green
}

# ---------- 启动前端 ----------
Write-Host ""
Write-Host "[4/4] 启动前端服务 (Vite + React)..." -ForegroundColor Yellow

$vitePath = Join-Path $FrontendDir "node_modules\.bin\vite.cmd"
if (-not (Test-Path $vitePath)) {
    $vitePath = "npx"
    $viteArgs = @("vite", "--host")
} else {
    $viteArgs = @("--host")
}

$frontendProcess = Start-Process -FilePath $vitePath `
    -ArgumentList $viteArgs `
    -WorkingDirectory $FrontendDir `
    -WindowStyle Minimized `
    -PassThru

Write-Host "  前端 PID: $($frontendProcess.Id), 端口: $FrontendPort" -ForegroundColor Green

# 等待前端就绪
Write-Host "  等待前端就绪..." -ForegroundColor Gray
$retryCount = 0
do {
    Start-Sleep -Seconds 1
    $retryCount++
    try {
        $response = Invoke-WebRequest -Uri $FrontendUrl -UseBasicParsing -TimeoutSec 2 -ErrorAction SilentlyContinue
        if ($response.StatusCode -eq 200) {
            break
        }
    } catch {
        # 继续等待
    }
} while ($retryCount -lt $maxRetries)

if ($retryCount -ge $maxRetries) {
    Write-Host "  [WARN] 前端启动超时" -ForegroundColor Yellow
} else {
    Write-Host "  前端已就绪!" -ForegroundColor Green
}

# ---------- 打开浏览器 ----------
Start-Process $FrontendUrl

# ---------- 汇总信息 ----------
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  全部服务已启动!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  后端 API:     $BackendUrl" -ForegroundColor White
Write-Host "  健康检查:     $BackendUrl/health" -ForegroundColor White
Write-Host "  API 文档:     $BackendUrl/docs" -ForegroundColor White
Write-Host "  前端页面:     $FrontendUrl" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "按 Ctrl+C 停止所有服务" -ForegroundColor Yellow
Write-Host ""

# ---------- 注册清理 ----------
$cleanup = {
    Write-Host ""
    Write-Host "正在停止服务..." -ForegroundColor Yellow
    if ($args[0] -and -not $args[0].HasExited) {
        Stop-Process -Id $args[0].Id -Force -ErrorAction SilentlyContinue
        Write-Host "  后端已停止" -ForegroundColor Gray
    }
    if ($args[1] -and -not $args[1].HasExited) {
        Stop-Process -Id $args[1].Id -Force -ErrorAction SilentlyContinue
        Write-Host "  前端已停止" -ForegroundColor Gray
    }
    Write-Host "所有服务已停止" -ForegroundColor Green
}

try {
    # 持续等待用户按 Ctrl+C
    while ($true) {
        if ($backendProcess.HasExited) {
            Write-Host "[WARN] 后端进程意外退出" -ForegroundColor Red
            break
        }
        if ($frontendProcess.HasExited) {
            Write-Host "[WARN] 前端进程意外退出" -ForegroundColor Red
            break
        }
        Start-Sleep -Seconds 2
    }
} finally {
    & $cleanup $backendProcess $frontendProcess
    Read-Host "按 Enter 退出"
}