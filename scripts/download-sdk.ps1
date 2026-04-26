# Downloads the latest ReXGlue SDK nightly into sdk\win-amd64\ and updates .sdk-version
param(
    [string]$OutDir = "sdk"
)

$ErrorActionPreference = "Stop"

$Repo = "rexglue/rexglue-sdk"
$Platform = "win-amd64"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VersionFile = Join-Path $ScriptDir "..\.sdk-version"

Write-Host "Fetching latest nightly release for $Platform..."
$releases = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases?per_page=20"
$nightly = $releases | Where-Object { $_.tag_name -like "nightly-*" } | Select-Object -First 1
$asset = $nightly.assets | Where-Object { $_.name -like "*$Platform*" } | Select-Object -First 1

if (-not $asset) {
    Write-Error "No asset found for $Platform in release $($nightly.tag_name)"
    exit 1
}

$dest = Join-Path $OutDir $Platform
$InstalledVersionFile = Join-Path $dest ".sdk-version"

if ((Test-Path $InstalledVersionFile) -and ((Get-Content $InstalledVersionFile -Raw).Trim() -eq $nightly.tag_name)) {
    Write-Host "SDK already at $($nightly.tag_name). Skipping download."
} else {
    $TmpZip = Join-Path $env:TEMP "rexglue-sdk.zip"
    $TmpDir = Join-Path $env:TEMP "rexglue-sdk-tmp"

    Write-Host "Downloading $($nightly.tag_name)..."
    Invoke-WebRequest $asset.browser_download_url -OutFile $TmpZip

    Write-Host "Extracting..."
    if (Test-Path $TmpDir) { Remove-Item $TmpDir -Recurse -Force }
    Expand-Archive $TmpZip -DestinationPath $TmpDir

    $extracted = Get-ChildItem $TmpDir | Select-Object -First 1
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    Move-Item $extracted.FullName $dest
    Remove-Item $TmpZip -Force

    $nightly.tag_name | Set-Content $InstalledVersionFile
    Write-Host "SDK installed to $dest ($($nightly.tag_name))"
}

$nightly.tag_name | Set-Content $VersionFile
Write-Host "Pinned version updated to $($nightly.tag_name)"
