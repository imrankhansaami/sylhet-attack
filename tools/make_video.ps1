# make_video.ps1 - record the cinematic demo to a video file
#
#   powershell -ExecutionPolicy Bypass -File tools\make_video.ps1
#   powershell -ExecutionPolicy Bypass -File tools\make_video.ps1 -Seconds 30 -Size 960x540
#
# The game renders with a fixed time step while recording, so the file plays
# smoothly even on a slow machine: 108 seconds of demo at 20 fps becomes a valid
# MJPEG AVI (about 190 MB at 1280x720, less at smaller sizes). Windows Media
# Player, VLC, Films & TV and PowerPoint all play it. The demo itself is 108 s
# long, so the recording ends on its closing fade.
#
# If ffmpeg happens to be on PATH the script also writes an MP4 next to the AVI,
# which is much smaller and is what you would embed in the thesis folder.

param(
    [int] $Seconds = 108,
    [int] $Fps = 20,
    [string] $Size = '1280x720',
    [string] $Out = 'build\video\demo.avi'
)

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

$exe = Join-Path (Get-Location) 'SylhetAttack.exe'
$outAbs = Join-Path (Get-Location) $Out
$outDir = Split-Path -Parent $outAbs
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
if (Test-Path $outAbs) { Remove-Item $outAbs }

$wh = $Size -split 'x'
if ($wh.Count -ne 2) { Write-Output 'Size must look like 1280x720'; exit 1 }

$log = Join-Path $outDir 'record.log'
$args = @('--demo', '--video', $Out, '--videofps', "$Fps", '--videotimed', "$Seconds",
          '--size', $wh[0], $wh[1])

Write-Output ("recording {0} s of demo at {1} fps, {2} ..." -f $Seconds, $Fps, $Size)
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru `
     -RedirectStandardOutput $log -RedirectStandardError ($log + '.err')
$timeout = [int]($Seconds * 4000) + 120000        # generous: a slow machine may need it
if (-not $p.WaitForExit($timeout)) { try { $p.Kill() } catch {}; Write-Output 'recording timed out'; exit 1 }
$sw.Stop()

$done = Select-String -Path $log -Pattern 'VIDEO_DONE' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $done) { Write-Output 'no VIDEO_DONE line - see build\video\record.log'; exit 1 }
Write-Output $done.Line.Trim()
Write-Output ("render time: {0:N0} s" -f $sw.Elapsed.TotalSeconds)

$avi = Get-Item $outAbs
Write-Output ("file: {0} ({1:N1} MB, {2:N1} s of video)" -f $avi.Name, ($avi.Length / 1MB), ($Seconds))

# ---- structural check ----
& powershell -ExecutionPolicy Bypass -File (Join-Path (Get-Location) 'tools\verify_video.ps1') $Out

# ---- optional MP4 ----
$ff = Get-Command ffmpeg -ErrorAction SilentlyContinue
if ($ff)
{
    $mp4 = [System.IO.Path]::ChangeExtension($outAbs, '.mp4')
    Write-Output ''
    Write-Output 'ffmpeg found - writing an MP4 as well'
    & ffmpeg -y -loglevel error -i $outAbs -c:v libx264 -crf 20 -pix_fmt yuv420p $mp4
    if (Test-Path $mp4)
    {
        $m = Get-Item $mp4
        Write-Output ("mp4 : {0} ({1:N1} MB)" -f $m.Name, ($m.Length / 1MB))
    }
}
else
{
    Write-Output ''
    Write-Output 'ffmpeg is not installed, so only the AVI was written - it plays as it is.'
    Write-Output 'For a smaller MP4, install ffmpeg and run this script again, or use any'
    Write-Output 'converter (HandBrake, VLC: Media -> Convert/Save).'
}