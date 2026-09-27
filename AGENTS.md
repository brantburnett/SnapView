# SnapView Agent Guide

## Repository overview

SnapView is a native Windows desktop screenshot utility. The application is a
Unicode Win32 C++ project; its installer is an MSIX packaging project. The solution is
`SnapView.slnx`, with these projects:

- `SnapView\SnapView.vcxproj`: application (`Debug|x64`, `Release|x64`,
  `Debug|ARM64`, and `Release|ARM64`)
- `SnapViewPackage\SnapViewPackage.wapproj`: MSIX package, built as part of the
  solution

The native project uses the `v145` toolset, the Windows App SDK, and
manifest-mode vcpkg. Its only vcpkg dependency is Xerces-C, installed using
the `x64-windows-static` or `arm64-windows-static` triplet in
`vcpkg_installed\`.

SnapView remains framework-dependent for the Windows App SDK. The SDK
bootstrapper initializes before the application's entry point. If the matching
runtime is missing, Windows displays acquisition UI; the MSIX package does not
bundle or silently install the runtime.

## Prerequisites

- Windows 10 version 1809 or later, or Windows 11
- Visual Studio with the **Desktop development with C++** workload, the
  `v145` toolset, a Windows 10/11 SDK version 10.0.26100.0 or later, and
  Visual Studio's MSIX packaging tools
- vcpkg; use the copy supplied with Visual Studio or a separately bootstrapped
  vcpkg executable
- Visual Studio's MSIX packaging tools
- Network access on the first restore to download vcpkg and NuGet packages

Do not check in `vcpkg_installed\`, build output, or Visual Studio user files;
they are intentionally ignored.

## Restore and build

Run the following from an **x64 Developer Command Prompt for Visual Studio** at
the repository root. Restore the vcpkg manifest before building; it creates the
project-local `vcpkg_installed\` directory using the pinned baseline in
`vcpkg-configuration.json`.

```bat
vcpkg install --triplet x64-windows-static
```

For ARM64 builds, install the ARM64 triplet instead:

```bat
vcpkg install --triplet arm64-windows-static
```

If `vcpkg` is not on `PATH`, Visual Studio installs it at a path similar to
`C:\Program Files\Microsoft Visual Studio\<version>\<edition>\VC\vcpkg\vcpkg.exe`.
Then build the solution. `/restore` restores the Windows App SDK NuGet
packages.

```bat
msbuild SnapView.slnx /restore /m /p:Configuration=Debug /p:Platform=x64
```

For a release build:

```bat
msbuild SnapView.slnx /restore /m /p:Configuration=Release /p:Platform=x64
```

For an ARM64 release build:

```bat
msbuild SnapView.slnx /restore /m /p:Configuration=Release /p:Platform=ARM64
```

To build only the executable, which avoids packaging MSIX:

```bat
msbuild SnapView\SnapView.vcxproj /restore /m /p:Configuration=Debug /p:Platform=x64
```

The application is emitted to
`artifacts\bin\SnapView\<configuration>-<architecture>\SnapView.exe` (for
example, `artifacts\bin\SnapView\release-arm64\SnapView.exe`). The solution
build also produces an architecture-specific MSIX under
`artifacts\publish\<configuration>\`. Build both architectures, then create a
bundle with:

```bat
powershell -File tools\New-MsixBundle.ps1 ^
  -PackageDirectory artifacts\publish\release ^
  -Version <version> ^
  -OutputPath artifacts\publish\release\SnapView-<version>.msixbundle
```

Intermediate files are stored under
`artifacts\obj\<project>\<configuration>-<architecture>\`. All configuration
and architecture path components are lowercase. Local package builds are
unsigned. CI signs the executable, each MSIX, and the final bundle only in the
`artifact-signing` environment. Before archiving signed MSIX packages, CI
validates the final archive manifest's resource language and verifies that the
package identity publisher matches the signing certificate subject.

The `artifact-signing` GitHub environment requires the existing Azure
federated-credential secrets `AZURE_CLIENT_ID`, `AZURE_TENANT_ID`, and
`AZURE_SUBSCRIPTION_ID`, and the existing
`AZURE_ARTIFACT_SIGNING_ENDPOINT`,
`AZURE_ARTIFACT_SIGNING_ACCOUNT_NAME`, and
`AZURE_ARTIFACT_SIGNING_CERTIFICATE_PROFILE_NAME` variables. It additionally
requires `MSIX_PACKAGE_PUBLISHER`, containing the exact subject distinguished
name of the Azure Trusted Signing certificate profile. Obtain the exact value
from a previously signed executable with:

```powershell
(Get-AuthenticodeSignature .\SnapView.exe).SignerCertificate.Subject
```

The MSIX manifest publisher must exactly match that subject. MSIX provides the
Start menu entry; it intentionally does not replace the removed WiX desktop or
startup shortcut selections.

There is no automated test project or test runner in this repository. Validate
native changes by building the affected configuration, and manually exercise
the Windows UI when changes affect capture behavior, options, hotkeys, email,
or installer content.

## Implementation guidance

- Keep the application x64 and ARM64 settings aligned when updating project,
  dependency, or installer behavior.
- Add C++ source, headers, resources, and images to
  `SnapView\SnapView.vcxproj` and keep `SnapView\SnapView.vcxproj.filters` in
  sync for Visual Studio users.
- `stdafx.cpp` creates the precompiled header. Files using shared Windows or
  Xerces headers should include `stdafx.h` first.
- Preserve the project runtime-library selection: `/MTd` for Debug and `/MT`
  for Release. New native dependencies must be compatible with the static
  vcpkg triplet.
- Update `SnapViewPackage\Package.appxmanifest` and
  `SnapViewPackage\SnapViewPackage.wapproj` when package identity, installable
  files, or installer behavior changes. Do not hand-edit generated build
  output.
- Use Unicode Win32 APIs and project conventions (`TCHAR`, `wstring`, and
  resource identifiers) when modifying existing UI code.
