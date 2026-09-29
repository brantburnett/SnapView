[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackageDirectory,

    [Parameter(Mandatory)]
    [string]$OutputPath,

    [Parameter(Mandatory)]
    [string]$Version
)

$ErrorActionPreference = 'Stop'

$versionMatch = [System.Text.RegularExpressions.Regex]::Match(
    $Version,
    '^(?<core>\d+\.\d+\.\d+)(?:\.(?<revision>\d+))?(?:[-+].*)?$')
if (-not $versionMatch.Success) {
    throw "Version '$Version' must contain a three-part numeric version with an optional numeric fourth component."
}

$bundleVersion = if ($versionMatch.Groups['revision'].Success) {
    $versionMatch.Value -replace '(?:[-+].*)$', ''
}
else {
    "$($versionMatch.Groups['core'].Value).0"
}
$packageDirectory = (Resolve-Path -LiteralPath $PackageDirectory).Path
$packages = @(Get-ChildItem -LiteralPath $packageDirectory -Recurse -File -Filter '*.msix')

if ($packages.Count -ne 2) {
    throw "Expected exactly two architecture-specific MSIX packages under '$packageDirectory', but found $($packages.Count)."
}

$expectedArchitectures = @('arm64', 'x64')
foreach ($architecture in $expectedArchitectures) {
    if (-not ($packages.BaseName | Where-Object { $_ -match "(?i)(?:_|-)$architecture(?:_|-|$)" })) {
        throw "No $architecture MSIX package was found under '$packageDirectory'."
    }
}

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$bundleInputDirectory = Join-Path ([System.IO.Path]::GetTempPath()) "SnapViewBundle-$([System.Guid]::NewGuid())"
New-Item -ItemType Directory -Path $bundleInputDirectory -Force | Out-Null

try {
    foreach ($package in $packages) {
        Copy-Item -LiteralPath $package.FullName -Destination $bundleInputDirectory -Force
    }

    $makeAppx = Get-Command MakeAppx.exe -ErrorAction SilentlyContinue
    if ($null -eq $makeAppx) {
        $sdkBinDirectory = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
        $makeAppx = Get-ChildItem -LiteralPath $sdkBinDirectory -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName 'x64\MakeAppx.exe' } |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
    }
    else {
        $makeAppx = $makeAppx.Source
    }

    if ([string]::IsNullOrEmpty($makeAppx)) {
        throw 'MakeAppx.exe was not found. Install the Windows SDK MSIX packaging tools.'
    }

    & $makeAppx bundle /d $bundleInputDirectory /p $OutputPath /bv $bundleVersion /o
    if ($LASTEXITCODE -ne 0) {
        throw "MakeAppx.exe failed while creating '$OutputPath'."
    }
}
finally {
    Remove-Item -LiteralPath $bundleInputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
