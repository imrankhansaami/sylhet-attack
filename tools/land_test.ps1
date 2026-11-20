# land_test.ps1 - fly the Landing Challenge with the MOUSE, the way a player
# does: small analog stick movements based on the game's own telemetry. Then
# save the result screen with F2.
#
#   powershell -ExecutionPolicy Bypass -File tools\land_test.ps1 -Out screenshots\live\landing.png
param([string]$Out = 'screenshots\live\landing.png')

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class LT {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  public static void Move(IntPtr h,int x,int y){ SendMessage(h,0x0200,(IntPtr)0,(IntPtr)((y<<16)|(x&0xffff))); }
  public static void Down(IntPtr h,int vk){SendMessage(h,0x0100,(IntPtr)vk,(IntPtr)0x00010001);}
  public static void Up(IntPtr h,int vk){SendMessage(h,0x0101,(IntPtr)vk,(IntPtr)0xC0010001);}
}
"@

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'SylhetAttack.exe'
Set-Location $root
Get-Process -Name 'SylhetAttack' -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
Start-Sleep -Milliseconds 400
Remove-Item 'probe.csv' -ErrorAction SilentlyContinue
Remove-Item 'shot_manual.png' -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $exe -ArgumentList '--landing','--probe' -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 80; $i++) {
    Start-Sleep -Milliseconds 250
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne 0) { $hwnd = $proc.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { try { $proc.Kill() } catch {}; Write-Error 'no window'; exit 1 }
Start-Sleep -Milliseconds 1200

$r = New-Object LT+RECT
[void][LT]::GetClientRect($hwnd, [ref]$r)
$cx = [int](($r.R - $r.L) / 2); $cy = [int](($r.B - $r.T) / 2)

$VK_F2 = 0x71

# runway geometry, from the same numbers the world uses
$rad = 50.0 * [math]::PI / 180.0
$dirX = [math]::Sin($rad); $dirZ = -[math]::Cos($rad)
$thrX = -2600.0 + $dirX * (-1450.0)
$thrZ = -3200.0 + $dirZ * (-1450.0)
$rightX = -$dirZ; $rightZ = $dirX

function Sample {
    if (-not (Test-Path 'probe.csv')) { return $null }
    $last = Get-Content 'probe.csv' -Tail 1
    if (-not $last) { return $null }
    $f = $last.Split(',')
    if ($f.Count -lt 15) { return $null }
    return @{
        t = [double]$f[0]; x = [double]$f[1]; y = [double]$f[2]; z = [double]$f[3]
        kt = [double]$f[4]; onGnd = [int]$f[13]; thr = [double]$f[14]
    }
}

$prevY = 0.0; $prevT = 0.0
$touchdown = $false
$deadline = (Get-Date).AddSeconds(70)
$log = @()

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 260
    $s = Sample
    if (-not $s -or $s.t -le $prevT) { continue }
    $vy = ($s.y - $prevY) / ($s.t - $prevT)
    $prevY = $s.y; $prevT = $s.t

    $rem = (($thrX - $s.x) * $dirX) + (($thrZ - $s.z) * $dirZ)
    $lat = (($s.x + 2600.0) * $rightX) + (($s.z + 3200.0) * $rightZ)
    $gs = $s.kt / 1.94384

    # wanted sink: aim at the threshold itself (2 m over it), then flare
    if ($s.y -lt 7.0) { $want = -0.3 }
    else {
        $want = -((($s.y - 2.0) * $gs) / [math]::Max($rem, 60.0))
        if ($want -lt -5.0) { $want = -5.0 }
        if ($want -gt -0.6) { $want = -0.6 }
    }

    $log += ("t={0,5:N1} rem={1,6:N0} lat={2,5:N0} y={3,6:N1} kt={4,4:N0} vy={5,5:N1} want={6,5:N1} gnd={7}" -f `
        $s.t, $rem, $lat, $s.y, $s.kt, $vy, $want, $s.onGnd)
    if ($s.onGnd -eq 1) { $touchdown = $true; break }

    # mouse stick: small nudges only (full deflection 260 px would throw it about)
    $pitchPx = ($want - $vy) * 40.0
    if ($pitchPx -gt 70) { $pitchPx = 70 }
    if ($pitchPx -lt -70) { $pitchPx = -70 }
    $rollPx = -$lat * 0.8
    if ($rollPx -gt 60) { $rollPx = 60 }
    if ($rollPx -lt -60) { $rollPx = -60 }

    if ([math]::Abs($pitchPx) -gt 18 -or [math]::Abs($rollPx) -gt 18) {
        [LT]::Move($hwnd, $cx + [int]$rollPx, $cy + [int]$pitchPx)
    }
}

$log | Select-Object -Last 10
Write-Output ("touchdown: " + $touchdown)

Start-Sleep -Milliseconds 4500
[LT]::Down($hwnd, $VK_F2); Start-Sleep -Milliseconds 120; [LT]::Up($hwnd, $VK_F2)
Start-Sleep -Milliseconds 1500
try { $proc.Kill() } catch {}

$shot = Join-Path $root 'shot_manual.png'
if (-not (Test-Path $shot)) { Write-Error 'the game wrote no frame'; exit 1 }
$img = [System.Drawing.Image]::FromFile($shot)
$dir = Split-Path -Parent $Out
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
$img.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output ("saved $Out (" + $img.Width + "x" + $img.Height + ")")
$img.Dispose()
Remove-Item $shot -Force