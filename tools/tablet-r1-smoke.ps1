param(
    [Parameter(Mandatory=$true)][string]$Adb,
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$Package = 'org.openxray.olr',
    [string]$Tag = 'r1-smoke',
    [string]$ArgumentsText,
    [switch]$Interact,
    [switch]$DirectionSweep,
    [ValidateSet(-200,200)][int]$SweepStep = 200,
    [switch]$ConfigureOnly,
    [ValidateRange(20,180)][int]$Seconds = 80,
    [string]$LogDirectory = (Join-Path $PSScriptRoot '../logs')
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$measurements = @()
function Read-Thermals {
    $battery = & $Adb shell dumpsys battery
    $tempLine = $battery | Select-String '^  temperature:' | Select-Object -First 1
    $thermalLine = & $Adb shell dumpsys thermalservice | Select-String '^Thermal Status:' | Select-Object -First 1
    if (!$tempLine -or !$thermalLine) { throw 'Thermal readings unavailable; test not started/continued.' }
    $temp = [int]([regex]::Match($tempLine.ToString(), '\d+').Value)
    $status = [int]([regex]::Match($thermalLine.ToString(), '\d+').Value)
    if ($temp -ge 400 -or $status -ge 3) { throw "Thermal stop: battery=$($temp/10.0) C, Android status=$status" }
    return @{time=(Get-Date).ToString('o'); battery_celsius=$temp/10.0; thermal_status=$status}
}
function Read-UI {
    & $Adb shell uiautomator dump /sdcard/olr-r1-smoke-ui.xml | Out-Null
    $file = Join-Path $LogDirectory "$Tag-ui.xml"
    & $Adb pull /sdcard/olr-r1-smoke-ui.xml $file | Out-Null
    return [xml](Get-Content -LiteralPath $file -Raw)
}
function Tap-Node($node) {
    if (!$node -or $node.enabled -ne 'true') { throw 'Required launcher control is unavailable.' }
    $v = [regex]::Matches($node.bounds,'\d+') | ForEach-Object { [int]$_.Value }
    & $Adb shell input tap ([int](($v[0]+$v[2])/2)) ([int](($v[1]+$v[3])/2))
}
try {
    $measurements += Read-Thermals
    & $Adb shell input keyevent 224
    Start-Sleep -Milliseconds 600
    & $Adb shell wm dismiss-keyguard
    & $Adb shell am start -n "$Package/org.openxray.LauncherActivity"
    Start-Sleep -Milliseconds 1200
    $ui = Read-UI
    # Force-stop intentionally leaves the launcher's previous-session marker.
    # Dismiss its local report dialog; never submit reports automatically.
    $skip = $ui.SelectSingleNode("//node[@resource-id='android:id/button2' and @text='SKIP']")
    if ($skip) { Tap-Node $skip; $ui = Read-UI }
    if ($PSBoundParameters.ContainsKey('ArgumentsText')) {
        if ($ArgumentsText.Contains("'") -or $ArgumentsText.Contains('"')) { throw 'Use simple unquoted smoke-test arguments.' }
        $open = $ui.SelectSingleNode("//node[@resource-id='$($Package):id/btn_open_settings']")
        if ($open) { Tap-Node $open; $ui = Read-UI }
        Tap-Node $ui.SelectSingleNode("//node[@resource-id='$($Package):id/edit_custom_args']")
        Start-Sleep -Milliseconds 300
        & $Adb shell input keycombination 113 29
        $encoded = $ArgumentsText.Replace(' ', '%s')
        if ($encoded) { & $Adb shell input text "'$encoded'" }
        else { & $Adb shell input keyevent 67 }
        & $Adb shell input keyevent 111
        Start-Sleep -Milliseconds 700
        $ui = Read-UI
        $entered = $ui.SelectSingleNode("//node[@resource-id='$($Package):id/edit_custom_args']")
        if (!$entered -or $entered.text -ne $ArgumentsText) { throw 'Launcher argument edit did not match requested value.' }
    }
    $back = $ui.SelectSingleNode("//node[@resource-id='$($Package):id/btn_settings_back']")
    if ($back) { Tap-Node $back; $ui = Read-UI }
    if ($ConfigureOnly) { return }
    Tap-Node $ui.SelectSingleNode("//node[@resource-id='$($Package):id/btn_start_game']")
    $runPid = (& $Adb shell pidof $Package).Trim()
    if (!$runPid) { throw 'Game process did not start.' }
    for ($elapsed=10; $elapsed -le $Seconds; $elapsed+=10) {
        Start-Sleep -Seconds 10
        $measurements += Read-Thermals
        $currentPid = (& $Adb shell pidof $Package) -join ' '
        if ($currentPid.Trim() -ne $runPid) { throw 'Game process exited or restarted; ending capture immediately.' }
        Write-Output "elapsed=$elapsed battery=$($measurements[-1].battery_celsius) C thermal=$($measurements[-1].thermal_status)"
        if ($DirectionSweep -and $elapsed -in @(80,100,120,140,160)) {
            & $Adb shell screencap -p /sdcard/olr-r1-smoke.png
            & $Adb pull /sdcard/olr-r1-smoke.png (Join-Path $LogDirectory "$Tag-direction-$elapsed.png")
            if ($elapsed -lt 160) { & $Adb shell input swipe 2100 800 (2100+$SweepStep) 800 400 }
        }
        if ($Interact -and $elapsed -eq 80) {
            & $Adb shell screencap -p /sdcard/olr-r1-smoke.png
            & $Adb pull /sdcard/olr-r1-smoke.png (Join-Path $LogDirectory "$Tag-before-input.png")
            & $Adb shell input swipe 2100 800 2300 800 400
            Start-Sleep -Seconds 1
            & $Adb shell screencap -p /sdcard/olr-r1-smoke.png
            & $Adb pull /sdcard/olr-r1-smoke.png (Join-Path $LogDirectory "$Tag-after-look.png")
            & $Adb shell input swipe 410 1150 410 1000 700
            Start-Sleep -Seconds 1
            & $Adb shell screencap -p /sdcard/olr-r1-smoke.png
            & $Adb pull /sdcard/olr-r1-smoke.png (Join-Path $LogDirectory "$Tag-after-move.png")
        }
        if ($elapsed -eq 40 -or $elapsed + 10 -gt $Seconds) {
            & $Adb shell screencap -p /sdcard/olr-r1-smoke.png
            & $Adb pull /sdcard/olr-r1-smoke.png (Join-Path $LogDirectory "$Tag-$($elapsed)s.png")
        }
    }
    $engineFiles = & $Adb shell "find '$GameRoot' -maxdepth 1 -name 'openxray_u*.log'"
    foreach ($file in $engineFiles) {
        if ($file.Trim()) { & $Adb pull $file.Trim() (Join-Path $LogDirectory "$Tag-$([IO.Path]::GetFileName($file.Trim()))") }
    }
    & $Adb logcat -d -t 3000 -v threadtime > (Join-Path $LogDirectory "$Tag-logcat.log")
    & $Adb shell pidof $Package
} finally {
    & $Adb shell am force-stop $Package
    & $Adb shell input keyevent 223
    $measurements | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $LogDirectory "$Tag-thermals.json")
}
