# screenshot.ps1 - start the simulator, drive it with the keyboard/mouse and
# capture the real window from the desktop.
#
#   powershell -ExecutionPolicy Bypass -File tools\screenshot.ps1 -Out shots\xx.png -Scenario airport
#
# Scenarios: title, free, cockpit, takeoff, cruise, city, landing, rings, tour
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Scenario = 'free',
    [int]$X = 60, [int]$Y = 40, [int]$W = 1280, [int]$H = 760
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
  [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int k);
  public const byte VK_W = 0x57, VK_S = 0x53, VK_SHIFT = 0x10;
  public static void KeyDown(byte k) { keybd_event(k, 0, 0, UIntPtr.Zero); }
  public static void KeyUp(byte k) { keybd_event(k, 0, 2, UIntPtr.Zero); }
  public static void Tap(byte k) { KeyDown(k); System.Threading.Thread.Sleep(60); KeyUp(k); }
}
"@

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'SylhetAttack.exe'
if (-not (Test-Path $exe)) { Write-Error "build the game first"; exit 1 }

# no other copy may steal the screen or the focus
Get-Process -Name 'SylhetAttack' -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
Start-Sleep -Milliseconds 600

$proc = Start-Process -FilePath $exe -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 250
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne 0) { $hwnd = $proc.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { $proc.Kill(); Write-Error "no window"; exit 1 }

$top = [IntPtr](-1)   # HWND_TOPMOST
[void][Win]::SetWindowPos($hwnd, $top, $X, $Y, $W, $H, 0x40)
[void][Win]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 800

$cx = $X + [int]($W / 2)
$cy = $Y + [int]($H / 2)
[void][Win]::SetCursorPos($cx, $cy)

function Move-Mouse([int]$dx, [int]$dy) {
    for ($i = 0; $i -lt 12; $i++) {
        [void][Win]::SetCursorPos($cx + [int]($dx * ($i + 1) / 12), $cy + [int]($dy * ($i + 1) / 12))
        Start-Sleep -Milliseconds 25
    }
}

function Grab([string]$file) {
    $bmp = New-Object System.Drawing.Bitmap $W, $H
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($X, $Y, 0, 0, (New-Object System.Drawing.Size $W, $H))
    $dir = Split-Path -Parent $file
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "saved $file"
}

function Tap([byte]$k) {
    [void][Win]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 250
    [Win]::Tap($k)
}

switch ($Scenario) {
    'title' { Start-Sleep -Milliseconds 1200 }
    'free'  { Tap 0x31; Start-Sleep -Milliseconds 2500 }
    'cockpit' { Tap 0x31; Start-Sleep -Milliseconds 2000; Tap 0x43; Start-Sleep -Milliseconds 1200 }
    'takeoff' {
        Tap 0x31; Start-Sleep -Milliseconds 1500
        [Win]::KeyDown([Win]::VK_W)
        Start-Sleep -Milliseconds 9000               # full power
        Move-Mouse 0 70                               # rotate
        Start-Sleep -Milliseconds 2500
        Move-Mouse 0 -70                              # release, fly level
        Start-Sleep -Milliseconds 2000
    }
    'cruise' {
        Tap 0x31; Start-Sleep -Milliseconds 1500
        [Win]::KeyDown([Win]::VK_W)
        Start-Sleep -Milliseconds 9000
        Move-Mouse 0 70
        Start-Sleep -Milliseconds 2500
        Move-Mouse 0 -70
        Start-Sleep -Milliseconds 6000
        [Win]::KeyUp([Win]::VK_W)
        Start-Sleep -Milliseconds 800
    }
    'city' {
        Tap 0x31; Start-Sleep -Milliseconds 1500
        [Win]::KeyDown([Win]::VK_W)
        Start-Sleep -Milliseconds 9000
        Move-Mouse 0 70
        Start-Sleep -Milliseconds 3000
        Move-Mouse 0 -70
        Start-Sleep -Milliseconds 14000              # fly towards the city
        [Win]::KeyUp([Win]::VK_W)
        Start-Sleep -Milliseconds 600
    }
    'landing' { Tap 0x35; Start-Sleep -Milliseconds 4000 }
    'rings'   { Tap 0x34; Start-Sleep -Milliseconds 4000 }
    'tour'    { Tap 0x33; Start-Sleep -Milliseconds 4000 }
    default   { Tap 0x31; Start-Sleep -Milliseconds 2500 }
}

Grab $Out
$proc.Kill()
Start-Sleep -Milliseconds 400
