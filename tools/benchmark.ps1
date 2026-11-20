# benchmark.ps1 - measured frame rate for the thesis
#
# Each run starts the game in a fixed viewpoint, lets it run for 10 seconds and
# writes one CSV row every half second (time_s,fps,state,x,y,z). The game prints
# a BENCH_SUMMARY line at the end and saves the last frame as a PNG next to the
# CSV, so every case has its own graph data and screen grab.
#
#   powershell -ExecutionPolicy Bypass -File tools\benchmark.ps1
#
# Results:  build\benchmark.txt   readable table
#           build\benchmark.csv   one row per case (averages)
#           build\bench\<case>.csv / .png   per-sample data and picture
#
# The first three city cases isolate one optimisation at a time on the same
# binary: culled (shipped), unculled (--nochunk) and no LOD (--nolod).

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

$exe = Join-Path (Get-Location) 'SylhetAttack.exe'
$benchDir = Join-Path (Get-Location) 'build\bench'
$outTxt = Join-Path (Get-Location) 'build\benchmark.txt'
$outCsv = Join-Path (Get-Location) 'build\benchmark.csv'
New-Item -ItemType Directory -Force -Path $benchDir | Out-Null

$seconds = 10
$cases = @(
    @{ name = 'campus_culled';   settings = 'shipped';              args = @('--free', '--at', '1900', '1905', '3') },
    @{ name = 'campus_unculled'; settings = '--nochunk';            args = @('--free', '--nochunk', '--at', '1900', '1905', '3') },
    @{ name = 'campus_nolod';    settings = '--nolod';              args = @('--free', '--nolod', '--at', '1900', '1905', '3') },
    @{ name = 'campus_none';     settings = '--nochunk --nolod';    args = @('--free', '--nochunk', '--nolod', '--at', '1900', '1905', '3') },
    @{ name = 'city_culled';     settings = 'shipped';              args = @('--free', '--at', '0', '0', '300') },
    @{ name = 'city_unculled';   settings = '--nochunk';            args = @('--free', '--nochunk', '--at', '0', '0', '300') },
    @{ name = 'city_nolod';      settings = '--nolod';              args = @('--free', '--nolod', '--at', '0', '0', '300') },
    @{ name = 'city_none';       settings = '--nochunk --nolod';    args = @('--free', '--nochunk', '--nolod', '--at', '0', '0', '300') },
    @{ name = 'demo_clock';      settings = 'cinematic demo t=35';  args = @('--demo', '--demotime', '35') }
)

$txt = @()
$txt += 'Sylhet Attack - frame rate benchmark'
$txt += ('date: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm'))
$txt += "window: 1280x720, 4x MSAA, 18 textures (144 MB decoded)"
$txt += "method: $seconds s per case, one sample every 0.5 s, average of the samples"
$txt += ''
$csv = @('case,avg_fps,min_fps,max_fps,samples,seconds,settings')

Write-Output ("Sylhet Attack benchmark - $seconds s per case")
Write-Output ''

foreach ($c in $cases)
{
    $csvPath = Join-Path $benchDir ($c.name + '.csv')
    $csvArg = "build\bench\$($c.name).csv"   # relative: the working folder has a space in its name
    $logPath = Join-Path $benchDir ($c.name + '.log')
    Remove-Item $csvPath, $logPath, (Join-Path $benchDir ($c.name + '.png')) -ErrorAction SilentlyContinue

    $a = @($c.args) + @('--benchmark', $csvArg, '--benchtimed', "$seconds")
    $p = Start-Process -FilePath $exe -ArgumentList $a -PassThru `
         -RedirectStandardOutput $logPath -RedirectStandardError ($logPath + '.err')
    if (-not $p.WaitForExit(120000)) { try { $p.Kill() } catch {} }

    $line = Select-String -Path $logPath -Pattern 'BENCH_SUMMARY' -ErrorAction SilentlyContinue |
            Select-Object -First 1
    if ($line)
    {
        $m = [regex]::Match($line.Line, 'avg=([0-9.]+) min=([0-9.]+) max=([0-9.]+) samples=(\d+) seconds=([0-9.]+)')
        if ($m.Success)
        {
            $avg = $m.Groups[1].Value
            $min = $m.Groups[2].Value
            $max = $m.Groups[3].Value
            $n = $m.Groups[4].Value
            $sec = $m.Groups[5].Value
            $row = "$($c.name),$avg,$min,$max,$n,$sec,$($c.settings)"
            $csv += $row
            $txt += ("{0,-18} avg {1,6} fps   min {2,6}   max {3,6}   ({4} samples, {5} s)   {6}" -f `
                     $c.name, $avg, $min, $max, $n, $sec, $c.settings)
            Write-Output ("{0,-18} avg {1,6} fps   min {2,6}   max {3,6}" -f $c.name, $avg, $min, $max)
            continue
        }
    }
    $txt += ("{0,-18} NO RESULT (see {1})" -f $c.name, $logPath)
    $csv += "$($c.name),,,,0,0,$($c.settings)"
    Write-Output ("{0,-18} NO RESULT" -f $c.name)
}

$txt += ''
$txt += 'culled = city chunks beyond 2200 m are skipped (shipped build)'
$txt += 'unculled = every chunk is submitted (--nochunk)'
$txt += 'nolod = every submitted chunk uses the detailed list (--nolod)'
$txt += 'none = no culling and no LOD, i.e. the pre-optimisation configuration'
$txt += 'per-sample CSV and a PNG for each case are in build\bench\'

Set-Content -Path $outTxt -Value $txt -Encoding UTF8
Set-Content -Path $outCsv -Value $csv -Encoding UTF8

Write-Output ''
Write-Output ("written: " + $outTxt)
Write-Output ("written: " + $outCsv)