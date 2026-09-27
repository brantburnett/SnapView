[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$SourcePath,

    [Parameter(Mandatory)]
    [string]$DestinationPath,

    [Parameter(Mandatory)]
    [string]$Name,

    [Parameter(Mandatory)]
    [string]$Publisher,

    [Parameter(Mandatory)]
    [string]$PublisherDisplayName,

    [Parameter(Mandatory)]
    [string]$Version,

    [Parameter(Mandatory)]
    [string]$Architecture,

    [switch]$RemovePackageDependencies
)

$ErrorActionPreference = 'Stop'

if ($Version -notmatch '^\d{1,5}\.\d{1,5}\.\d{1,5}\.\d{1,5}$') {
    throw "MSIX version '$Version' must contain four numeric components."
}

[xml]$manifest = Get-Content -LiteralPath $SourcePath
$namespaceManager = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
$namespaceManager.AddNamespace('appx', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
$identity = $manifest.SelectSingleNode('/appx:Package/appx:Identity', $namespaceManager)
$properties = $manifest.SelectSingleNode('/appx:Package/appx:Properties', $namespaceManager)

if ($null -eq $identity) {
    throw "No package identity was found in '$SourcePath'."
}

if ($null -eq $properties) {
    throw "No package properties were found in '$SourcePath'."
}

$publisherDisplayNameElement = $properties.SelectSingleNode('appx:PublisherDisplayName', $namespaceManager)
if ($null -eq $publisherDisplayNameElement) {
    throw "No package publisher display name was found in '$SourcePath'."
}

$identity.SetAttribute('Name', $Name)
$identity.SetAttribute('Publisher', $Publisher)
$identity.SetAttribute('Version', $Version)
$publisherDisplayNameElement.InnerText = $PublisherDisplayName
$architecture = $Architecture.ToLowerInvariant()
if ($architecture -notin @('arm64', 'x64')) {
    throw "MSIX architecture '$Architecture' is not supported."
}

$identity.SetAttribute('ProcessorArchitecture', $architecture)

if ($RemovePackageDependencies) {
    $dependencies = $manifest.SelectSingleNode('/appx:Package/appx:Dependencies', $namespaceManager)
    if ($null -ne $dependencies) {
        @($dependencies.SelectNodes('appx:PackageDependency', $namespaceManager)) |
            ForEach-Object { [void]$dependencies.RemoveChild($_) }
    }
}

$destinationDirectory = Split-Path -Parent $DestinationPath
New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null

$settings = [System.Xml.XmlWriterSettings]::new()
$settings.Encoding = [System.Text.UTF8Encoding]::new($false)
$settings.Indent = $true
$settings.NewLineChars = "`r`n"

$writer = [System.Xml.XmlWriter]::Create($DestinationPath, $settings)
try {
    $manifest.Save($writer)
}
finally {
    $writer.Dispose()
}
