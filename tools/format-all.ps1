$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $scriptDir
$excludeFile = Join-Path $root ".format-exclude"

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    Write-Error "clang-format not found"
}

$excludeSet = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)

if (Test-Path $excludeFile) {
    Get-Content $excludeFile | ForEach-Object {
        $line = $_.Trim()

        if ($line.Length -eq 0 -or $line.StartsWith("#")) {
            return
        }

        $normalized = $line.Replace('\', '/')
        [void]$excludeSet.Add($normalized)
    }
}

$sourceRoot = Join-Path $root "mandelbrot/src"
$extensions = @(".cpp", ".h", ".hpp", ".hh", ".cxx", ".cc", ".inl")

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    Write-Error "Source root not found: $sourceRoot"
}

$files = Get-ChildItem -Path $sourceRoot -Recurse -File | Where-Object {
    $extensions -contains $_.Extension
} | ForEach-Object {
    $relativePath = [System.IO.Path]::GetRelativePath($sourceRoot, $_.FullName)
    [PSCustomObject]@{
        FullName = $_.FullName
        Relative = $relativePath.Replace('\', '/')
    }
}

foreach ($file in $files) {
    if ($excludeSet.Contains($file.Relative)) {
        continue
    }

    & clang-format -i -style=file $file.FullName
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Write-Host "Formatted: $($file.Relative)"
}
