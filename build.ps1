<#
.SYNOPSIS
    Build / install / run helper for SimpleWindowsDriver. Run on a test machine.

.DESCRIPTION
    Wraps msbuild and sc.exe. The install/start/stop/uninstall actions need an
    elevated PowerShell, and loading the driver needs test signing enabled.

.EXAMPLE
    .\build.ps1 build
    .\build.ps1 install
    .\build.ps1 start
#>
[CmdletBinding()]
param(
    [ValidateSet('build', 'install', 'start', 'stop', 'uninstall')]
    [string]$Action = 'build',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

$ServiceName = 'SimpleWindowsDriver'
$ProjectFile = Join-Path $PSScriptRoot 'Simple Windows Driver.vcxproj'
$SysName     = 'SimpleWindowsDriver.sys'

# The exact msbuild output folder for a driver package can vary by WDK version,
# so locate the built .sys under the matching Platform\Configuration tree rather
# than hard-coding a path.
function Resolve-SysPath {
    $root = Join-Path $PSScriptRoot (Join-Path $Platform $Configuration)
    if (-not (Test-Path $root)) { return $null }
    $hit = Get-ChildItem -Path $root -Filter $SysName -Recurse -File -ErrorAction SilentlyContinue |
           Select-Object -First 1
    if ($hit) { return $hit.FullName }
    return $null
}

function Assert-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($id)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "This action needs an elevated (Administrator) PowerShell."
    }
}

switch ($Action) {
    'build' {
        Write-Host "Building $ServiceName ($Configuration|$Platform)..."
        & msbuild $ProjectFile "/p:Configuration=$Configuration" "/p:Platform=$Platform"
        if ($LASTEXITCODE -ne 0) { throw "msbuild failed with exit code $LASTEXITCODE." }
        $sys = Resolve-SysPath
        if ($sys) { Write-Host "Done. Built: $sys" }
        else { Write-Host "Done. (Could not locate $SysName under $Platform\$Configuration — check the build output.)" }
    }
    'install' {
        Assert-Admin
        $sys = Resolve-SysPath
        if (-not $sys) { throw "Could not find $SysName under '$Platform\$Configuration'. Build it first." }
        Write-Host "Creating service '$ServiceName' -> $sys"
        & sc.exe create $ServiceName type= kernel start= demand binPath= $sys
    }
    'start'     { Assert-Admin; & sc.exe start $ServiceName }
    'stop'      { Assert-Admin; & sc.exe stop  $ServiceName }
    'uninstall' {
        Assert-Admin
        & sc.exe stop   $ServiceName 2>$null | Out-Null
        & sc.exe delete $ServiceName
    }
}
