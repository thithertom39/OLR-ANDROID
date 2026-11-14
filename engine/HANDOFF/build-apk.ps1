param([string]$SdkRoot=$env:ANDROID_SDK_ROOT,[string]$Keystore,[switch]$GpuProfile)
$ErrorActionPreference='Stop'
if (-not $SdkRoot) { throw 'Set ANDROID_SDK_ROOT or pass -SdkRoot' }
$projectRoot=Split-Path -Parent $PSScriptRoot
$sdkPath=$SdkRoot.Replace('\','/')
Set-Content -LiteralPath (Join-Path $projectRoot 'android/local.properties') -Value ('sdk.dir='+$sdkPath) -Encoding ascii
$names=@('ogg','vorbis','lzo','openal-soft','sdl2')
$oldCount=$env:CMAKE_ARGS
$saved=@{}
foreach($n in $names){$k='ORG_GRADLE_PROJECT_unused';$saved[$n]=$null}
# CMAKE_PROJECT_INCLUDE supplies dependency locations before FetchContent.
$cache=Join-Path $projectRoot 'HANDOFF/vendor-cache.cmake'
$savedInclude=$env:CMAKE_PROJECT_INCLUDE
Push-Location (Join-Path $projectRoot 'android')
try {
 $argv=@('assembleRelease','-POXRAY_TEST_BUILD=true','-POXRAY_TEST_VARIANT=skadovsk3','-POXR_GITHUB_TOKEN=','--no-daemon','--console=plain','--max-workers=4')
 if($Keystore){$argv+=('-POXRAY_KEYSTORE='+$Keystore)}
 if($GpuProfile){$argv+='-POXRAY_GPU_PROFILE=true'}
 # Gradle's cmake arguments are augmented by a separate, documented init script.
 & ./gradlew.bat @argv --init-script ../HANDOFF/vendor.init.gradle
 if($LASTEXITCODE -ne 0){throw 'APK build failed'}
} finally {Pop-Location}
