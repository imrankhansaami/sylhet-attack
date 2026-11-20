// textures.h - photographic textures loaded from assets/textures (CC0, Poly Haven)
//
// The first block is the original set; everything after it was added in the
// "more textures" pass that gives each surface its own photograph. The order of
// this enum is the order of the file table in textures.cpp, and the ids are
// handed out to the world code, so entries must not be reordered without
// updating the file table too.
//
// Files are fetched by tools\get_textures.ps1; assets\textures\_sources.txt
// lists which Poly Haven asset each local file came from.
#pragma once

enum TexId
{
    // ---- original set -------------------------------------------------
    TEX_GRASS = 0,      // grass_ground
    TEX_ASPHALT,        // aerial_asphalt_01
    TEX_PLASTER,        // grey_plaster
    TEX_BRICK,          // brick_4
    TEX_TIN,            // corrugated_iron
    TEX_ROOF,           // clay_roof_tiles
    TEX_BRICK_RED,      // red_brick
    TEX_PLASTER_WHITE,  // white_plaster_02
    TEX_CONCRETE,       // concrete_wall_003
    TEX_ROOF_TILE,      // grey_roof_tiles
    TEX_ROOF_RUST,      // rusty_metal_sheet
    TEX_SHEET,          // box_profile_metal_sheet
    TEX_DIRT,           // dirt
    TEX_GRAVEL,         // gravel
    TEX_LEAVES,         // forest_leaves_02
    TEX_BARK,           // bark_brown_01
    TEX_SAND,           // aerial_sand
    TEX_WOOD,           // wood_planks

    // ---- ground and terrain -------------------------------------------
    TEX_GRASS_LEAFY,    // leafy_grass
    TEX_GRASS_SPARSE,   // sparse_grass
    TEX_GRASS_DRY,      // withered_grass
    TEX_GRASS_PATH,     // grass_path_2
    TEX_FOREST_FLOOR,   // forest_ground_04
    TEX_MUD,            // brown_mud_03
    TEX_MUD_DRY,        // brown_mud_dry
    TEX_MUD_CRACKED,    // mud_cracked_dry_03
    TEX_SOIL_RED,       // red_laterite_soil_stones
    TEX_SOIL_AERIAL,    // dirt_aerial_02
    TEX_FIELD_DRY,      // dry_mud_field_001
    TEX_GRAVEL_SANDY,   // sandy_gravel
    TEX_SAND_GRAVEL,    // gravelly_sand
    TEX_SAND_COAST,     // coast_sand_01
    TEX_ROCK_TERRAIN,   // rocky_terrain_02
    TEX_GRASS_ROCK,     // aerial_grass_rock
    TEX_ROCKS_GROUND,   // rocks_ground_02
    TEX_PEBBLES,        // ganges_river_pebbles
    TEX_RIVERBED,       // dry_riverbed_rock

    // ---- roads, pavements, kerbs --------------------------------------
    TEX_ASPHALT2,       // asphalt_02
    TEX_ROAD_WORN,      // asphalt_pit_lane
    TEX_ASPHALT_WORN,   // worn_asphalt
    TEX_ASPHALT_CLEAN,  // clean_asphalt
    TEX_ROAD_GRAVEL,    // gravel_road
    TEX_PAVEMENT,       // concrete_pavement_02
    TEX_COBBLE,         // cobblestone_floor_08
    TEX_PAVING_TILES,   // checkered_pavement_tiles
    TEX_PAVING_SQUARE,  // square_floor_patern_01
    TEX_PAVING_RED,     // red_sandstone_pavement

    // ---- walls ---------------------------------------------------------
    TEX_WALL_PAINTED,   // painted_plaster_wall
    TEX_WALL_BEIGE,     // beige_wall_001
    TEX_PLASTER_GREY,   // plaster_grey_04
    TEX_PLASTER_ROUGH,  // white_rough_plaster
    TEX_PLASTER_STONE,  // plastered_stone_wall
    TEX_CONCRETE_CRACKED,  // cracked_concrete_wall
    TEX_CONCRETE_PANEL,    // concrete_wall_007
    TEX_CONCRETE_WALL2,    // concrete_wall_008
    TEX_CONCRETE_LAYERS,   // concrete_layers_02
    TEX_CONCRETE_PAINTED,  // painted_concrete_02
    TEX_BRICK_RED2,     // red_brick_03
    TEX_BRICK_WALL,     // brick_wall_001
    TEX_BRICK_WALL2,    // brick_wall_006
    TEX_BRICK_RED3,     // red_bricks_04
    TEX_BRICK_CASTLE,   // castle_brick_02_red
    TEX_BRICK_STONE,    // stone_brick_wall_001
    TEX_BRICK_CONCRETE, // concrete_brick_wall_001
    TEX_BLOCK_ROUGH,    // rough_block_wall
    TEX_BRICK_FACTORY,  // factory_brick
    TEX_BRICK_OLD,      // medieval_red_brick
    TEX_BAMBOO,         // bamboo_wall
    TEX_BAMBOO2,        // bamboo_wall_02
    TEX_PLANK_OLD,      // weathered_brown_planks
    TEX_PLANK_BROWN,    // brown_planks_03
    TEX_PLANK_PAINTED,  // wood_peeling_paint_weathered
    TEX_PLANK_BLUE,     // blue_painted_planks
    TEX_FACADE_TILE,    // rectangular_facade_tiles
    TEX_FACADE_TILE2,   // rectangular_facade_tiles_02
    TEX_STONE_WALL,     // stone_wall
    TEX_SANDSTONE_WALL, // sandstone_brick_wall_01
    TEX_SANDSTONE_BLOCK,// large_sandstone_blocks
    TEX_MARBLE,         // marble_01

    // ---- metal ---------------------------------------------------------
    TEX_METAL_GREEN,    // green_metal_rust
    TEX_METAL_RUST,     // rusty_metal_02
    TEX_METAL_RUST2,    // rusty_metal_03
    TEX_METAL_PLATE,    // metal_plate
    TEX_METAL_BLUE,     // blue_metal_plate
    TEX_METAL_SHUTTER,  // painted_metal_shutter
    TEX_TIN2,           // corrugated_iron_02
    TEX_METAL_SIDING,   // factory_wall

    // ---- roofs ---------------------------------------------------------
    TEX_ROOF_CLAY,      // clay_roof_tiles_02
    TEX_ROOF_CERAMIC,   // ceramic_roof_01
    TEX_ROOF_TILES2,    // roof_tiles_14
    TEX_ROOF_09,        // roof_09
    TEX_ROOF_GREY,      // grey_roof_01
    TEX_ROOF_TILES_GREY,// grey_roof_tiles_02
    TEX_ROOF_SLATE,     // roof_slates_03
    TEX_ROOF_THATCH,    // thatch_roof_angled

    // ---- nature, floors, fabric ----------------------------------------
    TEX_BARK2,          // bark_brown_02
    TEX_BARK_PINE,      // pine_bark
    TEX_BARK_PALM,      // palm_tree_bark
    TEX_GRANITE,        // granite_tile_03
    TEX_TERRAZZO,       // terrazzo_tiles
    TEX_STONE_TILES,    // seaworn_stone_tiles
    TEX_TILE_WALL,      // interior_tiles
    TEX_FABRIC,         // fabric_pattern_07
    TEX_FABRIC2,        // fabric_pattern_05
    TEX_HESSIAN,        // hessian_230

    // ---- signage (not from Poly Haven: the artwork for the big billboards) ----
    TEX_BILLBOARD,      // assets/textures/imran_billboard.jpg
    TEX_BILLBOARD2,     // assets/textures/alamin_billboard.jpg
    TEX_BILLBOARD3,     // assets/textures/photo_billboard.jpg

    TEX_COUNT
};

void texturesLoad();              // call once, after the GL context exists
unsigned textureId(TexId id);     // 0 when the file was missing
bool textureBind(TexId id);       // binds + enables GL_TEXTURE_2D; false if missing
void textureUnbind();             // disables texturing again

int texturesLoaded();             // how many files were found and uploaded
long texturesDecodedBytes();      // width * height * 3 summed over them