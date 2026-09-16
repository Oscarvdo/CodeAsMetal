$ErrorActionPreference = "Stop"

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )

    & $Command

    if ($LASTEXITCODE -ne 0) {
        throw "Native command failed ($LASTEXITCODE)."
    }
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$VcpkgRoot   = "C:\dev\vcpkg-codeasmetal"
$VcpkgRef    = "2025.12.12"

Write-Host "CodeAsMetal dependency preparation"
Write-Host "Project root: $ProjectRoot"
Write-Host "vcpkg root:   $VcpkgRoot"
Write-Host "vcpkg ref:    $VcpkgRef"
Write-Host ""

# ------------------------------------------------------------
# 1. Prepare vcpkg checkout
# ------------------------------------------------------------

if (-not (Test-Path $VcpkgRoot)) {
    Write-Host "Cloning vcpkg..."

    Invoke-Native {
        git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    }
}
elseif (-not (Test-Path (Join-Path $VcpkgRoot ".git"))) {
    throw "$VcpkgRoot exists but is not a Git checkout of vcpkg."
}

Push-Location $VcpkgRoot

try {
    Write-Host "Checking vcpkg working tree..."

    $GitStatus = git status --porcelain

    if ($LASTEXITCODE -ne 0) {
        throw "Unable to inspect the vcpkg Git working tree."
    }

    if ($GitStatus) {
        throw @"
The vcpkg checkout at:

    $VcpkgRoot

contains local modifications.

CodeAsMetal will not overwrite them automatically.
Clean or restore that checkout before continuing.
"@
    }

    Write-Host "Fetching vcpkg registry information..."

    Invoke-Native {
        git fetch --tags origin
    }

    Write-Host "Checking out vcpkg $VcpkgRef..."

    Invoke-Native {
        git checkout --detach $VcpkgRef
    }

    $VcpkgCommit = (git rev-parse HEAD).Trim()

    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($VcpkgCommit)) {
        throw "Unable to determine the effective vcpkg commit."
    }

    Write-Host "vcpkg commit: $VcpkgCommit"

    # --------------------------------------------------------
    # 2. Verify OpenCASCADE port major version
    # --------------------------------------------------------

    $OcctPortFile = Join-Path $VcpkgRoot "ports\opencascade\vcpkg.json"

    if (-not (Test-Path $OcctPortFile)) {
        throw "OpenCASCADE vcpkg port metadata was not found at $OcctPortFile."
    }

    $OcctPort = Get-Content $OcctPortFile -Raw | ConvertFrom-Json

    $OcctVersion = $null

    foreach ($Property in @(
        "version",
        "version-semver",
        "version-date",
        "version-string"
    )) {
        if ($OcctPort.PSObject.Properties.Name -contains $Property) {
            $Value = $OcctPort.$Property

            if (-not [string]::IsNullOrWhiteSpace($Value)) {
                $OcctVersion = $Value
                break
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($OcctVersion)) {
        throw "Unable to determine the OpenCASCADE port version."
    }

    if ($OcctVersion -notmatch '^7(\.|$)') {
        throw "CodeAsMetal V1 expects OpenCASCADE 7.x, but vcpkg provides '$OcctVersion'."
    }

    Write-Host "OpenCASCADE port version: $OcctVersion"

    # --------------------------------------------------------
    # 3. Bootstrap vcpkg
    # --------------------------------------------------------

    Write-Host "Bootstrapping vcpkg..."

    Invoke-Native {
        & (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
    }

    $VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"

    if (-not (Test-Path $VcpkgExe)) {
        throw "vcpkg.exe was not generated successfully."
    }

    # --------------------------------------------------------
    # 4. Install CodeAsMetal dependencies
    #
    # qtbase:
    #   - widgets: Qt Widgets
    #   - sql-odbc: Qt SQL ODBC driver support
    #
    # IMPORTANT:
    # windeployqt is a Qt deployment executable. It is NOT a
    # qtbase feature in the pinned vcpkg registry, so it must
    # not appear inside qtbase[...].
    # --------------------------------------------------------

    Write-Host "Installing CodeAsMetal dependencies..."
    Write-Host "This can take considerable time on the first run."

    Invoke-Native {
        & $VcpkgExe install `
            'qtbase[widgets,sql-odbc]:x64-windows' `
            'opencascade:x64-windows' `
            'gtest:x64-windows'
    }

    # --------------------------------------------------------
    # 5. Obtain effective installed package versions
    # --------------------------------------------------------

    Write-Host "Reading installed dependency versions..."

    $InstalledPackages = & $VcpkgExe list

    if ($LASTEXITCODE -ne 0) {
        throw "Unable to obtain the installed vcpkg package list."
    }

    function Get-VcpkgInstalledVersion {
        param(
            [Parameter(Mandatory = $true)]
            [string]$PackageName
        )

        $Prefix = "${PackageName}:x64-windows"

        $Line = $InstalledPackages |
            Where-Object { $_ -like "$Prefix*" } |
            Select-Object -First 1

        if (-not $Line) {
            return $null
        }

        $Parts = $Line -split '\s+'

        if ($Parts.Count -lt 2) {
            return $null
        }

        return $Parts[1]
    }

    $QtVersion    = Get-VcpkgInstalledVersion "qtbase"
    $GTestVersion = Get-VcpkgInstalledVersion "gtest"
    $OcctInstalledVersion = Get-VcpkgInstalledVersion "opencascade"

    if (-not $QtVersion) {
        throw "qtbase:x64-windows was not found after installation."
    }

    if (-not $GTestVersion) {
        throw "gtest:x64-windows was not found after installation."
    }

    if (-not $OcctInstalledVersion) {
        throw "opencascade:x64-windows was not found after installation."
    }

    # --------------------------------------------------------
    # 6. Locate windeployqt
    #
    # Do not assume it is a vcpkg feature. Search the installed
    # Qt tool locations produced by vcpkg.
    # --------------------------------------------------------

    $CandidateWinDeployQtPaths = @(
        (Join-Path $VcpkgRoot "installed\x64-windows\tools\Qt6\bin\windeployqt.exe"),
        (Join-Path $VcpkgRoot "installed\x64-windows\tools\qt6\bin\windeployqt.exe"),
        (Join-Path $VcpkgRoot "installed\x64-windows\tools\Qt6\windeployqt.exe"),
        (Join-Path $VcpkgRoot "installed\x64-windows\tools\qt6\windeployqt.exe")
    )

    $WinDeployQt = $CandidateWinDeployQtPaths |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    if (-not $WinDeployQt) {
        $WinDeployQt = Get-ChildItem `
            -Path (Join-Path $VcpkgRoot "installed\x64-windows") `
            -Filter "windeployqt.exe" `
            -File `
            -Recurse `
            -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName -First 1
    }

    if ($WinDeployQt) {
        Write-Host "windeployqt: $WinDeployQt"
    }
    else {
        Write-Warning @"
windeployqt.exe was not found after installing Qt.

The Qt libraries may still be installed correctly, but the Windows
deployment step will need to be checked before running the desktop
application outside the build environment.
"@
    }

    # --------------------------------------------------------
    # 7. Write dependency lock information
    # --------------------------------------------------------

    $LockFile = Join-Path $ProjectRoot "dependencies.lock.json"

    $Lock = [ordered]@{
        generatedUtc = (Get-Date).ToUniversalTime().ToString("o")

        vcpkg = [ordered]@{
            root   = $VcpkgRoot
            ref    = $VcpkgRef
            commit = $VcpkgCommit
        }

        triplet = "x64-windows"

        packages = [ordered]@{
            qtbase = [ordered]@{
                version  = $QtVersion
                features = @(
                    "widgets",
                    "sql-odbc"
                )
            }

            opencascade = [ordered]@{
                version = $OcctInstalledVersion
            }

            gtest = [ordered]@{
                version = $GTestVersion
            }
        }

        tools = [ordered]@{
            windeployqt = $WinDeployQt
        }
    }

    $Lock |
        ConvertTo-Json -Depth 10 |
        Set-Content `
            -Path $LockFile `
            -Encoding UTF8

    Write-Host "Dependency lock written to:"
    Write-Host "  $LockFile"

    # --------------------------------------------------------
    # 8. Configure VCPKG_ROOT for future Visual Studio sessions
    # --------------------------------------------------------

    [Environment]::SetEnvironmentVariable(
        "VCPKG_ROOT",
        $VcpkgRoot,
        [EnvironmentVariableTarget]::User
    )

    # Also update this PowerShell process so commands executed
    # after this script see the same vcpkg root immediately.
    $env:VCPKG_ROOT = $VcpkgRoot

    Write-Host ""
    Write-Host "VCPKG_ROOT configured as:"
    Write-Host "  $VcpkgRoot"
    Write-Host ""
    Write-Host "Dependencies prepared."
    Write-Host "Restart Visual Studio so it reads the updated VCPKG_ROOT."
}
finally {
    Pop-Location
}