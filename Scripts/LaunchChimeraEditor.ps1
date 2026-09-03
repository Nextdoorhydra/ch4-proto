# Starts the Chimera editor with writable, project-local cache/temp paths.
# Unreal uses the process TEMP directory for ShaderCompileWorker transfer files;
# redirecting it avoids the protected user-temp path on the development machine.
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$savedRoot = Join-Path $projectRoot 'Saved'
$tempRoot = Join-Path $savedRoot 'Temp'
$localAppData = Join-Path $savedRoot 'LocalAppData'
$appData = Join-Path $savedRoot 'AppData'
$userData = Join-Path $savedRoot 'UserData'
$zenData = Join-Path $savedRoot 'Zen\Data'
$ddc = Join-Path $projectRoot 'DerivedDataCache'

foreach ($path in @($tempRoot, $localAppData, $appData, $userData, $zenData, $ddc)) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}

[Environment]::SetEnvironmentVariable('TEMP', $tempRoot, 'Process')
[Environment]::SetEnvironmentVariable('TMP', $tempRoot, 'Process')
[Environment]::SetEnvironmentVariable('LOCALAPPDATA', $localAppData, 'Process')
[Environment]::SetEnvironmentVariable('APPDATA', $appData, 'Process')
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $ddc, 'Process')
[Environment]::SetEnvironmentVariable('MCP_NATIVE_PORT', '3000', 'Process')

$editor = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe'
if (!(Test-Path -LiteralPath $editor)) {
    throw "UnrealEditor was not found at $editor"
}

Start-Process -FilePath $editor -WorkingDirectory $projectRoot -ArgumentList @(
    (Join-Path $projectRoot 'Chimera.uproject'),
    '-NoSplash', '-NoP4', '-unattended', '-Messaging', '-log',
    "-UserDir=$userData",
    '-DDC=NoZenLocalFallback',
    '-LocalDataCachePath=C:\Chimera\DerivedDataCache'
)
