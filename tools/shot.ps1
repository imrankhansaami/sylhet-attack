# shot.ps1 - launch the simulator, drive it through window messages, and let the
# GAME save its own frame (--shot), so nothing depends on the desktop compositor.
#
#   powershell -ExecutionPolicy Bypass -File tools\shot.ps1 -Out screenshots\a.png -Scenario free
#
# Scenarios: title, free, cockpit, takeoff, cruise, city, landing, rings, tour,
#            school, pause, help, crash
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Scenario = 'free'
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class GW {
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  // freeglut takes printable keys from WM_CHAR and special keys from WM_KEYDOWN
  public static void Char(IntPtr h,int c){
    SendMessage(h,0x0102,(IntPtr)c,(IntPtr)1);
    System.Threading.Thread.Sleep(90);
    SendMessage(h,0x0101,(IntPtr)char.ToUpper((char)c),(IntPtr)0xC0010001);
  }
  public static void Down(IntPtr h,int vk){SendMessage(h,0x0100,(IntPtr)vk,(IntPtr)0x00010001);}
  public static void Up(IntPtr h,int vk){SendMessage(h,0x0101,(IntPtr)vk,(IntPtr)0xC0010001);}
}
"@

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'SylhetAttack.exe'
if (-not (Test-Path $exe)) { Write-Error 'build the game first'; exit 1 }
Set-Location $root

# the mission this scenario needs
$missionArg = switch ($Scenario) {
    'landing' { '--landing' }
    'rings'   { '--rings' }
    'tour'    { '--tour' }
    'school'  { '--school' }
    'crash'   { '--landing' }
    'title'   { $null }
    default   { '--free' }
}

# how long the scenario needs before the frame is worth saving
$delay = switch ($Scenario) {
    'title'   { 4 }
    'free'    { 5 }
    'cockpit' { 5 }
    'takeoff' { 34 }
    'cruise'  { 46 }
    'city'    { 62 }
    'landing' { 6 }
    'rings'   { 7 }
    'tour'    { 7 }
    'school'  { 6 }
    'pause'   { 4 }
    'help'    { 4 }
    'crash'   { 16 }
    default   { 5 }
}

$bmp = 'build\_shot.bmp'   # relative: the project path has spaces
$bmpFull = Join-Path $root $bmp
if (Test-Path $bmpFull) { Remove-Item $bmpFull -Force }
Get-Process -Name 'SylhetAttack' -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
Start-Sleep -Milliseconds 400

$argList = @()
if ($missionArg) { $argList += $missionArg }
$argList += @('--shot', $bmp, '--shotdelay', "$delay")
$proc = Start-Process -FilePath $exe -ArgumentList $argList -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 80; $i++) {
    Start-Sleep -Milliseconds 250
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne 0) { $hwnd = $proc.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { try { $proc.Kill() } catch {}; Write-Error 'no window'; exit 1 }
Start-Sleep -Milliseconds 900

$K_C = 0x43; $K_E = 0x45; $K_W = 0x57
$VK_UP = 0x26; $VK_DOWN = 0x28

function Tap([int]$k) { [GW]::Char($hwnd, $k); Start-Sleep -Milliseconds 220 }
function HoldKey([int]$vk, [int]$ms) { [GW]::Down($hwnd, $vk); Start-Sleep -Milliseconds $ms; [GW]::Up($hwnd, $vk) }
function HoldChar([int]$k, [int]$ms) {
    [GW]::SendMessage($hwnd, 0x0102, [IntPtr]$k, [IntPtr]1) | Out-Null
    Start-Sleep -Milliseconds $ms
    [GW]::Up($hwnd, [int][char]::ToUpper([char]$k))
}

switch ($Scenario) {
    'cockpit' { Tap $K_C; Start-Sleep -Milliseconds 1200 }
    'takeoff' {
        HoldChar $K_W 20000
        HoldKey $VK_UP 2500
        Start-Sleep -Milliseconds 3000
    }
    'cruise' {
        HoldChar $K_W 20000
        HoldKey $VK_UP 2500
        Start-Sleep -Milliseconds 14000
    }
    'city' {
        HoldChar $K_W 20000
        HoldKey $VK_UP 2500
        Start-Sleep -Milliseconds 30000
    }
    'school' { Tap $K_E; Start-Sleep -Milliseconds 3000 }
    'pause'  { Tap 0x1B; Start-Sleep -Milliseconds 900 }
    'help'   { Tap 0x48; Start-Sleep -Milliseconds 900 }
    'crash'  { HoldKey $VK_DOWN 5000; Start-Sleep -Milliseconds 8000 }
    default  { Start-Sleep -Milliseconds 500 }
}

# the game writes the frame and quits by itself
if (-not $proc.WaitForExit(60000)) { try { $proc.Kill() } catch {}; Write-Error 'the game did not save the shot'; exit 1 }
if (-not (Test-Path $bmpFull)) { Write-Error 'no frame written'; exit 1 }

$img = [System.Drawing.Image]::FromFile($bmpFull)
$dir = Split-Path -Parent $Out
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
$img.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output ("saved $Out  (" + $img.Width + "x" + $img.Height + ")")
$img.Dispose()
Remove-Item $bmpFull -Force