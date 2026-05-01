param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)),
    [string]$QtToolsBin = "C:\Qt\6.11.0\msvc2022_64\bin"
)

$ErrorActionPreference = "Stop"

$lupdate = Join-Path $QtToolsBin "lupdate.exe"
$translationsRoot = Join-Path $RepoRoot "mandelbrot-gui\translations"
$listFile = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "translations.lst"

if (-not (Test-Path -LiteralPath $lupdate)) {
    Write-Error "lupdate not found: $lupdate"
}

if (-not (Test-Path -LiteralPath $translationsRoot)) {
    Write-Error "Translations root not found: $translationsRoot"
}

$translations = Get-ChildItem -Path $translationsRoot -Filter *.ts -File
if (-not $translations) {
    Write-Error "No translation files found in: $translationsRoot"
}

$sourceRoots = @(
    "mandelbrot-gui\app"
    "mandelbrot-gui\controllers"
    "mandelbrot-gui\dialogs"
    "mandelbrot-gui\locale"
    "mandelbrot-gui\runtime"
    "mandelbrot-gui\services"
    "mandelbrot-gui\settings"
    "mandelbrot-gui\util"
    "mandelbrot-gui\widgets"
    "mandelbrot-gui\windows"
)

Push-Location $RepoRoot
try {
    $files = & rg --files @sourceRoots -g *.cpp -g *.h -g *.ui | Sort-Object -Unique
    if (-not $files) {
        Write-Error "No GUI source files found"
    }

    [System.IO.File]::WriteAllLines($listFile, $files, [System.Text.UTF8Encoding]::new($false))

    & $lupdate -no-obsolete -locations relative "@$listFile" -ts $translations.FullName
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    if (Test-Path -LiteralPath $listFile) {
        Remove-Item $listFile
    }

    Pop-Location
}
