# takeoff_test.ps1 - run the take-off several times and prove it works every time.
# It waits for the aircraft to actually reach rotation speed (the frame rate, and
# so how much simulation time passes per wall second, varies), then rotates and
# checks that it leaves the ground. Saves a before/after frame for each run.
#
#   powershell -ExecutionPolicy Bypass -File tools\takeoff_test.ps1 -Runs 3
param([int]$Runs = 3, [string]$OutDir = 'screenshots\live')

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class TT {
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  public static void DownChar(IntPtr h,int c){SendMessage(h,0x0102,(IntPtr)c,(IntPtr)1);}
  public static void UpChar(IntPtr h,int c){SendMessage(h,0x0101,(IntPtr)char.ToUpper((char)c),(IntPtr)0xC0010001);}
  public static void Down(IntPtr h,int vk){SendMessage(h,0x0100,(IntPtr)vk,(IntPtr)0x00010001);}
  public static void Up(IntPtr h,int vk){SendMessage(h,0x0101,(IntPtr)vk,(IntPtr)0xC0010001);}
}
"@

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'SylhetAttack.exe'
Set-Location $root
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$VK_UP = 0x26
$VK_F2 = 0x71

function Snap($hwnd, [string]$name) {
    Remove-Item 'shot_manual.png' -ErrorAction SilentlyContinue
    [TT]::Down($hwnd, $VK_F2); Start-Sleep -Milliseconds 90; [TT]::Up($hwnd, $VK_F2)
    for ($i = 0; $i -lt 25; $i++) { Start-Sleep -Milliseconds 100; if (Test-Path 'shot_manual.png') { break } }
    if (-not (Test-Path 'shot_manual.png')) { return }
    $img = [System.Drawing.Image]::FromFile((Join-Path $root 'shot_manual.png'))
    $img.Save((Join-Path $root (Join-Path $OutDir ($name + '.png'))), [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Remove-Item 'shot_manual.png' -Force
}

function LastSample {
    if (-not (Test-Path 'probe.csv')) { return $null }
    $last = Get-Content 'probe.csv' -Tail 1
    if (-not $last) { return $null }
    $f = $last.Split(',')
    if ($f.Count -lt 15) { return $null }
    return @{ kt = [double]$f[4]; y = [double]$f[2]; onGnd = [int]$f[13] }
}

$pass = 0
for ($run = 1; $run -le $Runs; $run++) {
    Get-Process -Name 'SylhetAttack' -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
    Start-Sleep -Milliseconds 400
    Remove-Item 'probe.csv' -ErrorAction SilentlyContinue

    $proc = Start-Process -FilePath $exe -ArgumentList '--free','--probe' -PassThru
    $hwnd = [IntPtr]::Zero
    for ($i = 0; $i -lt 80; $i++) {
        Start-Sleep -Milliseconds 250
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne 0) { $hwnd = $proc.MainWindowHandle; break }
    }
    if ($hwnd -eq [IntPtr]::Zero) { Write-Output ("run " + $run + ": no window"); continue }
    Start-Sleep -Milliseconds 1500

    Snap $hwnd ("run" + $run + "_a_parked")

    # full power and wait until there is really enough speed to rotate
    [TT]::DownChar($hwnd, [int][char]'w')
    $rotateAt = 54.0
    $deadline = (Get-Date).AddSeconds(45)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 400
        $s = LastSample
        if ($s -and $s.kt -ge $rotateAt) { break }
    }

    # rotate and hold it until the wheels leave the ground
    [TT]::Down($hwnd, $VK_UP)
    $liftDeadline = (Get-Date).AddSeconds(12)
    $lifted = $false
    while ((Get-Date) -lt $liftDeadline) {
        Start-Sleep -Milliseconds 300
        $s = LastSample
        if ($s -and $s.onGnd -eq 0) { $lifted = $true; break }
    }
    [TT]::UpChar($hwnd, [int][char]'w')
    Start-Sleep -Milliseconds 1200     # ease off the back pressure after rotating
    [TT]::Up($hwnd, $VK_UP)

    # let it settle into a climb, then look at where it got to
    Start-Sleep -Seconds 8
    Snap $hwnd ("run" + $run + "_b_climbing")

    $s = LastSample
    $ok = $lifted -and $s -and ($s.onGnd -eq 0) -and ($s.y -gt 5.0)
    if ($ok) { $pass++ }
    Write-Output ("run {0}: {1}  lifted={2}  alt {3:N1} m  {4:N0} kt" -f `
        $run, $(if ($ok) { "PASS" } else { "FAIL" }), $lifted, $s.y, $s.kt)
    if (-not $proc.HasExited) { $proc.Kill() }
}
Remove-Item 'probe.csv' -ErrorAction SilentlyContinue
Write-Output ("RESULT: $pass / $Runs take-offs succeeded")