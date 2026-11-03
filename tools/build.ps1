param([ValidateSet('Debug','Release')][string]$Config='Debug', [switch]$Clean)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$bundleRoot=Join-Path $env:LOCALAPPDATA 'stm32cube\bundles'
$gccBin=Join-Path $bundleRoot 'gnu-tools-for-stm32\14.3.1+st.2\bin'
$cmakeExe=Join-Path $bundleRoot 'cmake\4.4.0+st.1\bin\cmake.exe'
$ninjaBin=Join-Path $bundleRoot 'ninja\1.13.2+st.1\bin'
foreach($item in @($cmakeExe,(Join-Path $gccBin 'arm-none-eabi-gcc.exe'),(Join-Path $ninjaBin 'ninja.exe'))) {
    if (!(Test-Path -LiteralPath $item)) { throw "STM32 tool not found: $item" }
}
$env:Path="$gccBin;$ninjaBin;"+$env:Path
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force -Path 'build\verification' | Out-Null
    & $cmakeExe --preset $Config
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    $buildArgs=@('--build',"build/$Config")
    if ($Clean) { $buildArgs+='--clean-first' }
    & $cmakeExe @buildArgs 2>&1 | Tee-Object "build\verification\build_$Config.log"
    if ($LASTEXITCODE) { throw 'ARM build failed' }
    & (Join-Path $gccBin 'arm-none-eabi-size.exe') "build/$Config/ActiveVisionTracking.elf"
    Get-FileHash "build/$Config/ActiveVisionTracking.elf" -Algorithm SHA256
} finally { Pop-Location }
