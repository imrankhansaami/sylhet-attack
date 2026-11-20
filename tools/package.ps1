# package.ps1 - build the two things you actually hand in
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1 -IncludeVideo:$false
#
# Produces:
#   dist\SylhetAttack-run\      what you copy to a USB stick: the game, the
#                                  textures, the licences, the evidence files and
#                                  the defence script (plus the demo video)
#   dist\SylhetAttack-run.zip   the same folder zipped
#   dist\SylhetAttack-source.zip  every source file needed to rebuild it
#
# The run folder is deliberately self-contained: no installer, no DLLs, no
# console window - double-click SylhetAttack.exe (or run.bat).

param(
    [switch] $IncludeVideo = $true,
    [switch] $SkipBuild = $false
)

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$root = (Get-Location).Path

# ---------------------------------------------------------------- build
if (-not $SkipBuild)
{
    Write-Output 'building the release executable ...'
    cmd /c "build.bat nopause < nul" | Select-Object -Last 2
}
if (-not (Test-Path 'SylhetAttack.exe'))
{
    Write-Output 'SylhetAttack.exe is missing - fix the build first'; exit 1
}

$dist = Join-Path $root 'dist'
$run = Join-Path $dist 'SylhetAttack-run'
if (Test-Path $run) { Remove-Item $run -Recurse -Force }
New-Item -ItemType Directory -Force -Path $run | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $run 'evidence') | Out-Null

# ------------------------------------------------------------ run folder
Copy-Item 'SylhetAttack.exe' $run -Force
Copy-Item 'run.bat' $run -Force
Copy-Item 'README.md' $run -Force
Copy-Item 'LICENSES.txt' $run -Force
New-Item -ItemType Directory -Force -Path (Join-Path $run 'docs') | Out-Null
Copy-Item 'docs\DEFENCE_SCRIPT.md' (Join-Path $run 'docs') -Force
if (Test-Path 'docs\THESIS_OUTLINE.md') { Copy-Item 'docs\THESIS_OUTLINE.md' (Join-Path $run 'docs') -Force }

# textures (optional at run time, but they are what makes it look right)
New-Item -ItemType Directory -Force -Path (Join-Path $run 'assets\textures') | Out-Null
Copy-Item 'assets\textures\*.jpg' (Join-Path $run 'assets\textures') -Force

# a read-me for whoever plugs in the USB stick
$first = @'
SYLHET ATTACK - how to run this copy
=============================================

1. Double-click SylhetAttack.exe  (or run.bat). Nothing is installed and no
   DLLs are needed: the game is a single static executable.

2. On the title screen: click a mission or press 1-5, or press D for the
   cinematic city demo. Press H while flying for the full control list.

3. The video of the cinematic demo is in video\demo.avi (Windows Media Player,
   VLC or Films & TV play it as it is).

4. All the measurements quoted in the defence are in evidence\ :
      evidence.txt      source lines, scene counts, binary size, harness results
      benchmark.txt     frame rate of the shipped build versus each optimisation
      smoke_test.txt    every screen started and rendered (10 of 10)
      bench\*.csv       the frame rate sampled every 0.5 s for each case

5. To rebuild from source, take SylhetAttack-source.zip, unpack it and run
   build.bat (MinGW g++ required; freeglut is vendored and builds itself).

Licences and attribution: LICENSES.txt
'@
Set-Content -Path (Join-Path $run 'RUN_ME_FIRST.txt') -Value $first -Encoding UTF8

# evidence and measurements
foreach ($f in @('benchmark.txt', 'benchmark.csv', 'smoke_test.txt', 'smoke_test.csv',
                 'evidence.txt', 'evidence.csv', 'flight_test_out.txt', 'stats.txt'))
{
    $p = Join-Path $root ('build\' + $f)
    if (Test-Path $p) { Copy-Item $p (Join-Path $run 'evidence') -Force }
}
$benchDir = Join-Path $root 'build\bench'
if (Test-Path $benchDir)
{
    New-Item -ItemType Directory -Force -Path (Join-Path $run 'evidence\bench') | Out-Null
    Copy-Item (Join-Path $benchDir '*.csv') (Join-Path $run 'evidence\bench') -Force
}

# the recorded demo
if ($IncludeVideo)
{
    $avi = Join-Path $root 'build\video\demo.avi'
    if (Test-Path $avi)
    {
        New-Item -ItemType Directory -Force -Path (Join-Path $run 'video') | Out-Null
        Copy-Item $avi (Join-Path $run 'video') -Force
    }
    else { Write-Output 'note: build\video\demo.avi not found - run tools\make_video.ps1 for the video' }
}

# --------------------------------------------------------------- sources
$srcZip = Join-Path $dist 'SylhetAttack-source.zip'
if (Test-Path $srcZip) { Remove-Item $srcZip -Force }
$staging = Join-Path $dist '_src'
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

foreach ($item in @('src', 'tools', 'docs', 'third_party', 'assets'))
{
    if (Test-Path $item) { Copy-Item $item (Join-Path $staging $item) -Recurse -Force }
}
foreach ($f in @('build.bat', 'run.bat', 'README.md', 'LICENSES.txt'))
{
    if (Test-Path $f) { Copy-Item $f $staging -Force }
}
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $srcZip -CompressionLevel Optimal
Remove-Item $staging -Recurse -Force

# ------------------------------------------------------------- run zip
$runZip = Join-Path $dist 'SylhetAttack-run.zip'
if (Test-Path $runZip) { Remove-Item $runZip -Force }
Compress-Archive -Path (Join-Path $run '*') -DestinationPath $runZip -CompressionLevel Optimal

# ---------------------------------------------------------------- report
function Show-Size([string] $p)
{
    if (Test-Path $p)
    {
        $i = Get-Item $p
        if ($i.PSIsContainer)
        {
            $sum = (Get-ChildItem $p -Recurse -File | Measure-Object -Property Length -Sum).Sum
            Write-Output ("  {0,-40} {1,8:N1} MB" -f $i.Name, ($sum / 1MB))
        }
        else { Write-Output ("  {0,-40} {1,8:N1} MB" -f $i.Name, ($i.Length / 1MB)) }
    }
}

Write-Output ''
Write-Output 'packaged:'
Show-Size $run
Show-Size (Join-Path $run 'assets\textures')
Show-Size (Join-Path $run 'video\demo.avi')
Show-Size $runZip
Show-Size $srcZip
Write-Output ''
Write-Output 'run folder contents:'
Get-ChildItem $run | Select-Object -ExpandProperty Name | ForEach-Object { Write-Output ('  ' + $_) }