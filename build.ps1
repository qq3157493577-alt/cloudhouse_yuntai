param(
    [ValidateSet('debug', 'release')]
    [string]$Preset = 'debug'
)

$ErrorActionPreference = 'Stop'
$previousPath = $env:PATH

function Find-BuildTool([string]$Name, [string]$Bundle) {
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    # STM32 VS Code extensions install tools here without adding PATH.
    if ($env:LOCALAPPDATA) {
        $bundleRoot = Join-Path $env:LOCALAPPDATA "stm32cube/bundles/$Bundle"
        $candidates = Get-ChildItem -LiteralPath $bundleRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object { try { [version](($_.Name -split '\+')[0]) } catch { [version]'0.0' } } -Descending
        foreach ($candidate in $candidates) {
            $executable = Join-Path $candidate.FullName "bin/$Name.exe"
            if (Test-Path -LiteralPath $executable -PathType Leaf) { return $executable }
        }
    }
    throw "Cannot find $Name. Install $Bundle via STM32 tools, or add its bin directory to PATH."
}

try {
    $cmake = Find-BuildTool 'cmake' 'cmake'
    $ninja = Find-BuildTool 'ninja' 'ninja'
    $gcc = Find-BuildTool 'arm-none-eabi-gcc' 'gnu-tools-for-stm32'
    $toolDirectories = @($cmake, $ninja, $gcc) | ForEach-Object { Split-Path -Parent $_ }
    $env:PATH = ($toolDirectories -join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $previousPath
    Write-Host "CMake: $cmake"
    Write-Host "Ninja: $ninja"
    Write-Host "ARM GCC: $gcc"
    Push-Location -LiteralPath $PSScriptRoot
    try {
        & $cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)." }
        & $cmake --build --preset $Preset --parallel
        if ($LASTEXITCODE -ne 0) { throw "CMake build failed ($LASTEXITCODE)." }
    } finally { Pop-Location }
} finally { $env:PATH = $previousPath }
