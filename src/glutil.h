// glutil.h - drawing helpers: 3D primitives, 2D screen tools and text
#pragma once
#include <GL/freeglut.h>
#include "vecmath.h"

// ---------- colors ----------
void setColor(float r, float g, float b);
void setColorA(float r, float g, float b, float a);

// ---------- 3D primitives (immediate mode, GL 1.1 style) ----------
void drawBox(float w, float h, float d);                       // centered at origin
void drawBoxAt(float x, float y, float z, float w, float h, float d);
void drawBoxAtC(float x, float y, float z, float w, float h, float d, float r, float g, float b);
void drawBoxAtRot(float x, float y, float z, float w, float h, float d, float yawDeg); // rotated about Y, centered vertically
void drawPrism(float w, float h, float d, float topScale);     // box with a smaller top
void drawCylinderRaw(float rBot, float rTop, float h, int slices, bool caps); // base at y=0, top at y=h
void drawCylinder(float r, float h, int slices);               // centered at origin, axis Y
void drawDome(float r, int slices, int rings);                 // half sphere sitting on y=0
void drawSphere(float r, int slices, int rings);
void drawCone(float r, float h, int slices);                   // base at y=0, tip at y=h
void drawDiskXZ(float r, int segs);                            // flat circle on the y=0 plane
void drawTorus(float R, float r, int slices, int rings);       // in the XY plane (hole facing Z)
void groundQuad(float x0, float z0, float x1, float z1, float y);
void beginOverlay();  // polygon offset for decals on the ground
void endOverlay();

// ---------- modelling counters (reset before worldBuild, printed by --stats) ----------
extern int gCountBox, gCountCylinder, gCountDome, gCountSphere;
extern long gCountTriangles;   // triangles in those four primitive types

// ---------- text ----------
void text2DBegin(int w, int h);       // start a 2D pass (screen pixels, origin top-left)
void text2DEnd();
void drawText(void* font, const char* s, float x, float y);            // x,y = top-left corner
void drawTextCentered(void* font, const char* s, float cx, float y);   // centered on cx
float textW(void* font, const char* s);
float textH(void* font);
void drawTextShadow(void* font, const char* s, float x, float y);      // dark offset copy

// 2D shapes (screen pixels, origin top-left)
void fillRect(float x, float y, float w, float h);
void fillRectA(float x, float y, float w, float h, float alpha);
// a vertical gradient rectangle (top colour -> bottom colour), in 2D pixels
void gradRect(float x, float y, float w, float h,
              float r0, float g0, float b0, float a0,
              float r1, float g1, float b1, float a1);
void frameRect(float x, float y, float w, float h, float thick);
void drawLine2D(float x0, float y0, float x1, float y1, float thick);
void drawCircle2D(float cx, float cy, float r, int segs, bool filled);
void drawRing2D(float cx, float cy, float rOuter, float rInner, int segs);
void drawStar2D(float cx, float cy, float R, bool filled);

// 3D stroke text (for signs): draws centered at the current origin
float strokeTextLen(void* font, const char* s);
void strokeTextCentered(void* font, const char* s, float scale);
void strokeTextLeft(void* font, const char* s, float scale);
// 2D stroke text for the HUD (x,y = top of the capitals)
float strokeTextW2D(void* font, const char* s, float scale);
void drawStrokeText2D(void* font, const char* s, float x, float y, float scale, float lw);
void drawStrokeText2DCentered(void* font, const char* s, float cx, float y, float scale, float lw);
void drawStrokeText2DRight(void* font, const char* s, float rx, float y, float scale, float lw);
