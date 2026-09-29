# SnapView Agent Guide

## Repository overview

SnapView is a native Windows desktop screenshot utility. The application is a
Unicode Win32 C++ project; its installer is an MSIX packaging project. The solution is
`SnapView.slnx`, with these projects:

- `SnapView\SnapView.vcxproj`: application (`Debug|x64`, `Release|x64`,
  `Debug|ARM64`, and `Release|ARM64`)
- `SnapViewPackage\SnapViewPackage.wapproj`: MSIX package, built as part of the
  solution

The native project uses the `v145` toolset and the Windows App SDK. Per-user
settings are stored in the MSIX package's `ApplicationData.LocalSettings`
store. Visual Studio debugging launches the `SnapViewPackage` project, which
builds, deploys, and starts SnapView with package identity.
The package project rebuilds SnapView before staging its payload so the
debugger never launches an outdated executable.

SnapView remains framework-dependent for the Windows App SDK. The package
project declares the framework package dependency. The native project disables
the unpackaged bootstrapper; Visual Studio package deployment and the MSIX
framework dependency provide the runtime for local debug and installed
launches while preserving its static C++ runtime configuration.

## Prerequisites

- Windows 10 version 1809 or later, or Windows 11
- Visual Studio with the **Desktop development with C++** workload, the
  `v145` toolset, a Windows 10/11 SDK version 10.0.26100.0 or later, and
  Visual Studio's MSIX packaging tools
- Visual Studio's MSIX packaging tools
- Network access on the first restore to download NuGet packages

Do not check in build output or Visual Studio user files; they are
intentionally ignored.

## Restore and build

Run the following from an **x64 Developer Command Prompt for Visual Studio** at
the repository root. `/restore` restores the Windows App SDK NuGet packages.

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
unsigned. The release workflow uploads the unsigned MSIX bundle to Microsoft
Store, which validates and signs it during submission. Unsigned release
bundles are not published as GitHub release assets.

## Microsoft Store publishing setup

Tag pushes matching `v*` or `V*` run the `Release` workflow. Its `publish` job
uses the `publish` GitHub environment and requires the following configuration:

The tag release workflow derives the MSIX package and bundle version from the
three-part numeric tag version and appends the GitHub Actions run number as the
fourth component (for example, `1.0.0-beta.2` on run `15` becomes `1.0.0.15`).
Local and CI package builds continue to use `0` as that fourth component.

| GitHub location | Name | Value and source |
| --- | --- | --- |
| `publish` environment secret | `AZURE_TENANT_ID` | Reuse the existing Microsoft Entra tenant ID. In the Microsoft Entra admin center, open **Identity** > **Overview** and copy **Tenant ID**. |
| `publish` environment secret | `AZURE_CLIENT_ID` | Reuse the existing application (client) ID for the Entra app registered in Partner Center. In the Entra admin center, open **App registrations**, select the app, and copy **Application (client) ID**. |
| Repository variable | `MS_STORE_SELLER_ID` | Partner Center **Account settings** > **Organization profile** > **Legal info** > **Seller ID**. |
| Repository variable | `MS_STORE_PRODUCT_ID` | The SnapView Store product ID from Partner Center. |
| Repository variable | `MS_STORE_FLIGHT_ID` | The ID of the existing test flight that receives tagged builds. After authenticating the Store CLI, run `msstore flights list <product-id>` to list its IDs. |
| Repository variable | `MS_STORE_PACKAGE_IDENTITY_NAME` | The package identity name reserved for SnapView in Partner Center, from the product's package identity details. |
| Repository variable | `MS_STORE_PACKAGE_PUBLISHER` | The publisher distinguished name reserved for SnapView in Partner Center, from the product's package identity details. It must match the publisher in the MSIX manifest submitted to Store. |
| Repository variable | `MS_STORE_PACKAGE_PUBLISHERDISPLAYNAME` | The human-readable publisher name from Partner Center. It is written to the MSIX manifest's `PublisherDisplayName` property. |

Configure a Microsoft Entra federated credential on the existing application
registration for GitHub Actions. Select **Certificates & secrets** >
**Federated credentials**, add a GitHub Actions credential, and limit it to:

```text
Organization: brantburnett
Repository: SnapView
Entity type: Environment
Environment: publish
```

This produces the required subject
`repo:brantburnett/SnapView:environment:publish`, allowing only jobs that pass
the `publish` environment's rules to exchange their short-lived GitHub OIDC
token. In Partner Center **Account settings** > **User management** >
**Microsoft Entra applications**, add this same Entra application and assign
the **Manager** role. The Store product and its target test flight must already
exist before the first tagged release.

MSIX provides the Start menu entry; it intentionally does not replace the
removed WiX desktop or startup shortcut selections.

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
  C++/WinRT headers should include `stdafx.h` first.
- Preserve the project runtime-library selection: `/MTd` for Debug and `/MT`
  for Release.
- Update `SnapViewPackage\Package.appxmanifest` and
  `SnapViewPackage\SnapViewPackage.wapproj` when package identity, installable
  files, or installer behavior changes. Do not hand-edit generated build
  output.
- Use Unicode Win32 APIs and project conventions (`TCHAR`, `wstring`, and
  resource identifiers) when modifying existing UI code.
