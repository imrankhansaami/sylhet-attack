# smoke_test.ps1 - does every screen still start and render?
#
# Runs the title screen, all five missions, the cinematic demo, photo mode and
# the two documentation pages. For each one it checks that the process exits,
# that the capture file exists and is not blank, and (for missions) that the
# aircraft starts where the mission is supposed to put it.
#
#   powershell -ExecutionPolicy Bypass -File tools\smoke_test.ps1
#
# Results: build\smoke_test.txt  readable PASS/FAIL list
#          build\smoke_test.csv  one row per scenario
#          build\smoke\<name>.png
#
# The blank test samples a grid of pixels and requires a real spread of
# brightness and colour: a black or single-colour frame fails.

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
Add-Type -AssemblyName System.Drawing

$exe = Join-Path (Get-Location) 'SylhetAttack.exe'
$shotDir = Join-Path (Get-Location) 'build\smoke'
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

$cases = @(
    @{ name = 'title';    args = @();                              expect = $null },
    @{ name = 'free';     args = @('--free');                      expect = @(1600, 1875) },
    @{ name = 'school';   args = @('--school');                    expect = @(-2444, -2796) },
    @{ name = 'tour';     args = @('--tour');                      expect = @(1600, 1875) },
    @{ name = 'rings';    args = @('--rings');                     expect = @(-2466, -2195); tol = 150 },
    @{ name = 'landing';  args = @('--landing');                   expect = @(-4136, -1911) },
    @{ name = 'demo';     args = @('--demo', '--demotime', '30');   expect = $null },
    @{ name = 'photo';    args = @('--free', '--photo');            expect = $null },
    @{ name = 'controls'; args = @('--help');                       expect = $null },
    @{ name = 'credits';  args = @('--credits');                    expect = $null }
)

function Get-ImageStats([string] $path)
{
    $bmp = [System.Drawing.Bitmap]::FromFile($path)
    $sum = 0.0; $sum2 = 0.0; $n = 0
    $colours = New-Object 'System.Collections.Generic.HashSet[int]'
    for ($y = 0; $y -lt $bmp.Height; $y += 16)
    {
        for ($x = 0; $x -lt $bmp.Width; $x += 16)
        {
            $c = $bmp.GetPixel($x, $y)
            $b = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
            $sum += $b
            $sum2 += $b * $b
            $n++
            $key = ($c.R -shl 16) -bor ($c.G -shl 8) -bor $c.B
            [void]$colours.Add($key)
        }
    }
    $bmp.Dispose()
    $mean = $sum / $n
    $var = $sum2 / $n - $mean * $mean
    if ($var -lt 0) { $var = 0 }
    return @{ mean = $mean; stdev = [math]::Sqrt($var); colours = $colours.Count }
}

$rows = @('scenario,pass,fps_at_shot,x,y,z,png_bytes,mean,stdev,colours,note')
$lines = @('Sylhet Attack - screen smoke test',
           ('date: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm')),
           'fps_at_shot is the game''s smoothed value about two seconds after start - for',
           'frame-rate evidence use tools\benchmark.ps1, which samples for 10 s per case.',
           '')
$failed = 0

foreach ($c in $cases)
{
    $pngArg = "build\smoke\$($c.name).png"
    $pngAbs = Join-Path $shotDir ($c.name + '.png')
    $log = Join-Path $shotDir ($c.name + '.log')
    Remove-Item $pngAbs, $log -ErrorAction SilentlyContinue

    $a = @($c.args) + @('--shot', $pngArg, '--shotdelay', '2.0')
    $p = Start-Process -FilePath $exe -ArgumentList $a -PassThru `
         -RedirectStandardOutput $log -RedirectStandardError ($log + '.err')
    $exited = $p.WaitForExit(90000)
    if (-not $exited) { try { $p.Kill() } catch {} }

    $note = @()
    $fps = ''; $x = ''; $y = ''; $z = ''
    $bench = Select-String -Path $log -Pattern 'BENCH t=' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($bench)
    {
        $m = [regex]::Match($bench.Line, 'BENCH t=([0-9.]+) fps=([0-9.]+) x=(-?[0-9.]+) y=(-?[0-9.]+) z=(-?[0-9.]+)')
        if ($m.Success)
        {
            $fps = $m.Groups[2].Value
            $x = $m.Groups[3].Value; $y = $m.Groups[4].Value; $z = $m.Groups[5].Value
        }
        else { $note += 'bench-parse' }
    }
    else { $note += 'no-bench' }

    if (-not $exited) { $note += 'timeout' }

    $pngBytes = 0; $mean = 0; $sd = 0; $colours = 0
    if (Test-Path $pngAbs)
    {
        $pngBytes = (Get-Item $pngAbs).Length
        if ($pngBytes -lt 20000) { $note += 'png-small' }
        try
        {
            $st = Get-ImageStats $pngAbs
            $mean = [math]::Round($st.mean, 1)
            $sd = [math]::Round($st.stdev, 1)
            $colours = $st.colours
            if ($st.mean -lt 8) { $note += 'too-dark' }
            if ($st.stdev -lt 4) { $note += 'flat' }
            if ($st.colours -lt 8) { $note += 'few-colours' }
        }
        catch { $note += 'png-unreadable' }
    }
    else { $note += 'no-png' }

    if ($c.expect -and $x -ne '' -and $z -ne '')
    {
        $tol = if ($c.tol) { $c.tol } else { 60 }
        $dx = [math]::Abs([double]$x - $c.expect[0])
        $dz = [math]::Abs([double]$z - $c.expect[1])
        if ($dx -gt $tol -or $dz -gt $tol) { $note += ('start-pos=' + $x + '/' + $z) }
    }

    $pass = ($note.Count -eq 0)
    if (-not $pass) { $failed++ }
    $noteText = if ($pass) { 'ok' } else { $note -join '+' }

    $rows += "$($c.name),$([int]$pass),$fps,$x,$y,$z,$pngBytes,$mean,$sd,$colours,$noteText"
    $lines += ("{0,-9} {1,-4} fps {2,6}  pos {3},{4}  mean {5,5} sd {6,5} colours {7,4}  {8}" -f `
              $c.name, $(if ($pass) { 'PASS' } else { 'FAIL' }), $fps, $x, $z, $mean, $sd, $colours, $noteText)
    Write-Output ("{0,-9} {1}  {2}" -f $c.name, $(if ($pass) { 'PASS' } else { 'FAIL' }), $noteText)
}

$lines += ''
$lines += ("$($cases.Count - $failed) of $($cases.Count) screens passed")
Set-Content -Path (Join-Path (Get-Location) 'build\smoke_test.txt') -Value $lines -Encoding UTF8
Set-Content -Path (Join-Path (Get-Location) 'build\smoke_test.csv') -Value $rows -Encoding UTF8

Write-Output ''
Write-Output ("$($cases.Count - $failed) of $($cases.Count) screens passed")
Write-Output 'written: build\smoke_test.txt and build\smoke_test.csv'
if ($failed -gt 0) { exit 1 }