# get_textures.ps1 - download the CC0 photographic textures the world uses
#
#   powershell -ExecutionPolicy Bypass -File tools\get_textures.ps1
#   powershell -ExecutionPolicy Bypass -File tools\get_textures.ps1 -Force     # re-download
#   powershell -ExecutionPolicy Bypass -File tools\get_textures.ps1 -Only wall # one group
#
# Everything comes from Poly Haven (https://polyhaven.com) under the CC0 1.0
# public-domain dedication, so the files may be redistributed with the project.
# The list below is the mapping the game expects: local file name -> Poly Haven
# slug + resolution. 2K is used for the surfaces that fill the screen (ground,
# runways, the common wall and roof), 1K for the variety textures, which keeps
# the decoded texture budget near 300 MB instead of several gigabytes.
#
# Sources are recorded in assets\textures\_sources.txt for the report.

param(
    [switch] $Force,
    [string] $Only = ''
)

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$dest = Join-Path (Get-Location) 'assets\textures'
New-Item -ItemType Directory -Force -Path $dest | Out-Null

$BASE = 'https://dl.polyhaven.org/file/ph-assets/Textures/jpg'

# group, slug, local file, resolution
$list = @(
    # ---------------- ground and terrain ----------------
    @{ g = 'ground'; slug = 'leafy_grass';           file = 'grass_leafy.jpg';      res = '2k' },
    @{ g = 'ground'; slug = 'sparse_grass';          file = 'grass_sparse.jpg';     res = '1k' },
    @{ g = 'ground'; slug = 'withered_grass';        file = 'grass_dry.jpg';        res = '1k' },
    @{ g = 'ground'; slug = 'grass_path_2';          file = 'grass_path.jpg';       res = '1k' },
    @{ g = 'ground'; slug = 'forest_ground_04';      file = 'forest_floor.jpg';     res = '2k' },
    @{ g = 'ground'; slug = 'brown_mud_03';          file = 'mud.jpg';              res = '1k' },
    @{ g = 'ground'; slug = 'brown_mud_dry';         file = 'mud_dry.jpg';          res = '1k' },
    @{ g = 'ground'; slug = 'mud_cracked_dry_03';    file = 'mud_cracked.jpg';      res = '1k' },
    @{ g = 'ground'; slug = 'red_laterite_soil_stones'; file = 'soil_red.jpg';      res = '2k' },
    @{ g = 'ground'; slug = 'dirt_aerial_02';        file = 'soil_aerial.jpg';      res = '2k' },
    @{ g = 'ground'; slug = 'dry_mud_field_001';     file = 'field_dry.jpg';        res = '2k' },
    @{ g = 'ground'; slug = 'sandy_gravel';          file = 'gravel_sandy.jpg';     res = '1k' },
    @{ g = 'ground'; slug = 'gravelly_sand';         file = 'sand_gravel.jpg';      res = '1k' },
    @{ g = 'ground'; slug = 'coast_sand_01';         file = 'sand_coast.jpg';       res = '1k' },
    @{ g = 'ground'; slug = 'rocky_terrain_02';      file = 'rock_terrain.jpg';     res = '2k' },
    @{ g = 'ground'; slug = 'aerial_grass_rock';     file = 'grass_rock.jpg';       res = '2k' },
    @{ g = 'ground'; slug = 'rocks_ground_02';       file = 'rocks_ground.jpg';     res = '1k' },
    @{ g = 'ground'; slug = 'ganges_river_pebbles';  file = 'pebbles.jpg';          res = '1k' },
    @{ g = 'ground'; slug = 'dry_riverbed_rock';     file = 'riverbed.jpg';         res = '1k' },

    # ---------------- roads, pavements, kerbs ----------------
    @{ g = 'road'; slug = 'asphalt_02';              file = 'asphalt2.jpg';         res = '2k' },
    @{ g = 'road'; slug = 'asphalt_pit_lane';        file = 'road_worn.jpg';        res = '2k' },
    @{ g = 'road'; slug = 'worn_asphalt';            file = 'asphalt_worn.jpg';     res = '1k' },
    @{ g = 'road'; slug = 'clean_asphalt';           file = 'asphalt_clean.jpg';    res = '1k' },
    @{ g = 'road'; slug = 'gravel_road';             file = 'road_gravel.jpg';      res = '1k' },
    @{ g = 'road'; slug = 'concrete_pavement_02';    file = 'pavement.jpg';         res = '2k' },
    @{ g = 'road'; slug = 'cobblestone_floor_08';    file = 'cobble.jpg';           res = '1k' },
    @{ g = 'road'; slug = 'checkered_pavement_tiles'; file = 'paving_tiles.jpg';    res = '1k' },
    @{ g = 'road'; slug = 'square_floor_patern_01';  file = 'paving_square.jpg';    res = '1k' },
    @{ g = 'road'; slug = 'red_sandstone_pavement';  file = 'paving_red.jpg';       res = '1k' },

    # ---------------- walls ----------------
    @{ g = 'wall'; slug = 'painted_plaster_wall';    file = 'wall_painted.jpg';     res = '2k' },
    @{ g = 'wall'; slug = 'beige_wall_001';          file = 'wall_beige.jpg';       res = '2k' },
    @{ g = 'wall'; slug = 'plaster_grey_04';         file = 'plaster_grey.jpg';     res = '1k' },
    @{ g = 'wall'; slug = 'white_rough_plaster';     file = 'plaster_rough.jpg';    res = '1k' },
    @{ g = 'wall'; slug = 'plastered_stone_wall';    file = 'plaster_stone.jpg';    res = '1k' },
    @{ g = 'wall'; slug = 'cracked_concrete_wall';   file = 'concrete_cracked.jpg'; res = '2k' },
    @{ g = 'wall'; slug = 'concrete_wall_007';       file = 'concrete_panel.jpg';   res = '1k' },
    @{ g = 'wall'; slug = 'concrete_wall_008';       file = 'concrete_wall2.jpg';   res = '1k' },
    @{ g = 'wall'; slug = 'concrete_layers_02';      file = 'concrete_layers.jpg';  res = '1k' },
    @{ g = 'wall'; slug = 'painted_concrete_02';     file = 'concrete_painted.jpg'; res = '1k' },
    @{ g = 'wall'; slug = 'red_brick_03';            file = 'brick_red2.jpg';       res = '2k' },
    @{ g = 'wall'; slug = 'brick_wall_001';          file = 'brick_wall.jpg';       res = '2k' },
    @{ g = 'wall'; slug = 'brick_wall_006';          file = 'brick_wall2.jpg';      res = '1k' },
    @{ g = 'wall'; slug = 'red_bricks_04';           file = 'brick_red3.jpg';       res = '1k' },
    @{ g = 'wall'; slug = 'castle_brick_02_red';     file = 'brick_castle.jpg';     res = '1k' },
    @{ g = 'wall'; slug = 'stone_brick_wall_001';    file = 'brick_stone.jpg';      res = '1k' },
    @{ g = 'wall'; slug = 'concrete_brick_wall_001'; file = 'brick_concrete.jpg';   res = '1k' },
    @{ g = 'wall'; slug = 'rough_block_wall';        file = 'block_rough.jpg';      res = '1k' },
    @{ g = 'wall'; slug = 'factory_brick';           file = 'brick_factory.jpg';    res = '1k' },
    @{ g = 'wall'; slug = 'medieval_red_brick';      file = 'brick_old.jpg';        res = '1k' },
    @{ g = 'wall'; slug = 'bamboo_wall';             file = 'bamboo.jpg';           res = '1k' },
    @{ g = 'wall'; slug = 'bamboo_wall_02';          file = 'bamboo2.jpg';          res = '1k' },
    @{ g = 'wall'; slug = 'weathered_brown_planks';  file = 'plank_old.jpg';        res = '1k' },
    @{ g = 'wall'; slug = 'brown_planks_03';         file = 'plank_brown.jpg';      res = '1k' },
    @{ g = 'wall'; slug = 'wood_peeling_paint_weathered'; file = 'plank_painted.jpg'; res = '1k' },
    @{ g = 'wall'; slug = 'blue_painted_planks';     file = 'plank_blue.jpg';       res = '1k' },
    @{ g = 'wall'; slug = 'rectangular_facade_tiles'; file = 'facade_tile.jpg';     res = '1k' },
    @{ g = 'wall'; slug = 'rectangular_facade_tiles_02'; file = 'facade_tile2.jpg'; res = '1k' },
    @{ g = 'wall'; slug = 'stone_wall';              file = 'stone_wall.jpg';       res = '1k' },
    @{ g = 'wall'; slug = 'sandstone_brick_wall_01'; file = 'sandstone_wall.jpg';   res = '1k' },
    @{ g = 'wall'; slug = 'large_sandstone_blocks';  file = 'sandstone_block.jpg';  res = '1k' },
    @{ g = 'wall'; slug = 'marble_01';               file = 'marble.jpg';           res = '2k' },

    # ---------------- metal, tin, shutters ----------------
    @{ g = 'metal'; slug = 'green_metal_rust';       file = 'metal_green.jpg';      res = '1k' },
    @{ g = 'metal'; slug = 'rusty_metal_02';         file = 'metal_rust.jpg';       res = '1k' },
    @{ g = 'metal'; slug = 'rusty_metal_03';         file = 'metal_rust2.jpg';      res = '1k' },
    @{ g = 'metal'; slug = 'metal_plate';            file = 'metal_plate.jpg';      res = '1k' },
    @{ g = 'metal'; slug = 'blue_metal_plate';       file = 'metal_blue.jpg';       res = '1k' },
    @{ g = 'metal'; slug = 'painted_metal_shutter';  file = 'metal_shutter.jpg';    res = '1k' },
    @{ g = 'metal'; slug = 'corrugated_iron_02';     file = 'tin2.jpg';             res = '2k' },
    @{ g = 'metal'; slug = 'factory_wall';           file = 'metal_siding.jpg';     res = '2k' },

    # ---------------- roofs ----------------
    @{ g = 'roof'; slug = 'clay_roof_tiles_02';      file = 'roof_clay.jpg';        res = '2k' },
    @{ g = 'roof'; slug = 'ceramic_roof_01';         file = 'roof_ceramic.jpg';     res = '1k' },
    @{ g = 'roof'; slug = 'roof_tiles_14';           file = 'roof_tiles2.jpg';      res = '1k' },
    @{ g = 'roof'; slug = 'roof_09';                 file = 'roof_09.jpg';          res = '1k' },
    @{ g = 'roof'; slug = 'grey_roof_01';            file = 'roof_grey.jpg';        res = '1k' },
    @{ g = 'roof'; slug = 'grey_roof_tiles_02';      file = 'roof_tiles_grey.jpg';  res = '1k' },
    @{ g = 'roof'; slug = 'roof_slates_03';          file = 'roof_slate.jpg';       res = '1k' },
    @{ g = 'roof'; slug = 'thatch_roof_angled';      file = 'roof_thatch.jpg';      res = '1k' },

    # ---------------- nature, floors, fabric ----------------
    @{ g = 'nature'; slug = 'bark_brown_02';         file = 'bark2.jpg';            res = '1k' },
    @{ g = 'nature'; slug = 'pine_bark';             file = 'bark_pine.jpg';        res = '1k' },
    @{ g = 'nature'; slug = 'palm_tree_bark';        file = 'bark_palm.jpg';        res = '1k' },
    @{ g = 'detail'; slug = 'granite_tile_03';       file = 'granite.jpg';          res = '1k' },
    @{ g = 'detail'; slug = 'terrazzo_tiles';        file = 'terrazzo.jpg';         res = '1k' },
    @{ g = 'detail'; slug = 'seaworn_stone_tiles';   file = 'stone_tiles.jpg';      res = '1k' },
    @{ g = 'detail'; slug = 'interior_tiles';        file = 'tile_wall.jpg';        res = '1k' },
    @{ g = 'fabric'; slug = 'cotton_jersey';         file = 'fabric.jpg';           res = '1k' },
    @{ g = 'fabric'; slug = 'hessian_380';            file = 'fabric2.jpg';          res = '1k' },
    @{ g = 'fabric'; slug = 'hessian_230';           file = 'hessian.jpg';          res = '1k' }
)

$todo = $list
if ($Only) { $todo = $list | Where-Object { $_.g -eq $Only } }

$sources = @()
$sources += 'Poly Haven textures used by Sylhet Attack'
$sources += 'All files are CC0 1.0 (public domain dedication) - https://polyhaven.com/license'
$sources += ('downloaded: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm'))
$sources += ''
$sources += ("{0,-18} {1,-32} {2,-26} {3}" -f 'group', 'poly haven slug', 'local file', 'resolution')

$ok = 0; $skipped = 0; $failed = @()
foreach ($t in $todo)
{
    $file = Join-Path $dest $t.file
    if ((Test-Path $file) -and ((Get-Item $file).Length -gt 10240) -and -not $Force)
    {
        $skipped++
        $sources += ("{0,-18} {1,-32} {2,-26} {3}" -f $t.g, $t.slug, $t.file, $t.res)
        continue
    }

    # Ask the files API for the exact URL: Poly Haven is not consistent with the
    # file name (_diff_ for most, _diffuse_ for some, _col_ for a few).
    $url = $null
    $leaf = ''
    try
    {
        $info = Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$($t.slug)" -TimeoutSec 60
        $diff = $info.Diffuse
        if (-not $diff) { $diff = $info.col_1 }
        if (-not $diff) { $diff = $info.col_01 }
        foreach ($res in @($t.res, '2k', '1k', '4k'))
        {
            if ($diff -and $diff.$res -and $diff.$res.jpg)
            {
                $url = $diff.$res.jpg.url
                $leaf = Split-Path $url -Leaf
                break
            }
        }
    }
    catch { }

    # fall back to the usual CDN pattern if the API did not help
    if (-not $url)
    {
        foreach ($res in @($t.res, '2k', '1k', '4k'))
        {
            $tryUrl = "$BASE/$res/$($t.slug)/$($t.slug)_diff_$res.jpg"
            try
            {
                Invoke-WebRequest -Uri $tryUrl -Method Head -UseBasicParsing -TimeoutSec 30 | Out-Null
                $url = $tryUrl; $leaf = Split-Path $tryUrl -Leaf
                break
            }
            catch { }
        }
    }

    $done = $false
    if ($url)
    {
        try
        {
            Invoke-WebRequest -Uri $url -OutFile $file -UseBasicParsing -TimeoutSec 180
            if ((Get-Item $file).Length -gt 10240)
            {
                $done = $true
                $mb = [math]::Round((Get-Item $file).Length / 1MB, 2)
                Write-Output ("  ok   {0,-26} {1,6} MB   {2}" -f $t.file, $mb, $leaf)
                $sources += ("{0,-18} {1,-32} {2,-26} {3}" -f $t.g, $t.slug, $t.file, $leaf)
                $ok++
            }
            else { Remove-Item $file -Force }
        }
        catch { Remove-Item $file -Force -ErrorAction SilentlyContinue }
    }
    if (-not $done)
    {
        Write-Output ("  FAIL {0,-26} {1}" -f $t.file, $t.slug)
        $failed += $t.slug
    }
}

Set-Content -Path (Join-Path $dest '_sources.txt') -Value $sources -Encoding UTF8

$totalMb = 0
Get-ChildItem $dest -Filter *.jpg | ForEach-Object { $totalMb += $_.Length }
Write-Output ''
Write-Output ("downloaded: {0}   already present: {1}   failed: {2}" -f $ok, $skipped, $failed.Count)
if ($failed.Count) { Write-Output ("failed slugs: " + ($failed -join ', ')) }
Write-Output ("assets\textures now holds {0} files, {1:N1} MB" -f (Get-ChildItem $dest -Filter *.jpg).Count, ($totalMb / 1MB))
Write-Output 'sources written to assets\textures\_sources.txt'
if ($failed.Count) { exit 1 }