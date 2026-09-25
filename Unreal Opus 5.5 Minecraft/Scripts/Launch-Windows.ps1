param(
    [switch]$SkipBuild,
    [switch]$NoLaunch
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'Unreal_Minecraft.uproject'

function Test-UnrealRoot {
    param([string]$Root)

    if ([string]::IsNullOrWhiteSpace($Root)) {
        return $false
    }

    $editor = Join-Path $Root 'Engine\Binaries\Win64\UnrealEditor.exe'
    $versionFile = Join-Path $Root 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) {
        return $false
    }

    if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
        try {
            $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
            return $version.MajorVersion -eq 5 -and $version.MinorVersion -eq 8
        }
        catch {
            return $false
        }
    }

    return $false
}

function Find-UnrealRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()

    foreach ($variableName in @('UE_5_8_ROOT', 'UE_ROOT')) {
        $value = [Environment]::GetEnvironmentVariable($variableName)
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $candidates.Add($value)
        }
    }

    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (-not [string]::IsNullOrWhiteSpace($base)) {
            $candidates.Add((Join-Path $base 'Epic Games\UE_5.8'))
        }
    }

    $launcherManifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path -LiteralPath $launcherManifest -PathType Leaf) {
        try {
            $manifest = Get-Content -LiteralPath $launcherManifest -Raw | ConvertFrom-Json
            foreach ($entry in $manifest.InstallationList) {
                if ($entry.AppName -like 'UE_5.8*' -or $entry.InstallLocation -match '[\\/]UE_5\.8(?:[\\/]|$)') {
                    $candidates.Add($entry.InstallLocation)
                }
            }
        }
        catch {
            Write-Warning 'Epic Games Launcher installation list could not be read.'
        }
    }

    $customBuildsKey = 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds'
    if (Test-Path $customBuildsKey) {
        $properties = (Get-ItemProperty $customBuildsKey).PSObject.Properties
        foreach ($property in $properties) {
            if ($property.Name -notlike 'PS*' -and $property.Value -is [string]) {
                $candidates.Add($property.Value)
            }
        }
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-UnrealRoot $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Project file not found: $projectFile"
}

$unrealRoot = Find-UnrealRoot
if (-not $unrealRoot) {
    throw @'
Unreal Engine 5.8 was not found.

Install Unreal Engine 5.8.x in Epic Games Launcher. For a custom installation,
set the UE_5_8_ROOT environment variable to the engine folder and run this file again.
'@
}

$editor = Join-Path $unrealRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$buildScript = Join-Path $unrealRoot 'Engine\Build\BatchFiles\Build.bat'

Write-Host "Project: $projectFile"
Write-Host "Unreal Engine: $unrealRoot"

if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
        throw "Unreal build script not found: $buildScript"
    }

    Write-Host ''
    Write-Host 'Building the editor target. The first build can take a while...'
    & $buildScript 'Unreal_MinecraftEditor' 'Win64' 'Development' "-Project=$projectFile" '-WaitMutex' '-NoHotReloadFromIDE'
    if ($LASTEXITCODE -ne 0) {
        throw @'
The C++ build failed. Install Visual Studio 2022 with the "Game development
with C++" workload and a Windows 11 SDK, then run Launch-Windows.bat again.
'@
    }
}

if (-not $NoLaunch) {
    Write-Host ''
    Write-Host 'Starting Unreal Editor...'
    Start-Process -FilePath $editor -ArgumentList @("`"$projectFile`"")
}
