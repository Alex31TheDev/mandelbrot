$files = git diff --name-only -- '*.h'
$stage = @()

foreach ($file in $files) {
    $patch = git diff --unified=0 -- $file
    $body = $patch | Where-Object { $_ -notmatch '^(diff --git|index |--- |\+\+\+ |@@ )' }
    $changes = $body | Where-Object { $_ -match '^[+-]' -and $_ -notmatch '^(---|\+\+\+)' }

    if ($changes.Count -ne 2) {
        continue
    }

    $minus = @($changes | Where-Object { $_.StartsWith('-') })
    $plus = @($changes | Where-Object { $_.StartsWith('+') })

    if ($minus.Count -ne 1 -or $plus.Count -ne 1) {
        continue
    }

    if ($minus[0].Substring(1) -cne $plus[0].Substring(1)) {
        continue
    }

    $stage += $file
}

if ($stage.Count -lt 1) {
    Write-Output "No EOF only header changes found."
    exit 0
}

git add -- $stage
Write-Output "Staged EOF only header changes:"
$stage
