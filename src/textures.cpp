// textures.cpp - loads the CC0 photographic textures used by the world
//
// The table below matches enum TexId in textures.h one-to-one. Files are fetched
// by tools\get_textures.ps1 from Poly Haven (CC0); a missing file is not an
// error - textureBind() returns false and the caller falls back to a flat colour,
// so the game still runs with an empty assets folder.
#include "textures.h"
#include <GL/freeglut.h>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

static GLuint sTex[TEX_COUNT] = { 0 };
static int sLoaded = 0;
static long sDecodedBytes = 0;

static const char* sFile[TEX_COUNT] = {
    // original set
    "assets/textures/grass_ground.jpg",
    "assets/textures/aerial_asphalt_01.jpg",
    "assets/textures/grey_plaster.jpg",
    "assets/textures/brick_4.jpg",
    "assets/textures/corrugated_iron.jpg",
    "assets/textures/clay_roof_tiles.jpg",
    "assets/textures/red_brick.jpg",
    "assets/textures/white_plaster_02.jpg",
    "assets/textures/concrete_wall_003.jpg",
    "assets/textures/grey_roof_tiles.jpg",
    "assets/textures/rusty_metal_sheet.jpg",
    "assets/textures/box_profile_metal_sheet.jpg",
    "assets/textures/dirt.jpg",
    "assets/textures/gravel.jpg",
    "assets/textures/forest_leaves_02.jpg",
    "assets/textures/bark_brown_01.jpg",
    "assets/textures/aerial_sand.jpg",
    "assets/textures/wood_planks.jpg",

    // ground and terrain
    "assets/textures/grass_leafy.jpg",
    "assets/textures/grass_sparse.jpg",
    "assets/textures/grass_dry.jpg",
    "assets/textures/grass_path.jpg",
    "assets/textures/forest_floor.jpg",
    "assets/textures/mud.jpg",
    "assets/textures/mud_dry.jpg",
    "assets/textures/mud_cracked.jpg",
    "assets/textures/soil_red.jpg",
    "assets/textures/soil_aerial.jpg",
    "assets/textures/field_dry.jpg",
    "assets/textures/gravel_sandy.jpg",
    "assets/textures/sand_gravel.jpg",
    "assets/textures/sand_coast.jpg",
    "assets/textures/rock_terrain.jpg",
    "assets/textures/grass_rock.jpg",
    "assets/textures/rocks_ground.jpg",
    "assets/textures/pebbles.jpg",
    "assets/textures/riverbed.jpg",

    // roads, pavements, kerbs
    "assets/textures/asphalt2.jpg",
    "assets/textures/road_worn.jpg",
    "assets/textures/asphalt_worn.jpg",
    "assets/textures/asphalt_clean.jpg",
    "assets/textures/road_gravel.jpg",
    "assets/textures/pavement.jpg",
    "assets/textures/cobble.jpg",
    "assets/textures/paving_tiles.jpg",
    "assets/textures/paving_square.jpg",
    "assets/textures/paving_red.jpg",

    // walls
    "assets/textures/wall_painted.jpg",
    "assets/textures/wall_beige.jpg",
    "assets/textures/plaster_grey.jpg",
    "assets/textures/plaster_rough.jpg",
    "assets/textures/plaster_stone.jpg",
    "assets/textures/concrete_cracked.jpg",
    "assets/textures/concrete_panel.jpg",
    "assets/textures/concrete_wall2.jpg",
    "assets/textures/concrete_layers.jpg",
    "assets/textures/concrete_painted.jpg",
    "assets/textures/brick_red2.jpg",
    "assets/textures/brick_wall.jpg",
    "assets/textures/brick_wall2.jpg",
    "assets/textures/brick_red3.jpg",
    "assets/textures/brick_castle.jpg",
    "assets/textures/brick_stone.jpg",
    "assets/textures/brick_concrete.jpg",
    "assets/textures/block_rough.jpg",
    "assets/textures/brick_factory.jpg",
    "assets/textures/brick_old.jpg",
    "assets/textures/bamboo.jpg",
    "assets/textures/bamboo2.jpg",
    "assets/textures/plank_old.jpg",
    "assets/textures/plank_brown.jpg",
    "assets/textures/plank_painted.jpg",
    "assets/textures/plank_blue.jpg",
    "assets/textures/facade_tile.jpg",
    "assets/textures/facade_tile2.jpg",
    "assets/textures/stone_wall.jpg",
    "assets/textures/sandstone_wall.jpg",
    "assets/textures/sandstone_block.jpg",
    "assets/textures/marble.jpg",

    // metal
    "assets/textures/metal_green.jpg",
    "assets/textures/metal_rust.jpg",
    "assets/textures/metal_rust2.jpg",
    "assets/textures/metal_plate.jpg",
    "assets/textures/metal_blue.jpg",
    "assets/textures/metal_shutter.jpg",
    "assets/textures/tin2.jpg",
    "assets/textures/metal_siding.jpg",

    // roofs
    "assets/textures/roof_clay.jpg",
    "assets/textures/roof_ceramic.jpg",
    "assets/textures/roof_tiles2.jpg",
    "assets/textures/roof_09.jpg",
    "assets/textures/roof_grey.jpg",
    "assets/textures/roof_tiles_grey.jpg",
    "assets/textures/roof_slate.jpg",
    "assets/textures/roof_thatch.jpg",

    // nature, floors, fabric
    "assets/textures/bark2.jpg",
    "assets/textures/bark_pine.jpg",
    "assets/textures/bark_palm.jpg",
    "assets/textures/granite.jpg",
    "assets/textures/terrazzo.jpg",
    "assets/textures/stone_tiles.jpg",
    "assets/textures/tile_wall.jpg",
    "assets/textures/fabric.jpg",
    "assets/textures/fabric2.jpg",
    "assets/textures/hessian.jpg",

    // signage
    "assets/textures/imran_billboard.jpg",
    "assets/textures/alamin_billboard.jpg",
    "assets/textures/photo_billboard.jpg"
};

void texturesLoad()
{
    sLoaded = 0;
    sDecodedBytes = 0;
    int missing = 0;

    for (int i = 0; i < TEX_COUNT; i++)
    {
        int w = 0, h = 0, comp = 0;
        unsigned char* px = stbi_load(sFile[i], &w, &h, &comp, 3);
        if (!px)
        {
            printf("[tex] missing %s\n", sFile[i]);
            missing++;
            continue;
        }
        // keep the texture budget sane: every photo is reduced with a box
        // filter until its longest side is 512. At the distances the game is
        // played from that is still 128 texels per metre on the ground, and it
        // keeps 105 photographs inside a budget an integrated GPU can hold
        // without the driver paging textures in and out mid-flight.
        while ((w > 512 || h > 512) && (w % 2) == 0 && (h % 2) == 0)
        {
            int nw = w / 2, nh = h / 2;
            unsigned char* small = (unsigned char*)malloc((size_t)nw * nh * 3);
            if (!small) break;
            for (int y = 0; y < nh; y++)
            {
                for (int x = 0; x < nw; x++)
                {
                    for (int ch = 0; ch < 3; ch++)
                    {
                        int a = px[((y * 2) * w + x * 2) * 3 + ch];
                        int b = px[((y * 2) * w + x * 2 + 1) * 3 + ch];
                        int c = px[((y * 2 + 1) * w + x * 2) * 3 + ch];
                        int d = px[((y * 2 + 1) * w + x * 2 + 1) * 3 + ch];
                        small[(y * nw + x) * 3 + ch] = (unsigned char)((a + b + c + d) / 4);
                    }
                }
            }
            stbi_image_free(px);
            px = small;
            w = nw; h = nh;
        }
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // the first grass photo is a dry, brown ground: remap it to a lush green
        // while keeping its detail, so the fields match Sylhet
        if (i == TEX_GRASS)
        {
            for (int p = 0; p < w * h; p++)
            {
                int r = px[p * 3 + 0], g = px[p * 3 + 1], b = px[p * 3 + 2];
                int l = (r * 30 + g * 59 + b * 11) / 100;
                int nr = l * 62 / 100;
                int ng = l * 108 / 100;
                int nb = l * 55 / 100;
                px[p * 3 + 0] = (unsigned char)(nr > 255 ? 255 : nr);
                px[p * 3 + 1] = (unsigned char)(ng > 255 ? 255 : ng);
                px[p * 3 + 2] = (unsigned char)(nb > 255 ? 255 : nb);
            }
        }
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
        stbi_image_free(px);
        sTex[i] = t;
        sLoaded++;
        sDecodedBytes += (long)w * (long)h * 3L;
        printf("[tex] %s %dx%d\n", sFile[i], w, h);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    printf("[tex] %d of %d textures loaded, %ld MB decoded%s\n",
           sLoaded, TEX_COUNT, sDecodedBytes / (1024L * 1024L),
           missing ? " (run tools\\get_textures.ps1 to fetch what is missing)" : "");
}

unsigned textureId(TexId id)
{
    if (id < 0 || id >= TEX_COUNT) return 0;
    return sTex[id];
}

bool textureBind(TexId id)
{
    GLuint t = textureId(id);
    if (!t) return false;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, t);
    return true;
}

void textureUnbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

int texturesLoaded() { return sLoaded; }
long texturesDecodedBytes() { return sDecodedBytes; }