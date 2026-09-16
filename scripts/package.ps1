<# Package a previously validated Release build. No publishing or signing.
   Requires NSIS only when producing an installer instead of the default ZIP. #>
param([switch]$Installer)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    & .\scripts\build.cmd Release
    if($LASTEXITCODE -ne 0){throw 'Release build/tests failed.'}
    $stage=Join-Path $root 'out/package/CodeAsMetal'
    New-Item $stage -ItemType Directory -Force | Out-Null
    Get-ChildItem 'out/build/windows-release' -Filter '*.dll' | Copy-Item -Destination $stage
    Copy-Item 'out/build/windows-release/CodeAsMetal.exe' $stage
    foreach($d in @('platforms','sqldrivers','styles','imageformats','tls')) {
        $p=Join-Path 'out/build/windows-release' $d
        if(Test-Path $p){Copy-Item $p $stage -Recurse -Force}
    }
    Copy-Item @('README_ES.md','THIRD_PARTY.md','docs','data') $stage -Recurse -Force
    if(-not(Test-Path "$stage/platforms/qwindows.dll")){throw 'Qt platform plugin is missing.'}
    if(-not(Test-Path "$stage/sqldrivers/qsqlodbc.dll")){throw 'Qt ODBC plugin is missing.'}
    $licenses=Join-Path $stage 'licenses'
    New-Item $licenses -ItemType Directory -Force | Out-Null
    Get-ChildItem (Join-Path $env:VCPKG_ROOT 'installed/x64-windows/share') -Directory | ForEach-Object {
        $notice=Join-Path $_.FullName 'copyright'
        if(Test-Path $notice){Copy-Item $notice (Join-Path $licenses ($_.Name+'.txt'))}
    }
    Compress-Archive "$stage/*" 'out/CodeAsMetal-Windows-x64.zip' -Force
    if($Installer){
        & makensis /DSTAGE="$stage" scripts/installer.nsi
        if($LASTEXITCODE -ne 0){throw 'NSIS installer generation failed.'}
    }
} finally {Pop-Location}
