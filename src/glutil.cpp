// glutil.cpp - drawing helpers: 3D primitives, 2D screen tools and text
#include "glutil.h"
#include <cstring>
#include <cstdio>

static int sW = 1280, sH = 720;   // current 2D pass size

// Modelling counters. They are reset just before worldBuild(), so after start-up
// they describe the static world exactly; --stats prints them for the paper.
int gCountBox = 0, gCountCylinder = 0, gCountDome = 0, gCountSphere = 0;
long gCountTriangles = 0;

// =====================================================================
// colors
// =====================================================================
void setColor(float r, float g, float b) { glColor3f(r, g, b); }
void setColorA(float r, float g, float b, float a) { glColor4f(r, g, b, a); }

// =====================================================================
// 3D primitives
// =====================================================================
void drawBox(float w, float h, float d)
{
    gCountBox++;
    gCountTriangles += 12;          // six quads
    float x = w * 0.5f, y = h * 0.5f, z = d * 0.5f;
    // texture coordinates are one tile per 4 metres, so a 20 m wall gets 5 tiles
    const float T = 0.25f;
    float ux = w * T, uy = h * T, uz = d * T;
    glBegin(GL_QUADS);
    // front (-Z)
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0);      glVertex3f(-x, -y, -z);
    glTexCoord2f(0, uy);     glVertex3f(-x, y, -z);
    glTexCoord2f(ux, uy);    glVertex3f(x, y, -z);
    glTexCoord2f(ux, 0);     glVertex3f(x, -y, -z);
    // back (+Z)
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0);      glVertex3f(x, -y, z);
    glTexCoord2f(0, uy);     glVertex3f(x, y, z);
    glTexCoord2f(ux, uy);    glVertex3f(-x, y, z);
    glTexCoord2f(ux, 0);     glVertex3f(-x, -y, z);
    // left (-X)
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0);      glVertex3f(-x, -y, z);
    glTexCoord2f(0, uy);     glVertex3f(-x, y, z);
    glTexCoord2f(uz, uy);    glVertex3f(-x, y, -z);
    glTexCoord2f(uz, 0);     glVertex3f(-x, -y, -z);
    // right (+X)
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0);      glVertex3f(x, -y, -z);
    glTexCoord2f(0, uy);     glVertex3f(x, y, -z);
    glTexCoord2f(uz, uy);    glVertex3f(x, y, z);
    glTexCoord2f(uz, 0);     glVertex3f(x, -y, z);
    // top (+Y)
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);      glVertex3f(-x, y, -z);
    glTexCoord2f(0, uz);     glVertex3f(-x, y, z);
    glTexCoord2f(ux, uz);    glVertex3f(x, y, z);
    glTexCoord2f(ux, 0);     glVertex3f(x, y, -z);
    // bottom (-Y)
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0);      glVertex3f(-x, -y, z);
    glTexCoord2f(0, uz);     glVertex3f(-x, -y, -z);
    glTexCoord2f(ux, uz);    glVertex3f(x, -y, -z);
    glTexCoord2f(ux, 0);     glVertex3f(x, -y, z);
    glEnd();
}

void drawBoxAt(float x, float y, float z, float w, float h, float d)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    drawBox(w, h, d);
    glPopMatrix();
}

void drawBoxAtC(float x, float y, float z, float w, float h, float d, float r, float g, float b)
{
    setColor(r, g, b);
    drawBoxAt(x, y, z, w, h, d);
}

void drawBoxAtRot(float x, float y, float z, float w, float h, float d, float yawDeg)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0, 1, 0);
    drawBox(w, h, d);
    glPopMatrix();
}

void drawPrism(float w, float h, float d, float topScale)
{
    float x = w * 0.5f, z = d * 0.5f;
    float tx = x * topScale, tz = z * topScale;
    glBegin(GL_QUADS);
    // front
    glNormal3f(0, (1.0f - topScale) * 0.5f, -1);
    glVertex3f(-x, 0, -z); glVertex3f(-tx, h, -tz); glVertex3f(tx, h, -tz); glVertex3f(x, 0, -z);
    // back
    glNormal3f(0, (1.0f - topScale) * 0.5f, 1);
    glVertex3f(x, 0, z); glVertex3f(tx, h, tz); glVertex3f(-tx, h, tz); glVertex3f(-x, 0, z);
    // left
    glNormal3f(-1, (1.0f - topScale) * 0.5f, 0);
    glVertex3f(-x, 0, z); glVertex3f(-tx, h, tz); glVertex3f(-tx, h, -tz); glVertex3f(-x, 0, -z);
    // right
    glNormal3f(1, (1.0f - topScale) * 0.5f, 0);
    glVertex3f(x, 0, -z); glVertex3f(tx, h, -tz); glVertex3f(tx, h, tz); glVertex3f(x, 0, z);
    // bottom
    glNormal3f(0, -1, 0);
    glVertex3f(-x, 0, z); glVertex3f(-x, 0, -z); glVertex3f(x, 0, -z); glVertex3f(x, 0, z);
    glEnd();
    // top
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-tx, h, -tz); glVertex3f(-tx, h, tz); glVertex3f(tx, h, tz); glVertex3f(tx, h, -tz);
    glEnd();
}

void drawCylinderRaw(float rBot, float rTop, float h, int slices, bool caps)
{
    if (slices < 3) slices = 3;
    gCountCylinder++;
    gCountTriangles += 2L * slices + (caps ? 2L * slices : 0L);
    float step = 2.0f * PI_F / slices;
    float circumference = 2.0f * PI_F * ((rBot + rTop) * 0.5f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= slices; i++)
    {
        float a = i * step;
        float cx = cosf(a), sz = sinf(a);
        float u = (i / (float)slices) * circumference * 0.25f;
        glNormal3f(cx, (rBot - rTop) * 0.25f, sz);
        glTexCoord2f(u, 0.0f);          glVertex3f(cx * rBot, 0, sz * rBot);
        glTexCoord2f(u, h * 0.25f);     glVertex3f(cx * rTop, h, sz * rTop);
    }
    glEnd();
    if (caps)
    {
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, -1, 0);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(0, 0, 0);
        for (int i = 0; i <= slices; i++)
        {
            float a = -i * step;
            float cx = cosf(a) * rBot, sz = sinf(a) * rBot;
            glTexCoord2f(cx * 0.25f, sz * 0.25f); glVertex3f(cx, 0, sz);
        }
        glEnd();
        if (rTop > 0.001f)
        {
            glBegin(GL_TRIANGLE_FAN);
            glNormal3f(0, 1, 0);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(0, h, 0);
            for (int i = 0; i <= slices; i++)
            {
                float a = i * step;
                float cx = cosf(a) * rTop, sz = sinf(a) * rTop;
                glTexCoord2f(cx * 0.25f, sz * 0.25f); glVertex3f(cx, h, sz);
            }
            glEnd();
        }
    }
}

void drawCylinder(float r, float h, int slices)
{
    glPushMatrix();
    glTranslatef(0, -h * 0.5f, 0);
    drawCylinderRaw(r, r, h, slices, true);
    glPopMatrix();
}

void drawDome(float r, int slices, int rings)
{
    if (slices < 3) slices = 3;
    if (rings < 2) rings = 2;
    gCountDome++;
    gCountTriangles += 2L * slices * rings;
    float dphi = (PI_F * 0.5f) / rings;
    float dth = 2.0f * PI_F / slices;
    for (int j = 0; j < rings; j++)
    {
        float phi0 = j * dphi, phi1 = (j + 1) * dphi;
        float y0 = sinf(phi0) * r, rr0 = cosf(phi0) * r;
        float y1 = sinf(phi1) * r, rr1 = cosf(phi1) * r;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; i++)
        {
            float th = i * dth;
            float cx = cosf(th), sz = sinf(th);
            float u = (i / (float)slices) * 2.0f * PI_F * r * 0.25f;
            glNormal3f(cx * cosf(phi0), sinf(phi0), sz * cosf(phi0));
            glTexCoord2f(u, y0 * 0.25f); glVertex3f(cx * rr0, y0, sz * rr0);
            glNormal3f(cx * cosf(phi1), sinf(phi1), sz * cosf(phi1));
            glTexCoord2f(u, y1 * 0.25f); glVertex3f(cx * rr1, y1, sz * rr1);
        }
        glEnd();
    }
}

void drawSphere(float r, int slices, int rings)
{
    if (slices < 3) slices = 3;
    if (rings < 3) rings = 3;
    gCountSphere++;
    gCountTriangles += 2L * slices * rings;
    float dphi = PI_F / rings;
    float dth = 2.0f * PI_F / slices;
    for (int j = 0; j < rings; j++)
    {
        float phi0 = -PI_F * 0.5f + j * dphi, phi1 = phi0 + dphi;
        float y0 = sinf(phi0) * r, rr0 = cosf(phi0) * r;
        float y1 = sinf(phi1) * r, rr1 = cosf(phi1) * r;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; i++)
        {
            float th = i * dth;
            float cx = cosf(th), sz = sinf(th);
            float u = (i / (float)slices) * 2.0f * PI_F * r * 0.25f;
            glNormal3f(cx * cosf(phi0), sinf(phi0), sz * cosf(phi0));
            glTexCoord2f(u, y0 * 0.25f); glVertex3f(cx * rr0, y0, sz * rr0);
            glNormal3f(cx * cosf(phi1), sinf(phi1), sz * cosf(phi1));
            glTexCoord2f(u, y1 * 0.25f); glVertex3f(cx * rr1, y1, sz * rr1);
        }
        glEnd();
    }
}

void drawCone(float r, float h, int slices)
{
    // side
    drawCylinderRaw(r, 0.0001f, h, slices, false);
    // base
    float step = 2.0f * PI_F / slices;
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, -1, 0);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0, 0, 0);
    for (int i = 0; i <= slices; i++)
    {
        float a = -i * step;
        float cx = cosf(a) * r, sz = sinf(a) * r;
        glTexCoord2f(cx * 0.25f, sz * 0.25f); glVertex3f(cx, 0, sz);
    }
    glEnd();
}

void drawDiskXZ(float r, int segs)
{
    if (segs < 3) segs = 3;
    float step = 2.0f * PI_F / segs;
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= segs; i++)
    {
        float a = i * step;
        glVertex3f(cosf(a) * r, 0, sinf(a) * r);
    }
    glEnd();
}

void drawTorus(float R, float r, int slices, int rings)
{
    if (slices < 4) slices = 4;
    if (rings < 4) rings = 4;
    float dth = 2.0f * PI_F / slices;   // around the tube
    float dph = 2.0f * PI_F / rings;    // around the ring
    for (int i = 0; i < slices; i++)
    {
        float th0 = i * dth, th1 = th0 + dth;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= rings; j++)
        {
            float ph = j * dph;
            float cph = cosf(ph), sph = sinf(ph);
            float x0 = (R + r * cosf(th0)) * cph, y0 = (R + r * cosf(th0)) * sph;
            float z0 = r * sinf(th0);
            float nx0 = cosf(th0) * cph, ny0 = cosf(th0) * sph, nz0 = sinf(th0);
            float x1 = (R + r * cosf(th1)) * cph, y1 = (R + r * cosf(th1)) * sph;
            float z1 = r * sinf(th1);
            float nx1 = cosf(th1) * cph, ny1 = cosf(th1) * sph, nz1 = sinf(th1);
            glNormal3f(nx0, ny0, nz0); glVertex3f(x0, y0, z0);
            glNormal3f(nx1, ny1, nz1); glVertex3f(x1, y1, z1);
        }
        glEnd();
    }
}

void groundQuad(float x0, float z0, float x1, float z1, float y)
{
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(x0 * 0.035f, z0 * 0.035f); glVertex3f(x0, y, z1);
    glTexCoord2f(x1 * 0.035f, z0 * 0.035f); glVertex3f(x1, y, z1);
    glTexCoord2f(x1 * 0.035f, z1 * 0.035f); glVertex3f(x1, y, z0);
    glTexCoord2f(x0 * 0.035f, z1 * 0.035f); glVertex3f(x0, y, z0);
    glEnd();
}

void beginOverlay()
{
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
}

void endOverlay()
{
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0, 0);
}

// =====================================================================
// text and 2D helpers (origin top-left, y grows down)
// =====================================================================
void text2DBegin(int w, int h)
{
    sW = w; sH = h;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void text2DEnd()
{
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_FOG);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

float textW(void* font, const char* s)
{
    return (float)glutBitmapLength(font, (const unsigned char*)s);
}

float textH(void* font)
{
    return (float)glutBitmapHeight(font);
}

void drawText(void* font, const char* s, float x, float y)
{
    glRasterPos2f(x, (float)sH - y - textH(font));
    glutBitmapString(font, (const unsigned char*)s);
}

void drawTextCentered(void* font, const char* s, float cx, float y)
{
    drawText(font, s, cx - textW(font, s) * 0.5f, y);
}

void drawTextShadow(void* font, const char* s, float x, float y)
{
    setColorA(0, 0, 0, 0.7f);
    drawText(font, s, x + 1.5f, y + 1.5f);
    setColorA(1, 1, 1, 1);
    drawText(font, s, x, y);
}

// top-left rect to GL quad
static void quad2D(float x, float y, float w, float h)
{
    float x0 = x, x1 = x + w;
    float yTop = (float)sH - y;            // in GL coords (y up)
    float yBot = (float)sH - (y + h);
    glBegin(GL_QUADS);
    glVertex2f(x0, yBot); glVertex2f(x1, yBot); glVertex2f(x1, yTop); glVertex2f(x0, yTop);
    glEnd();
}

void fillRect(float x, float y, float w, float h)
{
    quad2D(x, y, w, h);
}

void fillRectA(float x, float y, float w, float h, float alpha)
{
    glColor4f(1, 1, 1, alpha);
    quad2D(x, y, w, h);
}

void gradRect(float x, float y, float w, float h,
              float r0, float g0, float b0, float a0,
              float r1, float g1, float b1, float a1)
{
    glBegin(GL_QUADS);
    glColor4f(r0, g0, b0, a0); glVertex2f(x, (float)sH - y);
    glColor4f(r0, g0, b0, a0); glVertex2f(x + w, (float)sH - y);
    glColor4f(r1, g1, b1, a1); glVertex2f(x + w, (float)sH - (y + h));
    glColor4f(r1, g1, b1, a1); glVertex2f(x, (float)sH - (y + h));
    glEnd();
}

void frameRect(float x, float y, float w, float h, float thick)
{
    glLineWidth(thick);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, (float)sH - y);
    glVertex2f(x + w, (float)sH - y);
    glVertex2f(x + w, (float)sH - (y + h));
    glVertex2f(x, (float)sH - (y + h));
    glEnd();
    glLineWidth(1);
}

void drawLine2D(float x0, float y0, float x1, float y1, float thick)
{
    glLineWidth(thick);
    glBegin(GL_LINES);
    glVertex2f(x0, (float)sH - y0);
    glVertex2f(x1, (float)sH - y1);
    glEnd();
    glLineWidth(1);
}

void drawCircle2D(float cx, float cy, float r, int segs, bool filled)
{
    float yc = (float)sH - cy;
    if (filled)
    {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, yc);
        for (int i = 0; i <= segs; i++)
        {
            float a = 2.0f * PI_F * i / segs;
            glVertex2f(cx + cosf(a) * r, yc + sinf(a) * r);
        }
        glEnd();
    }
    else
    {
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segs; i++)
        {
            float a = 2.0f * PI_F * i / segs;
            glVertex2f(cx + cosf(a) * r, yc + sinf(a) * r);
        }
        glEnd();
        glLineWidth(1);
    }
}

void drawRing2D(float cx, float cy, float rOuter, float rInner, int segs)
{
    float yc = (float)sH - cy;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++)
    {
        float a = 2.0f * PI_F * i / segs;
        glVertex2f(cx + cosf(a) * rOuter, yc + sinf(a) * rOuter);
        glVertex2f(cx + cosf(a) * rInner, yc + sinf(a) * rInner);
    }
    glEnd();
}

void drawStar2D(float cx, float cy, float R, bool filled)
{
    float yc = (float)sH - cy;
    float const pts = 10;
    if (filled)
    {
        glBegin(GL_POLYGON);
        for (int i = 0; i < (int)pts; i++)
        {
            float rr = (i % 2 == 0) ? R : R * 0.42f;
            float a = -PI_F * 0.5f + 2.0f * PI_F * i / pts;
            glVertex2f(cx + cosf(a) * rr, yc - sinf(a) * rr);
        }
        glEnd();
    }
    else
    {
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < (int)pts; i++)
        {
            float rr = (i % 2 == 0) ? R : R * 0.42f;
            float a = -PI_F * 0.5f + 2.0f * PI_F * i / pts;
            glVertex2f(cx + cosf(a) * rr, yc - sinf(a) * rr);
        }
        glEnd();
        glLineWidth(1);
    }
}

// =====================================================================
// stroke text (3D)
// =====================================================================
float strokeTextLen(void* font, const char* s)
{
    return (float)glutStrokeLength(font, (const unsigned char*)s);
}

void strokeTextCentered(void* font, const char* s, float scale)
{
    float L = strokeTextLen(font, s) * scale;
    glPushMatrix();
    glTranslatef(-L * 0.5f, 0, 0);
    glScalef(scale, scale, scale);
    glutStrokeString(font, (const unsigned char*)s);
    glPopMatrix();
}

void strokeTextLeft(void* font, const char* s, float scale)
{
    glPushMatrix();
    glScalef(scale, scale, scale);
    glutStrokeString(font, (const unsigned char*)s);
    glPopMatrix();
}

// ---------------------------------------------------------------------
// 2D stroke text for the HUD: crisp at any size, (x,y) is the top of the
// capital letters, scale is pixels per font unit
// ---------------------------------------------------------------------
float strokeTextW2D(void* font, const char* s, float scale)
{
    return strokeTextLen(font, s) * scale;
}

void drawStrokeText2D(void* font, const char* s, float x, float y, float scale, float lw)
{
    glPushMatrix();
    glTranslatef(x, (float)sH - (y + 100.0f * scale), 0.0f);
    glScalef(scale, scale, scale);
    glLineWidth(lw);
    glutStrokeString(font, (const unsigned char*)s);
    glPopMatrix();
    glLineWidth(1.0f);
}

void drawStrokeText2DCentered(void* font, const char* s, float cx, float y, float scale, float lw)
{
    drawStrokeText2D(font, s, cx - strokeTextW2D(font, s, scale) * 0.5f, y, scale, lw);
}

void drawStrokeText2DRight(void* font, const char* s, float rx, float y, float scale, float lw)
{
    drawStrokeText2D(font, s, rx - strokeTextW2D(font, s, scale), y, scale, lw);
}
