[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackagePath,

    [string]$ExpectedIdentityName,

    [string]$ExpectedPublisher,

    [string]$ExpectedPublisherDisplayName
)

$ErrorActionPreference = 'Stop'

$packagePath = (Resolve-Path -LiteralPath $PackagePath).Path
Add-Type -AssemblyName System.IO.Compression

$archive = [System.IO.Compression.ZipFile]::OpenRead($packagePath)
try {
    $manifestEntry = $archive.GetEntry('AppxManifest.xml')
    if ($null -eq $manifestEntry) {
        throw "Package '$packagePath' does not contain AppxManifest.xml."
    }

    $reader = [System.IO.StreamReader]::new($manifestEntry.Open())
    try {
        [xml]$manifest = $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
    }

    $namespaceManager = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespaceManager.AddNamespace('appx', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $identity = $manifest.SelectSingleNode('/appx:Package/appx:Identity', $namespaceManager)
    $publisherDisplayName = $manifest.SelectSingleNode('/appx:Package/appx:Properties/appx:PublisherDisplayName', $namespaceManager)
    $resource = $manifest.SelectSingleNode('/appx:Package/appx:Resources/appx:Resource', $namespaceManager)
    if ($null -eq $identity) {
        throw "Package '$packagePath' does not contain an identity."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedIdentityName) -and
        -not [string]::Equals($identity.Name, $ExpectedIdentityName, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Package identity name '$($identity.Name)' does not match expected name '$ExpectedIdentityName'."
    }

    if ($null -eq $resource -or [string]::IsNullOrWhiteSpace($resource.Language) -or $resource.Language -eq 'x-generate') {
        throw "Package '$packagePath' has an invalid generated resource language."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisher) -and
        -not [string]::Equals($identity.Publisher, $ExpectedPublisher, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Package identity publisher '$($identity.Publisher)' does not match expected publisher '$ExpectedPublisher'."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisherDisplayName) -and
        ($null -eq $publisherDisplayName -or
        -not [string]::Equals($publisherDisplayName.InnerText, $ExpectedPublisherDisplayName, [System.StringComparison]::Ordinal))) {
        throw "Package publisher display name '$($publisherDisplayName.InnerText)' does not match expected publisher display name '$ExpectedPublisherDisplayName'."
    }
}
finally {
    $archive.Dispose()
}
