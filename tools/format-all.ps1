param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)),
    [string]$SourceRoot = "mandelbrot/src"
)

$ErrorActionPreference = "Stop"

$excludeFile = Join-Path $RepoRoot ".format-exclude"
$sourceRoot = Join-Path $RepoRoot $SourceRoot

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    Write-Error "clang-format not found"
}

$exclude = @()
if (Test-Path -LiteralPath $excludeFile) {
    $exclude = Get-Content $excludeFile | ForEach-Object { $_.Trim().Replace('\', '/') } |
    Where-Object { $_ -and -not $_.StartsWith("#") }
}

$extensions = @(".cpp", ".h", ".hpp", ".hh", ".cxx", ".cc", ".inl")

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    Write-Error "Source root not found: $sourceRoot"
}

$files = Get-ChildItem -Path $sourceRoot -Recurse -File | Where-Object {
    $extensions -contains $_.Extension
} | ForEach-Object {
    $relativePath = $_.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
    [PSCustomObject]@{
        FullName = $_.FullName
        Relative = $relativePath.Replace('\', '/')
    }
}

foreach ($file in $files) {
    if ($exclude -contains $file.Relative) {
        continue
    }

    & clang-format -i -style=file $file.FullName
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Write-Host "Formatted: $($file.Relative)"
}
