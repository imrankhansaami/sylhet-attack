# evidence.ps1 - collect the numbers the thesis quotes, in one run
#
#   powershell -ExecutionPolicy Bypass -File tools\evidence.ps1
#
# Results: build\evidence.txt   a readable sheet
#          build\evidence.csv   the same numbers as metric,value rows
#
# It counts the source lines, asks the game itself for the scene statistics
# (--stats), measures the assets and the binary, and re-runs the headless
# flight-model harness if it has been built.

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

$txt = @()
$csv = @('metric,value,unit,note')
$txt += 'Sylhet Attack - evidence sheet'
$txt += ('date: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm'))
$txt += ''

# ---------------------------------------------------------------- source
$txt += '1. Source size'
$files = @(Get-ChildItem 'src\*.cpp', 'src\*.h' | Sort-Object Name)
$total = 0
foreach ($f in $files)
{
    $n = (Get-Content $f.FullName | Measure-Object -Line).Lines
    $total += $n
    $txt += ("   {0,-18} {1,6} lines" -f $f.Name, $n)
    $csv += ("loc_{0},{1},lines,{2}" -f $f.BaseName, $n, $f.Extension.TrimStart('.'))
}
$testLines = 0
if (Test-Path 'tools\flight_test.cpp')
{
    $testLines = (Get-Content 'tools\flight_test.cpp' | Measure-Object -Line).Lines
}
$txt += ("   {0,-18} {1,6} lines" -f 'TOTAL', $total)
$txt += ("   headless flight harness: $testLines lines in tools\flight_test.cpp")
$csv += "loc_total,$total,lines,src only"
$csv += "loc_flight_harness,$testLines,lines,tools/flight_test.cpp"

# ------------------------------------------------------- scene statistics
$txt += ''
$txt += '2. Scene content (SylhetAttack.exe --stats)'
$statsLog = Join-Path (Get-Location) 'build\stats.txt'
Remove-Item $statsLog -ErrorAction SilentlyContinue
$p = Start-Process -FilePath (Join-Path (Get-Location) 'SylhetAttack.exe') `
     -ArgumentList @('--stats') -PassThru `
     -RedirectStandardOutput $statsLog -RedirectStandardError ($statsLog + '.err')
if (-not $p.WaitForExit(120000)) { try { $p.Kill() } catch {} }

$statsLines = @(Select-String -Path $statsLog -Pattern '^\[stats\]' -ErrorAction SilentlyContinue)
foreach ($l in $statsLines)
{
    $txt += ('   ' + $l.Line)
    $m = [regex]::Match($l.Line, '^\[stats\]\s+(\S+)\s+(.*)$')
    if ($m.Success)
    {
        $group = $m.Groups[1].Value
        foreach ($pair in ($m.Groups[2].Value -split '\s+'))
        {
            $kv = $pair -split '=', 2
            if ($kv.Count -eq 2) { $csv += ("$group`_$($kv[0]),$($kv[1]),count,") }
        }
    }
}
if ($statsLines.Count -eq 0) { $txt += '   (no --stats output - run the exe once)' }

# ---------------------------------------------------------------- assets
$txt += ''
$txt += '3. Assets'
$texFiles = @(Get-ChildItem 'assets\textures\*.jpg' -ErrorAction SilentlyContinue)
$texBytes = 0
foreach ($f in $texFiles) { $texBytes += $f.Length }
$txt += ("   textures on disk: {0} files, {1:N1} MB (CC0, Poly Haven)" -f $texFiles.Count, ($texBytes / 1MB))
$csv += "textures_on_disk,$($texFiles.Count),files,assets/textures/*.jpg"
$csv += ("textures_on_disk_mb,{0:N1},MB,JPEG on disk" -f ($texBytes / 1MB))

$usedTex = 0; $usedMb = 0
foreach ($l in $statsLines)
{
    $m = [regex]::Match($l.Line, 'textures\s+loaded=(\d+) decodedMB=(\d+)')
    if ($m.Success) { $usedTex = [int]$m.Groups[1].Value; $usedMb = [int]$m.Groups[2].Value }
}
$txt += ("   textures used by the game: {0} (about {1} MB decoded, before mipmaps)" -f $usedTex, $usedMb)

# ---------------------------------------------------------------- binary
$txt += ''
$txt += '4. Binary'
$exe = Get-Item 'SylhetAttack.exe'
$txt += ("   SylhetAttack.exe: {0:N0} KB, statically linked (no DLLs to install)" -f ($exe.Length / 1KB))
$csv += ("exe_kb,{0:N0},KB,release build" -f ($exe.Length / 1KB))

# -------------------------------------------------------- flight harness
$txt += ''
$txt += '5. Flight model harness (tools\flight_test.cpp)'
$harness = Join-Path (Get-Location) 'build\flight_test.exe'
if (Test-Path $harness)
{
    $outFile = Join-Path (Get-Location) 'build\flight_test_out.txt'
    $h = Start-Process -FilePath $harness -PassThru -RedirectStandardOutput $outFile -RedirectStandardError ($outFile + '.err')
    if (-not $h.WaitForExit(180000)) { try { $h.Kill() } catch {} }
    $hLines = @(Get-Content $outFile -ErrorAction SilentlyContinue)
    $checks = @($hLines | Select-String -Pattern '^----------------\s+\d+\.')
    $bad = @($hLines | Select-String -Pattern 'NaN=yes|FAIL|water on the runway: yes')
    $txt += ("   {0} numbered checks, output in build\flight_test_out.txt" -f $checks.Count)
    foreach ($c in $checks) { $txt += ('     - ' + $c.Line.Trim('-', ' ')) }
    if ($bad.Count -gt 0)
    {
        $txt += '   FAILURES:'
        foreach ($b in $bad) { $txt += ('     ! ' + $b.Line) }
    }
    else { $txt += '   no failures reported' }
    $csv += "flight_checks,$($checks.Count),checks,headless harness"
    $csv += "flight_failures,$($bad.Count),checks,NaN or explicit failure"
}
else
{
    $txt += '   build\flight_test.exe not found - build it with the command in README.md'
    $csv += 'flight_checks,0,checks,harness not built'
}

# ------------------------------------------------------------ frame rate
$txt += ''
$txt += '6. Frame rate and screen checks'
$benchTxt = Join-Path (Get-Location) 'build\benchmark.txt'
if (Test-Path $benchTxt) { $txt += '   build\benchmark.txt (tools\benchmark.ps1)' } else { $txt += '   run tools\benchmark.ps1' }
$smokeCsv = Join-Path (Get-Location) 'build\smoke_test.csv'
if (Test-Path $smokeCsv)
{
    $rows = @(Get-Content $smokeCsv)
    $ok = @($rows | Select-String -Pattern ',1,').Count
    $txt += ("   smoke test: {0} of {1} screens passed (build\smoke_test.csv)" -f $ok, [math]::Max(0, $rows.Count - 1))
    $csv += "smoke_pass,$ok,screens,build/smoke_test.csv"
}
else { $txt += '   run tools\smoke_test.ps1' }

Set-Content -Path (Join-Path (Get-Location) 'build\evidence.txt') -Value $txt -Encoding UTF8
Set-Content -Path (Join-Path (Get-Location) 'build\evidence.csv') -Value $csv -Encoding UTF8

$txt | ForEach-Object { Write-Output $_ }
Write-Output ''
Write-Output 'written: build\evidence.txt and build\evidence.csv'