// video.cpp - MJPEG AVI writer (see video.h for why)
//
// The AVI container is written by hand: a RIFF header with one video stream,
// a 'movi' LIST holding one JPEG per frame, and an 'idx1' index so players can
// seek. Every field is little-endian, which is what x86 writes natively.

#include "video.h"
#include "../third_party/stb/stb_image_write.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
struct IdxEntry { unsigned int offset, size; };

FILE* sF = nullptr;
int sW = 0, sH = 0, sFps = 0, sQuality = 85;
int sIdxCount = 0, sIdxCap = 0;
IdxEntry* sIdx = nullptr;
long sRiffSizePos = 0, sHdrlSizePos = 0, sStrlSizePos = 0, sMoviSizePos = 0;
long sMoviFourccPos = 0, sAvihFramesPos = 0, sStrhLengthPos = 0;

void w4(const char* s) { fwrite(s, 1, 4, sF); }
void w32(unsigned int v) { fwrite(&v, 4, 1, sF); }
void w16(unsigned short v) { fwrite(&v, 2, 1, sF); }
void patch32(long pos, unsigned int v) { fseek(sF, pos, SEEK_SET); fwrite(&v, 4, 1, sF); }

void jpgSink(void* ctx, void* data, int size)
{
    (void)ctx;
    fwrite(data, 1, (size_t)size, sF);
}
}

bool videoBegin(const char* path, int width, int height, int fps, int maxFrames, int quality)
{
    videoEnd();
    sF = fopen(path, "wb");
    if (!sF) return false;

    sW = width; sH = height; sFps = fps; sQuality = quality;
    sIdxCount = 0;
    sIdxCap = maxFrames > 0 ? maxFrames : 1;
    sIdx = (IdxEntry*)malloc(sizeof(IdxEntry) * (size_t)sIdxCap);

    // ---- RIFF / AVI ----
    w4("RIFF");
    sRiffSizePos = ftell(sF); w32(0);
    w4("AVI ");

    // ---- LIST hdrl ----
    w4("LIST");
    sHdrlSizePos = ftell(sF); w32(0);
    long hdrlFourcc = ftell(sF);
    w4("hdrl");

    // avih: main header, 56 bytes
    w4("avih"); w32(56);
    long avihStart = ftell(sF);
    w32((unsigned int)(1000000 / (fps > 0 ? fps : 25)));   // microseconds per frame
    w32((unsigned int)(width * height * 3));               // max bytes per second (estimate)
    w32(0);                                                // padding granularity
    w32(0x10);                                             // flags: AVIF_HASINDEX
    sAvihFramesPos = ftell(sF); w32((unsigned int)maxFrames);
    w32(0);                                                // initial frames
    w32(1);                                                // streams
    w32((unsigned int)(width * height * 3));               // suggested buffer
    w32((unsigned int)width);
    w32((unsigned int)height);
    for (int i = 0; i < 4; i++) w32(0);                    // reserved
    (void)avihStart;

    // ---- LIST strl ----
    w4("LIST");
    sStrlSizePos = ftell(sF); w32(0);
    long strlFourcc = ftell(sF);
    w4("strl");

    // strh: stream header, 56 bytes
    w4("strh"); w32(56);
    w4("vids");
    w4("MJPG");
    w32(0);                        // flags
    w16(0); w16(0);                // priority, language
    w32(0);                        // initial frames
    w32(1);                        // scale
    w32((unsigned int)fps);        // rate -> fps = rate / scale
    w32(0);                        // start
    sStrhLengthPos = ftell(sF); w32((unsigned int)maxFrames);
    w32((unsigned int)(width * height * 3));   // suggested buffer
    w32(0xFFFFFFFFu);              // quality: default
    w32(0);                        // sample size (0 = variable, one JPEG per frame)
    w16(0); w16(0); w16((unsigned short)width); w16((unsigned short)height);   // rcFrame

    // strf: BITMAPINFOHEADER, 40 bytes
    w4("strf"); w32(40);
    w32(40);                       // biSize
    w32((unsigned int)width);
    w32((unsigned int)height);
    w16(1);                        // planes
    w16(24);                       // bit count
    w4("MJPG");                    // compression
    w32((unsigned int)(width * height * 3));   // image size
    w32(0); w32(0); w32(0); w32(0);            // resolution, palette
    (void)avihStart;

    long afterStrl = ftell(sF);
    patch32(sStrlSizePos, (unsigned int)(afterStrl - strlFourcc));
    patch32(sHdrlSizePos, (unsigned int)(afterStrl - hdrlFourcc));
    fseek(sF, afterStrl, SEEK_SET);      // patching moved the pointer: go back to the end

    // ---- LIST movi (the frames follow) ----
    w4("LIST");
    sMoviSizePos = ftell(sF); w32(0);
    sMoviFourccPos = ftell(sF);
    w4("movi");

    return true;
}

void videoAddFrame(const unsigned char* rgbTopDown)
{
    if (!sF || !rgbTopDown) return;

    long chunkPos = ftell(sF);
    w4("00dc");
    long sizePos = ftell(sF); w32(0);
    long dataPos = ftell(sF);

    stbi_write_jpg_to_func(jpgSink, nullptr, sW, sH, 3, rgbTopDown, sQuality);

    long endPos = ftell(sF);
    unsigned int payload = (unsigned int)(endPos - dataPos);
    if (payload & 1u)                       // every chunk is padded to even length
    {
        unsigned char pad = 0;
        fwrite(&pad, 1, 1, sF);
        endPos++;
    }
    patch32(sizePos, payload);
    fseek(sF, endPos, SEEK_SET);

    if (sIdxCount < sIdxCap)
    {
        sIdx[sIdxCount].offset = (unsigned int)(chunkPos - sMoviFourccPos);
        sIdx[sIdxCount].size = payload;
        sIdxCount++;
    }
}

void videoEnd()
{
    if (!sF) return;

    long moviEnd = ftell(sF);
    unsigned int moviSize = (unsigned int)(moviEnd - sMoviFourccPos);

    // index
    w4("idx1"); w32((unsigned int)(sIdxCount * 16));
    for (int i = 0; i < sIdxCount; i++)
    {
        w4("00dc");
        w32(0x10);                          // AVIIF_KEYFRAME
        w32(sIdx[i].offset);
        w32(sIdx[i].size);
    }
    long fileEnd = ftell(sF);

    // back-fill the sizes now that they are known
    patch32(sMoviSizePos, moviSize);
    patch32(sAvihFramesPos, (unsigned int)sIdxCount);
    patch32(sStrhLengthPos, (unsigned int)sIdxCount);
    patch32(sRiffSizePos, (unsigned int)(fileEnd - (sRiffSizePos + 4)));

    fclose(sF);
    sF = nullptr;
    free(sIdx);
    sIdx = nullptr;
    sIdxCount = 0;
    sIdxCap = 0;
}

bool videoActive() { return sF != nullptr; }
int videoFrameCount() { return sIdxCount; }