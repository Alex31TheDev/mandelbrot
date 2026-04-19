param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string[]]$RuntimeNames = @(
        "libmmd.dll",
        "svml_dispmd.dll"
    )
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$compilerRoots = @()
if ($env:CMPLR_ROOT) {
    $compilerRoots += $env:CMPLR_ROOT
}
if ($env:ONEAPI_ROOT) {
    $compilerRoots += (Join-Path $env:ONEAPI_ROOT "compiler")
}
if (${env:ProgramFiles(x86)}) {
    $compilerRoots += (Join-Path ${env:ProgramFiles(x86)} "Intel\\oneAPI\\compiler")
}
if ($env:ProgramFiles) {
    $compilerRoots += (Join-Path $env:ProgramFiles "Intel\\oneAPI\\compiler")
}

$copied = New-Object System.Collections.Generic.List[string]
$missing = New-Object System.Collections.Generic.List[string]

foreach ($runtimeName in $RuntimeNames) {
    $sourcePath = $null

    $whereResults = @()
    try {
        $whereResults = @(where.exe $runtimeName 2>$null)
    } catch {
        $whereResults = @()
    }

    foreach ($candidate in $whereResults) {
        if (Test-Path -LiteralPath $candidate) {
            $sourcePath = $candidate
            break
        }
    }

    if (-not $sourcePath) {
        foreach ($root in $compilerRoots) {
            if (-not $root -or -not (Test-Path -LiteralPath $root)) {
                continue
            }

            $candidate = Get-ChildItem -Path (Join-Path $root "*\\bin\\$runtimeName") -File -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending |
                Select-Object -First 1
            if ($candidate) {
                $sourcePath = $candidate.FullName
                break
            }
        }
    }

    if ($sourcePath) {
        $destinationPath = Join-Path $OutputDir $runtimeName
        Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
        $copied.Add($runtimeName) | Out-Null
        Write-Host "Copied Intel runtime $runtimeName from $sourcePath"
    } else {
        $missing.Add($runtimeName) | Out-Null
        Write-Warning "Could not locate Intel runtime $runtimeName."
    }
}

if ($copied.Count -gt 0) {
    Write-Host "Intel runtime copy complete. Copied: $($copied -join ', ')"
}
if ($missing.Count -gt 0) {
    Write-Warning "Intel runtime copy incomplete. Missing: $($missing -join ', ')"
}
