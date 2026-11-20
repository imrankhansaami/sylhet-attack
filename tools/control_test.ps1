# control_test.ps1 - press every control in turn, save a frame each time and
# report whether the game survived. Catches broken keys, crashes and dead-ends.
#
#   powershell -ExecutionPolicy Bypass -File tools\control_test.ps1
param([string]$OutDir = 'build\controls')

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class CT {
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  public static void Char(IntPtr h,int c){
    SendMessage(h,0x0102,(IntPtr)c,(IntPtr)1);
    System.Threading.Thread.Sleep(70);
    SendMessage(h,0x0101,(IntPtr)char.ToUpper((char)c),(IntPtr)0xC0010001);
  }
  public static void Down(IntPtr h,int vk){SendMessage(h,0x0100,(IntPtr)vk,(IntPtr)0x00010001);}
  public static void Up(IntPtr h,int vk){SendMessage(h,0x0101,(IntPtr)vk,(IntPtr)0xC0010001);}
}
"@

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'SylhetAttack.exe'
Set-Location $root
Get-Process -Name 'SylhetAttack' -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
Start-Sleep -Milliseconds 400
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }
Remove-Item (Join-Path $OutDir '*.png') -ErrorAction SilentlyContinue
Remove-Item 'shot_manual.png' -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $exe -ArgumentList '--free' -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 80; $i++) {
    Start-Sleep -Milliseconds 250
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne 0) { $hwnd = $proc.MainWindowHandle; break }
}
Start-Sleep -Milliseconds 1200

$VK_F2 = 0x71
function Snap([string]$name) {
    Remove-Item 'shot_manual.png' -ErrorAction SilentlyContinue
    [CT]::Down($hwnd, $VK_F2); Start-Sleep -Milliseconds 100; [CT]::Up($hwnd, $VK_F2)
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 100
        if (Test-Path 'shot_manual.png') { break }
    }
    if (-not (Test-Path 'shot_manual.png')) { Write-Output ("  !! no frame for " + $name); return }
    $img = [System.Drawing.Image]::FromFile((Join-Path $root 'shot_manual.png'))
    $img.Save((Join-Path $root (Join-Path $OutDir ($name + '.png'))), [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Remove-Item 'shot_manual.png' -Force
}

$steps = @(
    @{ k = 'm'; name = '01_map_zoom' },
    @{ k = 'l'; name = '02_labels' },
    @{ k = 'c'; name = '03_cockpit' },
    @{ k = 'c'; name = '04_chase' },
    @{ k = ']'; name = '05_sensitivity_up' },
    @{ k = '['; name = '06_sensitivity_down' },
    @{ k = '.'; name = '07_trim_up' },
    @{ k = ','; name = '08_trim_down' },
    @{ k = 'f'; name = '09_flaps' },
    @{ k = 'i'; name = '10_invert' },
    @{ k = 'h'; name = '11_help' },
    @{ k = 'x'; name = '12_back_from_help' },
    @{ k = 'p'; name = '13_photo' },
    @{ k = 'p'; name = '14_back_from_photo' },
    @{ k = 'r'; name = '15_restart' }
)

$ok = $true
foreach ($s in $steps) {
    [CT]::Char($hwnd, [int][char]$s.k)
    Start-Sleep -Milliseconds 700
    $proc.Refresh()
    if ($proc.HasExited) { Write-Output ("EXITED after key '" + $s.k + "' (code " + $proc.ExitCode + ")"); $ok = $false; break }
    Snap $s.name
    Write-Output ("key '" + $s.k + "' -> " + $s.name)
}

# Esc must open the pause menu, Esc again must close it
[CT]::Char($hwnd, 27); Start-Sleep -Milliseconds 600
Snap '16_pause'
[CT]::Char($hwnd, 27); Start-Sleep -Milliseconds 600
Snap '17_resume'

$proc.Refresh()
Write-Output ("survived: " + (-not $proc.HasExited))
if (-not $proc.HasExited) { $proc.Kill() }
Write-Output ("frames: " + (Get-ChildItem $OutDir -Filter *.png | Measure-Object).Count)