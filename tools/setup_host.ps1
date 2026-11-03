param([string]$Python='python')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    & $Python -m venv .venv
    if ($LASTEXITCODE) { throw 'Cannot create .venv. Pass -Python with a working Python 3.11+ executable.' }
    & '.\.venv\Scripts\python.exe' -m pip install -r host/requirements.txt -r tests/requirements.txt
    if ($LASTEXITCODE) { throw 'Python dependency installation failed' }
    & '.\.venv\Scripts\python.exe' -c "from host.transport import build_native; print(build_native())"
    if ($LASTEXITCODE) { throw 'Native simulator build failed. Ensure gcc (for Windows, e.g. Strawberry C) is on PATH.' }
} finally { Pop-Location }
