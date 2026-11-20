// world.cpp - procedural city of Sylhet: river, roads, bridges, landmarks,
// airport, railway, buildings, shops and trees. Static geometry is compiled
// into OpenGL display lists for speed.
#include "world.h"
#include "textures.h"
#include "glutil.h"
#include "aircraft.h"      // for the wind direction the windsocks point along
#include <cmath>
#include <cstring>
#include <vector>

// =====================================================================
// static layout tables (all coordinates in metres, +X east, +Z south)
// =====================================================================

// The Surma river, flowing from the north-west to the south-east
const float gRiverPts[][2] = {
    { -5600, -3200 }, { -4300, -2200 }, { -3200, -1400 }, { -2100, -700 },
    { -1000, -250 }, { 0, 105 }, { 900, 600 }, { 1600, 1300 },
    { 2000, 2300 }, { 2150, 3600 }, { 2250, 5400 }
};
const int gRiverPtsCount = sizeof(gRiverPts) / sizeof(gRiverPts[0]);

static const float RIVER_WATER_HALF = 65.0f;   // water half width
static const float RIVER_BANK_HALF  = 105.0f;  // dirt bank half width

// roads
struct RoadDef { const char* name; float width; int n; float pts[14][2]; };
static const RoadDef ROADS[] = {
    { "Zindabazar Road",    26.0f, 7, { { 0, -2500 }, { 0, -1200 }, { 0, 0 }, { 0, 1200 }, { 0, 2100 }, { 0, 2900 }, { 0, 3400 } } },
    { "Subid Bazar Road",   22.0f, 10, { { -3700, 0 }, { -2900, 0 }, { -2100, 0 }, { -1300, 0 }, { -500, 0 }, { 300, 0 }, { 1100, 0 }, { 1900, 0 }, { 2700, 0 }, { 3700, 0 } } },
    { "Ambarkhana Road",    22.0f, 9, { { -3400, 1200 }, { -2500, 1200 }, { -1600, 1200 }, { -700, 1200 }, { 100, 1200 }, { 900, 1200 }, { 1700, 1200 }, { 2500, 1200 }, { 3300, 1200 } } },
    { "Airport Road",       22.0f, 6, { { 0, 0 }, { -500, -500 }, { -1200, -1000 }, { -2000, -1500 }, { -2700, -1900 }, { -3100, -2170 } } },
    { "Bondor Bazar Road",  14.0f, 7, { { -1000, -2000 }, { -1000, -1200 }, { -1000, -400 }, { -1000, 400 }, { -1000, 1200 }, { -1000, 2000 }, { -1000, 2600 } } },
    { "Kazir Bazar Road",   14.0f, 6, { { -2100, -1800 }, { -2100, -1000 }, { -2100, -200 }, { -2100, 600 }, { -2100, 1400 }, { -2100, 2200 } } },
    { "Tilagarh Road",      14.0f, 8, { { 2600, -2400 }, { 2600, -1400 }, { 2600, -400 }, { 2600, 600 }, { 2600, 1600 }, { 2600, 2400 }, { 2600, 2900 }, { 2600, 3400 } } },
    { "SIU Road",           14.0f, 3, { { 1700, 1200 }, { 1850, 1330 }, { 1900, 1450 } } },
    { "Station Road",       10.0f, 5, { { 0, 3040 }, { 400, 3040 }, { 800, 3040 }, { 1200, 3040 }, { 1700, 3040 } } },
    { "Shibganj Road",      14.0f, 4, { { 1000, -2400 }, { 1000, -1400 }, { 1000, -400 }, { 1000, 600 } } }
};
const int gRoadCount = sizeof(ROADS) / sizeof(ROADS[0]);

const float gRailZ = 2900.0f;
const float gCrossX[2] = { 0.0f, 2600.0f };

// the ten landmarks of this project (the order is the 1-9,0 jump order)
const Landmark gLandmarks[] = {
    { "Keane Bridge",                     "Keane Br.",  0.0f,    105.0f, 45.0f },
    { "Ali Amjad's Clock Tower",          "Clock Twr",  90.0f,   -100.0f, 30.0f },
    { "Hazrat Shahjalal Mazar",           "Mazar",     -1750.0f, -1150.0f, 45.0f },
    { "Sylhet International University",  "SIU",        1900.0f,  1600.0f, 32.0f },
    { "Osmani International Airport",     "Airport",   -2600.0f, -3200.0f, 60.0f },
    { "Sylhet Railway Station",           "Rail Stn",   700.0f,  2900.0f, 30.0f },
    { "Sylhet District Stadium",          "Stadium",    1500.0f, -1600.0f, 30.0f },
    { "Telecom Tower",                    "Telecom",    650.0f,   -600.0f, 55.0f },
    { "Kadamtali Bus Terminal",           "Bus Stn",   -1650.0f,  2300.0f, 20.0f },
    { "Shahi Eidgah",                     "Eidgah",     2300.0f,   500.0f, 20.0f }
};
const int gLandmarkCount = sizeof(gLandmarks) / sizeof(gLandmarks[0]);

// ---------------------------------------------------------------------
// the low green hills that ring Sylhet (x, z, radius, height)
// ---------------------------------------------------------------------
static const float HILLS[][4] = {
    { -6500.0f,   800.0f, 1400.0f, 100.0f },
    { -6800.0f, -2000.0f, 1600.0f, 125.0f },
    { -6200.0f,  3200.0f, 1300.0f,  92.0f },
    { -3500.0f,  5600.0f, 1500.0f, 108.0f },
    {  -800.0f,  6200.0f, 1300.0f,  84.0f },
    {  2200.0f,  5900.0f, 1400.0f, 100.0f },
    {  4800.0f,  4400.0f, 1500.0f, 118.0f },
    {  6300.0f,  1800.0f, 1600.0f, 134.0f },
    {  6600.0f, -1200.0f, 1400.0f, 108.0f },
    {  5600.0f, -3600.0f, 1500.0f, 100.0f },
    {  3200.0f, -5200.0f, 1700.0f, 126.0f },
    {   300.0f, -6200.0f, 1600.0f, 118.0f },
    { -2600.0f, -5400.0f, 1500.0f, 108.0f },
    { -6200.0f, -5200.0f, 1400.0f,  92.0f }
};
const int gHillCount = (int)(sizeof(HILLS) / sizeof(HILLS[0]));

void worldHill(int i, float& x, float& z, float& r)
{
    x = HILLS[i][0];
    z = HILLS[i][1];
    r = HILLS[i][2];
}

float worldHillHeightAt(float x, float z)
{
    float best = 0.0f;
    for (int i = 0; i < gHillCount; i++)
    {
        float dx = x - HILLS[i][0], dz = z - HILLS[i][1];
        float rr = HILLS[i][2];
        float d2 = dx * dx + dz * dz;
        if (d2 < rr * rr)
        {
            float hh = HILLS[i][3] * sqrtf(1.0f - d2 / (rr * rr));
            if (hh > best) best = hh;
        }
    }
    return best;
}

// airport constants
// the airport sits on the south bank of the Surma, clear of the river, so the
// whole runway is on dry land
static const float AP_CX = -2600.0f, AP_CZ = -3200.0f;
static const float RWY_HEADING_DEG = 50.0f;
static const float RWY_HALF = 1450.0f;   // half length
static const float RWY_HALF_W = 22.5f;   // half width

V3 worldAirportCenter() { return v3(AP_CX, 0, AP_CZ); }
float worldRunwayHeadingDeg() { return RWY_HEADING_DEG; }
V3 worldRunwayDir() { float a = deg2rad(RWY_HEADING_DEG); return v3(sinf(a), 0, -cosf(a)); }
V3 worldRunwayRight() { V3 d = worldRunwayDir(); return v3(-d.z, 0, d.x); }
V3 worldRunwayPoint(float s, float lat)
{
    V3 d = worldRunwayDir(), r = worldRunwayRight();
    return v3(AP_CX + d.x * s + r.x * lat, 0, AP_CZ + d.z * s + r.z * lat);
}
V3 worldApronCenter() { return worldRunwayPoint(0.0f, 380.0f); }

bool worldIsOnRunway(float x, float z)
{
    V3 rel = v3(x - AP_CX, 0, z - AP_CZ);
    V3 d = worldRunwayDir(), r = worldRunwayRight();
    float a = dot(rel, d), b = dot(rel, r);
    return fabsf(a) <= RWY_HALF + 25.0f && fabsf(b) <= 30.0f;
}

static float distToSeg(float px, float pz, float ax, float az, float bx, float bz)
{
    float vx = bx - ax, vz = bz - az;
    float wx = px - ax, wz = pz - az;
    float L2 = vx * vx + vz * vz;
    float t = (L2 > 0) ? clampf((wx * vx + wz * vz) / L2, 0.0f, 1.0f) : 0.0f;
    float dx = wx - vx * t, dz = wz - vz * t;
    return sqrtf(dx * dx + dz * dz);
}

static float distToPoly(const float pts[][2], int n, float x, float z)
{
    float best = 1e9f;
    for (int i = 0; i + 1 < n; i++)
    {
        float d = distToSeg(x, z, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
        if (d < best) best = d;
    }
    return best;
}

float worldRiverDist(float x, float z) { return distToPoly(gRiverPts, gRiverPtsCount, x, z); }
bool worldIsWater(float x, float z) { return worldRiverDist(x, z) < RIVER_WATER_HALF; }

float worldRiverLength()
{
    static float cached = -1.0f;
    if (cached > 0) return cached;
    float L = 0;
    for (int i = 0; i + 1 < gRiverPtsCount; i++)
    {
        float dx = gRiverPts[i + 1][0] - gRiverPts[i][0];
        float dz = gRiverPts[i + 1][1] - gRiverPts[i][1];
        L += sqrtf(dx * dx + dz * dz);
    }
    cached = L;
    return L;
}

void worldRiverFrameAtS(float s, V3& p, V3& dirOut)
{
    float total = worldRiverLength();
    s = fmodf(s, total);
    if (s < 0) s += total;
    for (int i = 0; i + 1 < gRiverPtsCount; i++)
    {
        float ax = gRiverPts[i][0], az = gRiverPts[i][1];
        float bx = gRiverPts[i + 1][0], bz = gRiverPts[i + 1][1];
        float dx = bx - ax, dz = bz - az;
        float L = sqrtf(dx * dx + dz * dz);
        if (s <= L)
        {
            float t = s / L;
            p = v3(ax + dx * t, 0, az + dz * t);
            dirOut = v3(dx / L, 0, dz / L);
            return;
        }
        s -= L;
    }
    p = v3(gRiverPts[gRiverPtsCount - 1][0], 0, gRiverPts[gRiverPtsCount - 1][1]);
    dirOut = v3(1, 0, 0);
}

const char* worldRoadName(int i) { return ROADS[i].name; }
int worldRoadPointCount(int i) { return ROADS[i].n; }
void worldRoadPoint(int i, int j, float& x, float& z) { x = ROADS[i].pts[j][0]; z = ROADS[i].pts[j][1]; }
float worldRoadWidth(int i) { return ROADS[i].width; }

static float roadDist(float x, float z)
{
    float best = 1e9f;
    for (int i = 0; i < gRoadCount; i++)
    {
        float d = distToPoly(ROADS[i].pts, ROADS[i].n, x, z) - ROADS[i].width * 0.5f;
        if (d < best) best = d;
    }
    return best;
}

static float runwayDist(float x, float z)
{
    V3 a = worldRunwayPoint(-RWY_HALF, 0), b = worldRunwayPoint(RWY_HALF, 0);
    return distToSeg(x, z, a.x, a.z, b.x, b.z);
}

// =====================================================================
// collision boxes (tall structures)
// =====================================================================
struct AABB { float x0, z0, x1, z1, h; };
static AABB sTalls[800];
static int sTallCount = 0;

static void addTall(float x, float z, float w, float d, float h)
{
    if (sTallCount >= 600) return;
    AABB& b = sTalls[sTallCount++];
    b.x0 = x - w * 0.5f; b.x1 = x + w * 0.5f;
    b.z0 = z - d * 0.5f; b.z1 = z + d * 0.5f;
    b.h = h;
}

bool worldHitBuilding(float x, float y, float z)
{
    for (int i = 0; i < sTallCount; i++)
    {
        const AABB& b = sTalls[i];
        if (y < b.h && x > b.x0 - 1.0f && x < b.x1 + 1.0f && z > b.z0 - 1.0f && z < b.z1 + 1.0f)
            return true;
    }
    return false;
}

// building list kept for tree/shape rejection
static AABB sAll[3200];
static int sAllCount = 0;

// ponds (kept out of building sites and tree spots)
static float sPondX[20], sPondZ[20], sPondR[20];
static int sPondCount = 0;

// tree positions, used to cast blob shadows on the ground
static float sTreeS[1500][3];
static int sTreeCount = 0;
static int sTreeTotal = 0;      // every tree placed, including beyond the collision cap
static int sBuildingTotal = 0;  // city buildings generated (--stats)

static void addAll(float x, float z, float w, float d, float h)
{
    if (sAllCount >= 2200) return;
    AABB& b = sAll[sAllCount++];
    b.x0 = x - w * 0.5f; b.x1 = x + w * 0.5f;
    b.z0 = z - d * 0.5f; b.z1 = z + d * 0.5f;
    b.h = h;
}

static bool insideAnyBuilding(float x, float z)
{
    for (int i = 0; i < sAllCount; i++)
    {
        const AABB& b = sAll[i];
        if (x > b.x0 - 2 && x < b.x1 + 2 && z > b.z0 - 2 && z < b.z1 + 2) return true;
    }
    return false;
}

// =====================================================================
// small drawing helpers
// =====================================================================

static bool sGroundTextured = false;
static TexId sWallTex = TEX_PLASTER;   // wall material for the list being compiled

static void rotatedQuad(float cx, float cz, float y, float ex, float ez, float yawDeg, float r, float g, float b)
{
    if (sGroundTextured)
    {
        // let the photograph show: wash the flat palette out towards white
        r = (0.55f + 0.45f * r) * 0.88f;
        g = (0.55f + 0.45f * g) * 1.00f;
        b = (0.55f + 0.45f * b) * 0.80f;
    }
    setColor(r, g, b);
    glPushMatrix();
    glTranslatef(cx, y, cz);
    glRotatef(-yawDeg, 0, 1, 0);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0.0f, 0.0f);              glVertex3f(-ex, 0, ez);
    glTexCoord2f(ex * 2.0f * 0.16f, 0.0f); glVertex3f(ex, 0, ez);
    glTexCoord2f(ex * 2.0f * 0.16f, ez * 2.0f * 0.16f); glVertex3f(ex, 0, -ez);
    glTexCoord2f(0.0f, ez * 2.0f * 0.16f); glVertex3f(-ex, 0, -ez);
    glEnd();
    glPopMatrix();
}

// strip of quads along a polyline (used for roads, river, ballast)
static void polyStrip(const float pts[][2], int n, float halfW, float y,
                      float r, float g, float b, float u0, float u1)
{
    (void)u0; (void)u1;
    setColor(r, g, b);
    for (int i = 0; i + 1 < n; i++)
    {
        float ax = pts[i][0], az = pts[i][1];
        float bx = pts[i + 1][0], bz = pts[i + 1][1];
        float dx = bx - ax, dz = bz - az;
        float L = sqrtf(dx * dx + dz * dz);
        if (L < 0.01f) continue;
        float nx = -dz / L * halfW, nz = dx / L * halfW;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(ax + nx, y, az + nz);
        glTexCoord2f(0.0f, L * 0.14f); glVertex3f(bx + nx, y, bz + nz);
        glTexCoord2f(halfW * 0.28f, L * 0.14f); glVertex3f(bx - nx, y, bz - nz);
        glTexCoord2f(halfW * 0.28f, 0.0f); glVertex3f(ax - nx, y, az - nz);
        glEnd();
    }
}

// a strip offset sideways from a polyline (sidewalks, edge lines, kerbs)
static void polyStripOffset(const float pts[][2], int n, float off, float halfW, float y,
                            float r, float g, float b)
{
    setColor(r, g, b);
    for (int i = 0; i + 1 < n; i++)
    {
        float ax = pts[i][0], az = pts[i][1];
        float bx = pts[i + 1][0], bz = pts[i + 1][1];
        float dx = bx - ax, dz = bz - az;
        float L = sqrtf(dx * dx + dz * dz);
        if (L < 0.01f) continue;
        float nx = -dz / L, nz = dx / L;
        float ax2 = ax + nx * off, az2 = az + nz * off;
        float bx2 = bx + nx * off, bz2 = bz + nz * off;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(ax2 + nx * halfW, y, az2 + nz * halfW);
        glVertex3f(bx2 + nx * halfW, y, bz2 + nz * halfW);
        glVertex3f(bx2 - nx * halfW, y, bz2 - nz * halfW);
        glVertex3f(ax2 - nx * halfW, y, az2 - nz * halfW);
        glEnd();
    }
}

// utility poles along a road with gently sagging wires - very Bangladeshi
static void utilityPoles(const float pts[][2], int n, float halfW)
{
    std::vector<V3> w1, w2;
    for (int i = 0; i + 1 < n; i++)
    {
        float ax = pts[i][0], az = pts[i][1];
        float bx = pts[i + 1][0], bz = pts[i + 1][1];
        float dx = bx - ax, dz = bz - az;
        float L = sqrtf(dx * dx + dz * dz);
        if (L < 1.0f) continue;
        float ux = dx / L, uz = dz / L;
        float nx = -uz, nz = ux;
        float off = halfW + 5.5f;
        int count = (int)(L / 56.0f);
        int k0 = (i == 0) ? 0 : 1;
        for (int k = k0; k <= count; k++)
        {
            float t = (float)k / (count + 1);
            float px = ax + dx * t + nx * off;
            float pz = az + dz * t + nz * off;
            if (worldRiverDist(px, pz) < 115.0f) continue;
            setColor(0.42f, 0.38f, 0.33f);
            glPushMatrix(); glTranslatef(px, 0, pz);
            drawCylinderRaw(0.15f, 0.10f, 8.6f, 5, false);
            glPopMatrix();
            setColor(0.34f, 0.31f, 0.27f);
            float armYaw = rad2deg(atan2f(ux, -uz));
            glPushMatrix(); glTranslatef(px, 8.0f, pz); glRotatef(-armYaw, 0, 1, 0);
            drawBox(2.4f, 0.14f, 0.14f);
            glPopMatrix();
            w1.push_back(v3(px + nx * 0.95f, 8.2f, pz + nz * 0.95f));
            w2.push_back(v3(px - nx * 0.95f, 8.2f, pz - nz * 0.95f));
        }
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setColorA(0.24f, 0.24f, 0.26f, 0.42f);
    for (int w = 0; w < 2; w++)
    {
        const std::vector<V3>& wire = (w == 0) ? w1 : w2;
        for (size_t i = 0; i + 1 < wire.size(); i++)
        {
            V3 a = wire[i], b = wire[i + 1];
            float span = sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
            if (span > 130.0f) continue;
            glBegin(GL_LINE_STRIP);
            for (int s = 0; s <= 4; s++)
            {
                float t = (float)s / 4.0f;
                glVertex3f(a.x + (b.x - a.x) * t,
                           a.y + (b.y - a.y) * t - 0.6f * sinf(t * PI_F),
                           a.z + (b.z - a.z) * t);
            }
            glEnd();
        }
    }
    glDisable(GL_BLEND);
}

// =====================================================================
// display lists
// =====================================================================
static GLuint L_GROUND, L_HILLS, L_RIVER, L_ROADS, L_RAIL, L_BUILDINGS, L_TREES, L_AIRPORT, L_MAZAR, L_SIU, L_CLOCK;

// ---- distance LOD -------------------------------------------------------
// Every city chunk is compiled twice: once in full detail (body, window bands,
// balconies, rooftop clutter) and once as a plain box per building. Near the
// camera the full list is drawn; far away the cheap one is. A building is a
// handful of boxes when far, so the frame rate holds up over the city.
struct SimpleBuilding
{
    float x, z, w, d, h;
    float r, g, b;
    int rf;                             // roof style, for the per-chunk roof pass
};
static const int MAX_SIMPLE = 64;      // 25 grid slots per chunk, plus slack
static SimpleBuilding sSimpleB[196][MAX_SIMPLE];
static int sSimpleCount[196];
static GLuint sChunkSimple[196];
static GLuint sChunkRoof[196];         // one roof pass per chunk: see buildBuildings
static int sCurChunk = -1;             // chunk being compiled, or -1
static TexId sCurWallTex = TEX_PLASTER; // wall material of that chunk

// the shadows are a single blended pass over the whole city and they are barely
// visible from altitude, so they are skipped when the camera is high up
static float sCamY = 0.0f;
static float sCamX = 0.0f, sCamZ = 0.0f;
void worldSetCameraY(float y) { sCamY = y; }
void worldSetCameraXZ(float x, float z) { sCamX = x; sCamZ = z; }
static GLuint L_POIS, L_SHADOWS, L_CROWD, L_DETAILS, L_DETAILS2, L_BILLBOARD;

// generated textures (defined further down with the procedural texture code)
static GLuint sTexAsphalt = 0;
static GLuint sTexGrass = 0;
static GLuint sTexWater = 0;
static GLuint makeWaterTexture(int size);

static unsigned sSeed = 20241111u;

static void drawBanglaFlag(float x, float y, float z, float yawDeg, float size);

static void drawTree(float x, float z, int type)
{
    sTreeTotal++;
    // a photograph for the trunk and one for the crown
    bool barkBound = textureBind((type == 1) ? TEX_BARK_PALM : (type == 2 ? TEX_BARK : TEX_BARK2));
    if (sTreeCount < 1500)
    {
        sTreeS[sTreeCount][0] = x;
        sTreeS[sTreeCount][1] = z;
        sTreeS[sTreeCount][2] = (type == 1) ? 1.8f : 2.4f;
        sTreeCount++;
    }
    if (type == 1)
    {
        setColor(0.42f, 0.33f, 0.22f);
        glPushMatrix(); glTranslatef(x, 0, z); glTranslatef(0, 0, 0);
        drawCylinderRaw(0.20f, 0.14f, 7.5f, 5, false);
        glPopMatrix();
        if (barkBound) textureBind(TEX_LEAVES);
        setColor(0.55f, 0.80f, 0.45f);
        glPushMatrix(); glTranslatef(x, 7.6f, z);
        drawSphere(1.9f, 6, 4);
        glPopMatrix();
    }
    else if (type == 2)
    {
        // broadleaf shade tree
        setColor(0.40f, 0.31f, 0.21f);
        glPushMatrix(); glTranslatef(x, 0, z);
        drawCylinderRaw(0.34f, 0.26f, 3.6f, 5, false);
        glPopMatrix();
        float tone = frand01(sSeed) * 0.08f;
        if (barkBound) textureBind(TEX_LEAVES);
        setColor(0.62f + tone, 0.90f + tone, 0.50f);
        glPushMatrix(); glTranslatef(x - 0.8f, 4.4f, z); drawSphere(1.9f, 6, 4); glPopMatrix();
        setColor(0.70f + tone, 0.96f + tone, 0.58f);
        glPushMatrix(); glTranslatef(x + 0.9f, 4.9f, z - 0.4f); drawSphere(1.7f, 6, 4); glPopMatrix();
        glPushMatrix(); glTranslatef(x, 5.9f, z + 0.5f); drawSphere(1.5f, 6, 4); glPopMatrix();
    }
    else
    {
        setColor(0.40f, 0.31f, 0.21f);
        glPushMatrix(); glTranslatef(x, 0, z);
        drawCylinderRaw(0.30f, 0.24f, 3.2f, 5, false);
        glPopMatrix();
        float tone = frand01(sSeed) * 0.10f;
        if (barkBound) textureBind(TEX_LEAVES);
        setColor(0.55f + tone, 0.82f + tone, 0.42f + tone * 0.5f);
        glPushMatrix(); glTranslatef(x, 2.8f, z);
        drawCone(2.7f, 4.6f, 7);
        glPopMatrix();
        setColor(0.26f + tone, 0.52f + tone, 0.24f + tone * 0.5f);
        glPushMatrix(); glTranslatef(x, 5.6f, z);
        drawCone(1.8f, 3.4f, 7);
        glPopMatrix();
    }
}

// ---------------------------------------------------------------------
// ground: base plane + fields
// ---------------------------------------------------------------------
#define STB_PERLIN_IMPLEMENTATION
#include "../third_party/stb/stb_perlin.h"

static void buildGround()
{
    glNewList(L_GROUND = glGenLists(1), GL_COMPILE);

    // The ground is a large grid rather than one flat quad, with Perlin noise
    // modulating the green: that gives the fields the mottled look of real
    // farmland instead of a single sheet of colour.
    bool grassTex = false;
    {
        const int N = 40;
        const float half = 3400.0f, step = 2.0f * half / (float)N;
        grassTex = textureBind(TEX_GRASS);
        const float r0 = 0.44f, g0 = 0.56f, b0 = 0.33f;
        for (int i = 0; i < N; i++)
        {
            float x0 = -half + i * step, x1 = x0 + step;
            for (int j = 0; j < N; j++)
            {
                float z0 = -half + j * step, z1 = z0 + step;
                float n1 = stb_perlin_noise3(x0 * 0.0011f, z0 * 0.0011f, 0.0f, 0, 0, 0);
                float n2 = stb_perlin_noise3(x0 * 0.0050f, z0 * 0.0050f, 4.7f, 0, 0, 0);
                float k = 1.0f + 0.15f * n1 + 0.06f * n2;
                if (grassTex) setColor(0.72f * k, 1.02f * k, 0.58f * k);
                else setColor(r0 * k, g0 * k, b0 * k);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0);
                if (grassTex)
                {
                    glTexCoord2f(x0 * 0.035f, z0 * 0.035f); glVertex3f(x0, 0.0f, z1);
                    glTexCoord2f(x1 * 0.035f, z0 * 0.035f); glVertex3f(x1, 0.0f, z1);
                    glTexCoord2f(x1 * 0.035f, z1 * 0.035f); glVertex3f(x1, 0.0f, z0);
                    glTexCoord2f(x0 * 0.035f, z1 * 0.035f); glVertex3f(x0, 0.0f, z0);
                }
                else
                {
                    glVertex3f(x0, 0.0f, z1); glVertex3f(x1, 0.0f, z1);
                    glVertex3f(x1, 0.0f, z0); glVertex3f(x0, 0.0f, z0);
                }
                glEnd();
            }
        }
    }
    // flat ground beyond the detailed grid, out to the horizon
    sGroundTextured = grassTex;
    setColor(0.44f, 0.56f, 0.33f);
    groundQuad(-8000, -8000, -3400, 8000, 0.0f);
    groundQuad(3400, -8000, 8000, 8000, 0.0f);
    groundQuad(-3400, -8000, 3400, -3400, 0.0f);
    groundQuad(-3400, 3400, 3400, 8000, 0.0f);

    // paved and dusty ground under the built-up area
    static const float uPal[3][3] = {
        { 0.50f, 0.54f, 0.45f }, { 0.45f, 0.50f, 0.44f }, { 0.42f, 0.47f, 0.42f }
    };
    for (int i = 0; i < 700; i++)
    {
        float x = (frand01(sSeed) * 2 - 1) * 2500.0f;
        float z = (frand01(sSeed) * 2 - 1) * 2500.0f;
        int c = (int)(frand01(sSeed) * 3);
        rotatedQuad(x, z, 0.02f, 14 + frand01(sSeed) * 20, 13 + frand01(sSeed) * 19,
                    frand01(sSeed) * 180, uPal[c][0], uPal[c][1], uPal[c][2]);
    }

    static const float pal[6][3] = {
        { 0.50f, 0.62f, 0.34f }, { 0.58f, 0.65f, 0.36f }, { 0.62f, 0.62f, 0.38f },
        { 0.45f, 0.58f, 0.31f }, { 0.58f, 0.58f, 0.34f }, { 0.40f, 0.55f, 0.32f }
    };
    // The countryside patches each get a ground photograph. They are generated
    // into a table first and drawn grouped by material, so the display list
    // switches texture four times instead of five hundred.
    {
        struct FieldPatch { float x, z, ex, ez, yaw, k; int c, mat; };
        enum { MAX_FIELD = 640, FIELD_MATS = 4 };
        static FieldPatch fields[MAX_FIELD];
        static const TexId FIELD_TEX[FIELD_MATS] = {
            TEX_GRASS_LEAFY, TEX_GRASS_DRY, TEX_SOIL_AERIAL, TEX_FIELD_DRY
        };
        int nFields = 0;
        for (int i = 0; i < 520 && nFields < MAX_FIELD; i++)
        {
            float x = (frand01(sSeed) * 2 - 1) * 7600;
            float z = (frand01(sSeed) * 2 - 1) * 7600;
            float ex = 30 + frand01(sSeed) * 60;
            float ez = 28 + frand01(sSeed) * 55;
            float yaw = frand01(sSeed) * 180;
            if (x * x + z * z < 1700.0f * 1700.0f) continue;
            if (worldRiverDist(x, z) < 130) continue;
            if (runwayDist(x, z) < 200) continue;
            V3 ap = worldApronCenter();
            if ((x - ap.x) * (x - ap.x) + (z - ap.z) * (z - ap.z) < 300 * 300) continue;
            int c = (int)(frand01(sSeed) * 6);
            // vary each patch with slow noise so the countryside is not made of
            // perfectly flat colour tiles
            float nz = stb_perlin_noise3(x * 0.0016f, z * 0.0016f, 11.3f, 0, 0, 0);
            FieldPatch& fp = fields[nFields++];
            fp.x = x; fp.z = z; fp.ex = ex; fp.ez = ez; fp.yaw = yaw;
            fp.k = 1.0f + 0.10f * nz;
            fp.c = c;
            // which crop this field grows: paddy near the river, dry soil further
            // out. Chosen from the position (no extra RNG draws, so the rest of
            // the city is generated exactly as before).
            float wd = worldRiverDist(x, z);
            unsigned fh = (unsigned)(fabsf(x) * 13.0f + fabsf(z) * 29.0f);
            fp.mat = (wd < 900.0f) ? 0 : (1 + (int)(fh % 3u));
        }
        for (int m = 0; m < FIELD_MATS; m++)
        {
            bool bound = textureBind(FIELD_TEX[m]);
            for (int i = 0; i < nFields; i++)
            {
                if (fields[i].mat != m) continue;
                rotatedQuad(fields[i].x, fields[i].z, 0.03f, fields[i].ex, fields[i].ez, fields[i].yaw,
                            pal[fields[i].c][0] * fields[i].k, pal[fields[i].c][1] * fields[i].k,
                            pal[fields[i].c][2] * fields[i].k);
            }
            if (!bound) continue;
        }
        textureBind(TEX_GRASS);     // back to the base ground for what follows
    }
    // a few shiny paddy fields
    for (int i = 0; i < 26; i++)
    {
        float x = (frand01(sSeed) * 2 - 1) * 6400;
        float z = (frand01(sSeed) * 2 - 1) * 6400;
        if (x * x + z * z < 2400.0f * 2400.0f) continue;
        if (worldRiverDist(x, z) < 160) continue;
        if (runwayDist(x, z) < 300) continue;
        rotatedQuad(x, z, 0.02f, 60 + frand01(sSeed) * 120, 50 + frand01(sSeed) * 100, frand01(sSeed) * 180,
                    0.42f, 0.56f, 0.50f);
    }

    sGroundTextured = false;
    if (grassTex) textureUnbind();
    // village ponds - Sylhet is full of them
    sPondCount = 0;
    for (int i = 0; i < 30 && sPondCount < 20; i++)
    {
        float ang = frand01(sSeed) * 6.2831853f;
        float dist = 900.0f + frand01(sSeed) * 3300.0f;
        float x = cosf(ang) * dist, z = sinf(ang) * dist;
        if (worldRiverDist(x, z) < 230.0f) continue;
        if (runwayDist(x, z) < 320.0f) continue;
        if (fabsf(x - 1600.0f) < 90.0f && z > 1820.0f && z < 2300.0f) continue;   // SIU flying strip
        V3 ap = worldApronCenter();
        if ((x - ap.x) * (x - ap.x) + (z - ap.z) * (z - ap.z) < 430.0f * 430.0f) continue;
        float rx = 35 + frand01(sSeed) * 55;
        float rz = 28 + frand01(sSeed) * 45;
        sPondX[sPondCount] = x; sPondZ[sPondCount] = z; sPondR[sPondCount] = (rx > rz ? rx : rz);
        sPondCount++;
        glPushMatrix(); glTranslatef(x, 0.06f, z); glScalef(rx + 7.0f, 1.0f, rz + 7.0f);
        setColor(0.66f, 0.64f, 0.58f);
        drawDiskXZ(1.0f, 18);
        glPopMatrix();
        glPushMatrix(); glTranslatef(x, 0.10f, z); glScalef(rx, 1.0f, rz);
        setColor(0.22f, 0.40f, 0.44f);
        drawDiskXZ(1.0f, 18);
        glPopMatrix();
    }
    glEndList();
}

// ---------------------------------------------------------------------
// river: banks, water, ghat steps
// ---------------------------------------------------------------------
static void buildRiver()
{
    glNewList(L_RIVER = glGenLists(1), GL_COMPILE);
    setColor(0.62f, 0.56f, 0.43f);
    polyStrip(gRiverPts, gRiverPtsCount, RIVER_BANK_HALF, 0.05f, 0.62f, 0.56f, 0.43f, 0, 0);
    if (sTexWater)
    {
        // the water photograph is generated, and its coordinates are scrolled at
        // draw time (worldDrawStatic) so the river flows
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, sTexWater);
        polyStrip(gRiverPts, gRiverPtsCount, RIVER_WATER_HALF, 0.12f, 0.96f, 1.0f, 1.0f, 0, 0);
        textureUnbind();
    }
    else
    {
        polyStrip(gRiverPts, gRiverPtsCount, RIVER_WATER_HALF, 0.12f, 0.24f, 0.42f, 0.58f, 0, 0);
    }

    // ghat: stone steps down to the water, near the clock tower (river point index 5)
    float vx = gRiverPts[5][0], vz = gRiverPts[5][1];
    float d0x = vx - gRiverPts[4][0], d0z = vz - gRiverPts[4][1];
    float d1x = gRiverPts[6][0] - vx, d1z = gRiverPts[6][1] - vz;
    float l0 = sqrtf(d0x * d0x + d0z * d0z), l1 = sqrtf(d1x * d1x + d1z * d1z);
    float dx = d0x / l0 + d1x / l1, dz = d0z / l0 + d1z / l1;
    float dl = sqrtf(dx * dx + dz * dz);
    dx /= dl; dz /= dl;
    float lx = dz, lz = -dx;   // left bank normal (towards the clock tower)
    float yawDeg = rad2deg(atan2f(dx, -dz));
    for (int i = 0; i < 6; i++)
    {
        float d = 100.0f - i * 13.0f;
        float sy = 0.35f - i * 0.42f;
        float sx = vx + lx * d, szz = vz + lz * d;
        glPushMatrix();
        glTranslatef(sx, sy, szz);
        glRotatef(-yawDeg, 0, 1, 0);
        setColor(0.72f, 0.70f, 0.66f);
        drawBox(28.0f, 0.5f, 13.0f);
        glPopMatrix();
    }
    // two small bollards at the top
    for (int i = 0; i < 2; i++)
    {
        float d = 102.0f;
        float sx = vx + lx * d + dx * (i == 0 ? -14.0f : 14.0f);
        float szz = vz + lz * d + dz * (i == 0 ? -14.0f : 14.0f);
        setColor(0.55f, 0.52f, 0.48f);
        drawBoxAt(sx, 0.6f, szz, 0.7f, 1.2f, 0.7f);
    }

    // wooden jetties, moored boats and a little sparkle on the water
    {
        float L = worldRiverLength();
        for (int i = 0; i < 6; i++)
        {
            float s = L * (0.08f + 0.15f * i);
            V3 p, dr;
            worldRiverFrameAtS(s, p, dr);
            float side = (i % 2 == 0) ? 1.0f : -1.0f;
            V3 nrm = v3(dr.z, 0, -dr.x) * side;
            float px = p.x + nrm.x * 92.0f, pz = p.z + nrm.z * 92.0f;
            float yawb = rad2deg(yawFromDir(nrm));
            setColor(0.55f, 0.44f, 0.30f);
            glPushMatrix(); glTranslatef(px, 0.35f, pz); glRotatef(-yawb, 0, 1, 0);
            drawBox(4.4f, 0.30f, 24.0f);
            glPopMatrix();
            setColor(0.40f, 0.32f, 0.22f);
            for (int k = 0; k < 3; k++)
            {
                float dd = -9.0f + k * 9.0f;
                drawBoxAt(px - nrm.x * dd, 0.0f, pz - nrm.z * dd, 0.45f, 1.5f, 0.45f);
            }
            float bx = p.x + nrm.x * 42.0f, bz = p.z + nrm.z * 42.0f;
            float yawboat = rad2deg(yawFromDir(dr));
            glPushMatrix(); glTranslatef(bx, -0.05f, bz); glRotatef(-yawboat, 0, 1, 0);
            setColor(0.60f, 0.46f, 0.30f);
            drawBoxAt(0, 0.35f, 0, 1.6f, 0.55f, 5.6f);
            setColor(0.35f, 0.45f, 0.55f);
            drawBoxAt(0, 0.95f, 0.9f, 1.7f, 0.5f, 2.0f);
            glPopMatrix();
        }
        for (int i = 0; i < 90; i++)
        {
            float s = frand01(sSeed) * L;
            V3 p, dr;
            worldRiverFrameAtS(s, p, dr);
            float lat = (frand01(sSeed) * 2 - 1) * 48.0f;
            V3 nrm = v3(dr.z, 0, -dr.x);
            float px = p.x + nrm.x * lat, pz = p.z + nrm.z * lat;
            rotatedQuad(px, pz, 0.145f, 2.0f + frand01(sSeed) * 6.0f, 0.35f,
                        rad2deg(yawFromDir(dr)), 0.34f, 0.52f, 0.66f);
        }
    }
    glEndList();
}

// ---------------------------------------------------------------------
// bridges across the Surma
// ---------------------------------------------------------------------
static void keaneBridge()
{
    // the photographs show red-brown riveted steel: give the trusses a steel
    // surface instead of flat colour
    textureBind(TEX_METAL_RUST);
    // Zindabazar Road crosses the river at (0,105); road runs along Z
    float cx = 0.0f, cz = 105.0f;

    setColor(0.42f, 0.40f, 0.38f);
    drawBoxAt(cx, 1.05f, cz, 16.0f, 0.9f, 280.0f);            // deck
    setColor(0.31f, 0.31f, 0.33f);
    drawBoxAt(cx, 1.58f, cz, 14.0f, 0.15f, 276.0f);           // asphalt surface
    // railings: posts with a top rail, instead of one solid bar
    for (int side = 0; side < 2; side++)
    {
        float rx = cx + (side == 0 ? -7.7f : 7.7f);
        setColor(0.82f, 0.80f, 0.75f);
        drawBoxAt(rx, 2.62f, cz, 0.16f, 0.16f, 272.0f);       // top rail
        for (int p = 0; p <= 34; p++)
        {
            float z = cz - 136.0f + p * 8.0f;
            drawBoxAt(rx, 2.10f, z, 0.11f, 1.0f, 0.11f);      // posts
        }
    }

    // lamp posts along the bridge
    for (int i = 0; i < 6; i++)
    {
        float z = cz - 110.0f + i * 44.0f;
        for (int side = 0; side < 2; side++)
        {
            float x = cx + (side == 0 ? -7.6f : 7.6f);
            setColor(0.30f, 0.30f, 0.30f);
            drawBoxAt(x, 3.1f, z, 0.12f, 2.2f, 0.12f);
            setColor(0.95f, 0.85f, 0.55f);
            glPushMatrix(); glTranslatef(x, 4.30f, z); drawSphere(0.22f, 6, 4); glPopMatrix();
        }
    }

    // steel trusses on both sides
    for (int side = 0; side < 2; side++)
    {
        float sx = cx + (side == 0 ? -8.2f : 8.2f);
        setColor(0.58f, 0.27f, 0.21f);   // the bridge is painted red-brown
        for (int p = 0; p <= 10; p++)
        {
            float z = cz - 140.0f + p * 28.0f;
            drawBoxAt(sx, 3.6f, z, 0.5f, 4.2f, 0.5f);
        }
        drawBoxAt(sx, 5.75f, cz, 0.45f, 0.45f, 282.0f);       // top chord
        drawBoxAt(sx, 1.70f, cz, 0.45f, 0.45f, 282.0f);       // bottom chord
        // riveted gusset plates where the verticals meet the top chord
        setColor(0.64f, 0.31f, 0.24f);   // gusset plates, a shade brighter
        for (int p = 0; p <= 10; p++)
        {
            float z = cz - 140.0f + p * 28.0f;
            drawBoxAt(sx, 5.75f, z, 0.62f, 0.62f, 0.62f);
        }
        // diagonals
        for (int p = 0; p < 10; p++)
        {
            float z = cz - 140.0f + p * 28.0f + 14.0f;
            float ang = (p % 2 == 0) ? 81.0f : -81.0f;
            glPushMatrix();
            glTranslatef(sx, 3.6f, z);
            glRotatef(ang, 1, 0, 0);
            drawBox(0.35f, 28.6f, 0.35f);
            glPopMatrix();
        }
    }
    // cross bracing between the two trusses, under the deck
    setColor(0.34f, 0.42f, 0.38f);
    for (int p = 0; p < 10; p++)
    {
        float z0 = cz - 140.0f + p * 28.0f, z1 = z0 + 28.0f;
        for (int b = 0; b < 2; b++)
        {
            float ax = cx - 8.2f, bx = cx + 8.2f;
            float az = (b == 0) ? z0 : z1, bz = (b == 0) ? z1 : z0;
            V3 d = v3(bx - ax, 0, bz - az);
            float L = len(d);
            float ang = rad2deg(atan2f(d.x, -d.z));
            glPushMatrix();
            glTranslatef((ax + bx) * 0.5f, 1.45f, (az + bz) * 0.5f);
            glRotatef(ang, 0, 1, 0);
            drawBox(0.22f, 0.22f, L);
            glPopMatrix();
        }
    }

    // end towers with lamps
    for (int i = 0; i < 4; i++)
    {
        float sx = cx + ((i & 1) ? 8.2f : -8.2f);
        float sz = cz + ((i & 2) ? 142.0f : -132.0f);
        setColor(0.60f, 0.58f, 0.54f);
        drawBoxAt(sx, 4.9f, sz, 1.4f, 6.8f, 1.4f);
        setColor(0.95f, 0.85f, 0.45f);
        glPushMatrix(); glTranslatef(sx, 8.6f, sz); drawSphere(0.45f, 6, 4); glPopMatrix();
    }
    // ramps
    setColor(0.31f, 0.31f, 0.33f);
    glPushMatrix(); glTranslatef(cx, 0.85f, cz - 160.0f); glRotatef(-2.2f, 1, 0, 0); drawBox(14.0f, 0.5f, 46.0f); glPopMatrix();
    glPushMatrix(); glTranslatef(cx, 0.85f, cz + 160.0f); glRotatef(2.2f, 1, 0, 0);  drawBox(14.0f, 0.5f, 46.0f); glPopMatrix();

    // flags on poles and a small market under the bridge
    for (int i = 0; i < 5; i++)
    {
        float z = cz - 100.0f + i * 50.0f;
        setColor(0.55f, 0.55f, 0.58f);
        drawBoxAt(cx + 7.9f, 4.0f, z, 0.16f, 6.0f, 0.16f);
        drawBanglaFlag(cx + 7.9f, 6.2f, z, 0.0f, 1.6f);
    }
    for (int i = 0; i < 4; i++)
    {
        float sx = (i % 2 ? -1.0f : 1.0f) * 17.0f;
        float sz = cz - 80.0f + (i / 2) * 160.0f;
        setColor(0.62f, 0.55f, 0.45f);
        drawBoxAt(sx, 1.3f, sz, 4.0f, 2.6f, 3.4f);
        setColor(0.75f, 0.20f, 0.16f);
        glPushMatrix(); glTranslatef(sx, 2.75f, sz); glRotatef(18.0f, 0, 0, 1);
        drawBox(4.6f, 0.12f, 3.8f);
        glPopMatrix();
    }
    // bus stop near the bridge end
    setColor(0.60f, 0.62f, 0.66f);
    for (int p = 0; p < 4; p++)
        drawBoxAt(22.0f + ((p & 1) ? 3.0f : -3.0f), 1.3f, -30.0f + ((p & 2) ? 1.2f : -1.2f), 0.16f, 2.6f, 0.16f);
    setColor(0.42f, 0.52f, 0.62f);
    drawBoxAt(22.0f, 2.72f, -30.0f, 7.4f, 0.16f, 3.2f);
}

static void plainBridge(float cx, float cz, float yawDeg, float length, bool alongX)
{
    glPushMatrix();
    glTranslatef(cx, 0.55f, cz);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.45f, 0.44f, 0.42f);
    drawBox(alongX ? length : 17.0f, 0.6f, alongX ? 17.0f : length);
    setColor(0.31f, 0.31f, 0.33f);
    drawBox(alongX ? length - 2.0f : 15.0f, 0.12f, alongX ? 15.0f : length - 2.0f);
    setColor(0.78f, 0.76f, 0.72f);
    if (alongX)
    {
        drawBoxAt(0, 0.65f, -7.6f, length - 2.0f, 0.9f, 0.35f);
        drawBoxAt(0, 0.65f, 7.6f, length - 2.0f, 0.9f, 0.35f);
    }
    else
    {
        drawBoxAt(-7.6f, 0.65f, 0, 0.35f, 0.9f, length - 2.0f);
        drawBoxAt(7.6f, 0.65f, 0, 0.35f, 0.9f, length - 2.0f);
    }
    glPopMatrix();
    // piers
    setColor(0.55f, 0.53f, 0.50f);
    for (int i = -1; i <= 1; i += 2)
    {
        float ox = alongX ? cx + i * length * 0.25f : cx;
        float oz = alongX ? cz : cz + i * length * 0.25f;
        drawBoxAt(ox, 0.3f, oz, 5.0f, 0.6f, 5.0f);
    }
}

// roadside billboards; a few carry a stroke-font slogan
static void drawBillboard(float x, float z, float faceYawDeg, float r, float g, float b, const char* text)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(-faceYawDeg, 0, 1, 0);
    setColor(0.45f, 0.45f, 0.48f);
    drawBoxAt(-3.4f, 3.2f, 0, 0.4f, 6.4f, 0.4f);
    drawBoxAt(3.4f, 3.2f, 0, 0.4f, 6.4f, 0.4f);
    setColor(r, g, b);
    drawBoxAt(0, 7.4f, 0, 9.0f, 4.2f, 0.35f);
    setColor(r * 0.7f, g * 0.7f, b * 0.7f);
    drawBoxAt(0, 9.6f, 0, 9.3f, 0.4f, 0.5f);
    if (text)
    {
        setColor(0.98f, 0.98f, 0.95f);
        glLineWidth(2.0f);
        glPushMatrix();
        glTranslatef(0, 6.9f, -0.24f);
        glRotatef(180.0f, 0, 1, 0);
        strokeTextCentered(GLUT_STROKE_ROMAN, text, 0.014f);
        glPopMatrix();
        glLineWidth(1.0f);
    }
    glPopMatrix();
}

static void billboardsScene()
{
    drawBillboard(-260.0f, -240.0f, -45.0f, 0.10f, 0.42f, 0.24f, "SYLHET");
    drawBillboard(19.0f, -900.0f, -90.0f, 0.72f, 0.20f, 0.16f, "FLY SYLHET");
    drawBillboard(-900.0f, -19.0f, 180.0f, 0.16f, 0.32f, 0.66f, "SYLHET CITY");
    drawBillboard(19.0f, 1500.0f, -90.0f, 0.85f, 0.70f, 0.15f, 0);
    drawBillboard(900.0f, 19.0f, 0.0f, 0.55f, 0.18f, 0.52f, 0);
    drawBillboard(-1650.0f, -19.0f, 180.0f, 0.20f, 0.55f, 0.60f, 0);
    drawBillboard(2619.0f, -900.0f, -90.0f, 0.80f, 0.45f, 0.12f, 0);
    drawBillboard(-1012.0f, 600.0f, 90.0f, 0.88f, 0.88f, 0.86f, 0);
}

// street lights on both sides of a main road
static void streetLights(const float pts[][2], int n, float halfW)
{
    int step = 0;
    for (int i = 0; i + 1 < n; i++)
    {
        float ax = pts[i][0], az = pts[i][1];
        float bx = pts[i + 1][0], bz = pts[i + 1][1];
        float dx = bx - ax, dz = bz - az;
        float L = sqrtf(dx * dx + dz * dz);
        if (L < 1.0f) continue;
        float ux = dx / L, uz = dz / L;
        float nx = -uz, nz = ux;
        int count = (int)(L / 85.0f);
        for (int k = 0; k <= count; k++, step++)
        {
            float t = (float)k / (count + 1);
            float side = (step % 2 == 0) ? 1.0f : -1.0f;
            float px = ax + dx * t + nx * side * (halfW + 3.4f);
            float pz = az + dz * t + nz * side * (halfW + 3.4f);
            if (worldRiverDist(px, pz) < 115.0f) continue;
            setColor(0.60f, 0.60f, 0.63f);
            glPushMatrix(); glTranslatef(px, 0, pz);
            drawCylinderRaw(0.11f, 0.08f, 6.6f, 5, false);
            glPopMatrix();
            // arm reaching over the road
            float dirX = -nx * side, dirZ = -nz * side;
            float armYaw = rad2deg(atan2f(dirX, -dirZ));
            glPushMatrix(); glTranslatef(px, 6.5f, pz); glRotatef(-armYaw, 0, 1, 0);
            drawBoxAt(0, 0, -0.85f, 0.14f, 0.14f, 1.7f);
            setColor(0.93f, 0.91f, 0.80f);
            drawBoxAt(0, -0.16f, -1.7f, 0.55f, 0.22f, 0.9f);
            glPopMatrix();
        }
    }
}

// ---------------------------------------------------------------------
// roads + markings
// ---------------------------------------------------------------------
static void buildRoads()
{
    glNewList(L_ROADS = glGenLists(1), GL_COMPILE);
    // each road gets its own asphalt photograph: worn, fresh, patchy or gravel.
    // polyStrip() writes texture coordinates, so binding is all it takes.
    static const TexId ROADMAT[5] = {
        TEX_ASPHALT2, TEX_ROAD_WORN, TEX_ASPHALT_WORN, TEX_ASPHALT_CLEAN, TEX_ROAD_GRAVEL
    };
    for (int i = 0; i < gRoadCount; i++)
    {
        bool bound = textureBind(ROADMAT[i % 5]);
        polyStrip(ROADS[i].pts, ROADS[i].n, ROADS[i].width * 0.5f, 0.10f,
                  bound ? 0.90f : 0.29f, bound ? 0.90f : 0.29f, bound ? 0.92f : 0.31f, 0, 0);
    }
    // junction squares smooth out the overlaps
    static const float JUNC[][2] = {
        { 0, 0 }, { 0, 1200 }, { -1000, 0 }, { -1000, 1200 }, { -2100, 0 }, { -2100, 1200 },
        { 2600, 0 }, { 2600, 1200 }, { 0, 2900 }, { 2600, 2900 }, { 0, 3040 }, { 1700, 1200 }, { 1000, 600 }
    };
    for (int i = 0; i < (int)(sizeof(JUNC) / sizeof(JUNC[0])); i++)
    {
        rotatedQuad(JUNC[i][0], JUNC[i][1], 0.13f, 15.0f, 15.0f, 0, 0.27f, 0.27f, 0.29f);
    }

    // markings are painted, not asphalt: drop the texture so they stay white
    textureUnbind();

    // centre dashes on the main roads
    setColor(0.93f, 0.93f, 0.88f);
    for (int i = 0; i < gRoadCount; i++)
    {
        if (ROADS[i].width < 20.0f) continue;
        for (int s = 0; s + 1 < ROADS[i].n; s++)
        {
            float ax = ROADS[i].pts[s][0], az = ROADS[i].pts[s][1];
            float bx = ROADS[i].pts[s + 1][0], bz = ROADS[i].pts[s + 1][1];
            float dx = bx - ax, dz = bz - az;
            float L = sqrtf(dx * dx + dz * dz);
            if (L < 1) continue;
            float yaw = rad2deg(atan2f(dx, -dz));
            int n = (int)(L / 15.0f);
            for (int k = 0; k < n; k++)
            {
                float t = (k + 0.5f) / n;
                drawBoxAtRot(ax + dx * t, 0.15f, az + dz * t, 0.5f, 0.06f, 5.0f, yaw);
            }
        }
    }

    // zebra crossings near the main junctions (bars run along the road, like at home)
    setColor(0.90f, 0.90f, 0.86f);
    float zebraZ[4] = { -16.0f, 16.0f, 1184.0f, 1216.0f };
    for (int z = 0; z < 4; z++)
    {
        for (int i = -6; i <= 6; i++)
        {
            drawBoxAt(i * 1.9f, 0.17f, zebraZ[z], 1.0f, 0.05f, 4.0f);
        }
    }
    float zebraX[4] = { -16.0f, 16.0f, -1016.0f, -984.0f };
    for (int z = 0; z < 4; z++)
    {
        for (int i = -5; i <= 5; i++)
        {
            drawBoxAt(zebraX[z], 0.17f, i * 1.9f, 4.0f, 0.05f, 1.0f);
        }
    }

    // stop lines at the busiest junctions
    setColor(0.90f, 0.90f, 0.86f);
    drawBoxAt(0, 0.16f, 30.0f, 12.0f, 0.05f, 0.8f);
    drawBoxAt(0, 0.16f, -30.0f, 12.0f, 0.05f, 0.8f);
    drawBoxAt(30.0f, 0.16f, 0, 0.8f, 0.05f, 10.0f);
    drawBoxAt(-30.0f, 0.16f, 0, 0.8f, 0.05f, 10.0f);

    // sidewalks, kerbs and edge lines on the main roads
    for (int i = 0; i < gRoadCount; i++)
    {
        if (ROADS[i].width < 20.0f) continue;
        float hw = ROADS[i].width * 0.5f;
        polyStripOffset(ROADS[i].pts, ROADS[i].n,  hw + 2.0f, 1.9f, 0.12f, 0.70f, 0.69f, 0.65f);
        polyStripOffset(ROADS[i].pts, ROADS[i].n, -hw - 2.0f, 1.9f, 0.12f, 0.70f, 0.69f, 0.65f);
        polyStripOffset(ROADS[i].pts, ROADS[i].n,  hw + 0.4f, 0.25f, 0.13f, 0.82f, 0.81f, 0.77f);
        polyStripOffset(ROADS[i].pts, ROADS[i].n, -hw - 0.4f, 0.25f, 0.13f, 0.82f, 0.81f, 0.77f);
        polyStripOffset(ROADS[i].pts, ROADS[i].n,  hw - 1.1f, 0.22f, 0.14f, 0.92f, 0.92f, 0.88f);
        polyStripOffset(ROADS[i].pts, ROADS[i].n, -hw + 1.1f, 0.22f, 0.14f, 0.92f, 0.92f, 0.88f);
    }

    // yellow box junctions at the two main crossings
    setColor(0.85f, 0.78f, 0.22f);
    for (int j = 0; j < 2; j++)
    {
        float jz = (j == 0) ? 0.0f : 1200.0f;
        drawBoxAt(0.0f, 0.165f, jz - 10.0f, 20.0f, 0.03f, 0.5f);
        drawBoxAt(0.0f, 0.165f, jz + 10.0f, 20.0f, 0.03f, 0.5f);
        drawBoxAt(-10.0f, 0.165f, jz, 0.5f, 0.03f, 20.5f);
        drawBoxAt(10.0f, 0.165f, jz, 0.5f, 0.03f, 20.5f);
    }

    // festive bunting strung across the junctions
    {
        static const float BUNT[5][3] = {
            { 0.80f, 0.20f, 0.18f }, { 0.95f, 0.80f, 0.15f }, { 0.20f, 0.55f, 0.85f },
            { 0.25f, 0.60f, 0.30f }, { 0.75f, 0.35f, 0.75f }
        };
        static const float BPOS[3][2] = { { 0, 0 }, { 0, 1200 }, { -1000, 1200 } };
        for (int b = 0; b < 3; b++)
        {
            float bx = BPOS[b][0], bz = BPOS[b][1];
            for (int i = 0; i < 14; i++)
            {
                float t = (float)i / 13.0f;
                float px = bx - 13.0f + t * 26.0f;
                float py = 8.0f - 1.8f * sinf(t * PI_F);
                setColor(BUNT[i % 5][0], BUNT[i % 5][1], BUNT[i % 5][2]);
                glPushMatrix(); glTranslatef(px, py, bz);
                drawBox(1.1f, 1.2f, 0.06f);
                glPopMatrix();
            }
        }
    }

    // utility poles with wires along the main roads
    for (int i = 0; i < gRoadCount; i++)
    {
        if (ROADS[i].width >= 22.0f) utilityPoles(ROADS[i].pts, ROADS[i].n, ROADS[i].width * 0.5f);
    }

    // street lights and billboards
    streetLights(ROADS[0].pts, ROADS[0].n, ROADS[0].width * 0.5f);
    streetLights(ROADS[2].pts, ROADS[2].n, ROADS[2].width * 0.5f);
    billboardsScene();

    // bridges
    keaneBridge();
    plainBridge(-296.0f, 0.0f, 90.0f, 300.0f, true);      // Subid Bazar Road crossing
    plainBridge(1500.0f, 1200.0f, 90.0f, 300.0f, true);   // Ambarkhana Road crossing
    plainBridge(-2100.0f, -700.0f, 0.0f, 300.0f, false);  // Kazir Bazar Road crossing

    // a small roundabout with a monument on Ambarkhana Road
    glPushMatrix();
    glTranslatef(900.0f, 0.0f, 1200.0f);
    setColor(0.30f, 0.30f, 0.32f);
    glPushMatrix(); glTranslatef(0, 0.06f, 0); drawDiskXZ(30.0f, 24); glPopMatrix();
    setColor(0.45f, 0.58f, 0.34f);
    glPushMatrix(); glTranslatef(0, 0.14f, 0); drawDiskXZ(20.0f, 20); glPopMatrix();
    setColor(0.80f, 0.78f, 0.74f);
    glPushMatrix(); glTranslatef(0, 0.15f, 0); drawCylinderRaw(2.6f, 2.0f, 5.0f, 10, true); glPopMatrix();
    setColor(0.55f, 0.35f, 0.20f);
    glPushMatrix(); glTranslatef(0, 5.1f, 0); drawSphere(1.4f, 8, 5); glPopMatrix();
    glPopMatrix();
    glEndList();
}

// ---------------------------------------------------------------------
// railway + station + level crossings
// ---------------------------------------------------------------------
static void buildRail()
{
    glNewList(L_RAIL = glGenLists(1), GL_COMPILE);
    // ballast
    float x0 = -5400.0f, x1 = 5400.0f;
    setColor(0.46f, 0.42f, 0.37f);
    groundQuad(x0, 2894.0f, x1, 2912.0f, 0.05f);
    // sleepers (flat quads)
    setColor(0.33f, 0.28f, 0.23f);
    for (float x = x0; x < x1; x += 2.6f)
    {
        bool nearCross = false;
        for (int c = 0; c < 2; c++) if (fabsf(x - gCrossX[c]) < 10.0f) nearCross = true;
        if (nearCross) continue;
        groundQuad(x, 2898.7f, x + 0.9f, 2901.3f, 0.16f);
        groundQuad(x, 2904.7f, x + 0.9f, 2907.3f, 0.16f);
    }
    // rails
    setColor(0.72f, 0.72f, 0.74f);
    float railZ[4] = { 2898.7f, 2901.3f, 2904.7f, 2907.3f };
    for (int i = 0; i < 4; i++)
    {
        drawBoxAt((x0 + x1) * 0.5f, 0.18f, railZ[i] + 0.08f, x1 - x0, 0.16f, 0.16f);
    }

    // station: platform, shelter, building with a readable sign
    textureBind(TEX_CONCRETE_PANEL);
    setColor(0.70f, 0.68f, 0.64f);
    drawBoxAt(700.0f, 0.55f, 2887.0f, 240.0f, 1.1f, 8.0f);
    // shelter roof on posts
    setColor(0.55f, 0.53f, 0.50f);
    for (int i = 0; i < 8; i++)
    {
        drawBoxAt(620.0f + i * 23.0f, 2.2f, 2884.0f, 0.3f, 3.0f, 0.3f);
        drawBoxAt(620.0f + i * 23.0f, 2.2f, 2890.0f, 0.3f, 3.0f, 0.3f);
    }
    setColor(0.48f, 0.50f, 0.54f);
    drawBoxAt(700.0f, 3.9f, 2887.0f, 200.0f, 0.22f, 9.5f);
    // building (north of the track, facing the city)
    setColor(0.88f, 0.84f, 0.72f);
    drawBoxAt(700.0f, 4.5f, 2850.0f, 70.0f, 9.0f, 20.0f);
    setColor(0.62f, 0.60f, 0.56f);
    drawBoxAt(700.0f, 9.3f, 2850.0f, 71.5f, 0.7f, 21.5f);
    setColor(0.35f, 0.45f, 0.55f);
    drawBoxAt(700.0f, 4.6f, 2840.2f, 60.0f, 2.2f, 0.3f);
    addTall(700.0f, 2850.0f, 70.0f, 20.0f, 9.5f);
    // "SYLHET" sign
    glPushMatrix();
    glTranslatef(700.0f, 10.2f, 2841.0f);
    glRotatef(180, 0, 1, 0);
    setColor(0.10f, 0.35f, 0.20f);
    glLineWidth(3.0f);
    strokeTextCentered(GLUT_STROKE_ROMAN, "SYLHET", 0.10f);
    glLineWidth(1.0f);
    glPopMatrix();

    // level crossing props (booms are drawn animated by the traffic module)
    for (int c = 0; c < 2; c++)
    {
        float cx = gCrossX[c];
        // X warning signs, facing the road
        for (int s = 0; s < 2; s++)
        {
            float sz = 2900.0f + (s == 0 ? -12.0f : 12.0f);
            float sx = cx + (s == 0 ? 7.0f : -7.0f);
            setColor(0.75f, 0.73f, 0.70f);
            drawBoxAt(sx, 1.4f, sz, 0.3f, 2.8f, 0.3f);
            glPushMatrix();
            glTranslatef(sx, 2.9f, sz + (s == 0 ? 0.25f : -0.25f));
            if (s == 1) glRotatef(180, 0, 1, 0);
            setColor(0.92f, 0.92f, 0.90f);
            drawBox(2.2f, 2.2f, 0.12f);
            setColor(0.75f, 0.15f, 0.12f);
            glPushMatrix(); glRotatef(45, 0, 0, 1);  drawBox(3.0f, 0.30f, 0.16f); glPopMatrix();
            glPushMatrix(); glRotatef(-45, 0, 0, 1); drawBox(3.0f, 0.30f, 0.16f); glPopMatrix();
            glPopMatrix();
        }
        // boom pivot posts
        setColor(0.80f, 0.78f, 0.74f);
        drawBoxAt(cx - 13.6f, 0.6f, 2886.0f, 0.4f, 1.2f, 0.4f);
        drawBoxAt(cx + 13.6f, 0.6f, 2914.0f, 0.4f, 1.2f, 0.4f);
    }

    // platform canopy, footbridge, signals, water tower and a tea stall
    setColor(0.55f, 0.57f, 0.60f);
    for (int i = 0; i < 10; i++)
        drawBoxAt(600.0f + i * 24.0f, 1.9f, 2887.0f, 0.22f, 3.8f, 0.22f);
    setColor(0.48f, 0.50f, 0.54f);
    drawBoxAt(700.0f, 3.9f, 2887.0f, 240.0f, 0.22f, 8.0f);
    setColor(0.60f, 0.60f, 0.58f);
    drawBoxAt(300.0f, 7.0f, 2900.0f, 4.0f, 0.4f, 26.0f);
    for (int i = 0; i < 2; i++)
    {
        float z = (i == 0) ? 2887.0f : 2913.0f;
        setColor(0.58f, 0.58f, 0.56f);
        drawBoxAt(300.0f, 3.5f, z, 3.0f, 7.0f, 3.0f);
    }
    setColor(0.50f, 0.52f, 0.55f);
    drawBoxAt(300.0f, 8.1f, 2900.0f, 4.2f, 0.2f, 26.0f);
    for (int i = 0; i < 3; i++)
    {
        float x = -1200.0f + i * 1500.0f;
        setColor(0.55f, 0.55f, 0.58f);
        drawBoxAt(x, 2.2f, 2880.0f, 0.18f, 4.4f, 0.18f);
        setColor(0.20f, 0.22f, 0.24f);
        drawBoxAt(x, 4.9f, 2880.0f, 0.5f, 1.1f, 0.5f);
        setColor(0.85f, 0.15f, 0.12f);
        glPushMatrix(); glTranslatef(x, 5.05f, 2880.3f); glRotatef(90.0f, 1, 0, 0);
        drawDiskXZ(0.18f, 8);
        glPopMatrix();
    }
    setColor(0.70f, 0.70f, 0.68f);
    glPushMatrix(); glTranslatef(1150.0f, 9.0f, 2930.0f);
    drawCylinderRaw(3.2f, 3.2f, 5.0f, 12, true);
    glPopMatrix();
    setColor(0.55f, 0.55f, 0.58f);
    for (int i = 0; i < 4; i++)
    {
        float a = (float)i / 4.0f * 2.0f * PI_F + 0.78f;
        drawBoxAt(1150.0f + cosf(a) * 2.6f, 4.5f, 2930.0f + sinf(a) * 2.6f, 0.3f, 9.0f, 0.3f);
    }
    setColor(0.62f, 0.55f, 0.45f);
    drawBoxAt(980.0f, 1.3f, 2880.0f, 3.4f, 2.6f, 3.0f);
    setColor(0.80f, 0.75f, 0.20f);
    glPushMatrix(); glTranslatef(980.0f, 2.7f, 2880.0f); glRotatef(16.0f, 0, 0, 1);
    drawBox(4.0f, 0.12f, 3.4f);
    glPopMatrix();
    glEndList();
}

// ---------------------------------------------------------------------
// buildings and shops
// ---------------------------------------------------------------------
static bool siteFree(float x, float z)
{
    if (worldRiverDist(x, z) < 165.0f) return false;
    if (roadDist(x, z) < 14.0f) return false;
    for (int i = 0; i < sPondCount; i++)
    {
        float dx = x - sPondX[i], dz = z - sPondZ[i];
        float rad = sPondR[i] + 12.0f;
        if (dx * dx + dz * dz < rad * rad) return false;
    }
    if (fabsf(x - 1900) < 330 && z > 1300 && z < 2320) return false;   // SIU campus + flying strip
    if (fabsf(x - 1600) < 150 && z > 1800 && z < 2320) return false;   // SIU flying strip
    if (fabsf(x + 1750) < 320 && fabsf(z + 1150) < 300) return false;
    if ((x - 90) * (x - 90) + (z + 100) * (z + 100) < 150 * 150) return false;
    if (fabsf(z - 2900) < 150) return false;
    if (fabsf(x) < 130 && fabsf(z - 105) < 150) return false;
    if (fabsf(x + 296) < 180 && fabsf(z) < 70) return false;
    if (fabsf(x - 1500) < 180 && fabsf(z - 1200) < 70) return false;
    if (fabsf(x + 2100) < 70 && fabsf(z + 700) < 180) return false;
    if ((x - 900) * (x - 900) + (z - 1200) * (z - 1200) < 45 * 45) return false;
    if (fabsf(x - 1500) < 150 && fabsf(z + 1600) < 120) return false;      // stadium
    if ((x - 650) * (x - 650) + (z + 600) * (z + 600) < 100 * 100) return false;  // telecom tower
    if (fabsf(x + 1650) < 125 && fabsf(z - 2300) < 95) return false;       // bus terminal
    if (fabsf(x - 2300) < 115 && fabsf(z - 500) < 95) return false;        // shahi eidgah
    if (runwayDist(x, z) < 300.0f) return false;
    V3 ap = worldApronCenter();
    if ((x - ap.x) * (x - ap.x) + (z - ap.z) * (z - ap.z) < 340 * 340) return false;
    return true;
}

// ---------------------------------------------------------------------
// material palettes
// Each building picks a photograph for its walls and one for its roof from a
// set that depends on how central it is: painted plaster, brick and concrete
// downtown, timber, bamboo and tin at the edges of the city. The hash is taken
// from the building position rather than the RNG so the city layout - and every
// screenshot and recording made from it - stays exactly the same.
// ---------------------------------------------------------------------
// roofs: concrete, rusty tin, blue tin, grey tin, green tin, fresh tin
static const float ROOFC[6][3] = {
    { 0.64f, 0.62f, 0.58f }, { 0.58f, 0.34f, 0.24f }, { 0.38f, 0.45f, 0.53f },
    { 0.69f, 0.69f, 0.71f }, { 0.33f, 0.47f, 0.35f }, { 0.76f, 0.75f, 0.72f }
};

static TexId pickWallTex(float r, float roll)
{
    // mostly smooth plaster and concrete, with brick and tile as the minority:
    // a Sylhet street is painted plaster far more often than bare brick, and a
    // low-contrast photo keeps the building's own colour readable
    // smooth plaster, paint and concrete carry the colour; brick and board only
    // appear where they belong (one or two per list). A strongly patterned photo
    // on every wall reads as siding, which is not what a city of plastered
    // concrete looks like.
    // light, low-contrast surfaces: the palette colour multiplies the
    // photograph, so a pale plaster keeps the building's colour while a dark
    // concrete would just make the city grey
    static const TexId CENTRE[] = {
        TEX_WALL_PAINTED, TEX_WALL_BEIGE, TEX_PLASTER_WHITE, TEX_PLASTER_GREY,
        TEX_PLASTER_ROUGH, TEX_FACADE_TILE, TEX_TILE_WALL, TEX_CONCRETE_CRACKED,
        TEX_WALL_PAINTED, TEX_PLASTER_WHITE
    };
    static const TexId MID[] = {
        TEX_PLASTER, TEX_WALL_BEIGE, TEX_PLASTER_GREY, TEX_PLASTER_ROUGH, TEX_WALL_PAINTED,
        TEX_CONCRETE_PAINTED, TEX_FACADE_TILE2, TEX_TILE_WALL, TEX_PLASTER_WHITE, TEX_CONCRETE_CRACKED
    };
    static const TexId FARM[] = {
        TEX_PLANK_BROWN, TEX_BAMBOO, TEX_BAMBOO2, TEX_PLANK_OLD, TEX_TIN,
        TEX_METAL_SIDING, TEX_BRICK_FACTORY
    };
    int i;
    if (r < 1400.0f) { i = (int)(roll * 10.0f) % 10; return CENTRE[i]; }
    if (r < 2800.0f) { i = (int)(roll * 10.0f) % 10; return MID[i]; }
    i = (int)(roll * 7.0f) % 7; return FARM[i];
}

static TexId pickRoofTex(float r, float roll)
{
    static const TexId CENTRE[] = {
        TEX_CONCRETE, TEX_PAVEMENT, TEX_ROOF_GREY, TEX_ROOF_09, TEX_ROOF_TILES_GREY, TEX_METAL_SIDING
    };
    static const TexId MID[] = {
        TEX_ROOF_CLAY, TEX_ROOF_CERAMIC, TEX_ROOF_TILES2, TEX_TIN, TEX_ROOF_RUST, TEX_ROOF_TILES_GREY
    };
    static const TexId FARM[] = {
        TEX_ROOF_THATCH, TEX_TIN2, TEX_ROOF_RUST, TEX_METAL_SIDING, TEX_ROOF_CLAY
    };
    int i;
    if (r < 1400.0f) { i = (int)(roll * 6.0f) % 6; return CENTRE[i]; }
    if (r < 2800.0f) { i = (int)(roll * 6.0f) % 6; return MID[i]; }
    i = (int)(roll * 5.0f) % 5; return FARM[i];
}

static void genBuilding(float cx, float cz)
{
    sBuildingTotal++;
    float w = 15 + frand01(sSeed) * 26;
    float d = 15 + frand01(sSeed) * 26;
    float r = sqrtf(cx * cx + cz * cz);
    int floors;
    if (r < 1400) { floors = 2 + (int)(frand01(sSeed) * 7.0f); if (frand01(sSeed) < 0.13f) floors = 10 + (int)(frand01(sSeed) * 9.0f); }
    else if (r < 2800) floors = 1 + (int)(frand01(sSeed) * 5.0f);
    else floors = 1 + (int)(frand01(sSeed) * 2.0f);
    float floorH = 3.2f;
    float h = floors * floorH;

    // a few buildings are still under construction: bare concrete frames
    if (r < 2800 && frand01(sSeed) < 0.05f)
    {
        float hc = (floors > 1 ? floors : 2) * floorH;
        setColor(0.68f, 0.66f, 0.62f);
        for (int ci = 0; ci < 4; ci++)
        {
            float px2 = cx + ((ci & 1) ? 1.0f : -1.0f) * (w * 0.5f - 1.0f);
            float pz2 = cz + ((ci & 2) ? 1.0f : -1.0f) * (d * 0.5f - 1.0f);
            drawBoxAt(px2, hc * 0.5f, pz2, 0.8f, hc, 0.8f);
        }
        for (float fy = 3.2f; fy < hc; fy += 6.4f)
        {
            setColor(0.72f, 0.70f, 0.66f);
            drawBoxAt(cx, fy, cz, w, 0.35f, d);
        }
        setColor(0.74f, 0.72f, 0.68f);
        drawBoxAt(cx, hc + 0.2f, cz, w, 0.4f, d);
        addAll(cx, cz, w, d, hc);
        if (hc >= 20.0f) addTall(cx, cz, w, d, hc);
        return;
    }

    // plaster tones. The first nine are muted, as real apartment blocks are;
    // the last six are the painted houses Bangladesh is full of, so the streets
    // keep some colour next to the concrete.
    static const float WALLC[15][3] = {
        { 0.88f, 0.86f, 0.82f }, { 0.86f, 0.82f, 0.72f }, { 0.78f, 0.78f, 0.76f },
        { 0.74f, 0.80f, 0.84f }, { 0.76f, 0.82f, 0.74f }, { 0.86f, 0.80f, 0.78f },
        { 0.80f, 0.74f, 0.62f }, { 0.63f, 0.45f, 0.37f }, { 0.87f, 0.84f, 0.70f },
        { 0.58f, 0.72f, 0.68f }, { 0.80f, 0.66f, 0.52f }, { 0.70f, 0.54f, 0.60f },
        { 0.52f, 0.60f, 0.72f }, { 0.74f, 0.58f, 0.44f }, { 0.60f, 0.70f, 0.52f }
    };

    static const float GLASSC[3][3] = {
        { 0.30f, 0.36f, 0.44f }, { 0.35f, 0.42f, 0.50f }, { 0.25f, 0.30f, 0.38f }
    };

    int c = (int)(frand01(sSeed) * 15);
    if (c > 14) c = 14;
    float cr = WALLC[c][0], cg = WALLC[c][1], cb = WALLC[c][2];
    int rf = (int)(frand01(sSeed) * 6);
    int gi = (int)(frand01(sSeed) * 3);

    // material for this building, chosen from its position rather than the RNG so
    // the city layout (and every screenshot of it) stays exactly the same
    unsigned matHash = (unsigned)(fabsf(cx) * 37.0f + fabsf(cz) * 101.0f + 13.0f);
    TexId roofTex = pickRoofTex(r, (float)((matHash / 7u) % 89u) / 89.0f);
    bool wallBound = (textureId(sCurWallTex) != 0);

    // remember the footprint for the far-distance LOD list
    if (sCurChunk >= 0 && sSimpleCount[sCurChunk] < MAX_SIMPLE)
    {
        SimpleBuilding& sb = sSimpleB[sCurChunk][sSimpleCount[sCurChunk]++];
        sb.x = cx; sb.z = cz; sb.w = w; sb.d = d; sb.h = h;
        sb.r = cr; sb.g = cg; sb.b = cb;
        sb.rf = rf;
    }

    // body in its own wall photograph, tinted with the building's palette colour
    // so the street keeps its variety instead of turning uniformly grey
    if (wallBound) setColor(cr, cg, cb);   // the photograph supplies the shading, the palette the colour
    else setColor(cr, cg, cb);
    drawBoxAt(cx, h * 0.5f, cz, w, h, d);
    setColor(cr * 0.74f, cg * 0.74f, cb * 0.74f);
    drawBoxAt(cx, 0.75f, cz, w + 0.34f, 1.5f, d + 0.34f);

    // per-floor slabs and glass window bands. These are drawn without the wall
    // photograph on purpose: a tinted photograph on every band turns a block of
    // flats into a stack of stripes, while flat glass keeps it readable.
    if (r < 2800 && floors > 2)
    {
        int step = floors > 9 ? 2 : 1;
        float gw = cr * 0.46f + GLASSC[gi][0] * 0.55f;
        float gh = cg * 0.46f + GLASSC[gi][1] * 0.55f;
        float gb = cb * 0.46f + GLASSC[gi][2] * 0.55f;
        if (wallBound) glDisable(GL_TEXTURE_2D);
        for (int f = step; f < floors; f += step)
        {
            setColor(cr * 0.99f, cg * 0.99f, cb * 0.99f);
            drawBoxAt(cx, f * floorH, cz, w + 0.26f, 0.28f, d + 0.26f);
            setColor(gw, gh, gb);
            drawBoxAt(cx, f * floorH + 1.00f, cz, w + 0.08f, 0.95f, d + 0.08f);
        }
        if (wallBound) glEnable(GL_TEXTURE_2D);
    }

    // balconies with railings on two faces
    if (floors > 2 && floors < 12 && frand01(sSeed) < 0.42f)
    {
        int step = floors > 9 ? 2 : 1;
        setColor(cr * 0.86f, cg * 0.86f, cb * 0.86f);
        for (int f = step; f < floors; f += step)
        {
            drawBoxAt(cx, f * floorH + 0.14f, cz + d * 0.5f + 0.55f, w * 0.82f, 0.16f, 1.1f);
            drawBoxAt(cx, f * floorH + 0.14f, cz - d * 0.5f - 0.55f, w * 0.82f, 0.16f, 1.1f);
            setColor(cr * 0.7f, cg * 0.7f, cb * 0.7f);
            drawBoxAt(cx, f * floorH + 0.56f, cz + d * 0.5f + 1.04f, w * 0.82f, 0.70f, 0.08f);
            drawBoxAt(cx, f * floorH + 0.56f, cz - d * 0.5f - 1.04f, w * 0.82f, 0.70f, 0.08f);
            setColor(cr * 0.86f, cg * 0.86f, cb * 0.86f);
        }
    }

    // parapet and the roof slab in a realistic roof colour (seen from above!)
    setColor(cr * 0.90f, cg * 0.90f, cb * 0.90f);
    drawBoxAt(cx, h + 0.28f, cz, w + 0.5f, 0.56f, d + 0.5f);
    // The roof slab itself is drawn by the per-chunk roof pass (see
    // buildBuildings): one texture change per chunk instead of one per building,
    // which is worth about 20% of the frame rate over the city.
    (void)roofTex;

    // solar panels on some rooftops
    if (h > 9 && frand01(sSeed) < 0.18f)
    {
        int panels = 2 + (int)(frand01(sSeed) * 3.0f);
        setColor(0.16f, 0.20f, 0.42f);
        for (int i = 0; i < panels; i++)
        {
            float ox = (frand01(sSeed) - 0.5f) * (w - 5.0f);
            float oz = (frand01(sSeed) - 0.5f) * (d - 5.0f);
            glPushMatrix();
            glTranslatef(cx + ox, h + 1.05f, cz + oz);
            glRotatef(14.0f, 1, 0, 0);
            drawBox(3.2f, 0.10f, 1.9f);
            glPopMatrix();
        }
    }

    // rooftop clutter: water tanks, stair boxes, antennas, penthouses
    float pr = frand01(sSeed);
    if (h > 9 && pr < 0.42f)
    {
        static const float TANK[4][3] = {
            { 0.22f, 0.42f, 0.62f }, { 0.20f, 0.45f, 0.28f },
            { 0.60f, 0.60f, 0.58f }, { 0.76f, 0.76f, 0.73f }
        };
        int t = (int)(frand01(sSeed) * 4);
        setColor(TANK[t][0], TANK[t][1], TANK[t][2]);
        glPushMatrix();
        glTranslatef(cx + w * 0.18f, h + 0.62f, cz - d * 0.15f);
        drawCylinderRaw(1.1f, 1.1f, 1.9f, 8, true);
        glPopMatrix();
    }
    else if (h > 9 && pr < 0.62f)
    {
        setColor(cr * 0.90f, cg * 0.90f, cb * 0.90f);
        drawBoxAt(cx - w * 0.2f, h + 1.35f, cz + d * 0.2f, 3.4f, 2.2f, 3.4f);
    }
    if (h > 14 && frand01(sSeed) < 0.5f)
    {
        setColor(0.35f, 0.35f, 0.36f);
        glPushMatrix(); glTranslatef(cx + w * 0.22f, h + 0.6f, cz + d * 0.22f);
        drawCylinderRaw(0.06f, 0.03f, 4.5f, 4, false);
        glPopMatrix();
    }
    if (frand01(sSeed) < 0.22f)
    {
        setColor(cr * 0.94f, cg * 0.94f, cb * 0.94f);
        drawBoxAt(cx + w * 0.1f, h + 1.90f, cz - d * 0.1f, w * 0.4f, 3.6f, d * 0.4f);
    }

    addAll(cx, cz, w, d, h);
    if (h >= 20.0f) addTall(cx, cz, w, d, h);
}

static void buildShops()
{
    // two famous shopping streets: Zindabazar Road and Ambarkhana Road
    static const float AWN[6][3] = {
        { 0.78f, 0.20f, 0.16f }, { 0.16f, 0.32f, 0.68f }, { 0.14f, 0.50f, 0.26f },
        { 0.82f, 0.48f, 0.10f }, { 0.55f, 0.16f, 0.55f }, { 0.85f, 0.80f, 0.18f }
    };
    int idx = 0;
    // Zindabazar: shops face the road on both sides (road along Z, width 26)
    for (float z = -700.0f; z <= 1700.0f; z += 13.0f)
    {
        if (fabsf(z) < 32.0f || fabsf(z - 105.0f) < 46.0f || fabsf(z - 1200.0f) < 32.0f) continue;
        for (int side = 0; side < 2; side++)
        {
            float sx = side == 0 ? 19.5f : -19.5f;
            float facing = side == 0 ? 90.0f : -90.0f;   // front looks towards the road
            float h = 4.0f + frand01(sSeed) * 4.0f;
            static const float SP[5][3] = { { 0.85f, 0.83f, 0.78f }, { 0.78f, 0.72f, 0.64f }, { 0.87f, 0.80f, 0.72f }, { 0.80f, 0.78f, 0.74f }, { 0.76f, 0.68f, 0.60f } };
            int sc = (int)(frand01(sSeed) * 5);
            setColor(SP[sc][0], SP[sc][1], SP[sc][2]);
            glPushMatrix();
            glTranslatef(sx, 0, z + 6.0f);
            glRotatef(facing, 0, 1, 0);
            drawBox(9.0f, h, 12.0f);
            glPopMatrix();
            // awning
            int ac = (int)(frand01(sSeed) * 6);
            setColor(AWN[ac][0], AWN[ac][1], AWN[ac][2]);
            glPushMatrix();
            glTranslatef(sx, 0, z + 6.0f);
            glRotatef(facing, 0, 1, 0);
            glTranslatef(0, h - 0.15f, -6.0f);
            glRotatef(24.0f, 1, 0, 0);
            drawBox(8.6f, 0.12f, 2.2f);
            glPopMatrix();
            addAll(sx, z + 6.0f, 12.0f, 9.0f, h);
            idx++;
        }
    }
    // Ambarkhana Road (road along X)
    for (float x = -600.0f; x <= 800.0f; x += 13.0f)
    {
        if (fabsf(x) < 32.0f || fabsf(x - 900.0f) < 40.0f || fabsf(x + 1000.0f) < 32.0f) continue;
        for (int side = 0; side < 2; side++)
        {
            float sz = side == 0 ? 1200.0f + 17.5f : 1200.0f - 17.5f;
            float facing = side == 0 ? 0.0f : 180.0f;   // front looks towards the road
            float h = 4.0f + frand01(sSeed) * 4.0f;
            static const float SP[5][3] = { { 0.85f, 0.83f, 0.78f }, { 0.78f, 0.72f, 0.64f }, { 0.87f, 0.80f, 0.72f }, { 0.80f, 0.78f, 0.74f }, { 0.76f, 0.68f, 0.60f } };
            int sc = (int)(frand01(sSeed) * 5);
            setColor(SP[sc][0], SP[sc][1], SP[sc][2]);
            glPushMatrix();
            glTranslatef(x + 6.0f, 0, sz);
            glRotatef(facing, 0, 1, 0);
            drawBox(9.0f, h, 12.0f);
            glPopMatrix();
            int ac = (int)(frand01(sSeed) * 6);
            setColor(AWN[ac][0], AWN[ac][1], AWN[ac][2]);
            glPushMatrix();
            glTranslatef(x + 6.0f, 0, sz);
            glRotatef(facing, 0, 1, 0);
            glTranslatef(0, h - 0.15f, -6.0f);
            glRotatef(24.0f, 1, 0, 0);
            drawBox(8.6f, 0.12f, 2.2f);
            glPopMatrix();
            addAll(x + 6.0f, sz, 9.0f, 12.0f, h);
            idx++;
        }
    }

    // roadside tea stalls and small tin shops
    for (int i = 0; i < 20; i++)
    {
        float z = -650.0f + frand01(sSeed) * 2200.0f;
        if (fabsf(z) < 60.0f || fabsf(z - 105.0f) < 65.0f || fabsf(z - 1200.0f) < 60.0f) continue;
        int side = (frand01(sSeed) < 0.5f) ? 1 : -1;
        float sx = side * 24.5f;
        float hgt = 2.6f + frand01(sSeed) * 1.2f;
        setColor(0.62f, 0.55f, 0.45f);
        drawBoxAt(sx, hgt * 0.5f, z, 4.0f, hgt, 3.4f);
        int ac = (int)(frand01(sSeed) * 6);
        setColor(AWN[ac][0], AWN[ac][1], AWN[ac][2]);
        glPushMatrix();
        glTranslatef(sx, hgt + 0.18f, z);
        glRotatef(side * 18.0f, 0, 0, 1);
        drawBox(4.8f, 0.12f, 3.9f);
        glPopMatrix();
        addAll(sx, z, 4.0f, 3.4f, hgt);
    }

    // four simple bus shelters on the main roads
    static const float SHEL[4][3] = {
        { 20.0f, 420.0f, 0.0f }, { -20.0f, -520.0f, 0.0f },
        { -420.0f, 1218.0f, 90.0f }, { 560.0f, 1182.0f, 90.0f }
    };
    for (int i = 0; i < 4; i++)
    {
        float bx2 = SHEL[i][0], bz2 = SHEL[i][1], yaw2 = SHEL[i][2];
        glPushMatrix();
        glTranslatef(bx2, 0, bz2);
        glRotatef(yaw2, 0, 1, 0);
        setColor(0.66f, 0.68f, 0.72f);
        for (int p = 0; p < 4; p++)
        {
            float px2 = (p & 1) ? 3.0f : -3.0f;
            float pz2 = (p & 2) ? 1.2f : -1.2f;
            drawBoxAt(px2, 1.3f, pz2, 0.16f, 2.6f, 0.16f);
        }
        setColor(0.42f, 0.52f, 0.62f);
        drawBoxAt(0, 2.72f, 0, 7.4f, 0.16f, 3.2f);
        setColor(0.55f, 0.50f, 0.45f);
        drawBoxAt(0, 0.55f, 1.15f, 7.0f, 0.10f, 0.45f);
        glPopMatrix();
    }
}

// the city is compiled in square chunks so that only the ones near the camera
// are submitted each frame; that is what keeps the frame rate usable
// The city is deliberately compact and dense: 8 x 8 chunks of 550 m, centred on
// the old town. Everything a visitor is shown - the bridge, the clock tower, the
// Mazar, the campuses, the stadium and the airport - sits inside or just outside
// this square, so a demonstration never crosses empty outskirts to reach the next
// place, and the buildings that are drawn all sit close to the landmarks.
static const int CITY_CHUNKS = 7;
static const float CITY_CELL = 500.0f;
static const float CITY_ORIGIN = -0.5f * CITY_CHUNKS * CITY_CELL;   // -1750
static GLuint sChunkList[CITY_CHUNKS * CITY_CHUNKS];

static void buildBuildings()
{
    for (int ci = 0; ci < CITY_CHUNKS; ci++)
    {
        for (int cj = 0; cj < CITY_CHUNKS; cj++)
        {
            int ch = ci * CITY_CHUNKS + cj;
            sChunkList[ch] = glGenLists(1);
            sSimpleCount[ch] = 0;
            sCurChunk = ch;
            glNewList(sChunkList[ch], GL_COMPILE);
            // one wall photograph per chunk: neighbourhoods differ from each
            // other without a texture change for every building
            {
                float cxp = CITY_ORIGIN + (ci + 0.5f) * CITY_CELL;
                float czp = CITY_ORIGIN + (cj + 0.5f) * CITY_CELL;
                float cr = sqrtf(cxp * cxp + czp * czp);
                unsigned hsh = (unsigned)(fabsf(cxp) * 31.0f + fabsf(czp) * 73.0f + 7.0f);
                sCurWallTex = pickWallTex(cr, (float)(hsh % 101u) / 101.0f);
                if (!textureBind(sCurWallTex)) textureBind(sWallTex);
            }
            float x0 = CITY_ORIGIN + ci * CITY_CELL;
            float z0 = CITY_ORIGIN + cj * CITY_CELL;
            for (float gx = x0; gx < x0 + CITY_CELL; gx += 95.0f)
            {
                for (float gz = z0; gz < z0 + CITY_CELL; gz += 95.0f)
                {
                    float x = gx + (frand01(sSeed) * 2 - 1) * 46.0f;
                    float z = gz + (frand01(sSeed) * 2 - 1) * 46.0f;
                    float r = sqrtf(x * x + z * z);
                    float density;
                    if (r < 1000) density = 0.99f;
                    else if (r < 1700) density = 0.94f;
                    else if (r < 2500) density = 0.82f;   // the outer edge of the grid
                    else continue;
                    float roll = frand01(sSeed);
                    if (roll > density) continue;
                    if (!siteFree(x, z)) continue;
                    genBuilding(x, z);
                }
            }
            textureUnbind();
            glEndList();
            sCurChunk = -1;

            // The roof pass: one roof slab per building, all sharing the
            // chunk's roof material, so a 600 m neighbourhood has, say, clay
            // tiles while the next one is corrugated tin. Colour still varies
            // building by building, because that costs nothing.
            sChunkRoof[ch] = glGenLists(1);
            glNewList(sChunkRoof[ch], GL_COMPILE);
            {
                float cxp = CITY_ORIGIN + (ci + 0.5f) * CITY_CELL;
                float czp = CITY_ORIGIN + (cj + 0.5f) * CITY_CELL;
                float cr2 = sqrtf(cxp * cxp + czp * czp);
                unsigned hsh2 = (unsigned)(fabsf(cxp) * 53.0f + fabsf(czp) * 19.0f + 3.0f);
                TexId chRoof = pickRoofTex(cr2, (float)(hsh2 % 83u) / 83.0f);
                bool rbound = textureBind(chRoof);
                for (int b = 0; b < sSimpleCount[ch]; b++)
                {
                    const SimpleBuilding& sb = sSimpleB[ch][b];
                    int rf2 = sb.rf & 5;
                    float rr3 = ROOFC[rf2][0] * 1.18f, rg3 = ROOFC[rf2][1] * 1.18f, rb3 = ROOFC[rf2][2] * 1.18f;
                    if (rr3 > 1.0f) rr3 = 1.0f;
                    if (rg3 > 1.0f) rg3 = 1.0f;
                    if (rb3 > 1.0f) rb3 = 1.0f;
                    setColor(rr3, rg3, rb3);
                    drawBoxAt(sb.x, sb.h + 0.62f, sb.z, sb.w + 0.14f, 0.18f, sb.d + 0.14f);
                }
                if (rbound) textureBind(sCurWallTex);
            }
            glEndList();

            // the far-distance version: one textured box per building
            sChunkSimple[ch] = glGenLists(1);
            glNewList(sChunkSimple[ch], GL_COMPILE);
            textureBind(sWallTex);
            for (int b = 0; b < sSimpleCount[ch]; b++)
            {
                const SimpleBuilding& sb = sSimpleB[ch][b];
                setColor(sb.r, sb.g, sb.b);
                drawBoxAt(sb.x, sb.h * 0.5f, sb.z, sb.w, sb.h, sb.d);
            }
            textureUnbind();
            glEndList();
        }
    }

    // shops and roadside buildings stay in one list that is always drawn
    glNewList(L_BUILDINGS = glGenLists(1), GL_COMPILE);
    textureBind(sWallTex);
    buildShops();
    // villages in the countryside, away from the city
    for (int v = 0; v < 10; v++)
    {
        float ang = frand01(sSeed) * 6.2831853f;
        float dist = 4400.0f + frand01(sSeed) * 1300.0f;
        float vx = cosf(ang) * dist, vz = sinf(ang) * dist;
        if (fabsf(vx) > 7200 || fabsf(vz) > 7200) continue;
        if (worldRiverDist(vx, vz) < 250) continue;
        bool onHill = false;
        for (int hh = 0; hh < gHillCount; hh++)
        {
            float dx = vx - HILLS[hh][0], dz = vz - HILLS[hh][1];
            float rad = HILLS[hh][2] + 80.0f;
            if (dx * dx + dz * dz < rad * rad) onHill = true;
        }
        if (onHill) continue;
        int houses = 6 + (int)(frand01(sSeed) * 7);
        for (int h = 0; h < houses; h++)
        {
            float hx = vx + (frand01(sSeed) * 2 - 1) * 140.0f;
            float hz = vz + (frand01(sSeed) * 2 - 1) * 140.0f;
            if (roadDist(hx, hz) < 10) continue;
            if (worldRiverDist(hx, hz) < 130) continue;
            static const float HP[3][3] = { { 0.72f, 0.62f, 0.50f }, { 0.78f, 0.70f, 0.58f }, { 0.68f, 0.52f, 0.44f } };
            int c = (int)(frand01(sSeed) * 3);
            setColor(HP[c][0], HP[c][1], HP[c][2]);
            drawBoxAtRot(hx, 1.9f, hz, 9.0f, 3.8f, 7.0f, frand01(sSeed) * 180);
            addAll(hx, hz, 9.0f, 7.0f, 3.8f);
        }
        // and a few trees around the village
        for (int t = 0; t < 9; t++)
        {
            float tx = vx + (frand01(sSeed) * 2 - 1) * 220.0f;
            float tz = vz + (frand01(sSeed) * 2 - 1) * 220.0f;
            if (roadDist(tx, tz) < 8) continue;
            if (worldRiverDist(tx, tz) < 125) continue;
            drawTree(tx, tz, (int)(frand01(sSeed) * 3));
        }
    }
    textureUnbind();
    glEndList();
}

// ---------------------------------------------------------------------
// trees
// ---------------------------------------------------------------------
static void buildTrees()
{
    glNewList(L_TREES = glGenLists(1), GL_COMPILE);
    // forced trees at the landmarks (Sylhet is very green)
    for (int i = 0; i < 14; i++)
    {
        float a = ((float)i / 14.0f) * 6.2831853f;
        float x = -1750.0f + cosf(a) * (200.0f + (i % 3) * 30.0f);
        float z = -1150.0f + sinf(a) * (170.0f + (i % 3) * 30.0f);
        drawTree(x, z, ((i % 4) == 0) ? 1 : 2);
    }
    for (int i = 0; i < 16; i++)
    {
        float a = ((float)i / 16.0f) * 6.2831853f;
        float x = 1900.0f + cosf(a) * (250.0f + (i % 3) * 40.0f);
        float z = 1600.0f + sinf(a) * (210.0f + (i % 3) * 40.0f);
        drawTree(x, z, ((i % 3) == 0) ? 1 : 2);
    }
    for (int i = 0; i < 6; i++)
    {
        drawTree(-40.0f - i * 18.0f, 260.0f + i * 12.0f, 2);
        drawTree(60.0f + i * 16.0f, -140.0f - i * 10.0f, 2);
    }

    int placed = 0;
    for (int i = 0; i < 3800 && placed < 1400; i++)
    {
        float x = (frand01(sSeed) * 2 - 1) * 7400.0f;
        float z = (frand01(sSeed) * 2 - 1) * 7400.0f;
        float rd = worldRiverDist(x, z);
        float rdd = roadDist(x, z);
        if (rd < 115.0f) continue;
        if (runwayDist(x, z) < 160.0f) continue;
        if (worldHillHeightAt(x, z) > 1.0f) continue;
        bool inPond = false;
        for (int p = 0; p < sPondCount; p++)
        {
            float dx = x - sPondX[p], dz = z - sPondZ[p];
            float rad = sPondR[p] + 8.0f;
            if (dx * dx + dz * dz < rad * rad) inPond = true;
        }
        if (inPond) continue;
        V3 ap = worldApronCenter();
        if ((x - ap.x) * (x - ap.x) + (z - ap.z) * (z - ap.z) < 400 * 400) continue;
        if (fabsf(z - 2900.0f) < 45.0f) continue;
        if (insideAnyBuilding(x, z)) continue;
        float p = 0;
        if (rd < 400.0f) p = 0.75f;                       // dense riverside groves
        else if (rdd < 45.0f && rdd > 12.0f) p = 0.42f;   // roadside trees
        else p = 0.055f;
        if (frand01(sSeed) > p) continue;
        drawTree(x, z, (int)(frand01(sSeed) * 3));
        placed++;
    }
    glEndList();
}

// ---------------------------------------------------------------------
// the green hills around the city
// ---------------------------------------------------------------------
static void buildHills()
{
    glNewList(L_HILLS = glGenLists(1), GL_COMPILE);
    for (int i = 0; i < gHillCount; i++)
    {
        float x = HILLS[i][0], z = HILLS[i][1], r = HILLS[i][2], h = HILLS[i][3];
        float tone = frand01(sSeed) * 0.05f;
        setColor(0.33f + tone, 0.49f + tone, 0.27f + tone * 0.5f);
        glPushMatrix();
        glTranslatef(x, 0, z);
        glScalef(1.0f, h / r, 1.0f);
        drawDome(r, 24, 6);
        glPopMatrix();
        // wooded patches on the slopes
        for (int k = 0; k < 8; k++)
        {
            float ang = frand01(sSeed) * 6.2831853f;
            float rr = frand01(sSeed) * r * 0.66f;
            float px = x + cosf(ang) * rr, pz = z + sinf(ang) * rr;
            float frac = sqrtf(fmaxf(0.0f, 1.0f - (rr * rr) / (r * r)));
            setColor(0.25f, 0.40f, 0.21f);
            glPushMatrix();
            glTranslatef(px, h * frac * 0.90f, pz);
            glScalef(1.0f, 0.40f, 1.0f);
            drawDome(r * 0.17f + 60.0f, 8, 3);
            glPopMatrix();
        }
    }
    glEndList();
}

// ---------------------------------------------------------------------
// Osmani International Airport
// ---------------------------------------------------------------------
static void drawParkedPlane(float x, float z, float yawDeg, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(r, g, b);
    drawBoxAt(0, 1.35f, 0, 1.7f, 1.7f, 8.5f);              // fuselage
    drawBoxAt(0, 2.2f, -0.6f, 10.5f, 0.32f, 1.7f);         // wing
    setColor(r * 0.9f, g * 0.9f, b * 0.9f);
    drawBoxAt(0, 2.6f, 3.6f, 0.3f, 1.6f, 1.4f);            // fin
    drawBoxAt(0, 2.35f, 3.8f, 3.4f, 0.22f, 0.9f);          // tailplane
    setColor(0.2f, 0.2f, 0.2f);
    drawBoxAt(-1.1f, 0.4f, -1.2f, 0.25f, 0.8f, 0.25f);
    drawBoxAt(1.1f, 0.4f, -1.2f, 0.25f, 0.8f, 0.25f);
    drawBoxAt(-1.1f, 0.4f, 2.9f, 0.25f, 0.8f, 0.25f);
    glPopMatrix();
}

static void buildAirport()
{
    glNewList(L_AIRPORT = glGenLists(1), GL_COMPILE);
    textureBind(sWallTex);
    V3 c = worldAirportCenter();
    V3 d = worldRunwayDir();
    V3 r = worldRunwayRight();
    float yawDeg = RWY_HEADING_DEG;

    // runway
    glPushMatrix();
    glTranslatef(c.x, 0, c.z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.22f, 0.22f, 0.25f);
    drawBox(45.0f, 0.12f, RWY_HALF * 2.0f);
    glPopMatrix();

    // threshold bars
    setColor(0.93f, 0.93f, 0.90f);
    for (int end = 0; end < 2; end++)
    {
        float s = (end == 0 ? -1.0f : 1.0f) * (RWY_HALF - 30.0f);
        for (int i = 0; i < 8; i++)
        {
            float lat = (i < 4) ? -(4.7f + i * 4.4f) : (4.7f + (i - 4) * 4.4f);
            V3 p = worldRunwayPoint(s, lat);
            drawBoxAtRot(p.x, 0.15f, p.z, 1.8f, 0.13f, 20.0f, yawDeg);
        }
    }
    // centreline dashes
    for (float s = -1380.0f; s <= 1380.0f; s += 40.0f)
    {
        V3 p = worldRunwayPoint(s, 0);
        drawBoxAtRot(p.x, 0.15f, p.z, 0.9f, 0.13f, 18.0f, yawDeg);
    }
    // edge lights
    setColor(0.90f, 0.88f, 0.70f);
    for (float s = -1420.0f; s <= 1420.0f; s += 60.0f)
    {
        for (int side = 0; side < 2; side++)
        {
            V3 p = worldRunwayPoint(s, side == 0 ? -25.0f : 25.0f);
            drawBoxAt(p.x, 0.2f, p.z, 0.5f, 0.4f, 0.5f);
        }
    }

    // painted runway numbers (readable on final approach)
    glLineWidth(7.0f);
    setColor(0.94f, 0.94f, 0.92f);
    float th1 = 50.0f;
    {
        V3 p = worldRunwayPoint(-1230.0f, 0);
        glPushMatrix();
        glTranslatef(p.x, 0.22f, p.z);
        glRotatef(-th1, 0, 1, 0);
        glRotatef(-90.0f, 1, 0, 0);
        strokeTextCentered(GLUT_STROKE_ROMAN, "05", 0.06f);
        glPopMatrix();
    }
    {
        V3 p = worldRunwayPoint(1230.0f, 0);
        glPushMatrix();
        glTranslatef(p.x, 0.22f, p.z);
        glRotatef(-(th1 + 180.0f), 0, 1, 0);
        glRotatef(-90.0f, 1, 0, 0);
        strokeTextCentered(GLUT_STROKE_ROMAN, "23", 0.06f);
        glPopMatrix();
    }
    glLineWidth(1.0f);

    // taxiway
    V3 t0 = worldRunwayPoint(-160.0f, 26.0f);
    V3 t1 = worldRunwayPoint(-160.0f, 300.0f);
    V3 t2 = worldRunwayPoint(-60.0f, 360.0f);
    setColor(0.25f, 0.25f, 0.28f);
    for (int i = 0; i < 2; i++)
    {
        V3 a = (i == 0) ? t0 : t1;
        V3 b = (i == 0) ? t1 : t2;
        V3 dd = norm(b - a);
        V3 nn = v3(-dd.z, 0, dd.x) * 9.0f;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(a.x + nn.x, 0.10f, a.z + nn.z); glVertex3f(b.x + nn.x, 0.10f, b.z + nn.z);
        glVertex3f(b.x - nn.x, 0.10f, b.z - nn.z); glVertex3f(a.x - nn.x, 0.10f, a.z - nn.z);
        glEnd();
    }

    // blue taxiway edge lights
    setColor(0.25f, 0.40f, 0.85f);
    for (int i = 0; i < 2; i++)
    {
        V3 a = (i == 0) ? t0 : t1;
        V3 b = (i == 0) ? t1 : t2;
        V3 dd = norm(b - a);
        V3 nn = v3(-dd.z, 0, dd.x);
        int nseg = (int)(len(b - a) / 26.0f);
        for (int k = 0; k <= nseg; k++)
        {
            V3 p = a + (b - a) * ((float)k / (float)(nseg + 1));
            drawBoxAt(p.x + nn.x * 10.5f, 0.25f, p.z + nn.z * 10.5f, 0.35f, 0.5f, 0.35f);
            drawBoxAt(p.x - nn.x * 10.5f, 0.25f, p.z - nn.z * 10.5f, 0.35f, 0.5f, 0.35f);
        }
    }

    // approach lighting systems before both runway ends
    setColor(0.95f, 0.93f, 0.78f);
    for (int end = 0; end < 2; end++)
    {
        float sgn = (end == 0) ? -1.0f : 1.0f;
        for (int k = 1; k <= 7; k++)
        {
            float s = sgn * (RWY_HALF + 40.0f + k * 52.0f);
            for (int l = -1; l <= 1; l++)
            {
                V3 p = worldRunwayPoint(s, l * 6.0f);
                drawBoxAt(p.x, 0.5f, p.z, 0.5f, 1.0f, 0.5f);
            }
        }
    }

    // apron
    V3 ap = worldApronCenter();
    glPushMatrix();
    glTranslatef(ap.x, 0, ap.z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.26f, 0.26f, 0.29f);
    drawBox(190.0f, 0.12f, 340.0f);
    // yellow park lines
    setColor(0.85f, 0.75f, 0.20f);
    for (int i = -3; i <= 3; i++)
    {
        glPushMatrix();
        glTranslatef(i * 27.0f, 0.16f, 20.0f);
        drawBox(0.5f, 0.05f, 16.0f);
        glPopMatrix();
    }
    glPopMatrix();

    // terminal
    V3 term = worldRunwayPoint(-20.0f, 520.0f);
    glPushMatrix();
    glTranslatef(term.x, 0, term.z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.86f, 0.86f, 0.84f);
    drawBoxAt(0, 4.5f, 0, 38.0f, 9.0f, 120.0f);
    setColor(0.25f, 0.38f, 0.50f);
    drawBoxAt(0, 5.2f, 0, 38.6f, 2.4f, 120.6f);
    setColor(0.62f, 0.62f, 0.60f);
    drawBoxAt(0, 9.4f, 0, 39.5f, 0.8f, 121.5f);
    // glazed curtain wall with mullions on both long faces
    for (int side = 0; side < 2; side++)
    {
        float fx = (side == 0) ? -19.1f : 19.1f;
        setColor(0.30f, 0.44f, 0.58f);
        drawBoxAt(fx, 4.7f, 0, 0.16f, 5.4f, 117.0f);
        setColor(0.82f, 0.83f, 0.84f);
        for (int m = 0; m <= 20; m++)
            drawBoxAt(fx, 4.7f, -59.0f + m * 5.9f, 0.26f, 5.6f, 0.24f);
        drawBoxAt(fx, 7.55f, 0, 0.22f, 0.20f, 117.0f);
        drawBoxAt(fx, 2.10f, 0, 0.22f, 0.20f, 117.0f);
    }
    // rooftop plant
    setColor(0.70f, 0.71f, 0.72f);
    for (int u = 0; u < 5; u++)
        drawBoxAt(-6.0f + (u % 2) * 12.0f, 10.3f, -46.0f + u * 23.0f, 7.0f, 1.4f, 5.0f);
    glPopMatrix();
    addTall(term.x, term.z, 120.0f, 38.0f, 10.0f);

    // control tower
    V3 twr = worldRunwayPoint(-230.0f, 470.0f);
    setColor(0.88f, 0.88f, 0.86f);
    glPushMatrix(); glTranslatef(twr.x, 0, twr.z);
    drawCylinderRaw(3.4f, 3.0f, 20.0f, 10, false);
    glPopMatrix();
    setColor(0.25f, 0.35f, 0.45f);
    glPushMatrix(); glTranslatef(twr.x, 20.0f, twr.z);
    drawCylinderRaw(6.0f, 6.0f, 4.5f, 12, false);
    glPopMatrix();
    setColor(0.60f, 0.60f, 0.58f);
    glPushMatrix(); glTranslatef(twr.x, 24.6f, twr.z);
    drawDiskXZ(6.8f, 12);
    glPopMatrix();
    // antenna
    setColor(0.85f, 0.85f, 0.85f);
    drawBoxAt(twr.x, 27.8f, twr.z, 0.2f, 6.0f, 0.2f);
    setColor(0.85f, 0.20f, 0.16f);
    glPushMatrix(); glTranslatef(twr.x, 31.0f, twr.z); drawSphere(0.5f, 6, 4); glPopMatrix();
    addTall(twr.x, twr.z, 12.0f, 12.0f, 25.0f);

    // hangars
    for (int i = 0; i < 2; i++)
    {
        V3 hg = worldRunwayPoint(-620.0f - i * 170.0f, 500.0f);
        glPushMatrix();
        glTranslatef(hg.x, 0, hg.z);
        glRotatef(-yawDeg, 0, 1, 0);
        setColor(0.62f, 0.64f, 0.66f);
        drawBoxAt(0, 4.0f, 0, 26.0f, 8.0f, 40.0f);
        setColor(0.50f, 0.52f, 0.55f);
        glPushMatrix();
        glTranslatef(0, 8.0f, 0);
        drawPrism(26.0f, 4.0f, 40.0f, 0.55f);
        glPopMatrix();
        setColor(0.35f, 0.38f, 0.42f);
        drawBoxAt(0, 3.4f, -19.9f, 22.0f, 6.0f, 0.35f);
        glPopMatrix();
        addTall(hg.x, hg.z, 26.0f, 40.0f, 9.0f);
    }

    // two parked planes + a shuttle on the apron
    V3 pk1 = worldRunwayPoint(60.0f, 320.0f);
    V3 pk2 = worldRunwayPoint(130.0f, 350.0f);
    drawParkedPlane(pk1.x, pk1.z, yawDeg + 25.0f, 0.90f, 0.90f, 0.92f);
    drawParkedPlane(pk2.x, pk2.z, yawDeg - 170.0f, 0.80f, 0.83f, 0.88f);
    drawParkedPlane(ap.x + r.x * 120.0f + d.x * -280.0f, ap.z + r.z * 120.0f + d.z * -280.0f, yawDeg + 95.0f, 0.88f, 0.84f, 0.78f);

    // (windsocks are drawn animated in worldDrawWindsocks)

    // ---- terminal sign, entrance canopy, jet bridges, fence and apron lights ----
    {
        V3 term = worldRunwayPoint(-20.0f, 520.0f);
        glPushMatrix();
        glTranslatef(term.x, 0, term.z);
        glRotatef(-yawDeg, 0, 1, 0);
        // sign board on the terminal front (facing the apron)
        setColor(0.05f, 0.34f, 0.20f);
        drawBoxAt(-19.6f, 12.5f, 0.0f, 0.5f, 4.8f, 74.0f);
        setColor(0.96f, 0.96f, 0.92f);
        glPushMatrix();
        glTranslatef(-19.95f, 12.5f, 0.0f);
        glRotatef(-90.0f, 0, 1, 0);
        glLineWidth(2.5f);
        strokeTextCentered(GLUT_STROKE_ROMAN, "OSMANI INTERNATIONAL AIRPORT", 0.030f);
        glLineWidth(1.0f);
        glPopMatrix();
        // entrance canopy
        setColor(0.80f, 0.80f, 0.78f);
        for (int i = 0; i < 4; i++)
            drawBoxAt(-27.0f, 3.0f, -30.0f + i * 20.0f, 0.6f, 6.0f, 0.6f);
        setColor(0.55f, 0.60f, 0.66f);
        drawBoxAt(-32.0f, 6.4f, 0.0f, 13.0f, 0.5f, 88.0f);
        // jet bridges reaching out to the stands
        for (int i = 0; i < 3; i++)
        {
            float z = -60.0f + i * 60.0f;
            setColor(0.78f, 0.78f, 0.76f);
            drawBoxAt(-80.0f, 4.2f, z, 120.0f, 3.6f, 4.2f);
            setColor(0.70f, 0.70f, 0.68f);
            for (int l = 0; l < 4; l++)
            {
                float lx = -30.0f - l * 30.0f;
                drawBoxAt(lx, 1.2f, z, 1.4f, 2.4f, 3.4f);
            }
            setColor(0.35f, 0.42f, 0.50f);
            drawBoxAt(-138.0f, 4.6f, z, 3.0f, 2.0f, 3.6f);
        }
        // apron floodlight masts
        for (int i = 0; i < 4; i++)
        {
            float mx2 = (i & 1) ? 470.0f : 275.0f;
            float mz2 = (i & 2) ? 190.0f : -150.0f;
            setColor(0.58f, 0.58f, 0.60f);
            drawBoxAt(mx2, 9.0f, mz2, 0.45f, 18.0f, 0.45f);
            setColor(0.92f, 0.90f, 0.78f);
            drawBoxAt(mx2, 18.3f, mz2, 3.2f, 0.6f, 1.2f);
        }
        // perimeter fence with a gate and a guard house
        setColor(0.55f, 0.57f, 0.60f);
        for (int i = 0; i <= 100; i++)
        {
            float z = -1500.0f + i * 30.0f;
            drawBoxAt(800.0f, 1.2f, z, 0.18f, 2.4f, 0.18f);
            drawBoxAt(-150.0f, 1.2f, z, 0.18f, 2.4f, 0.18f);
        }
        setColor(0.60f, 0.62f, 0.64f);
        drawBoxAt(800.0f, 2.2f, 0.0f, 0.12f, 0.12f, 3000.0f);
        drawBoxAt(800.0f, 1.3f, 0.0f, 0.12f, 0.12f, 3000.0f);
        drawBoxAt(-150.0f, 2.2f, 0.0f, 0.12f, 0.12f, 3000.0f);
        setColor(0.82f, 0.80f, 0.74f);
        drawBoxAt(800.0f, 2.0f, -900.0f, 2.2f, 4.0f, 2.2f);
        drawBoxAt(800.0f, 2.0f, -860.0f, 2.2f, 4.0f, 2.2f);
        setColor(0.70f, 0.72f, 0.74f);
        drawBoxAt(806.0f, 2.4f, -930.0f, 9.0f, 4.8f, 8.0f);
        setColor(0.30f, 0.45f, 0.55f);
        drawBoxAt(810.6f, 2.6f, -930.0f, 0.3f, 1.6f, 6.0f);
        // fuel tanks and a bowser
        for (int i = 0; i < 2; i++)
        {
            float fx = -300.0f - i * 22.0f;
            setColor(0.78f, 0.78f, 0.75f);
            glPushMatrix(); glTranslatef(fx, 0, 560.0f);
            drawCylinderRaw(6.5f, 6.5f, 10.0f, 14, true);
            glPopMatrix();
            setColor(0.60f, 0.62f, 0.64f);
            glPushMatrix(); glTranslatef(fx, 10.0f, 560.0f);
            drawDome(6.5f, 12, 3);
            glPopMatrix();
        }
        setColor(0.30f, 0.42f, 0.55f);
        drawBoxAt(-200.0f, 1.3f, 545.0f, 2.6f, 2.2f, 5.5f);
        setColor(0.20f, 0.20f, 0.22f);
        drawBoxAt(-200.0f, 0.35f, 543.0f, 2.8f, 0.7f, 0.6f);
        drawBoxAt(-200.0f, 0.35f, 547.0f, 2.8f, 0.7f, 0.6f);
        // ground service vans near the terminal
        for (int i = 0; i < 3; i++)
        {
            float vx = 430.0f, vz = -60.0f + i * 22.0f;
            setColor(0.90f, 0.90f, 0.88f);
            drawBoxAt(vx, 1.3f, vz, 2.2f, 2.0f, 5.0f);
            setColor(0.25f, 0.32f, 0.42f);
            drawBoxAt(vx, 1.9f, vz + 1.6f, 2.0f, 0.9f, 1.6f);
            setColor(0.18f, 0.18f, 0.20f);
            drawBoxAt(vx - 1.05f, 0.4f, vz - 1.6f, 0.4f, 0.8f, 0.8f);
            drawBoxAt(vx + 1.05f, 0.4f, vz - 1.6f, 0.4f, 0.8f, 0.8f);
            drawBoxAt(vx - 1.05f, 0.4f, vz + 1.6f, 0.4f, 0.8f, 0.8f);
            drawBoxAt(vx + 1.05f, 0.4f, vz + 1.6f, 0.4f, 0.8f, 0.8f);
        }
        glPopMatrix();
    }
    // two more aircraft parked on the apron
    {
        V3 p3 = worldRunwayPoint(205.0f, 385.0f);
        V3 p4 = worldRunwayPoint(-45.0f, 345.0f);
        drawParkedPlane(p3.x, p3.z, yawDeg + 12.0f, 0.92f, 0.90f, 0.86f);
        drawParkedPlane(p4.x, p4.z, yawDeg + 168.0f, 0.55f, 0.62f, 0.72f);
    }
    textureUnbind();
    glEndList();
}

// ---------------------------------------------------------------------
// Hazrat Shahjalal Mazar complex
// ---------------------------------------------------------------------
static void buildMazar()
{
    glNewList(L_MAZAR = glGenLists(1), GL_COMPILE);
    textureBind(sWallTex);
    float mx = -1750.0f, mz = -1150.0f;

    // perimeter wall
    setColor(0.90f, 0.88f, 0.82f);
    drawBoxAt(mx, 1.1f, mz - 180.0f, 400.0f, 2.2f, 1.4f);
    drawBoxAt(mx, 1.1f, mz + 180.0f, 400.0f, 2.2f, 1.4f);
    drawBoxAt(mx - 200.0f, 1.1f, mz, 1.4f, 2.2f, 360.0f);
    drawBoxAt(mx + 200.0f, 1.1f, mz, 1.4f, 2.2f, 360.0f);

    // paved courtyard inside the walls
    rotatedQuad(mx, mz, 0.12f, 90.0f, 80.0f, 0.0f, 0.62f, 0.60f, 0.55f);

    // main shrine: white block + green dome + golden finial
    setColor(0.93f, 0.91f, 0.86f);
    drawBoxAt(mx, 3.5f, mz, 30.0f, 7.0f, 30.0f);
    setColor(0.88f, 0.86f, 0.80f);
    glPushMatrix(); glTranslatef(mx, 7.0f, mz);
    drawCylinderRaw(10.5f, 10.5f, 2.5f, 14, false);
    glPopMatrix();
    setColor(0.28f, 0.55f, 0.38f);
    glPushMatrix(); glTranslatef(mx, 9.5f, mz);
    drawDome(11.0f, 14, 5);
    glPopMatrix();
    setColor(0.95f, 0.80f, 0.30f);
    drawBoxAt(mx, 22.3f, mz, 0.25f, 3.4f, 0.25f);
    glPushMatrix(); glTranslatef(mx, 24.2f, mz); drawSphere(0.6f, 6, 4); glPopMatrix();
    addTall(mx, mz, 30.0f, 30.0f, 20.0f);

    // marble dado, cornice and arched entrances on the shrine
    setColor(0.86f, 0.84f, 0.78f);
    drawBoxAt(mx, 1.15f, mz, 30.7f, 1.3f, 30.7f);            // dado band
    setColor(0.90f, 0.88f, 0.82f);
    drawBoxAt(mx, 6.75f, mz, 31.2f, 0.7f, 31.2f);            // cornice
    setColor(0.97f, 0.95f, 0.90f);
    drawBoxAt(mx, 3.2f, mz, 31.0f, 0.22f, 31.0f);            // inlay line
    drawBoxAt(mx, 4.6f, mz, 31.0f, 0.16f, 31.0f);
    for (int fi = 0; fi < 4; fi++)
    {
        glPushMatrix();
        glTranslatef(mx, 0, mz);
        glRotatef(fi * 90.0f, 0, 1, 0);
        // stone surround
        setColor(0.94f, 0.92f, 0.87f);
        drawBoxAt(0, 2.6f, 15.12f, 5.4f, 5.2f, 0.14f);
        glPushMatrix(); glTranslatef(0, 5.2f, 15.12f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(2.7f, 12); glPopMatrix();
        // dark recess with an arch
        setColor(0.20f, 0.24f, 0.27f);
        drawBoxAt(0, 2.4f, 15.20f, 3.4f, 4.6f, 0.12f);
        glPushMatrix(); glTranslatef(0, 4.7f, 15.20f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(1.7f, 12); glPopMatrix();
        // steps up to it
        setColor(0.80f, 0.78f, 0.72f);
        drawBoxAt(0, 0.15f, 16.6f, 5.0f, 0.3f, 1.4f);
        drawBoxAt(0, 0.42f, 16.2f, 4.2f, 0.25f, 1.0f);
        glPopMatrix();
    }

    // ablution pool and paved path in the courtyard
    {
        float px = mx - 32.0f, pz = mz + 30.0f;
        setColor(0.78f, 0.76f, 0.70f);
        drawBoxAt(px, 0.22f, pz, 20.0f, 0.44f, 11.0f);       // rim
        setColor(0.30f, 0.52f, 0.62f);
        drawBoxAt(px, 0.36f, pz, 17.6f, 0.16f, 8.6f);        // water
        setColor(0.86f, 0.84f, 0.78f);
        drawBoxAt(px, 0.85f, pz - 5.9f, 3.0f, 1.7f, 1.2f);   // tap block
        // path from the gate to the shrine steps
        setColor(0.74f, 0.72f, 0.67f);
        drawBoxAt(mx, 0.16f, mz + 28.0f, 6.0f, 0.12f, 26.0f);
        drawBoxAt(mx - 20.0f, 0.16f, mz + 22.0f, 40.0f, 0.12f, 5.0f);
    }

    // four corner turrets
    for (int i = 0; i < 4; i++)
    {
        float tx = mx + ((i & 1) ? 13.5f : -13.5f);
        float tz = mz + ((i & 2) ? 13.5f : -13.5f);
        setColor(0.92f, 0.90f, 0.85f);
        glPushMatrix(); glTranslatef(tx, 0, tz);
        drawCylinderRaw(1.4f, 1.3f, 11.0f, 8, false);
        glPopMatrix();
        setColor(0.30f, 0.56f, 0.40f);
        glPushMatrix(); glTranslatef(tx, 11.0f, tz);
        drawDome(1.7f, 8, 3);
        glPopMatrix();
    }

    // attached mosque with three domes and two minarets
    setColor(0.94f, 0.92f, 0.87f);
    drawBoxAt(mx + 70.0f, 2.75f, mz + 110.0f, 34.0f, 5.5f, 20.0f);
    glPushMatrix(); glTranslatef(mx + 58.0f, 5.5f, mz + 110.0f); setColor(0.30f, 0.56f, 0.40f); drawDome(5.0f, 10, 4); glPopMatrix();
    glPushMatrix(); glTranslatef(mx + 82.0f, 5.5f, mz + 110.0f); drawDome(4.0f, 10, 4); glPopMatrix();
    glPushMatrix(); glTranslatef(mx + 70.0f, 5.5f, mz + 110.0f); drawDome(4.3f, 10, 4); glPopMatrix();
    for (int i = 0; i < 2; i++)
    {
        float tx = mx + (i ? 92.0f : 48.0f);
        setColor(0.94f, 0.92f, 0.87f);
        glPushMatrix(); glTranslatef(tx, 0, mz + 110.0f);
        drawCylinderRaw(1.5f, 1.4f, 17.0f, 8, false);
        glPopMatrix();
        // balcony rings on the minaret
        setColor(0.90f, 0.88f, 0.83f);
        glPushMatrix(); glTranslatef(tx, 8.6f, mz + 110.0f);
        drawCylinderRaw(2.0f, 2.0f, 0.35f, 10, true);
        glPopMatrix();
        glPushMatrix(); glTranslatef(tx, 13.2f, mz + 110.0f);
        drawCylinderRaw(1.9f, 1.9f, 0.35f, 10, true);
        glPopMatrix();
        glPushMatrix(); glTranslatef(tx, 17.0f, mz + 110.0f);
        setColor(0.30f, 0.56f, 0.40f); drawDome(1.8f, 8, 3);
        glPopMatrix();
        setColor(0.95f, 0.80f, 0.30f);
        glPushMatrix(); glTranslatef(tx, 19.4f, mz + 110.0f); drawSphere(0.35f, 6, 4); glPopMatrix();
        addTall(tx, mz + 110.0f, 4.0f, 4.0f, 19.0f);
    }

    // entrance gate (north side, facing the city)
    setColor(0.90f, 0.88f, 0.83f);
    drawBoxAt(mx - 7.0f, 3.5f, mz - 180.0f, 2.4f, 7.0f, 2.4f);
    drawBoxAt(mx + 7.0f, 3.5f, mz - 180.0f, 2.4f, 7.0f, 2.4f);
    drawBoxAt(mx, 7.6f, mz - 180.0f, 17.0f, 1.6f, 2.6f);
    glPushMatrix(); glTranslatef(mx, 9.2f, mz - 180.0f);
    setColor(0.30f, 0.56f, 0.40f); drawDome(2.2f, 8, 3);
    glPopMatrix();

    // dighi (pond) west of the complex
    glPushMatrix();
    glTranslatef(-2020.0f, 0.05f, -1050.0f);
    glScalef(128.0f, 1.0f, 88.0f);
    setColor(0.70f, 0.68f, 0.64f);
    drawDiskXZ(1.0f, 24);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-2020.0f, 0.10f, -1050.0f);
    glScalef(118.0f, 1.0f, 78.0f);
    setColor(0.24f, 0.45f, 0.52f);
    drawDiskXZ(1.0f, 24);
    glPopMatrix();

    // walkway from the gate to the shrine
    rotatedQuad(mx, mz - 90.0f, 0.14f, 6.0f, 90.0f, 0.0f, 0.72f, 0.70f, 0.66f);
    textureUnbind();
    glEndList();
}

// ---------------------------------------------------------------------
// Sylhet International University campus
// ---------------------------------------------------------------------
static void buildSIU()
{
    glNewList(L_SIU = glGenLists(1), GL_COMPILE);
    textureBind(sWallTex);
    float sx = 1900.0f, sz = 1600.0f;

    // boundary wall with a gap for the gate
    setColor(0.92f, 0.90f, 0.85f);
    drawBoxAt(sx - 125.0f, 0.9f, sz - 215.0f, 160.0f, 1.8f, 1.2f);
    drawBoxAt(sx + 125.0f, 0.9f, sz - 215.0f, 160.0f, 1.8f, 1.2f);
    drawBoxAt(sx, 0.9f, sz + 215.0f, 460.0f, 1.8f, 1.2f);
    drawBoxAt(sx - 230.0f, 0.9f, sz, 1.2f, 1.8f, 430.0f);
    drawBoxAt(sx + 230.0f, 0.9f, sz, 1.2f, 1.8f, 430.0f);

    // gate with the university name
    setColor(0.88f, 0.86f, 0.80f);
    drawBoxAt(sx - 6.0f, 3.2f, sz - 215.0f, 2.2f, 6.4f, 2.2f);
    drawBoxAt(sx + 6.0f, 3.2f, sz - 215.0f, 2.2f, 6.4f, 2.2f);
    setColor(0.30f, 0.52f, 0.34f);
    drawBoxAt(sx, 7.2f, sz - 215.0f, 16.0f, 1.6f, 2.6f);
    // sign panel
    setColor(0.95f, 0.95f, 0.92f);
    drawBoxAt(sx, 9.4f, sz - 215.0f, 20.0f, 3.0f, 0.5f);
    // letters, north face (towards the road)
    setColor(0.05f, 0.28f, 0.16f);
    glLineWidth(2.5f);
    glPushMatrix();
    glTranslatef(sx, 9.9f, sz - 215.3f);
    glRotatef(180, 0, 1, 0);
    strokeTextCentered(GLUT_STROKE_ROMAN, "SYLHET INTERNATIONAL", 0.013f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(sx, 8.2f, sz - 215.3f);
    glRotatef(180, 0, 1, 0);
    strokeTextCentered(GLUT_STROKE_ROMAN, "UNIVERSITY", 0.013f);
    glPopMatrix();
    // letters, south face (towards the campus)
    glPushMatrix();
    glTranslatef(sx, 9.9f, sz - 214.7f);
    strokeTextCentered(GLUT_STROKE_ROMAN, "SYLHET INTERNATIONAL", 0.013f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(sx, 8.2f, sz - 214.7f);
    strokeTextCentered(GLUT_STROKE_ROMAN, "UNIVERSITY", 0.013f);
    glPopMatrix();
    glLineWidth(1.0f);

    // main academic building
    setColor(0.87f, 0.86f, 0.83f);
    drawBoxAt(sx - 30.0f, 6.75f, sz - 50.0f, 70.0f, 13.5f, 22.0f);
    setColor(0.28f, 0.38f, 0.48f);
    drawBoxAt(sx - 30.0f, 4.3f, sz - 50.0f, 70.6f, 1.4f, 22.6f);
    drawBoxAt(sx - 30.0f, 7.5f, sz - 50.0f, 70.6f, 1.4f, 22.6f);
    drawBoxAt(sx - 30.0f, 10.7f, sz - 50.0f, 70.6f, 1.4f, 22.6f);
    // vertical mullions between the window bands, on both long faces
    setColor(0.76f, 0.75f, 0.72f);
    for (int m = 0; m <= 17; m++)
    {
        float mx2 = sx - 30.0f - 35.0f + m * 4.0f;
        drawBoxAt(mx2, 7.1f, sz - 50.0f + 11.2f, 0.35f, 4.6f, 0.30f);
        drawBoxAt(mx2, 7.1f, sz - 50.0f - 11.2f, 0.35f, 4.6f, 0.30f);
    }
    setColor(0.70f, 0.70f, 0.68f);
    drawBoxAt(sx - 30.0f, 13.8f, sz - 50.0f, 71.5f, 0.7f, 23.0f);
    // roof parapet with a few AC units
    setColor(0.80f, 0.80f, 0.77f);
    drawBoxAt(sx - 30.0f, 14.25f, sz - 50.0f + 11.0f, 71.0f, 0.4f, 0.5f);
    drawBoxAt(sx - 30.0f, 14.25f, sz - 50.0f - 11.0f, 71.0f, 0.4f, 0.5f);
    drawBoxAt(sx - 30.0f - 35.0f, 14.25f, sz - 50.0f, 0.5f, 0.4f, 22.0f);
    drawBoxAt(sx - 30.0f + 35.0f, 14.25f, sz - 50.0f, 0.5f, 0.4f, 22.0f);
    setColor(0.66f, 0.67f, 0.68f);
    for (int u = 0; u < 4; u++)
        drawBoxAt(sx - 52.0f + u * 15.0f, 14.8f, sz - 56.0f, 4.5f, 1.0f, 3.5f);
    // entrance portico
    setColor(0.80f, 0.79f, 0.76f);
    drawBoxAt(sx - 30.0f, 3.6f, sz - 62.0f, 16.0f, 7.2f, 5.0f);
    addTall(sx - 30.0f, sz - 50.0f, 70.0f, 22.0f, 14.5f);

    // administration building
    setColor(0.84f, 0.80f, 0.72f);
    drawBoxAt(sx - 120.0f, 4.5f, sz + 55.0f, 30.0f, 9.0f, 16.0f);
    setColor(0.30f, 0.40f, 0.50f);
    drawBoxAt(sx - 120.0f, 4.8f, sz + 55.0f, 30.6f, 1.3f, 16.6f);
    addTall(sx - 120.0f, sz + 55.0f, 30.0f, 16.0f, 9.5f);

    // library
    setColor(0.86f, 0.84f, 0.80f);
    drawBoxAt(sx + 100.0f, 5.0f, sz + 55.0f, 26.0f, 10.0f, 18.0f);
    setColor(0.30f, 0.40f, 0.50f);
    drawBoxAt(sx + 100.0f, 5.6f, sz + 55.0f, 26.6f, 1.5f, 18.6f);
    drawBoxAt(sx + 100.0f, 8.6f, sz + 55.0f, 26.6f, 1.5f, 18.6f);
    // mullions and parapet
    setColor(0.78f, 0.77f, 0.74f);
    for (int m = 0; m <= 8; m++)
        drawBoxAt(sx + 100.0f - 13.0f + m * 3.25f, 7.1f, sz + 55.0f + 9.2f, 0.30f, 4.4f, 0.26f);
    setColor(0.82f, 0.81f, 0.78f);
    drawBoxAt(sx + 100.0f, 10.35f, sz + 55.0f, 26.6f, 0.5f, 18.8f);
    setColor(0.66f, 0.67f, 0.68f);
    drawBoxAt(sx + 96.0f, 10.8f, sz + 52.0f, 5.0f, 0.9f, 4.0f);
    addTall(sx + 100.0f, sz + 55.0f, 26.0f, 18.0f, 10.5f);

    // playground
    rotatedQuad(sx + 20.0f, sz + 140.0f, 0.06f, 60.0f, 38.0f, 0.0f, 0.44f, 0.62f, 0.36f);

    // flag of Bangladesh on the parade ground
    setColor(0.85f, 0.85f, 0.83f);
    glPushMatrix(); glTranslatef(sx - 30.0f, 0, sz + 40.0f);
    drawCylinderRaw(0.12f, 0.10f, 13.0f, 6, false);
    glPopMatrix();
    setColor(0.00f, 0.42f, 0.25f);
    drawBoxAt(sx - 28.2f, 10.6f, sz + 40.0f, 3.4f, 2.1f, 0.08f);
    glPushMatrix();
    glTranslatef(sx - 27.4f, 10.6f, sz + 40.06f);
    glRotatef(90.0f, 1, 0, 0);
    setColor(0.85f, 0.13f, 0.15f);
    drawDiskXZ(0.62f, 12);
    glPopMatrix();

    // internal road from the gate
    rotatedQuad(sx, sz - 150.0f, 0.12f, 5.0f, 50.0f, 0.0f, 0.40f, 0.40f, 0.42f);

    // hostel block, cafeteria, sports field, fountain and flag poles
    setColor(0.88f, 0.86f, 0.82f);
    drawBoxAt(1720.0f, 6.0f, 1760.0f, 40.0f, 12.0f, 16.0f);
    setColor(0.30f, 0.40f, 0.50f);
    drawBoxAt(1720.0f, 6.0f, 1760.0f, 40.6f, 1.2f, 16.6f);
    setColor(0.72f, 0.72f, 0.70f);
    drawBoxAt(1720.0f, 12.3f, 1760.0f, 41.0f, 0.6f, 17.0f);
    // hostel balconies: a slab and railing at each floor on the long face
    for (int fl = 0; fl < 3; fl++)
    {
        float fy = 2.6f + fl * 3.4f;
        setColor(0.84f, 0.82f, 0.78f);
        drawBoxAt(1720.0f, fy, 1760.0f + 8.6f, 39.0f, 0.22f, 1.6f);
        setColor(0.70f, 0.72f, 0.74f);
        drawBoxAt(1720.0f, fy + 0.62f, 1760.0f + 9.3f, 39.0f, 0.10f, 0.12f);
        for (int b = 0; b <= 12; b++)
            drawBoxAt(1720.0f - 19.0f + b * 3.1f, fy + 0.35f, 1760.0f + 9.3f, 0.10f, 0.55f, 0.10f);
    }
    // rooftop tank and stair head
    setColor(0.66f, 0.67f, 0.68f);
    drawBoxAt(1712.0f, 13.4f, 1758.0f, 3.0f, 1.6f, 3.0f);
    drawBoxAt(1730.0f, 13.1f, 1764.0f, 2.4f, 1.0f, 2.4f);
    addTall(1720.0f, 1760.0f, 40.0f, 16.0f, 12.5f);
    setColor(0.85f, 0.83f, 0.76f);
    drawBoxAt(2060.0f, 3.0f, 1440.0f, 24.0f, 6.0f, 14.0f);
    setColor(0.30f, 0.45f, 0.35f);
    drawBoxAt(2060.0f, 6.2f, 1440.0f, 25.0f, 0.5f, 15.0f);
    rotatedQuad(1900.0f, 1800.0f, 0.07f, 46.0f, 30.0f, 0.0f, 0.36f, 0.60f, 0.32f);
    for (int i = 0; i < 2; i++)
    {
        float gz = 1800.0f + (i == 0 ? -30.0f : 30.0f);
        setColor(0.92f, 0.92f, 0.90f);
        drawBoxAt(1900.0f - 3.6f, 1.3f, gz, 0.16f, 2.6f, 0.16f);
        drawBoxAt(1900.0f + 3.6f, 1.3f, gz, 0.16f, 2.6f, 0.16f);
        drawBoxAt(1900.0f, 2.5f, gz, 7.4f, 0.16f, 0.16f);
    }
    setColor(0.78f, 0.76f, 0.72f);
    glPushMatrix(); glTranslatef(1900.0f, 0.35f, 1420.0f);
    drawCylinderRaw(7.0f, 7.0f, 0.7f, 16, true);
    glPopMatrix();
    setColor(0.30f, 0.50f, 0.60f);
    glPushMatrix(); glTranslatef(1900.0f, 0.75f, 1420.0f);
    drawCylinderRaw(6.2f, 6.2f, 0.1f, 16, true);
    glPopMatrix();
    setColor(0.85f, 0.86f, 0.88f);
    glPushMatrix(); glTranslatef(1900.0f, 0.8f, 1420.0f);
    drawCylinderRaw(0.9f, 0.6f, 2.6f, 10, true);
    glPopMatrix();
    setColor(0.60f, 0.62f, 0.66f);
    for (int p = 0; p < 4; p++)
        drawBoxAt(1960.0f + ((p & 1) ? 3.0f : -3.0f), 1.3f, 1400.0f + ((p & 2) ? 1.2f : -1.2f), 0.16f, 2.6f, 0.16f);
    setColor(0.42f, 0.52f, 0.62f);
    drawBoxAt(1960.0f, 2.72f, 1400.0f, 7.4f, 0.16f, 3.2f);
    for (int i = 0; i < 3; i++)
    {
        float fx = 1840.0f + i * 60.0f;
        setColor(0.85f, 0.85f, 0.83f);
        drawBoxAt(fx, 6.5f, 1385.0f, 0.22f, 13.0f, 0.22f);
        drawBanglaFlag(fx, 11.0f, 1385.0f, 0.0f, 2.4f);
    }

    // -----------------------------------------------------------------
    // the campus flying strip: a mown grass runway just south of the wall,
    // so the game can start from SIU instead of the airport
    // -----------------------------------------------------------------
    const float stX = 1600.0f, stZ0 = 1850.0f, stZ1 = 2260.0f, stHalfW = 28.0f;
    const float stMid = (stZ0 + stZ1) * 0.5f, stHalfL = (stZ1 - stZ0) * 0.5f;

    // mown grass in alternating stripes, like a real airfield
    {
        const int bands = 18;
        float bandH = (stZ1 - stZ0) / (float)bands;
        for (int b = 0; b < bands; b++)
        {
            float zc = stZ0 + bandH * (b + 0.5f);
            float k = (b & 1) ? 1.10f : 0.92f;
            rotatedQuad(stX, zc, 0.085f, stHalfW, bandH * 0.5f, 0.0f,
                        0.20f * k, 0.30f * k, 0.16f * k);
        }
    }
    // slightly worn centre line of grass
    rotatedQuad(stX, stMid, 0.09f, stHalfW * 0.55f, stHalfL - 6.0f, 0.0f, 0.205f, 0.305f, 0.165f);

    // white edge lines
    setColor(0.94f, 0.94f, 0.90f);
    rotatedQuad(stX - stHalfW + 1.5f, stMid, 0.10f, 0.55f, stHalfL - 8.0f, 0.0f, 0.94f, 0.94f, 0.90f);
    rotatedQuad(stX + stHalfW - 1.5f, stMid, 0.10f, 0.55f, stHalfL - 8.0f, 0.0f, 0.94f, 0.94f, 0.90f);

    // threshold bars at both ends (like a real grass strip)
    for (int e = 0; e < 2; e++)
    {
        float zz = (e == 0) ? stZ0 + 6.0f : stZ1 - 6.0f;
        for (int k = -3; k <= 3; k++)
        {
            if (k == 0) continue;
            rotatedQuad(stX + k * 5.2f, zz, 0.10f, 1.1f, 3.2f, 0.0f, 0.94f, 0.94f, 0.90f);
        }
    }
    // centreline dashes
    for (float zz = stZ0 + 26.0f; zz < stZ1 - 20.0f; zz += 34.0f)
        rotatedQuad(stX, zz, 0.10f, 0.7f, 6.0f, 0.0f, 0.94f, 0.94f, 0.90f);

    // markers along the edges
    setColor(0.90f, 0.86f, 0.55f);
    for (float zz = stZ0 + 20.0f; zz < stZ1; zz += 45.0f)
    {
        drawBoxAt(stX - stHalfW - 3.0f, 0.5f, zz, 0.5f, 1.0f, 0.5f);
        drawBoxAt(stX + stHalfW + 3.0f, 0.5f, zz, 0.5f, 1.0f, 0.5f);
    }

    // (windsocks are drawn animated in worldDrawWindsocks)

    // little club shed and fuel drums at the north end
    setColor(0.86f, 0.84f, 0.79f);
    drawBoxAt(stX - stHalfW - 14.0f, 2.0f, stZ0 + 12.0f, 12.0f, 4.0f, 7.0f);
    setColor(0.62f, 0.40f, 0.30f);
    drawBoxAt(stX - stHalfW - 14.0f, 4.35f, stZ0 + 12.0f, 13.0f, 0.35f, 8.0f);
    addTall(stX - stHalfW - 14.0f, stZ0 + 12.0f, 12.0f, 7.0f, 4.5f);
    setColor(0.32f, 0.55f, 0.35f);
    drawBoxAt(stX - stHalfW - 14.0f, 1.2f, stZ0 + 5.0f, 6.4f, 2.4f, 0.25f);
    setColor(0.95f, 0.95f, 0.90f);
    glLineWidth(2.0f);
    glPushMatrix();
    glTranslatef(stX - stHalfW - 14.0f, 1.55f, stZ0 + 4.8f);
    strokeTextCentered(GLUT_STROKE_ROMAN, "SIU FLYING CLUB", 0.020f);
    glPopMatrix();
    glLineWidth(1.0f);
    setColor(0.80f, 0.22f, 0.18f);
    drawBoxAt(stX - stHalfW - 6.0f, 0.55f, stZ0 + 8.0f, 0.9f, 1.1f, 0.9f);
    drawBoxAt(stX - stHalfW - 4.6f, 0.55f, stZ0 + 8.0f, 0.9f, 1.1f, 0.9f);

    textureUnbind();
    glEndList();
}

// ---------------------------------------------------------------------
// Ali Amjad's Clock Tower
// ---------------------------------------------------------------------
static void buildClockTower()
{
    glNewList(L_CLOCK = glGenLists(1), GL_COMPILE);
    textureBind(sWallTex);
    float x = 90.0f, z = -100.0f;

    // base building, in the same red iron
    setColor(0.80f, 0.19f, 0.15f);
    drawBoxAt(x, 3.0f, z, 9.5f, 6.0f, 9.5f);
    setColor(0.55f, 0.20f, 0.18f);
    drawBoxAt(x, 6.1f, z, 10.2f, 0.5f, 10.2f);
    // door + windows
    setColor(0.45f, 0.30f, 0.20f);
    drawBoxAt(x, 1.6f, z + 4.8f, 1.8f, 3.2f, 0.2f);
    setColor(0.35f, 0.45f, 0.55f);
    drawBoxAt(x - 3.0f, 3.6f, z + 4.8f, 1.4f, 1.6f, 0.2f);
    drawBoxAt(x + 3.0f, 3.6f, z + 4.8f, 1.4f, 1.6f, 0.2f);

    // tower shaft
    setColor(0.82f, 0.20f, 0.16f);
    drawBoxAt(x, 10.0f, z, 5.2f, 8.0f, 5.2f);
    setColor(0.55f, 0.20f, 0.18f);
    drawBoxAt(x, 13.0f, z, 5.6f, 0.4f, 5.6f);
    // corner pilasters on the base and the shaft
    for (int c = 0; c < 4; c++)
    {
        float pxc = x + ((c & 1) ? 1.0f : -1.0f) * 4.30f;
        float pzc = z + ((c & 2) ? 1.0f : -1.0f) * 4.30f;
        setColor(0.88f, 0.85f, 0.78f);
        drawBoxAt(pxc, 3.0f, pzc, 0.9f, 6.0f, 0.9f);
        setColor(0.80f, 0.77f, 0.71f);
        drawBoxAt(pxc, 6.5f, pzc, 1.1f, 0.4f, 1.1f);
    }
    for (int c = 0; c < 4; c++)
    {
        float pxc = x + ((c & 1) ? 1.0f : -1.0f) * 2.32f;
        float pzc = z + ((c & 2) ? 1.0f : -1.0f) * 2.32f;
        setColor(0.91f, 0.88f, 0.82f);
        drawBoxAt(pxc, 10.0f, pzc, 0.55f, 8.0f, 0.55f);
    }

    // stepped entrance with a stone arch
    setColor(0.80f, 0.77f, 0.71f);
    drawBoxAt(x, 0.15f, z + 5.7f, 4.8f, 0.3f, 1.7f);
    drawBoxAt(x, 0.45f, z + 5.25f, 3.9f, 0.3f, 1.2f);
    drawBoxAt(x, 0.75f, z + 4.95f, 3.0f, 0.3f, 0.9f);
    setColor(0.90f, 0.87f, 0.80f);
    drawBoxAt(x, 2.4f, z + 4.76f, 2.7f, 4.4f, 0.10f);
    glPushMatrix();
    glTranslatef(x, 4.6f, z + 4.76f);
    glRotatef(90.0f, 1, 0, 0);
    drawDiskXZ(1.35f, 12);
    glPopMatrix();

    // a tall arched window on each face of the shaft
    for (int fi = 0; fi < 4; fi++)
    {
        glPushMatrix();
        glTranslatef(x, 0, z);
        glRotatef(fi * 90.0f, 0, 1, 0);
        setColor(0.90f, 0.87f, 0.80f);
        drawBoxAt(0, 10.3f, 2.62f, 2.0f, 4.4f, 0.10f);
        glPushMatrix(); glTranslatef(0, 12.5f, 2.62f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(1.0f, 12); glPopMatrix();
        setColor(0.22f, 0.26f, 0.30f);
        drawBoxAt(0, 10.25f, 2.69f, 1.4f, 3.6f, 0.10f);
        glPushMatrix(); glTranslatef(0, 12.05f, 2.69f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(0.7f, 12); glPopMatrix();
        glPopMatrix();
    }

    // dentil band under the cornice
    for (int s = 0; s < 4; s++)
    {
        glPushMatrix();
        glTranslatef(x, 0, z);
        glRotatef(s * 90.0f, 0, 1, 0);
        for (int d = -5; d <= 5; d++)
        {
            setColor(0.85f, 0.82f, 0.76f);
            drawBoxAt(d * 0.58f, 13.55f, 2.86f, 0.28f, 0.32f, 0.14f);
        }
        glPopMatrix();
    }

    // cornice
    setColor(0.88f, 0.85f, 0.79f);
    drawBoxAt(x, 14.3f, z, 6.4f, 0.7f, 6.4f);

    // balcony railing around the clock stage
    for (int s = 0; s < 4; s++)
    {
        glPushMatrix();
        glTranslatef(x, 0, z);
        glRotatef(s * 90.0f, 0, 1, 0);
        for (int b = -3; b <= 3; b++)
        {
            setColor(0.93f, 0.91f, 0.86f);
            drawBoxAt(b * 0.62f, 15.05f, 2.36f, 0.16f, 1.0f, 0.16f);
        }
        glPopMatrix();
    }
    setColor(0.90f, 0.88f, 0.82f);
    drawBoxAt(x, 15.62f, z, 5.2f, 0.16f, 5.2f);
    // clock stage
    setColor(0.84f, 0.21f, 0.17f);
    drawBoxAt(x, 16.2f, z, 4.4f, 3.4f, 4.4f);

    // four clock faces (hands frozen at 10:10)
    for (int fi = 0; fi < 4; fi++)
    {
        glPushMatrix();
        glTranslatef(x, 16.3f, z);
        glRotatef(fi * 90.0f, 0, 1, 0);
        glTranslatef(0, 0, 2.28f);
        // dial: dark rim behind a white face
        setColor(0.20f, 0.18f, 0.16f);
        glPushMatrix(); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(1.80f, 16); glPopMatrix();
        setColor(0.98f, 0.97f, 0.93f);
        glPushMatrix(); glTranslatef(0, 0, 0.04f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(1.55f, 16); glPopMatrix();
        // hour ticks
        for (int h = 0; h < 12; h++)
        {
            glPushMatrix();
            glRotatef(h * 30.0f, 0, 0, 1);
            glTranslatef(0, 1.28f, 0.06f);
            setColor(0.18f, 0.17f, 0.16f);
            drawBox(0.09f, 0.26f, 0.05f);
            glPopMatrix();
        }
        // hands
        glPushMatrix();
        glTranslatef(0, 0, 0.08f);
        glRotatef(60.0f, 0, 0, 1);
        drawBox(0.16f, 0.95f, 0.06f);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0, 0, 0.08f);
        glRotatef(-55.0f, 0, 0, 1);
        drawBox(0.13f, 1.35f, 0.06f);
        glPopMatrix();
        glPopMatrix();
    }

    // the pyramid roof and its gold finial (the photographs show a red hipped
    // roof with a weather vane, not a dome)
    setColor(0.78f, 0.18f, 0.14f);
    glPushMatrix(); glTranslatef(x, 17.9f, z);
    drawCone(3.4f, 4.6f, 4);
    glPopMatrix();
    setColor(0.90f, 0.76f, 0.34f);
    drawBoxAt(x, 22.9f, z, 0.16f, 1.6f, 0.16f);
    glPushMatrix(); glTranslatef(x, 23.9f, z); drawSphere(0.35f, 6, 4); glPopMatrix();
    setColor(0.30f, 0.56f, 0.40f);
    glPushMatrix(); glTranslatef(x, 17.9f, z);
    drawDome(2.7f, 12, 4);
    glPopMatrix();
    setColor(0.95f, 0.80f, 0.30f);
    drawBoxAt(x, 21.2f, z, 0.18f, 2.0f, 0.18f);
    glPushMatrix(); glTranslatef(x, 22.3f, z); drawSphere(0.4f, 6, 4); glPopMatrix();
    addTall(x, z, 10.0f, 10.0f, 22.5f);

    // paved square, gardens, lamps and flags around the tower
    setColor(0.70f, 0.68f, 0.63f);
    glPushMatrix(); glTranslatef(x, 0.06f, z); drawDiskXZ(42.0f, 24); glPopMatrix();
    setColor(0.26f, 0.45f, 0.24f);
    for (int i = 0; i < 10; i++)
    {
        float a = (float)i / 10.0f * 2.0f * PI_F;
        drawBoxAt(x + cosf(a) * 36.0f, 0.8f, z + sinf(a) * 36.0f, 6.0f, 1.6f, 2.4f);
    }
    for (int i = 0; i < 4; i++)
    {
        float a = (float)i / 4.0f * 2.0f * PI_F + 0.78f;
        float lx = x + cosf(a) * 26.0f, lz = z + sinf(a) * 26.0f;
        setColor(0.50f, 0.50f, 0.52f);
        drawBoxAt(lx, 2.5f, lz, 0.22f, 5.0f, 0.22f);
        setColor(0.95f, 0.92f, 0.75f);
        drawBoxAt(lx, 5.2f, lz, 0.8f, 0.4f, 0.8f);
    }
    setColor(0.55f, 0.55f, 0.58f);
    drawBoxAt(x - 26.0f, 4.5f, z - 26.0f, 0.18f, 9.0f, 0.18f);
    drawBanglaFlag(x - 26.0f, 9.0f, z - 26.0f, 0.0f, 2.6f);
    // the old colonial veranda building next door
    {
        float vx = x + 48.0f, vz = z - 8.0f;
        setColor(0.90f, 0.87f, 0.80f);
        drawBoxAt(vx, 3.0f, vz, 22.0f, 6.0f, 13.0f);
        setColor(0.55f, 0.20f, 0.18f);
        drawBoxAt(vx, 6.2f, vz, 23.0f, 0.5f, 14.0f);
        setColor(0.80f, 0.78f, 0.72f);
        for (int i = 0; i < 6; i++)
            drawBoxAt(vx - 9.0f + i * 3.6f, 1.5f, vz + 7.2f, 0.45f, 3.0f, 0.45f);
        setColor(0.30f, 0.45f, 0.35f);
        drawBoxAt(vx, 3.0f, vz + 7.0f, 18.0f, 3.0f, 0.3f);
    }
    textureUnbind();
    glEndList();
}

// =====================================================================
// extra districts: stadium, telecom tower, bus terminal, Shahi Eidgah
// =====================================================================
static void drawSimpleBus(float x, float z, float yawDeg, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(r, g, b);
    drawBoxAt(0, 1.7f, 0, 2.5f, 2.5f, 9.6f);
    setColor(0.16f, 0.20f, 0.26f);
    drawBoxAt(0, 2.35f, 0, 2.56f, 0.8f, 9.0f);
    setColor(0.16f, 0.16f, 0.18f);
    drawBoxAt(-1.05f, 0.42f, -3.0f, 0.5f, 0.85f, 0.85f);
    drawBoxAt(1.05f, 0.42f, -3.0f, 0.5f, 0.85f, 0.85f);
    drawBoxAt(-1.05f, 0.42f, 3.0f, 0.5f, 0.85f, 0.85f);
    drawBoxAt(1.05f, 0.42f, 3.0f, 0.5f, 0.85f, 0.85f);
    glPopMatrix();
}

static void buildPois()
{
    glNewList(L_POIS = glGenLists(1), GL_COMPILE);
    textureBind(sWallTex);

    // ---- Sylhet District Stadium ----
    glPushMatrix();
    glTranslatef(1500.0f, 0, -1600.0f);
    setColor(0.62f, 0.60f, 0.56f);
    glPushMatrix(); glScalef(118.0f, 1.0f, 82.0f); drawDiskXZ(1.0f, 26); glPopMatrix();
    setColor(0.66f, 0.38f, 0.30f);
    glPushMatrix(); glTranslatef(0, 0.02f, 0); glScalef(106.0f, 1.0f, 71.0f); drawDiskXZ(1.0f, 26); glPopMatrix();
    setColor(0.34f, 0.58f, 0.30f);
    glPushMatrix(); glTranslatef(0, 0.04f, 0); glScalef(82.0f, 1.0f, 52.0f); drawDiskXZ(1.0f, 24); glPopMatrix();
    for (int i = 0; i < 4; i++)
    {
        float fx = ((i & 1) ? 1.0f : -1.0f) * 96.0f;
        float fz = ((i & 2) ? 1.0f : -1.0f) * 62.0f;
        setColor(0.56f, 0.56f, 0.59f);
        glPushMatrix(); glTranslatef(fx, 0, fz);
        drawCylinderRaw(0.40f, 0.28f, 22.0f, 6, false);
        glPopMatrix();
        setColor(0.88f, 0.87f, 0.84f);
        drawBoxAtRot(fx, 22.6f, fz, 3.4f, 0.5f, 1.5f, 0.0f);
        addTall(1500.0f + fx, -1600.0f + fz, 2.2f, 2.2f, 23.0f);
    }
    setColor(0.74f, 0.72f, 0.68f);
    for (int i = 0; i < 6; i++)
    {
        drawBoxAt(-104.0f - i * 1.7f, 0.6f + i * 0.95f, 0.0f, 3.2f, 0.95f, 42.0f);
    }
    setColor(0.62f, 0.60f, 0.57f);
    drawBoxAt(-117.0f, 6.9f, 0.0f, 9.0f, 0.4f, 48.0f);
    for (int i = -2; i <= 2; i++)
    {
        setColor(0.55f, 0.55f, 0.57f);
        drawBoxAt(-120.0f, 3.6f, i * 11.0f, 0.4f, 6.2f, 0.4f);
    }
    addTall(1500.0f - 117.0f, -1600.0f, 9.0f, 48.0f, 7.5f);
    glPopMatrix();

    // ---- Telecom Tower ----
    glPushMatrix();
    glTranslatef(650.0f, 0, -600.0f);
    setColor(0.88f, 0.30f, 0.24f);
    drawCylinderRaw(3.4f, 1.6f, 18.0f, 6, false);
    setColor(0.93f, 0.93f, 0.94f);
    glPushMatrix(); glTranslatef(0, 18.0f, 0);
    drawCylinderRaw(1.6f, 1.5f, 10.0f, 6, false);
    glPopMatrix();
    setColor(0.88f, 0.30f, 0.24f);
    glPushMatrix(); glTranslatef(0, 28.0f, 0);
    drawCylinderRaw(1.5f, 0.6f, 8.0f, 6, false);
    glPopMatrix();
    setColor(0.62f, 0.62f, 0.64f);
    drawBoxAt(0, 30.0f, 0, 4.6f, 0.35f, 4.6f);
    setColor(0.86f, 0.86f, 0.87f);
    glPushMatrix(); glTranslatef(0, 36.0f, 0);
    drawCylinderRaw(0.28f, 0.12f, 10.0f, 4, false);
    glPopMatrix();
    setColor(0.88f, 0.20f, 0.16f);
    glPushMatrix(); glTranslatef(0, 46.3f, 0); drawSphere(0.5f, 6, 4); glPopMatrix();
    glPopMatrix();
    addTall(650.0f, -600.0f, 8.0f, 8.0f, 47.0f);

    // ---- Kadamtali bus terminal ----
    rotatedQuad(-1650.0f, 2300.0f, 0.08f, 102.0f, 62.0f, 0.0f, 0.30f, 0.30f, 0.32f);
    setColor(0.80f, 0.78f, 0.72f);
    drawBoxAt(-1650.0f, 1.1f, 2300.0f - 65.0f, 212.0f, 2.2f, 1.4f);
    drawBoxAt(-1650.0f, 1.1f, 2300.0f + 65.0f, 212.0f, 2.2f, 1.4f);
    drawBoxAt(-1650.0f - 106.0f, 1.1f, 2300.0f, 1.4f, 2.2f, 132.0f);
    drawBoxAt(-1650.0f + 106.0f, 1.1f, 2300.0f - 42.0f, 1.4f, 2.2f, 46.0f);
    drawBoxAt(-1650.0f + 106.0f, 1.1f, 2300.0f + 42.0f, 1.4f, 2.2f, 46.0f);
    setColor(0.86f, 0.83f, 0.74f);
    drawBoxAt(-1710.0f, 2.6f, 2340.0f, 30.0f, 5.2f, 12.0f);
    setColor(0.30f, 0.40f, 0.50f);
    drawBoxAt(-1710.0f, 2.9f, 2334.0f, 28.0f, 1.4f, 0.3f);
    {
        static const float BC2[7][3] = {
            { 0.15f, 0.45f, 0.25f }, { 0.70f, 0.20f, 0.18f }, { 0.15f, 0.45f, 0.25f },
            { 0.80f, 0.72f, 0.20f }, { 0.20f, 0.35f, 0.62f }, { 0.15f, 0.45f, 0.25f },
            { 0.70f, 0.20f, 0.18f }
        };
        for (int i = 0; i < 7; i++)
        {
            float bx = -1745.0f + (i % 4) * 52.0f;
            float bz = (i < 4) ? (2300.0f - 30.0f) : (2300.0f + 30.0f);
            drawSimpleBus(bx, bz, 0.0f, BC2[i][0], BC2[i][1], BC2[i][2]);
        }
    }
    addTall(-1710.0f, 2340.0f, 30.0f, 12.0f, 5.5f);

    // ---- Shahi Eidgah ----
    rotatedQuad(2300.0f, 500.0f, 0.06f, 90.0f, 60.0f, 0.0f, 0.48f, 0.58f, 0.38f);
    rotatedQuad(2300.0f, 500.0f, 0.09f, 58.0f, 38.0f, 0.0f, 0.70f, 0.68f, 0.62f);
    setColor(0.86f, 0.84f, 0.78f);
    drawBoxAt(2300.0f, 1.25f, 500.0f - 92.0f, 214.0f, 2.5f, 1.4f);
    drawBoxAt(2300.0f, 1.25f, 500.0f + 92.0f, 214.0f, 2.5f, 1.4f);
    drawBoxAt(2300.0f + 106.0f, 1.25f, 500.0f, 1.4f, 2.5f, 186.0f);
    drawBoxAt(2300.0f - 106.0f, 1.25f, 500.0f - 45.0f, 1.4f, 2.5f, 96.0f);
    drawBoxAt(2300.0f - 106.0f, 1.25f, 500.0f + 45.0f, 1.4f, 2.5f, 96.0f);
    // The photographs of Shahi Eidgah show a long red-brick prayer wall behind
    // tiered brick terraces, with a white parapet and small domes along the top.
    textureBind(TEX_BRICK_RED2);
    for (int s = 0; s < 3; s++)
    {
        setColor(0.74f, 0.35f, 0.28f);
        drawBoxAt(2300.0f - 24.0f - s * 10.0f, 0.6f + s * 1.15f, 500.0f, 20.0f, 1.15f, 188.0f - s * 5.0f);
    }
    setColor(0.66f, 0.30f, 0.24f);
    drawBoxAt(2300.0f - 92.0f, 3.1f, 500.0f, 12.0f, 6.2f, 104.0f);
    setColor(0.93f, 0.92f, 0.88f);
    drawBoxAt(2300.0f - 92.0f, 6.6f, 500.0f, 13.0f, 1.0f, 186.0f);
    for (int i = -8; i <= 8; i++)
    {
        drawBoxAt(2300.0f - 92.0f, 7.35f, 500.0f + i * 11.0f, 1.2f, 1.0f, 1.2f);
    }
    textureBind(sWallTex);
    setColor(0.35f, 0.32f, 0.28f);
    for (int i = -4; i <= 4; i++)
    {
        drawBoxAt(2300.0f - 85.6f, 1.7f, 500.0f + i * 10.5f, 0.5f, 3.4f, 3.2f);
    }
    for (int i = 0; i < 2; i++)
    {
        float mz = 500.0f + (i ? 56.0f : -56.0f);
        setColor(0.91f, 0.89f, 0.83f);
        glPushMatrix(); glTranslatef(2300.0f - 92.0f, 0, mz);
        drawCylinderRaw(1.3f, 1.2f, 12.0f, 8, false);
        glPopMatrix();
        setColor(0.30f, 0.56f, 0.40f);
        glPushMatrix(); glTranslatef(2300.0f - 92.0f, 12.0f, mz);
        drawDome(1.5f, 8, 3);
        glPopMatrix();
        addTall(2300.0f - 92.0f, mz, 4.0f, 4.0f, 13.5f);
    }
    addTall(2300.0f - 92.0f, 500.0f, 12.0f, 104.0f, 6.5f);

    textureUnbind();
    glEndList();
}

// =====================================================================
// sun shadows for the static city
// =====================================================================
static void buildShadows()
{
    glNewList(L_SHADOWS = glGenLists(1), GL_COMPILE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const float sx = -0.565f, sz = -0.484f;   // shadows run away from the sun
    glColor4f(0.06f, 0.07f, 0.06f, 0.15f);
    for (int i = 0; i < sAllCount; i++)
    {
        const AABB& b = sAll[i];
        float off = b.h * 0.45f;
        float ox = sx * off, oz = sz * off;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(b.x0 + ox, 0.20f, b.z1 + oz);
        glVertex3f(b.x1 + ox, 0.20f, b.z1 + oz);
        glVertex3f(b.x1 + ox, 0.20f, b.z0 + oz);
        glVertex3f(b.x0 + ox, 0.20f, b.z0 + oz);
        glEnd();
    }
    glColor4f(0.06f, 0.07f, 0.06f, 0.13f);
    for (int i = 0; i < sTreeCount; i++)
    {
        float x = sTreeS[i][0] + sx * 4.0f, z = sTreeS[i][1] + sz * 4.0f;
        float r = sTreeS[i][2];
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, 1, 0);
        glVertex3f(x, 0.19f, z);
        for (int k = 0; k <= 8; k++)
        {
            float a = (float)k / 8.0f * 2.0f * PI_F;
            glVertex3f(x + cosf(a) * r, 0.19f, z + sinf(a) * r);
        }
        glEnd();
    }
    // a few landmark shadows
    glColor4f(0.06f, 0.07f, 0.06f, 0.16f);
    static const float LSH[6][4] = {
        { -1750.0f, -1150.0f, 30.0f, 30.0f },   // mazar shrine
        { 90.0f, -100.0f, 12.0f, 12.0f },       // clock tower
        { 1870.0f, 1550.0f, 70.0f, 24.0f },     // SIU main block
        { -3056.0f, -2109.0f, 90.0f, 40.0f },   // terminal
        { 650.0f, -600.0f, 10.0f, 10.0f },      // telecom tower
        { 1500.0f, -1600.0f, 120.0f, 60.0f }    // stadium
    };
    for (int i = 0; i < 6; i++)
    {
        float off = 16.0f;
        float ox = sx * off, oz = sz * off;
        float x0 = LSH[i][0] - LSH[i][2] * 0.5f + ox, x1 = LSH[i][0] + LSH[i][2] * 0.5f + ox;
        float z0 = LSH[i][1] - LSH[i][3] * 0.5f + oz, z1 = LSH[i][1] + LSH[i][3] * 0.5f + oz;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(x0, 0.21f, z1); glVertex3f(x1, 0.21f, z1);
        glVertex3f(x1, 0.21f, z0); glVertex3f(x0, 0.21f, z0);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEndList();
}

// =====================================================================
// people, flags, street props and the crowds that fill the city
// =====================================================================
void worldDrawPerson(float x, float y, float z, float yawDeg, float sr, float sg, float sb, bool female)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(sr, sg, sb);
    if (female) drawBoxAt(0, 0.95f, 0, 0.42f, 1.05f, 0.30f);   // sari / dress
    else        drawBoxAt(0, 1.05f, 0, 0.44f, 0.80f, 0.26f);   // shirt
    setColor(0.92f, 0.78f, 0.66f);
    glPushMatrix(); glTranslatef(0, female ? 1.62f : 1.58f, 0);
    drawSphere(0.13f, 6, 4);
    glPopMatrix();
    setColor(0.25f, 0.26f, 0.30f);
    if (female)
    {
        drawBoxAt(-0.10f, 0.16f, 0, 0.14f, 0.32f, 0.16f);
        drawBoxAt(0.10f, 0.16f, 0, 0.14f, 0.32f, 0.16f);
    }
    else
    {
        drawBoxAt(-0.11f, 0.32f, 0, 0.15f, 0.64f, 0.18f);
        drawBoxAt(0.11f, 0.32f, 0, 0.15f, 0.64f, 0.18f);
    }
    glPopMatrix();
}

static void drawBanglaFlag(float x, float y, float z, float yawDeg, float size)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.00f, 0.42f, 0.25f);
    drawBoxAt(0, 0, 0, size, size * 0.62f, 0.06f);
    setColor(0.85f, 0.13f, 0.15f);
    glPushMatrix();
    glTranslatef(-size * 0.18f, 0, 0.05f);
    glRotatef(90.0f, 1, 0, 0);
    drawDiskXZ(size * 0.19f, 12);
    glPopMatrix();
    glPopMatrix();
}

static void drawCow(float x, float z, float yawDeg)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.58f, 0.48f, 0.40f);
    drawBoxAt(0, 0.85f, 0, 0.75f, 0.75f, 1.70f);
    drawBoxAt(0, 1.02f, -0.95f, 0.38f, 0.42f, 0.45f);
    setColor(0.45f, 0.36f, 0.30f);
    drawBoxAt(-0.22f, 0.25f, -0.55f, 0.16f, 0.50f, 0.18f);
    drawBoxAt(0.22f, 0.25f, -0.55f, 0.16f, 0.50f, 0.18f);
    drawBoxAt(-0.22f, 0.25f, 0.58f, 0.16f, 0.50f, 0.18f);
    drawBoxAt(0.22f, 0.25f, 0.58f, 0.16f, 0.50f, 0.18f);
    setColor(0.88f, 0.85f, 0.78f);
    drawBoxAt(0, 1.30f, -0.88f, 0.46f, 0.10f, 0.16f);
    glPopMatrix();
}

static void drawBicycle(float x, float z, float yawDeg, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.15f, 0.15f, 0.16f);
    glPushMatrix(); glTranslatef(-0.55f, 0.36f, 0); glRotatef(90.0f, 0, 0, 1);
    drawCylinderRaw(0.34f, 0.34f, 0.08f, 8, true);
    glPopMatrix();
    glPushMatrix(); glTranslatef(0.55f, 0.36f, 0); glRotatef(90.0f, 0, 0, 1);
    drawCylinderRaw(0.34f, 0.34f, 0.08f, 8, true);
    glPopMatrix();
    setColor(r, g, b);
    drawBoxAt(0.0f, 0.62f, 0, 0.9f, 0.07f, 0.07f);
    drawBoxAt(-0.30f, 0.48f, 0, 0.07f, 0.5f, 0.07f);
    drawBoxAt(0.30f, 0.48f, 0, 0.07f, 0.5f, 0.07f);
    setColor(0.20f, 0.20f, 0.22f);
    drawBoxAt(-0.55f, 0.72f, 0, 0.06f, 0.06f, 0.5f);
    glPopMatrix();
}

static void drawBench(float x, float z, float yawDeg)
{
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(-yawDeg, 0, 1, 0);
    setColor(0.55f, 0.40f, 0.26f);
    drawBoxAt(0, 0.45f, 0, 1.9f, 0.10f, 0.45f);
    drawBoxAt(0, 0.72f, -0.20f, 1.9f, 0.45f, 0.08f);
    setColor(0.35f, 0.35f, 0.38f);
    drawBoxAt(-0.75f, 0.22f, 0, 0.10f, 0.45f, 0.42f);
    drawBoxAt(0.75f, 0.22f, 0, 0.10f, 0.45f, 0.42f);
    glPopMatrix();
}

static void crowdCluster(unsigned& seed, float cx, float cz, int n, float spreadX, float spreadZ)
{
    for (int i = 0; i < n; i++)
    {
        float x = cx + (frand01(seed) * 2 - 1) * spreadX;
        float z = cz + (frand01(seed) * 2 - 1) * spreadZ;
        float r = 0.25f + frand01(seed) * 0.60f;
        float g2 = 0.25f + frand01(seed) * 0.60f;
        float b = 0.25f + frand01(seed) * 0.60f;
        float yaw = frand01(seed) * 360.0f;
        bool fem = frand01(seed) < 0.45f;
        worldDrawPerson(x, 0.0f, z, yaw, r, g2, b, fem);
    }
}

static void buildCrowds()
{
    glNewList(L_CROWD = glGenLists(1), GL_COMPILE);
    unsigned seed = 31337u;

    // airport terminal forecourt
    V3 T = worldRunwayPoint(-20.0f, 460.0f);
    crowdCluster(seed, T.x, T.z, 24, 45.0f, 16.0f);
    // railway station platform
    crowdCluster(seed, 700.0f, 2872.0f, 20, 95.0f, 7.0f);
    crowdCluster(seed, 540.0f, 2860.0f, 8, 26.0f, 6.0f);
    // clock tower square and the ghat
    crowdCluster(seed, 90.0f, -175.0f, 16, 34.0f, 22.0f);
    crowdCluster(seed, 20.0f, -25.0f, 8, 14.0f, 12.0f);
    // university gate and quad
    crowdCluster(seed, 1900.0f, 1365.0f, 18, 34.0f, 18.0f);
    // bus terminal yard
    crowdCluster(seed, -1660.0f, 2252.0f, 16, 60.0f, 10.0f);
    // stadium gates
    crowdCluster(seed, 1382.0f, -1600.0f, 12, 12.0f, 34.0f);
    crowdCluster(seed, 1618.0f, -1600.0f, 10, 12.0f, 34.0f);
    // eidgah entrance
    crowdCluster(seed, 2300.0f, 565.0f, 12, 26.0f, 10.0f);
    // the shopping street
    crowdCluster(seed, 19.0f, 150.0f, 26, 4.0f, 170.0f);
    crowdCluster(seed, -19.0f, -620.0f, 16, 4.0f, 120.0f);

    // benches
    drawBench(120.0f, -150.0f, 0.0f);
    drawBench(60.0f, -150.0f, 180.0f);
    drawBench(560.0f, 2866.0f, 0.0f);
    drawBench(840.0f, 2866.0f, 0.0f);
    drawBench(1880.0f, 1380.0f, 90.0f);
    // produce crates at the market
    static const float CRATE[4][3] = {
        { 0.75f, 0.25f, 0.18f }, { 0.85f, 0.65f, 0.15f }, { 0.25f, 0.55f, 0.25f }, { 0.80f, 0.80f, 0.25f }
    };
    for (int i = 0; i < 14; i++)
    {
        float z = -250.0f + i * 32.0f + frand01(seed) * 8.0f;
        float x = 15.0f + (i % 2) * 2.0f;
        int c = (int)(frand01(seed) * 4);
        setColor(CRATE[c][0], CRATE[c][1], CRATE[c][2]);
        drawBoxAt(x, 0.22f, z, 0.8f, 0.44f, 0.8f);
        setColor(0.55f, 0.42f, 0.28f);
        drawBoxAt(x, 0.47f, z, 0.85f, 0.08f, 0.85f);
    }
    // bicycles
    drawBicycle(24.0f, -180.0f, 20.0f, 0.70f, 0.20f, 0.20f);
    drawBicycle(26.0f, -176.0f, 10.0f, 0.20f, 0.35f, 0.65f);
    drawBicycle(620.0f, 2862.0f, 80.0f, 0.75f, 0.75f, 0.20f);
    drawBicycle(-1640.0f, 2250.0f, 100.0f, 0.25f, 0.50f, 0.35f);
    // luggage carts on the platform
    for (int i = 0; i < 3; i++)
    {
        float x = 480.0f + i * 90.0f;
        setColor(0.35f, 0.38f, 0.42f);
        drawBoxAt(x, 0.45f, 2870.0f, 1.3f, 0.7f, 0.8f);
        setColor(0.20f, 0.20f, 0.22f);
        drawBoxAt(x - 0.5f, 0.12f, 2870.0f, 0.25f, 0.2f, 0.6f);
        drawBoxAt(x + 0.5f, 0.12f, 2870.0f, 0.25f, 0.2f, 0.6f);
    }
    // cows on and beside the roads
    drawCow(17.0f, -350.0f, 30.0f);
    drawCow(-18.0f, 700.0f, 200.0f);
    drawCow(-700.0f, 14.0f, 95.0f);
    drawCow(150.0f, 1214.0f, 270.0f);
    drawCow(-1200.0f, 900.0f, 40.0f);

    glEndList();
}

// =====================================================================
// the clock hands of Ali Amjad's Clock Tower (they really move)
// =====================================================================
void worldDrawClockHands(float hourAngleDeg, float minuteAngleDeg)
{
    float x = 90.0f, z = -100.0f;
    for (int fi = 0; fi < 4; fi++)
    {
        glPushMatrix();
        glTranslatef(x, 16.3f, z);
        glRotatef(fi * 90.0f, 0, 1, 0);
        glTranslatef(0, 0, 2.34f);
        setColor(0.12f, 0.10f, 0.10f);
        glPushMatrix();
        glTranslatef(0, 0, 0.12f);
        glRotatef(hourAngleDeg, 0, 0, 1);
        drawBox(0.20f, 1.05f, 0.08f);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0, 0, 0.14f);
        glRotatef(minuteAngleDeg, 0, 0, 1);
        drawBox(0.14f, 1.45f, 0.08f);
        glPopMatrix();
        glPopMatrix();
    }
}

// =====================================================================
// extra detail for the major landmarks
// =====================================================================
static void buildLandmarkDetails()
{
    glNewList(L_DETAILS = glGenLists(1), GL_COMPILE);

    // ================= Osmani International Airport =================
    {
        // helipad with a big H
        V3 heli = worldRunwayPoint(-1150.0f, 300.0f);
        setColor(0.26f, 0.28f, 0.26f);
        glPushMatrix(); glTranslatef(heli.x, 0.10f, heli.z); drawDiskXZ(19.0f, 20); glPopMatrix();
        setColor(0.92f, 0.92f, 0.88f);
        glPushMatrix(); glTranslatef(heli.x, 0.14f, heli.z); drawDiskXZ(16.0f, 20); glPopMatrix();
        setColor(0.30f, 0.32f, 0.30f);
        glPushMatrix(); glTranslatef(heli.x, 0.16f, heli.z); drawDiskXZ(14.5f, 20); glPopMatrix();
        setColor(0.95f, 0.95f, 0.92f);
        drawBoxAt(heli.x - 3.2f, 0.20f, heli.z, 1.4f, 0.06f, 11.0f);
        drawBoxAt(heli.x + 3.2f, 0.20f, heli.z, 1.4f, 0.06f, 11.0f);
        drawBoxAt(heli.x, 0.20f, heli.z, 7.5f, 0.06f, 1.4f);

        // cargo warehouse with loading docks and trucks
        V3 cargo = worldRunwayPoint(-950.0f, 690.0f);
        glPushMatrix();
        glTranslatef(cargo.x, 0, cargo.z);
        glRotatef(-worldRunwayHeadingDeg(), 0, 1, 0);
        setColor(0.70f, 0.72f, 0.74f);
        drawBoxAt(0, 7.0f, 0, 150.0f, 14.0f, 60.0f);
        setColor(0.55f, 0.57f, 0.60f);
        for (int i = -5; i <= 5; i++) drawBoxAt(i * 13.0f, 14.4f, 0, 1.2f, 0.8f, 60.0f);
        setColor(0.25f, 0.27f, 0.30f);
        for (int i = -3; i <= 3; i++) drawBoxAt(i * 18.0f, 2.6f, -30.2f, 9.0f, 5.2f, 0.4f);
        setColor(0.30f, 0.45f, 0.55f);
        for (int i = -2; i <= 2; i++) drawBoxAt(i * 20.0f, 1.8f, -40.0f, 6.0f, 3.6f, 3.0f);
        glPopMatrix();

        // fire station with an engine outside
        V3 fire = worldRunwayPoint(-1450.0f, 470.0f);
        glPushMatrix();
        glTranslatef(fire.x, 0, fire.z);
        glRotatef(-worldRunwayHeadingDeg(), 0, 1, 0);
        setColor(0.72f, 0.20f, 0.18f);
        drawBoxAt(0, 4.5f, 0, 34.0f, 9.0f, 18.0f);
        setColor(0.25f, 0.25f, 0.28f);
        drawBoxAt(-8.0f, 2.6f, -9.2f, 9.0f, 5.2f, 0.4f);
        drawBoxAt(8.0f, 2.6f, -9.2f, 9.0f, 5.2f, 0.4f);
        setColor(0.85f, 0.25f, 0.18f);
        drawBoxAt(-8.0f, 2.2f, -16.0f, 3.0f, 3.4f, 8.0f);
        setColor(0.85f, 0.85f, 0.88f);
        drawBoxAt(-8.0f, 4.6f, -16.0f, 1.6f, 1.2f, 8.0f);
        glPopMatrix();

        // staff mosque with a minaret
        V3 mos = worldRunwayPoint(-1150.0f, 545.0f);
        setColor(0.92f, 0.90f, 0.84f);
        drawBoxAt(mos.x, 3.5f, mos.z, 18.0f, 7.0f, 14.0f);
        setColor(0.28f, 0.54f, 0.38f);
        glPushMatrix(); glTranslatef(mos.x, 7.0f, mos.z); drawDome(5.2f, 12, 4); glPopMatrix();
        setColor(0.92f, 0.90f, 0.84f);
        glPushMatrix(); glTranslatef(mos.x + 12.0f, 0, mos.z + 6.0f);
        drawCylinderRaw(1.4f, 1.2f, 14.0f, 8, false);
        glPopMatrix();
        setColor(0.28f, 0.54f, 0.38f);
        glPushMatrix(); glTranslatef(mos.x + 12.0f, 14.0f, mos.z + 6.0f); drawDome(1.6f, 8, 3); glPopMatrix();
        addTall(mos.x, mos.z, 18.0f, 14.0f, 7.5f);

        // staff car park
        V3 park = worldRunwayPoint(-560.0f, 640.0f);
        glPushMatrix();
        glTranslatef(park.x, 0, park.z);
        glRotatef(-worldRunwayHeadingDeg(), 0, 1, 0);
        rotatedQuad(0, 0, 0.08f, 55.0f, 34.0f, 0.0f, 0.30f, 0.30f, 0.32f);
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 6; c++)
            {
                float cx2 = -40.0f + c * 16.0f;
                float cz2 = -20.0f + r * 20.0f;
                static const float CC[4][3] = {
                    { 0.85f, 0.85f, 0.87f }, { 0.30f, 0.33f, 0.38f },
                    { 0.62f, 0.18f, 0.16f }, { 0.18f, 0.33f, 0.55f }
                };
                int ci = (r * 6 + c) % 4;
                setColor(CC[ci][0], CC[ci][1], CC[ci][2]);
                drawBoxAt(cx2, 0.75f, cz2, 1.8f, 0.9f, 4.2f);
                setColor(0.15f, 0.17f, 0.22f);
                drawBoxAt(cx2, 1.45f, cz2 - 0.3f, 1.6f, 0.6f, 2.2f);
            }
        }
        glPopMatrix();

        // radar dish on the control tower
        V3 twr = worldRunwayPoint(-230.0f, 470.0f);
        setColor(0.88f, 0.88f, 0.90f);
        glPushMatrix();
        glTranslatef(twr.x + 4.0f, 25.8f, twr.z + 3.0f);
        glRotatef(28.0f, 0, 0, 1);
        drawDome(2.4f, 10, 3);
        glPopMatrix();
        setColor(0.40f, 0.40f, 0.44f);
        drawBoxAt(twr.x + 4.0f, 25.0f, twr.z + 3.0f, 0.25f, 1.6f, 0.25f);
    }

    // ================= Keane Bridge =================
    {
        // name board on the north approach
        setColor(0.10f, 0.38f, 0.22f);
        drawBoxAt(19.0f, 4.2f, -34.0f, 16.0f, 3.0f, 0.4f);
        setColor(0.40f, 0.40f, 0.42f);
        drawBoxAt(13.0f, 2.0f, -34.0f, 0.4f, 4.0f, 0.4f);
        drawBoxAt(25.0f, 2.0f, -34.0f, 0.4f, 4.0f, 0.4f);
        setColor(0.95f, 0.95f, 0.92f);
        glPushMatrix();
        glTranslatef(19.0f, 4.0f, -34.25f);
        glRotatef(180.0f, 0, 1, 0);
        glLineWidth(2.0f);
        strokeTextCentered(GLUT_STROKE_ROMAN, "KEANE BRIDGE", 0.012f);
        glLineWidth(1.0f);
        glPopMatrix();

        // police post
        setColor(0.92f, 0.90f, 0.84f);
        drawBoxAt(-22.0f, 1.8f, -30.0f, 5.0f, 3.6f, 5.0f);
        setColor(0.15f, 0.30f, 0.55f);
        drawBoxAt(-22.0f, 4.0f, -32.6f, 2.4f, 1.0f, 0.2f);
        setColor(0.20f, 0.20f, 0.22f);
        drawBoxAt(-22.0f, 1.2f, -30.0f, 5.6f, 0.3f, 5.6f);
    }

    // ================= riverside promenade =================
    {
        float L = worldRiverLength();
        float s0 = L * 0.30f, s1 = L * 0.52f;
        float step = 26.0f;
        for (float s = s0; s < s1; s += step)
        {
            V3 p, d;
            worldRiverFrameAtS(s, p, d);
            V3 n = v3(d.z, 0, -d.x);          // towards the city side
            float wallX = p.x + n.x * 104.0f, wallZ = p.z + n.z * 104.0f;
            float yaw = rad2deg(atan2f(d.x, -d.z));
            // retaining wall
            setColor(0.62f, 0.60f, 0.55f);
            drawBoxAtRot(wallX, 0.6f, wallZ, 2.0f, 1.2f, step + 1.0f, yaw);
            // walkway
            setColor(0.70f, 0.69f, 0.65f);
            drawBoxAtRot(wallX + n.x * 6.0f, 0.12f, wallZ + n.z * 6.0f, 10.0f, 0.22f, step + 1.0f, yaw);
        }
        // lamps and trees along the promenade
        for (float s = s0; s < s1; s += 56.0f)
        {
            V3 p, d;
            worldRiverFrameAtS(s, p, d);
            V3 n = v3(d.z, 0, -d.x);
            float lx = p.x + n.x * 112.0f, lz = p.z + n.z * 112.0f;
            setColor(0.45f, 0.45f, 0.48f);
            drawBoxAt(lx, 2.6f, lz, 0.22f, 5.2f, 0.22f);
            setColor(0.95f, 0.92f, 0.75f);
            drawBoxAt(lx, 5.3f, lz, 0.8f, 0.35f, 0.8f);
        }
        // launch terminal on stilts
        {
            V3 p, d;
            worldRiverFrameAtS(L * 0.40f, p, d);
            V3 n = v3(d.z, 0, -d.x);
            float tx = p.x + n.x * 88.0f, tz = p.z + n.z * 88.0f;
            float yaw = rad2deg(atan2f(d.x, -d.z));
            setColor(0.55f, 0.45f, 0.34f);
            for (int i = 0; i < 4; i++)
            {
                float off = -6.0f + i * 4.0f;
                drawBoxAtRot(tx + n.x * off, 0.0f, tz + n.z * off, 0.5f, 3.0f, 0.5f, yaw);
            }
            setColor(0.72f, 0.68f, 0.58f);
            drawBoxAtRot(tx, 4.2f, tz, 16.0f, 0.5f, 12.0f, yaw);
            setColor(0.45f, 0.42f, 0.36f);
            drawBoxAtRot(tx, 3.0f, tz, 14.0f, 2.4f, 10.0f, yaw);
            // ramp down to the water
            setColor(0.60f, 0.55f, 0.48f);
            glPushMatrix();
            glTranslatef(tx - n.x * 10.0f, 1.6f, tz - n.z * 10.0f);
            glRotatef(-rad2deg(atan2f(d.x, -d.z)), 0, 1, 0);
            glRotatef(-18.0f, 1, 0, 0);
            drawBox(4.0f, 0.3f, 10.0f);
            glPopMatrix();
        }
    }

    // ================= clock tower square =================
    {
        // bazaar stalls facing the tower
        for (int i = 0; i < 8; i++)
        {
            float z = -170.0f + i * 15.0f;
            float x = 34.0f + (i % 2) * 13.0f;
            setColor(0.62f, 0.55f, 0.45f);
            drawBoxAt(x, 1.4f, z, 5.0f, 2.8f, 4.0f);
            static const float AWN2[4][3] = {
                { 0.78f, 0.20f, 0.16f }, { 0.18f, 0.35f, 0.62f },
                { 0.20f, 0.52f, 0.26f }, { 0.86f, 0.72f, 0.16f }
            };
            int ai = i % 4;
            setColor(AWN2[ai][0], AWN2[ai][1], AWN2[ai][2]);
            glPushMatrix();
            glTranslatef(x, 2.95f, z);
            glRotatef(16.0f, 0, 0, 1);
            drawBox(5.6f, 0.12f, 4.6f);
            glPopMatrix();
            // produce crates
            setColor(0.80f, 0.65f, 0.20f);
            drawBoxAt(x - 4.0f, 0.25f, z, 1.0f, 0.5f, 1.0f);
        }
        // a second colonial building opposite the tower
        setColor(0.90f, 0.87f, 0.80f);
        drawBoxAt(44.0f, 2.6f, 26.0f, 18.0f, 5.2f, 12.0f);
        setColor(0.55f, 0.20f, 0.18f);
        drawBoxAt(44.0f, 5.4f, 26.0f, 19.0f, 0.5f, 13.0f);
        setColor(0.80f, 0.78f, 0.72f);
        for (int i = 0; i < 5; i++)
            drawBoxAt(37.0f + i * 3.4f, 1.4f, 19.6f, 0.45f, 2.8f, 0.45f);
        // fountain in the square
        setColor(0.80f, 0.78f, 0.73f);
        glPushMatrix(); glTranslatef(90.0f, 0.35f, -165.0f); drawCylinderRaw(6.5f, 6.5f, 0.7f, 16, true); glPopMatrix();
        setColor(0.32f, 0.55f, 0.62f);
        glPushMatrix(); glTranslatef(90.0f, 0.78f, -165.0f); drawCylinderRaw(5.6f, 5.6f, 0.1f, 16, true); glPopMatrix();
        setColor(0.86f, 0.87f, 0.89f);
        glPushMatrix(); glTranslatef(90.0f, 0.8f, -165.0f); drawCylinderRaw(0.8f, 0.5f, 2.4f, 10, true); glPopMatrix();
        // heritage sign
        setColor(0.92f, 0.91f, 0.86f);
        drawBoxAt(58.0f, 3.0f, -138.0f, 12.0f, 2.2f, 0.3f);
        setColor(0.40f, 0.40f, 0.42f);
        drawBoxAt(54.0f, 1.4f, -138.0f, 0.35f, 2.8f, 0.35f);
        drawBoxAt(62.0f, 1.4f, -138.0f, 0.35f, 2.8f, 0.35f);
        setColor(0.12f, 0.30f, 0.50f);
        glPushMatrix();
        glTranslatef(58.0f, 2.9f, -138.2f);
        glRotatef(180.0f, 0, 1, 0);
        glLineWidth(1.6f);
        strokeTextCentered(GLUT_STROKE_ROMAN, "HERITAGE", 0.006f);
        glLineWidth(1.0f);
        glPopMatrix();
    }

    glEndList();
}

// =====================================================================
// more landmark detail: mazar, station, university, stadium, bus terminal
// =====================================================================
static void buildLandmarkDetails2()
{
    glNewList(L_DETAILS2 = glGenLists(1), GL_COMPILE);

    // ================= Hazrat Shahjalal Mazar =================
    {
        float mx = -1750.0f, mz = -1150.0f;
        // the old graveyard behind the north wall
        for (int r = 0; r < 5; r++)
        {
            for (int c = 0; c < 14; c++)
            {
                float gx = mx - 150.0f + c * 23.0f + (r % 2) * 11.0f;
                float gz = mz - 205.0f - r * 14.0f;
                float h = 0.7f + ((r + c) % 3) * 0.18f;
                setColor(0.86f, 0.85f, 0.80f);
                drawBoxAt(gx, h * 0.5f, gz, 0.7f, h, 0.35f);
                setColor(0.74f, 0.73f, 0.69f);
                drawBoxAt(gx, h + 0.06f, gz, 0.8f, 0.12f, 0.45f);
            }
        }
        // dargah library building
        setColor(0.93f, 0.91f, 0.86f);
        drawBoxAt(mx - 110.0f, 4.0f, mz + 70.0f, 26.0f, 8.0f, 16.0f);
        setColor(0.55f, 0.20f, 0.18f);
        drawBoxAt(mx - 110.0f, 8.3f, mz + 70.0f, 27.0f, 0.5f, 17.0f);
        setColor(0.28f, 0.54f, 0.38f);
        glPushMatrix(); glTranslatef(mx - 110.0f, 8.5f, mz + 70.0f); drawDome(5.0f, 12, 4); glPopMatrix();
        setColor(0.20f, 0.25f, 0.32f);
        for (int i = 0; i < 4; i++)
            drawBoxAt(mx - 118.0f + i * 5.6f, 3.0f, mz + 61.8f, 2.2f, 4.4f, 0.3f);
        addTall(mx - 110.0f, mz + 70.0f, 26.0f, 16.0f, 8.5f);
        // ablution fountain
        setColor(0.80f, 0.78f, 0.73f);
        glPushMatrix(); glTranslatef(mx + 70.0f, 0.35f, mz + 120.0f); drawCylinderRaw(3.2f, 3.2f, 0.7f, 12, true); glPopMatrix();
        setColor(0.30f, 0.55f, 0.62f);
        glPushMatrix(); glTranslatef(mx + 70.0f, 0.78f, mz + 120.0f); drawCylinderRaw(2.7f, 2.7f, 0.1f, 12, true); glPopMatrix();
        setColor(0.70f, 0.72f, 0.75f);
        for (int i = 0; i < 3; i++) drawBoxAt(mx + 67.5f + i * 2.5f, 1.4f, mz + 118.6f, 0.25f, 0.9f, 0.25f);
        // minarets at the main gate
        for (int i = 0; i < 2; i++)
        {
            float gx = mx + (i ? 20.0f : -20.0f);
            float gz = mz - 180.0f;
            setColor(0.93f, 0.91f, 0.86f);
            glPushMatrix(); glTranslatef(gx, 0, gz); drawCylinderRaw(1.6f, 1.4f, 16.0f, 10, false); glPopMatrix();
            setColor(0.28f, 0.54f, 0.38f);
            glPushMatrix(); glTranslatef(gx, 16.0f, gz); drawDome(1.9f, 10, 3); glPopMatrix();
            setColor(0.95f, 0.80f, 0.30f);
            glPushMatrix(); glTranslatef(gx, 18.4f, gz); drawSphere(0.4f, 6, 4); glPopMatrix();
            addTall(gx, gz, 4.0f, 4.0f, 18.0f);
        }
        // flower beds along the path
        static const float FLOW[3][3] = { { 0.80f, 0.20f, 0.25f }, { 0.90f, 0.80f, 0.20f }, { 0.65f, 0.25f, 0.65f } };
        for (int i = 0; i < 8; i++)
        {
            int ci = i % 3;
            setColor(FLOW[ci][0], FLOW[ci][1], FLOW[ci][2]);
            drawBoxAt(mx - 60.0f + i * 16.0f, 0.22f, mz - 148.0f, 12.0f, 0.3f, 5.0f);
        }
        // donation box by the gate
        setColor(0.30f, 0.48f, 0.30f);
        drawBoxAt(mx - 8.0f, 0.6f, mz - 168.0f, 1.2f, 1.2f, 1.2f);
    }

    // ================= Sylhet Railway Station =================
    {
        // a train standing at the platform
        for (int i = 0; i < 4; i++)
        {
            float x = 960.0f + i * 26.0f;
            setColor(0.22f, 0.48f, 0.30f);
            drawBoxAt(x, 2.1f, 2900.0f, 24.0f, 2.8f, 3.0f);
            setColor(0.88f, 0.86f, 0.80f);
            drawBoxAt(x, 2.3f, 2898.4f, 23.0f, 0.8f, 0.3f);
            setColor(0.16f, 0.16f, 0.18f);
            drawBoxAt(x - 8.0f, 0.4f, 2900.0f, 2.0f, 0.8f, 2.6f);
            drawBoxAt(x + 8.0f, 0.4f, 2900.0f, 2.0f, 0.8f, 2.6f);
        }
        // semaphore signal
        setColor(0.55f, 0.55f, 0.58f);
        drawBoxAt(200.0f, 3.0f, 2908.0f, 0.25f, 6.0f, 0.25f);
        setColor(0.85f, 0.20f, 0.16f);
        glPushMatrix();
        glTranslatef(200.0f, 5.6f, 2908.0f);
        glRotatef(-35.0f, 0, 0, 1);
        drawBox(0.3f, 2.6f, 0.12f);
        glPopMatrix();
        setColor(0.92f, 0.92f, 0.88f);
        drawBoxAt(200.0f, 6.4f, 2907.7f, 0.5f, 0.5f, 0.2f);
        // goods shed and freight wagons
        setColor(0.62f, 0.58f, 0.50f);
        drawBoxAt(-400.0f, 5.0f, 2965.0f, 120.0f, 10.0f, 24.0f);
        setColor(0.45f, 0.42f, 0.36f);
        drawBoxAt(-400.0f, 10.3f, 2965.0f, 121.0f, 0.6f, 25.0f);
        for (int i = 0; i < 3; i++)
        {
            float x = -380.0f + i * 26.0f;
            setColor(0.42f, 0.30f, 0.22f);
            drawBoxAt(x, 1.9f, 2906.0f, 22.0f, 3.4f, 3.2f);
            setColor(0.30f, 0.22f, 0.17f);
            drawBoxAt(x, 3.7f, 2906.0f, 22.4f, 0.4f, 3.6f);
            setColor(0.16f, 0.16f, 0.18f);
            drawBoxAt(x - 7.0f, 0.4f, 2906.0f, 1.8f, 0.8f, 2.6f);
            drawBoxAt(x + 7.0f, 0.4f, 2906.0f, 1.8f, 0.8f, 2.6f);
        }
        // ticket counter
        setColor(0.88f, 0.85f, 0.78f);
        drawBoxAt(790.0f, 2.5f, 2846.0f, 18.0f, 5.0f, 8.0f);
        setColor(0.20f, 0.25f, 0.32f);
        for (int i = 0; i < 3; i++) drawBoxAt(784.0f + i * 6.0f, 2.4f, 2841.9f, 3.2f, 2.2f, 0.3f);
        setColor(0.55f, 0.55f, 0.58f);
        drawBoxAt(790.0f, 5.2f, 2846.0f, 19.0f, 0.4f, 9.0f);
        // cycle rickshaw rank
        for (int i = 0; i < 5; i++)
        {
            float x = 850.0f + i * 7.0f, z = 2836.0f;
            setColor(0.85f, 0.85f, 0.86f);
            drawBoxAt(x, 0.85f, z, 1.2f, 0.9f, 2.4f);
            setColor(0.75f, 0.20f, 0.16f);
            drawBoxAt(x, 1.6f, z + 0.6f, 1.3f, 1.0f, 1.4f);
            setColor(0.15f, 0.15f, 0.16f);
            drawBoxAt(x, 0.32f, z + 0.8f, 0.2f, 0.6f, 0.6f);
            drawBoxAt(x, 0.32f, z - 0.8f, 0.2f, 0.6f, 0.6f);
        }
        // big name board on the station building
        setColor(0.94f, 0.94f, 0.90f);
        drawBoxAt(700.0f, 7.0f, 2839.6f, 30.0f, 4.0f, 0.4f);
        setColor(0.10f, 0.30f, 0.55f);
        glPushMatrix();
        glTranslatef(700.0f, 6.6f, 2839.3f);
        glRotatef(180.0f, 0, 1, 0);
        glLineWidth(3.0f);
        strokeTextCentered(GLUT_STROKE_ROMAN, "SYLHET", 0.035f);
        glLineWidth(1.0f);
        glPopMatrix();
    }

    // ================= Sylhet International University =================
    {
        // auditorium with a colonnade
        setColor(0.88f, 0.86f, 0.80f);
        glPushMatrix();
        glTranslatef(2020.0f, 0, 1500.0f);
        drawCylinderRaw(22.0f, 22.0f, 11.0f, 18, false);
        glPopMatrix();
        setColor(0.72f, 0.70f, 0.64f);
        glPushMatrix();
        glTranslatef(2020.0f, 11.0f, 1500.0f);
        drawCylinderRaw(23.0f, 20.0f, 2.0f, 18, true);
        glPopMatrix();
        setColor(0.80f, 0.78f, 0.72f);
        for (int i = 0; i < 8; i++)
        {
            float a = -0.9f + i * 0.26f;
            drawBoxAt(2020.0f + sinf(a) * 26.0f, 3.5f, 1500.0f + cosf(a) * 26.0f, 1.0f, 7.0f, 1.0f);
        }
        addTall(2020.0f, 1500.0f, 44.0f, 44.0f, 12.0f);
        // library block
        setColor(0.90f, 0.88f, 0.83f);
        drawBoxAt(1720.0f, 6.0f, 1420.0f, 36.0f, 12.0f, 20.0f);
        setColor(0.30f, 0.42f, 0.55f);
        drawBoxAt(1720.0f, 6.0f, 1409.8f, 36.6f, 2.0f, 0.4f);
        setColor(0.82f, 0.80f, 0.74f);
        for (int i = 0; i < 6; i++)
            drawBoxAt(1706.0f + i * 5.6f, 3.0f, 1408.0f, 1.1f, 6.0f, 1.1f);
        setColor(0.72f, 0.70f, 0.64f);
        drawBoxAt(1720.0f, 0.3f, 1404.0f, 40.0f, 0.6f, 8.0f);
        addTall(1720.0f, 1420.0f, 36.0f, 20.0f, 12.5f);
        // canteen with outdoor tables
        setColor(0.86f, 0.83f, 0.74f);
        drawBoxAt(2065.0f, 3.0f, 1700.0f, 22.0f, 6.0f, 12.0f);
        setColor(0.30f, 0.46f, 0.34f);
        drawBoxAt(2065.0f, 6.2f, 1700.0f, 23.0f, 0.4f, 13.0f);
        for (int i = 0; i < 5; i++)
        {
            float tx = 2040.0f + i * 9.0f, tz = 1714.0f;
            setColor(0.75f, 0.55f, 0.35f);
            glPushMatrix(); glTranslatef(tx, 0, tz); drawCylinderRaw(1.1f, 1.1f, 0.75f, 10, true); glPopMatrix();
            setColor(0.45f, 0.45f, 0.48f);
            for (int k = 0; k < 3; k++)
            {
                float a = (float)k / 3.0f * 6.2831853f;
                drawBoxAt(tx + cosf(a) * 1.9f, 0.45f, tz + sinf(a) * 1.9f, 0.6f, 0.9f, 0.6f);
            }
        }
        // basketball court
        rotatedQuad(1790.0f, 1690.0f, 0.08f, 26.0f, 14.0f, 0.0f, 0.35f, 0.45f, 0.55f);
        setColor(0.90f, 0.90f, 0.86f);
        drawBoxAt(1790.0f, 0.12f, 1690.0f, 26.0f, 0.05f, 0.25f);
        for (int s = 0; s < 2; s++)
        {
            float hx = 1790.0f + (s ? 26.0f : -26.0f);
            setColor(0.35f, 0.35f, 0.38f);
            drawBoxAt(hx, 1.7f, 1690.0f, 0.22f, 3.4f, 0.22f);
            setColor(0.90f, 0.90f, 0.86f);
            drawBoxAt(hx, 3.2f, 1690.0f, 0.15f, 1.0f, 1.8f);
        }
        // guardhouse at the gate
        setColor(0.86f, 0.84f, 0.78f);
        drawBoxAt(1858.0f, 1.6f, 1400.0f, 4.5f, 3.2f, 4.5f);
        setColor(0.30f, 0.46f, 0.34f);
        drawBoxAt(1858.0f, 3.4f, 1400.0f, 5.2f, 0.3f, 5.2f);
        // campus bus
        setColor(0.85f, 0.85f, 0.87f);
        drawBoxAt(1930.0f, 1.8f, 1425.0f, 2.6f, 2.6f, 10.0f);
        setColor(0.20f, 0.26f, 0.34f);
        drawBoxAt(1930.0f, 2.2f, 1425.0f, 2.66f, 0.9f, 9.2f);
        setColor(0.16f, 0.16f, 0.18f);
        drawBoxAt(1929.0f, 0.45f, 1422.0f, 0.5f, 0.9f, 0.9f);
        drawBoxAt(1929.0f, 0.45f, 1428.0f, 0.5f, 0.9f, 0.9f);
    }

    // ================= District Stadium =================
    {
        // cricket pitch with mown stripes and creases
        for (int i = 0; i < 8; i++)
        {
            float z = -1610.0f + i * 2.6f;
            setColor((i % 2) ? 0.38f : 0.44f, (i % 2) ? 0.62f : 0.66f, (i % 2) ? 0.34f : 0.36f);
            drawBoxAt(1500.0f, 0.10f, z, 22.0f, 0.10f, 2.6f);
        }
        setColor(0.92f, 0.92f, 0.88f);
        drawBoxAt(1500.0f, 0.16f, -1620.0f, 14.0f, 0.05f, 0.25f);
        drawBoxAt(1500.0f, 0.16f, -1592.0f, 14.0f, 0.05f, 0.25f);
        drawBoxAt(1494.0f, 0.16f, -1616.0f, 0.25f, 0.05f, 7.0f);
        drawBoxAt(1506.0f, 0.16f, -1616.0f, 0.25f, 0.05f, 7.0f);
        drawBoxAt(1494.0f, 0.16f, -1596.0f, 0.25f, 0.05f, 7.0f);
        drawBoxAt(1506.0f, 0.16f, -1596.0f, 0.25f, 0.05f, 7.0f);
        for (int s = 0; s < 2; s++)
        {
            float sz = (s ? -1592.0f : -1620.0f);
            setColor(0.92f, 0.88f, 0.70f);
            for (int k = 0; k < 3; k++) drawBoxAt(1499.0f + k * 0.5f, 0.45f, sz, 0.12f, 0.75f, 0.12f);
        }
        // scoreboard
        setColor(0.25f, 0.27f, 0.30f);
        drawBoxAt(1370.0f, 8.0f, -1600.0f, 22.0f, 12.0f, 1.2f);
        setColor(0.12f, 0.14f, 0.16f);
        drawBoxAt(1370.0f, 8.5f, -1600.7f, 20.0f, 8.0f, 0.2f);
        setColor(0.85f, 0.90f, 0.70f);
        for (int i = 0; i < 5; i++) drawBoxAt(1364.0f + i * 3.0f, 9.5f, -1600.9f, 2.0f, 2.4f, 0.1f);
        setColor(0.45f, 0.45f, 0.48f);
        drawBoxAt(1370.0f, 1.5f, -1600.0f, 1.0f, 3.0f, 1.0f);
        // stand roof and coloured seats
        setColor(0.55f, 0.57f, 0.60f);
        glPushMatrix();
        glTranslatef(1378.0f, 7.4f, -1600.0f);
        glRotatef(-10.0f, 0, 0, 1);
        drawBox(52.0f, 0.4f, 50.0f);
        glPopMatrix();
        static const float SEATC[4][3] = { { 0.25f, 0.40f, 0.65f }, { 0.70f, 0.25f, 0.22f }, { 0.85f, 0.80f, 0.25f }, { 0.25f, 0.55f, 0.35f } };
        for (int i = 0; i < 4; i++)
        {
            for (int k = 0; k < 10; k++)
            {
                int ci = (i + k) % 4;
                setColor(SEATC[ci][0], SEATC[ci][1], SEATC[ci][2]);
                drawBoxAt(1362.0f + i * 3.0f, 0.6f + i * 1.1f, -1620.0f + k * 4.5f, 2.4f, 1.0f, 3.6f);
            }
        }
    }

    // ================= Kadamtali Bus Terminal =================
    {
        setColor(0.62f, 0.64f, 0.66f);
        drawBoxAt(-1650.0f, 5.0f, 2245.0f, 200.0f, 0.4f, 14.0f);
        setColor(0.50f, 0.52f, 0.55f);
        for (int i = 0; i < 6; i++) drawBoxAt(-1730.0f + i * 32.0f, 2.5f, 2245.0f, 0.7f, 5.0f, 0.7f);
        // route board
        setColor(0.22f, 0.24f, 0.28f);
        drawBoxAt(-1560.0f, 4.5f, 2300.0f, 16.0f, 6.0f, 0.6f);
        static const float RT[6][3] = {
            { 0.85f, 0.25f, 0.20f }, { 0.20f, 0.45f, 0.85f }, { 0.25f, 0.60f, 0.30f },
            { 0.90f, 0.75f, 0.20f }, { 0.70f, 0.25f, 0.70f }, { 0.30f, 0.70f, 0.70f }
        };
        for (int i = 0; i < 6; i++)
        {
            setColor(RT[i][0], RT[i][1], RT[i][2]);
            drawBoxAt(-1566.0f + (i % 3) * 5.0f, 5.6f - (i / 3) * 2.4f, 2299.6f, 4.0f, 1.6f, 0.2f);
        }
        setColor(0.45f, 0.45f, 0.48f);
        drawBoxAt(-1560.0f, 1.4f, 2300.0f, 0.5f, 2.8f, 0.5f);
        drawBoxAt(-1556.0f, 1.4f, 2300.0f, 0.5f, 2.8f, 0.5f);
        // ticket windows on the office
        setColor(0.20f, 0.25f, 0.32f);
        for (int i = 0; i < 3; i++) drawBoxAt(-1716.0f + i * 6.0f, 2.4f, 2333.8f, 3.4f, 2.4f, 0.3f);
    }

    glEndList();
}

// =====================================================================
// public API
// =====================================================================
// ---------------------------------------------------------------------
// the Imran International Airlines billboard
// ---------------------------------------------------------------------
static void drawSignText(const char* s, float scale, float r, float g, float b, float y,
                         float zOff, bool backFace)
{
    glPushMatrix();
    glTranslatef(0.0f, y, zOff);
    if (!backFace) glRotatef(180.0f, 0, 1, 0);   // the stroke font draws facing +Z
    setColor(r, g, b);
    strokeTextCentered(GLUT_STROKE_ROMAN, s, scale);
    glPopMatrix();
}

// both faces of a panel carry the print, so the sign is readable from either side
static void drawSignTextBothFaces(const char* s, float scale, float r, float g, float b, float y)
{
    drawSignText(s, scale, r, g, b, y, -0.78f, false);
    drawSignText(s, scale, r, g, b, y,  0.78f, true);
}

static void drawBillboard(float x, float z, float yawDeg, float w, float h, float footY, TexId art)
{
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(-yawDeg, 0, 1, 0);       // the panel faces -Z in its own frame

    // two steel posts and a cross beam
    setColor(0.34f, 0.36f, 0.38f);
    drawBoxAt(-w * 0.34f, footY * 0.5f, 0.0f, 2.4f, footY, 2.4f);
    drawBoxAt( w * 0.34f, footY * 0.5f, 0.0f, 2.4f, footY, 2.4f);
    setColor(0.30f, 0.32f, 0.34f);
    drawBoxAt(0.0f, footY * 0.62f, 0.0f, w * 0.68f, 0.8f, 0.8f);

    // panel: dark frame, grey back, artwork printed on the front
    setColor(0.17f, 0.18f, 0.20f);
    drawBoxAt(0.0f, footY + h * 0.5f, 0.0f, w + 1.8f, h + 1.8f, 1.4f);
    // (no grey back panel: both faces carry the print, so the billboard works
    // doubled-sided, like a real one seen from the other side of the road)

    glDisable(GL_LIGHTING);            // a printed panel reads better unlit
    if (textureBind(art))
    {
        setColor(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, -1.0f);
        // +X is the viewer's left when the panel faces -Z, so u runs 1 -> 0
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-w * 0.5f, footY,     -0.72f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f( w * 0.5f, footY,     -0.72f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f( w * 0.5f, footY + h, -0.72f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-w * 0.5f, footY + h, -0.72f);
        // and the same print on the back, mirrored so it reads the right way round
        glNormal3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-w * 0.5f, footY,      0.72f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( w * 0.5f, footY,      0.72f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( w * 0.5f, footY + h,  0.72f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-w * 0.5f, footY + h,  0.72f);
        glEnd();
        textureUnbind();
    }
    else
    {
        // no artwork on disk: paint the sign in code, so the billboard still
        // says something with an empty assets folder
        setColor(0.05f, 0.09f, 0.20f);
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(-w * 0.5f, footY,     -0.72f);
        glVertex3f( w * 0.5f, footY,     -0.72f);
        glVertex3f( w * 0.5f, footY + h, -0.72f);
        glVertex3f(-w * 0.5f, footY + h, -0.72f);
        glNormal3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-w * 0.5f, footY,      0.72f);
        glVertex3f( w * 0.5f, footY,      0.72f);
        glVertex3f( w * 0.5f, footY + h,  0.72f);
        glVertex3f(-w * 0.5f, footY + h,  0.72f);
        glEnd();
        // a gold band makes every fallback panel look deliberate
        setColor(0.85f, 0.72f, 0.30f);
        drawBoxAt(0.0f, footY + h * 0.66f, -0.74f, w * 0.70f, h * 0.035f, 0.08f);
        drawBoxAt(0.0f, footY + h * 0.24f, -0.74f, w * 0.70f, h * 0.035f, 0.08f);
        if (art == TEX_BILLBOARD)
        {
            drawSignTextBothFaces("IMRAN",                    0.055f, 0.88f, 0.74f, 0.30f, footY + h * 0.30f);
            drawSignTextBothFaces("INTERNATIONAL",            0.022f, 0.95f, 0.95f, 0.95f, footY + h * 0.50f);
            drawSignTextBothFaces("AIRLINES",                 0.022f, 0.95f, 0.95f, 0.95f, footY + h * 0.42f);
            drawSignTextBothFaces("YOUR JOURNEY, OUR PASSION", 0.016f, 0.82f, 0.68f, 0.28f, footY + h * 0.14f);
        }
        else if (art == TEX_BILLBOARD2)
        {
            drawSignTextBothFaces("AL-AMIN ENTERPRISES",       0.024f, 0.95f, 0.95f, 0.95f, footY + h * 0.50f);
            drawSignTextBothFaces("BUILDING THE FUTURE",       0.036f, 0.88f, 0.74f, 0.30f, footY + h * 0.36f);
        }
    }
    glEnable(GL_LIGHTING);

    // two floodlights aimed at the panel
    setColor(0.60f, 0.60f, 0.58f);
    drawBoxAt(-w * 0.5f - 1.2f, footY + h * 0.74f, -1.7f, 1.6f, 0.7f, 1.2f);
    drawBoxAt( w * 0.5f + 1.2f, footY + h * 0.74f, -1.7f, 1.6f, 0.7f, 1.2f);
    drawBoxAt(-w * 0.5f - 1.2f, footY + h * 0.74f,  1.7f, 1.6f, 0.7f, 1.2f);
    drawBoxAt( w * 0.5f + 1.2f, footY + h * 0.74f,  1.7f, 1.6f, 0.7f, 1.2f);

    glPopMatrix();
}

static void buildBillboards()
{
    glNewList(L_BILLBOARD = glGenLists(1), GL_COMPILE);
    // beside the SIU campus strip, facing south: the opening shot of the demo
    // and the first thing you see climbing out of campus
    drawBillboard(1515.0f, 2300.0f, 180.0f, 40.0f, 40.0f, 15.0f, TEX_BILLBOARD);
    // on the Surma bank, facing the approach to Keane Bridge
    drawBillboard(190.0f, 470.0f, 115.0f, 26.0f, 26.0f, 13.0f, TEX_BILLBOARD);
    // on the Osmani apron, facing the runway
    {
        V3 ap = worldRunwayPoint(120.0f, 300.0f);
        drawBillboard(ap.x, ap.z, 320.0f, 22.0f, 22.0f, 12.0f, TEX_BILLBOARD);
    }
    // Al-Amin Enterprises: on the Surma bank where the city tour passes, so the
    // panel faces the aircraft coming up the river
    drawBillboard(514.0f, 550.0f, 132.0f, 34.0f, 34.0f, 15.0f, TEX_BILLBOARD2);
    // the third one, at the airport, on the apron beside the terminal
    {
        V3 ap = worldRunwayPoint(-140.0f, 320.0f);
        drawBillboard(ap.x, ap.z, 330.0f, 26.0f, 26.0f, 12.0f, TEX_BILLBOARD3);
    }
    glEndList();
}

// ---------------------------------------------------------------------
// windsocks: white with a red band, drawn every frame so they swing with the
// wind. They sit at both airfields, the stadium and the city square, and they
// are visible in every mode because worldDrawStatic runs in every mode.
// ---------------------------------------------------------------------
static void drawWindsock(float x, float z, float dirDeg, float t)
{
    const float poleH = 8.0f;
    setColor(0.80f, 0.80f, 0.78f);
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    drawCylinderRaw(0.11f, 0.09f, poleH, 6, false);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x, poleH, z);
    glRotatef(-dirDeg, 0, 1, 0);                 // point downwind
    glRotatef(sinf(t * 0.9f) * 5.0f, 0, 0, 1);   // and flutter a little
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);         // the strips run along -Z

    const float LEN[3] = { 1.7f, 1.5f, 1.4f };
    const float R[4] = { 0.62f, 0.50f, 0.38f, 0.22f };
    float y = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        if (i == 1) setColor(0.86f, 0.22f, 0.16f);   // the red band
        else setColor(0.97f, 0.97f, 0.95f);          // white, as asked
        glPushMatrix();
        glTranslatef(0.0f, y, 0.0f);
        drawCylinderRaw(R[i], R[i + 1], LEN[i], 8, false);
        glPopMatrix();
        y += LEN[i];
    }
    glPopMatrix();
}

void worldDrawWindsocks()
{
    float t = (float)glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    float dirDeg = rad2deg(gWindDirRad);

    // campus strip (both sides and the north end), the stadium and the city square
    static const float POS[5][3] = {
        { 1540.0f, 2040.0f, 0.0f },
        { 1668.0f, 2100.0f, 1.3f },
        { 1600.0f, 2306.0f, 2.1f },
        { 1620.0f, -1600.0f, 0.9f },
        { 40.0f,   1420.0f, 2.8f }
    };
    for (int i = 0; i < 5; i++) drawWindsock(POS[i][0], POS[i][1], dirDeg, t + POS[i][2]);

    // Osmani, beside the runway
    V3 ap = worldRunwayPoint(240.0f, 60.0f);
    drawWindsock(ap.x, ap.z, dirDeg, t + 1.9f);
}

void worldBuild()
{
    sSeed = 20241111u;
    sTallCount = 0;
    sAllCount = 0;
    sTreeCount = 0;

    // generated once, used by the river (and not a file, so it also works with
    // an empty assets folder)
    if (!sTexWater) sTexWater = makeWaterTexture(512);

    buildGround();
    buildRiver();
    buildRoads();
    buildRail();
    buildBuildings();
    buildTrees();
    buildHills();
    buildAirport();
    buildMazar();
    buildSIU();
    buildClockTower();
    buildPois();
    buildBillboards();
    buildShadows();
    buildCrowds();
    buildLandmarkDetails();
    buildLandmarkDetails2();
}

// ---------------------------------------------------------------------
// procedural asphalt texture for the runway and apron (stb_perlin noise,
// generated at start-up; this is also the fallback when no photo is loaded)
// ---------------------------------------------------------------------
static GLuint makeNoiseTexture(int size, float base, float contrast, float warm)
{
    unsigned char* px = (unsigned char*)malloc((size_t)size * size * 3);
    if (!px) return 0;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float u = (float)x / (float)size, v = (float)y / (float)size;
            float n1 = stb_perlin_noise3(u * 7.0f, v * 7.0f, 1.7f, 3, 3, 0);
            float n2 = stb_perlin_noise3(u * 41.0f, v * 41.0f, 5.3f, 0, 0, 0);
            float k = base + contrast * (0.65f * n1 + 0.35f * n2);
            if (k < 0.0f) k = 0.0f;
            if (k > 1.0f) k = 1.0f;
            unsigned char r = (unsigned char)(255.0f * k * (1.0f + warm));
            unsigned char g = (unsigned char)(255.0f * k);
            unsigned char b = (unsigned char)(255.0f * k * (1.0f - warm));
            if (r > 255) r = 255;
            int i = (y * size + x) * 3;
            px[i + 0] = r; px[i + 1] = g; px[i + 2] = b;
        }
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, size, size, GL_RGB, GL_UNSIGNED_BYTE, px);
    free(px);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

// River water. Poly Haven has no moving-water photograph that tiles well, so
// this one is generated: a teal ripple field with bright glints where the short
// waves catch the light. The texture coordinates are scrolled every frame in
// worldDrawStatic(), which is what makes the Surma flow.
static GLuint makeWaterTexture(int size)
{
    unsigned char* px = (unsigned char*)malloc((size_t)size * size * 3);
    if (!px) return 0;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float u = (float)x / (float)size, v = (float)y / (float)size;
            // three octaves: a broad swell, ripples, and fine structure. The
            // glints come only from the sharpest one, and only from its peaks,
            // so the surface reads as water instead of noise.
            float n1 = stb_perlin_noise3(u * 3.0f, v * 3.0f, 2.3f, 2, 2, 0);
            float n2 = stb_perlin_noise3(u * 8.0f, v * 8.0f, 7.1f, 0, 0, 0);
            float n3 = stb_perlin_noise3(u * 19.0f, v * 19.0f, 3.7f, 0, 0, 0);
            float k = 0.5f + 0.5f * (0.55f * n1 + 0.30f * n2 + 0.15f * n3);
            if (k < 0.0f) k = 0.0f;
            if (k > 1.0f) k = 1.0f;
            float r = 0.10f + 0.16f * k;
            float g = 0.30f + 0.28f * k;
            float b = 0.42f + 0.34f * k;
            float glint = (n3 > 0.30f) ? (n3 - 0.30f) : 0.0f;
            glint = glint * glint * 1.1f;
            r += glint * 0.75f; g += glint * 0.90f; b += glint * 0.90f;
            if (r > 1.0f) r = 1.0f; if (g > 1.0f) g = 1.0f; if (b > 1.0f) b = 1.0f;
            int i = (y * size + x) * 3;
            px[i + 0] = (unsigned char)(255.0f * r);
            px[i + 1] = (unsigned char)(255.0f * g);
            px[i + 2] = (unsigned char)(255.0f * b);
        }
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, size, size, GL_RGB, GL_UNSIGNED_BYTE, px);
    free(px);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

// a quad in the airport's own frame, with texture coordinates
static void texturedQuadLocal(float hx, float hz, float y, float uRep, float vRep)
{
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);        glVertex3f(-hx, y, -hz);
    glTexCoord2f(uRep, 0);     glVertex3f(hx, y, -hz);
    glTexCoord2f(uRep, vRep);  glVertex3f(hx, y, hz);
    glTexCoord2f(0, vRep);     glVertex3f(-hx, y, hz);
    glEnd();
}

static void drawTexturedApron()
{
    if (!sTexAsphalt)
    {
        sTexAsphalt = makeNoiseTexture(256, 0.52f, 0.30f, 0.01f);
        sTexGrass = makeNoiseTexture(256, 0.55f, 0.34f, -0.06f);
    }
    if (!sTexAsphalt) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, sTexAsphalt);
    // modulation with the material colour keeps it in the same palette
    setColor(0.62f, 0.62f, 0.66f);

    // runway surface (in runway-local space)
    {
        V3 c = worldRunwayPoint(0, 0);
        glPushMatrix();
        glTranslatef(c.x, 0, c.z);
        glRotatef(-(float)RWY_HEADING_DEG, 0, 1, 0);
        texturedQuadLocal(22.5f, RWY_HALF, 0.075f, 2.0f, 120.0f);
        glPopMatrix();
    }
    // apron
    {
        V3 ap = worldApronCenter();
        glPushMatrix();
        glTranslatef(ap.x, 0, ap.z);
        glRotatef(-(float)RWY_HEADING_DEG, 0, 1, 0);
        texturedQuadLocal(95.0f, 170.0f, 0.075f, 6.0f, 11.0f);
        glPopMatrix();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// --nochunk: draw every city chunk regardless of distance. Used only by the
// benchmark, to measure the gain from distance culling on the same build.
bool gNoChunk = false;
bool gNoLod = false;       // --nolod: always draw the detailed chunk list

// Q: the shadow pass is the single most expensive blended pass in the scene, so
// it gets its own switch. Turning it off roughly doubles the frame rate on weak
// integrated graphics, and the scene still reads correctly without it.
bool gDrawShadows = true;

// --hide=... benchmarking switches: each removes one display list so its cost
// can be measured on its own. Not exposed in the game.
bool gHideTrees = false, gHideCity = false, gHideDetails = false, gHidePois = false;

void worldDrawStatic()
{
    glCallList(L_GROUND);
    glCallList(L_HILLS);
    {
        // scroll the water texture so the Surma flows; the matrix only affects
        // the river list, which binds the water texture before its water strip
        float wt = (float)glutGet(GLUT_ELAPSED_TIME) * 0.001f;
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(wt * 0.07f, wt * 0.025f, 0.0f);   // the flow itself
        glScalef(0.30f, 0.30f, 1.0f);                  // bigger waves than the raw tiling
        glMatrixMode(GL_MODELVIEW);
        glCallList(L_RIVER);
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }
    glCallList(L_ROADS);
    glCallList(L_RAIL);
    if (gDrawShadows && sCamY < 220.0f) glCallList(L_SHADOWS);
    if (!gHideCity)
    {
        const float R = 2200.0f;   // matches GL_FOG_START in game.cpp
        const float LOD_DIST = 900.0f;   // beyond this, buildings lose their detail
        for (int ci = 0; ci < CITY_CHUNKS; ci++)
        {
            for (int cj = 0; cj < CITY_CHUNKS; cj++)
            {
                float px = CITY_ORIGIN + (ci + 0.5f) * CITY_CELL;
                float pz = CITY_ORIGIN + (cj + 0.5f) * CITY_CELL;
                float dx = px - sCamX, dz = pz - sCamZ;
                float d2 = dx * dx + dz * dz;
                if (!gNoChunk && d2 > R * R) continue;
                // near chunks get the full detail, far ones the cheap boxes
                glCallList((!gNoLod && d2 > LOD_DIST * LOD_DIST)
                           ? sChunkSimple[ci * CITY_CHUNKS + cj]
                           : sChunkList[ci * CITY_CHUNKS + cj]);
                glCallList(sChunkRoof[ci * CITY_CHUNKS + cj]);
            }
        }
        glCallList(L_BUILDINGS);
    }
    if (!gHideTrees) glCallList(L_TREES);
    glCallList(L_AIRPORT);
    drawTexturedApron();
    glCallList(L_MAZAR);
    glCallList(L_SIU);
    glCallList(L_CLOCK);
    glCallList(L_BILLBOARD);
    if (!gHidePois) glCallList(L_POIS);
    if (!gHideDetails)
    {
        glCallList(L_DETAILS);
        glCallList(L_DETAILS2);
        glCallList(L_CROWD);
    }
}
// ---------------------------------------------------------------------
// scene statistics for the thesis report (--stats)
// ---------------------------------------------------------------------
void worldGetStats(WorldStats& s)
{
    s.buildings = sBuildingTotal;
    s.trees = sTreeTotal;
    s.chunks = CITY_CHUNKS * CITY_CHUNKS;
    s.landmarks = gLandmarkCount;
    s.displayLists = s.chunks * 2 + 16;   // full + LOD city lists, plus the main lists
    s.roadSegments = 0;
    for (int i = 0; i < gRoadCount; i++) s.roadSegments += worldRoadPointCount(i) - 1;
}