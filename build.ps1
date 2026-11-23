param(
    [string]$BuildName = 'OLR-Android-01-arm64',
    [string]$SdkRoot = $env:ANDROID_HOME,
    [string]$JavaHome = $env:JAVA_HOME,
    [string]$GradleCache = $env:GRADLE_USER_HOME,
    [string]$HostCompilerBin = 'C:\MinGW\bin',
    [switch]$Offline,
    [switch]$GpuProfile
)

$ErrorActionPreference = 'Stop'
if (!$SdkRoot -or !$JavaHome) {
    throw 'Set ANDROID_HOME and JAVA_HOME or pass -SdkRoot and -JavaHome.'
}
$env:ANDROID_HOME = $SdkRoot
$env:ANDROID_SDK_ROOT = $SdkRoot
$env:JAVA_HOME = $JavaHome
if ($GradleCache) { $env:GRADLE_USER_HOME = $GradleCache }
$env:ANDROID_USER_HOME = Join-Path $PSScriptRoot '.android'
$env:PATH = "$JavaHome\bin;$HostCompilerBin;$env:PATH"
$env:CMAKE_BUILD_PARALLEL_LEVEL = '10'
$androidProject = Join-Path $PSScriptRoot 'engine\android'
$logs = Join-Path $PSScriptRoot 'logs'
$artifacts = Join-Path $PSScriptRoot 'artifacts'
New-Item -ItemType Directory -Path $env:ANDROID_USER_HOME,$logs,$artifacts -Force | Out-Null
$key = Join-Path $env:ANDROID_USER_HOME 'debug.keystore'
if (!(Test-Path -LiteralPath $key)) {
    & "$JavaHome\bin\keytool.exe" -genkeypair -noprompt -keystore $key -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 3650 -dname 'CN=OLR Android Local Test'
    if ($LASTEXITCODE) { throw 'Could not create the local APK signing key.' }
}

$arguments = @('assembleRelease', "-POXRAY_KEYSTORE=$key", '-POXR_GITHUB_TOKEN=',
    '--console=plain', '--max-workers=10', '--init-script', '../HANDOFF/vendor.init.gradle')
if ($Offline) { $arguments += '--offline' }
if ($GpuProfile) { $arguments += '-POXRAY_GPU_PROFILE=true' }
$log = Join-Path $logs "$BuildName.build.log"

Push-Location -LiteralPath $androidProject
try {
    $sdkProperty = 'sdk.dir=' + $SdkRoot.Replace('\', '/')
    Set-Content -LiteralPath 'local.properties' -Value $sdkProperty -Encoding ascii
    $ErrorActionPreference = 'Continue'
& .\gradlew.bat @arguments *> $log
$ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE) {
        Get-Content -LiteralPath $log -Tail 75
        throw "APK build failed. Log: $log"
    }
    $apk = Join-Path $artifacts "$BuildName.apk"
    Copy-Item -LiteralPath 'app\build\outputs\apk\release\app-release.apk' -Destination $apk
    Get-FileHash -LiteralPath $apk
} finally {
    Pop-Location
}
