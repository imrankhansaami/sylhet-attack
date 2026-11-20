# verify_video.ps1 - check that a recorded .avi is a valid MJPEG file
#
#   powershell -ExecutionPolicy Bypass -File tools\verify_video.ps1 build\video\demo.avi
#
# It walks the RIFF structure, reads the frame count and rate out of the stream
# headers, follows the idx1 index to every frame chunk and checks that each one
# starts with a JPEG SOI marker and ends with EOI. Exit code 1 if anything is off.

param([Parameter(Mandatory = $true)][string] $Path)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Path)) { Write-Output "missing: $Path"; exit 1 }
$full = (Resolve-Path $Path).Path
$bytes = [System.IO.File]::ReadAllBytes($full)

function U32([int] $at) { return [BitConverter]::ToUInt32($bytes, $at) }
function Tag([int] $at) { return [System.Text.Encoding]::ASCII.GetString($bytes, $at, 4) }

if ((Tag 0) -ne 'RIFF') { Write-Output 'not a RIFF file'; exit 1 }
if ((Tag 8) -ne 'AVI ') { Write-Output 'not an AVI file'; exit 1 }
$riffSize = U32 4
if (($riffSize + 8) -ne $bytes.Length) { Write-Output ("riff size {0} does not match file size {1}" -f $riffSize, $bytes.Length); exit 1 }

# ---- top level chunks ----
$moviStart = -1; $moviEnd = -1; $idxStart = -1
$avihFrames = 0; $avihWidth = 0; $avihHeight = 0; $avihUsec = 0
$strhRate = 0; $strhScale = 1; $strhLength = 0

$p = 12
while ($p + 8 -le $bytes.Length)
{
    $id = Tag $p
    $size = U32 ($p + 4)
    $data = $p + 8
    if ($id -eq 'LIST')
    {
        $listType = Tag $data
        if ($listType -eq 'movi') { $moviStart = $data + 4; $moviEnd = $data + $size }
        elseif ($listType -eq 'hdrl')
        {
            $q = $data + 4
            $hdrlEnd = $data + $size
            while ($q + 8 -le $hdrlEnd)
            {
                $hid = Tag $q
                $hsize = U32 ($q + 4)
                $hdata = $q + 8
                if ($hid -eq 'avih')
                {
                    $avihUsec = U32 $hdata
                    $avihFrames = U32 ($hdata + 16)
                    $avihWidth = U32 ($hdata + 32)
                    $avihHeight = U32 ($hdata + 36)
                }
                elseif ($hid -eq 'LIST' -and (Tag $hdata) -eq 'strl')
                {
                    $r = $hdata + 4
                    $strlEnd = $hdata + $hsize
                    while ($r + 8 -le $strlEnd)
                    {
                        if ((Tag $r) -eq 'strh')
                        {
                            $strhScale = U32 ($r + 8 + 20)
                            $strhRate = U32 ($r + 8 + 24)
                            $strhLength = U32 ($r + 8 + 32)
                        }
                        $r += 8 + (U32 ($r + 4)) + ((U32 ($r + 4)) % 2)
                    }
                }
                $q += 8 + $hsize + ($hsize % 2)
            }
        }
    }
    elseif ($id -eq 'idx1') { $idxStart = $data }
    $p += 8 + $size + ($size % 2)
}

if ($moviStart -lt 0) { Write-Output 'no movi list'; exit 1 }
if ($idxStart -lt 0) { Write-Output 'no idx1 index'; exit 1 }

# ---- walk the frames in the movi list ----
$frames = 0; $badJpeg = 0; $offsets = New-Object 'System.Collections.Generic.List[int]'
$sizes = New-Object 'System.Collections.Generic.List[int]'
$q = $moviStart
while ($q + 8 -le $moviEnd)
{
    $id = Tag $q
    $size = [int](U32 ($q + 4))
    if ($id -ne '00dc') { break }
    $data = $q + 8
    $offsets.Add($q - $moviStart + 4)      # first frame sits 4 bytes into the 'movi' data
    $sizes.Add($size)
    if ($bytes[$data] -ne 0xFF -or $bytes[$data + 1] -ne 0xD8) { $badJpeg++ }
    if ($bytes[$data + $size - 2] -ne 0xFF -or $bytes[$data + $size - 1] -ne 0xD9) { $badJpeg++ }
    $frames++
    $q = $data + $size + ($size % 2)
}

# ---- compare with the index ----
$idxCount = [int](U32 ($idxStart - 4)) / 16
$idxMismatch = 0
for ($i = 0; $i -lt $idxCount; $i++)
{
    $e = $idxStart + $i * 16
    $off = [int](U32 ($e + 8))
    $siz = [int](U32 ($e + 12))
    if ($i -lt $offsets.Count)
    {
        if ($off -ne $offsets[$i] -or $siz -ne $sizes[$i]) { $idxMismatch++ }
    }
    else { $idxMismatch++ }
}

$fps = if ($strhScale -gt 0) { $strhRate / $strhScale } else { 0 }
$seconds = if ($fps -gt 0) { [math]::Round($frames / $fps, 2) } else { 0 }
$mb = [math]::Round($bytes.Length / 1MB, 1)

Write-Output ("file      : {0} ({1} MB)" -f $full, $mb)
Write-Output ("video     : {0}x{1} at {2} fps" -f $avihWidth, $avihHeight, $fps)
Write-Output ("frames    : {0} in movi, {1} in idx1, {2} in the headers" -f $frames, $idxCount, $avihFrames)
Write-Output ("duration  : {0} s" -f $seconds)
Write-Output ("jpeg      : {0} frames checked, {1} bad markers" -f $frames, $badJpeg)
Write-Output ("index     : {0} mismatched entries" -f $idxMismatch)

$ok = ($frames -gt 0) -and ($badJpeg -eq 0) -and ($idxMismatch -eq 0) -and ($frames -eq $idxCount)
Write-Output ("result    : {0}" -f $(if ($ok) { 'VALID' } else { 'PROBLEM' }))
if (-not $ok) { exit 1 }