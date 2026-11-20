// traffic.cpp - vehicles on the roads (left-hand traffic, like Bangladesh),
// boats on the Surma, a train, birds and AI aircraft in the pattern
#include "traffic.h"
#include "aircraft.h"
#include "glutil.h"
#include "world.h"
#include <vector>
#include <cmath>

// =====================================================================
// road loops for the vehicles (closed polylines, drawn on the real roads)
// =====================================================================
struct Loop { std::vector<V3> pts; float length; };

static std::vector<Loop> sLoops;

static Loop makeLoop(const float* xy, int n)
{
    Loop L;
    L.length = 0;
    for (int i = 0; i < n; i++) L.pts.push_back(v3(xy[i * 2], 0, xy[i * 2 + 1]));
    L.pts.push_back(L.pts[0]);
    for (size_t i = 0; i + 1 < L.pts.size(); i++)
    {
        V3 d = L.pts[i + 1] - L.pts[i];
        L.length += len(d);
    }
    return L;
}

static void pointOnLoop(const Loop& L, float s, V3& pos, V3& dir)
{
    while (s >= L.length) s -= L.length;
    while (s < 0) s += L.length;
    for (size_t i = 0; i + 1 < L.pts.size(); i++)
    {
        V3 d = L.pts[i + 1] - L.pts[i];
        float seg = len(d);
        if (s <= seg && seg > 0.01f)
        {
            dir = d * (1.0f / seg);
            pos = L.pts[i] + dir * s;
            return;
        }
        s -= seg;
    }
    pos = L.pts[0];
    dir = v3(0, 0, -1);
}

// =====================================================================
// vehicle drawing (model faces -Z)
// =====================================================================
static void drawWheel(float x, float y, float z, float r)
{
    setColor(0.10f, 0.10f, 0.11f);
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(90.0f, 0, 0, 1);
    drawCylinderRaw(r, r, 0.16f, 6, true);
    glPopMatrix();
}

static void drawRickshaw(float r, float g, float b)
{
    drawWheel(-0.52f, 0.33f, 0.60f, 0.33f);
    drawWheel(0.52f, 0.33f, 0.60f, 0.33f);
    drawWheel(0.0f, 0.30f, -0.95f, 0.30f);
    setColor(0.85f, 0.85f, 0.85f);
    drawBoxAt(0, 0.85f, 0.05f, 1.10f, 0.75f, 2.20f);
    setColor(r, g, b);
    drawBoxAt(0, 1.60f, 0.45f, 1.18f, 1.00f, 1.25f);      // canopy
    setColor(0.95f, 0.80f, 0.30f);
    drawBoxAt(0, 1.15f, -0.85f, 0.90f, 0.55f, 0.30f);     // front trim
    setColor(0.25f, 0.20f, 0.18f);
    drawBoxAt(0, 1.18f, -0.30f, 0.55f, 0.80f, 0.55f);     // driver
}

static void drawCNG(float r, float g, float b)
{
    drawWheel(-0.55f, 0.32f, 0.78f, 0.32f);
    drawWheel(0.55f, 0.32f, 0.78f, 0.32f);
    drawWheel(0.0f, 0.30f, -0.95f, 0.30f);
    setColor(r, g, b);
    drawBoxAt(0, 1.00f, 0, 1.35f, 1.35f, 2.40f);
    setColor(0.12f, 0.12f, 0.14f);
    drawBoxAt(0, 1.80f, 0.10f, 1.30f, 0.25f, 2.20f);      // roof
    setColor(0.18f, 0.22f, 0.28f);
    drawBoxAt(0, 1.20f, -1.18f, 1.15f, 0.70f, 0.10f);     // windshield
}

static void drawCar(float r, float g, float b)
{
    drawWheel(-0.85f, 0.33f, -1.30f, 0.33f);
    drawWheel(0.85f, 0.33f, -1.30f, 0.33f);
    drawWheel(-0.85f, 0.33f, 1.35f, 0.33f);
    drawWheel(0.85f, 0.33f, 1.35f, 0.33f);
    setColor(r, g, b);
    drawBoxAt(0, 0.72f, 0, 1.80f, 0.72f, 4.20f);
    drawBoxAt(0, 1.32f, 0.25f, 1.66f, 0.62f, 2.20f);      // cabin
    setColor(0.18f, 0.22f, 0.28f);
    drawBoxAt(0, 1.32f, -0.88f, 1.58f, 0.50f, 0.08f);
}

static void drawBus(float r, float g, float b)
{
    drawWheel(-1.10f, 0.45f, -3.20f, 0.45f);
    drawWheel(1.10f, 0.45f, -3.20f, 0.45f);
    drawWheel(-1.10f, 0.45f, 3.10f, 0.45f);
    drawWheel(1.10f, 0.45f, 3.10f, 0.45f);
    setColor(r, g, b);
    drawBoxAt(0, 1.75f, 0, 2.50f, 2.55f, 10.0f);
    setColor(0.16f, 0.20f, 0.26f);
    drawBoxAt(0, 2.40f, 0, 2.56f, 0.85f, 9.40f);          // window band
    drawBoxAt(0, 1.90f, -5.02f, 2.20f, 1.30f, 0.10f);     // windshield
    setColor(0.90f, 0.90f, 0.88f);
    drawBoxAt(0, 3.10f, 0, 2.45f, 0.12f, 9.9f);           // roof
}

static void drawTruck(float r, float g, float b)
{
    drawWheel(-1.05f, 0.42f, -2.60f, 0.42f);
    drawWheel(1.05f, 0.42f, -2.60f, 0.42f);
    drawWheel(-1.05f, 0.42f, 1.40f, 0.42f);
    drawWheel(1.05f, 0.42f, 1.40f, 0.42f);
    drawWheel(-1.05f, 0.42f, 2.60f, 0.42f);
    drawWheel(1.05f, 0.42f, 2.60f, 0.42f);
    setColor(r, g, b);
    drawBoxAt(0, 1.40f, -2.30f, 2.30f, 1.90f, 2.20f);     // cab
    setColor(0.80f, 0.80f, 0.78f);
    drawBoxAt(0, 2.05f, 0.90f, 2.40f, 2.40f, 5.20f);      // cargo box
    setColor(0.18f, 0.22f, 0.28f);
    drawBoxAt(0, 1.65f, -3.38f, 2.00f, 1.00f, 0.10f);
}

// =====================================================================
// the moving world
// =====================================================================
struct Veh
{
    int loop;
    float s;
    float speed;
    int type;         // 0 rickshaw, 1 CNG, 2 car, 3 bus, 4 truck
    float r, g, b;
    V3 pos, dir;
    float yaw;
};

struct Boat
{
    float s;          // arc position on the river
    float speed;
    float lateral;
    int type;         // 0 small, 1 launch
    float phase;
    V3 pos, dir;
};

static std::vector<Veh> sVehs;
static std::vector<Veh> sParked;
static std::vector<Boat> sBoats;

static float sTrainX = -5600.0f;
static double sTime = 0;

// AI planes
struct AiPlane
{
    V3 pos;
    float yaw, roll, alt;
    int wp;
    float speed;
    float r, g, b;
};
static AiPlane sAi[2];
static V3 sWps[2][4];      // waypoints per plane, 4 legs

// birds: five kinds, in flocks that circle the river, the city and the campus
enum BirdKind { BIRD_CROW = 0, BIRD_EGRET, BIRD_PIGEON, BIRD_GOOSE, BIRD_KITE, BIRD_KINDS };

struct Bird { V3 pos; float phase; float yaw; };

struct BirdKindDef
{
    float scale;              // wingspan is 2.1 m times this
    float r, g, b;
    float flapRate, flapAmp;
    int formation;            // 0 = loose ring, 1 = V of geese
};

static const BirdKindDef BIRDS[BIRD_KINDS] = {
    // crows: black, quick wing beats, over the rooftops (3.2 m span)
    { 1.50f, 0.10f, 0.10f, 0.12f, 12.0f, 0.45f, 0 },
    // egrets: white, slow beats, low over the water (5.5 m span)
    { 2.60f, 0.97f, 0.97f, 0.95f,  5.0f, 0.28f, 0 },
    // pigeons: grey, fastest beats, over the bazaar (2.3 m span)
    { 1.10f, 0.62f, 0.64f, 0.68f, 16.0f, 0.50f, 0 },
    // geese: brown, in a V, following the river (4.6 m span)
    { 2.20f, 0.46f, 0.38f, 0.30f,  8.0f, 0.38f, 1 },
    // kites: dark, mostly soaring, high above the town (6.7 m span)
    { 3.20f, 0.27f, 0.21f, 0.16f,  2.5f, 0.12f, 0 }
};

static std::vector<Bird> sBirds;
static const int MAX_FLOCKS = 16;
static float sBirdR[MAX_FLOCKS], sBirdA[MAX_FLOCKS], sBirdY[MAX_FLOCKS], sBirdSpin[MAX_FLOCKS];
static V3 sBirdC[MAX_FLOCKS];
static int sFlockCount[MAX_FLOCKS];
static unsigned char sBirdKind[MAX_FLOCKS];
static int sFlockN = 0;

// pedestrians on the pavements
struct Walker
{
    float pos;          // position along the pavement
    float side;         // which side of the road
    float dir;          // +1 or -1
    float speed;
    float phase;
    float r, g, b;
    bool fem;
    int axis;           // 0 = along Z, 1 = along X
    float minPos, maxPos;
};
static Walker sWalkers[24];

// traffic driving across Keane Bridge
struct BridgeCar
{
    float z;
    float speed;
    float dir;
    float lane;
    int type;
    float r, g, b;
};
static BridgeCar sBridge[7];


// ---------------------------------------------------------------------
void trafficInit()
{
    sLoops.clear();
    // centre square
    static const float L1[] = { 0, 0, 0, 1200, -1000, 1200, -1000, 0 };
    // big east loop
    static const float L2[] = { 0, 0, 0, 1200, 2600, 1200, 2600, 0 };
    // west loop
    static const float L3[] = { -1000, 0, -1000, 1200, -2100, 1200, -2100, 0 };
    // south loop (over the level crossings)
    static const float L4[] = { 0, 1200, 0, 2900, 2600, 2900, 2600, 1200 };
    sLoops.push_back(makeLoop(L1, 4));
    sLoops.push_back(makeLoop(L2, 4));
    sLoops.push_back(makeLoop(L3, 4));
    sLoops.push_back(makeLoop(L4, 4));

    // airport apron loop, aligned with the runway
    {
        V3 ap = worldApronCenter();
        V3 d = worldRunwayDir(), r = worldRunwayRight();
        float corners[4][2] = { { -150, -75 }, { 150, -75 }, { 150, 75 }, { -150, 75 } };
        float xy[8];
        for (int i = 0; i < 4; i++)
        {
            V3 p = ap + d * corners[i][0] + r * corners[i][1];
            xy[i * 2] = p.x;
            xy[i * 2 + 1] = p.z;
        }
        sLoops.push_back(makeLoop(xy, 4));
    }

    // distribute vehicles
    sVehs.clear();
    unsigned seed = 4242u;
    int counts[5] = { 12, 16, 9, 20, 4 };   // vehicles per loop
    for (int li = 0; li < 5; li++)
    {
        for (int i = 0; i < counts[li]; i++)
        {
            Veh v;
            v.loop = li;
            v.s = (float)i * (sLoops[li].length / counts[li]);
            float roll = frand01(seed);
            if (roll < 0.30f) v.type = 0;
            else if (roll < 0.55f) v.type = 1;
            else if (roll < 0.80f) v.type = 2;
            else if (roll < 0.92f) v.type = 3;
            else v.type = 4;
            switch (v.type)
            {
                case 0: v.speed = 3.0f + frand01(seed) * 2.0f; break;
                case 1: v.speed = 6.0f + frand01(seed) * 2.0f; break;
                case 2: v.speed = 10.0f + frand01(seed) * 5.0f; break;
                case 3: v.speed = 8.0f + frand01(seed) * 2.0f; break;
                default: v.speed = 8.0f + frand01(seed) * 3.0f; break;
            }
            // colours
            if (v.type == 0)   // rickshaw: bright canopy
            {
                static const float RC[5][3] = { { 0.80f, 0.20f, 0.16f }, { 0.15f, 0.40f, 0.65f }, { 0.20f, 0.55f, 0.25f }, { 0.85f, 0.55f, 0.10f }, { 0.55f, 0.20f, 0.50f } };
                int c = (int)(frand01(seed) * 5);
                v.r = RC[c][0]; v.g = RC[c][1]; v.b = RC[c][2];
            }
            else if (v.type == 1) { v.r = 0.12f; v.g = 0.55f; v.b = 0.25f; }
            else if (v.type == 2)
            {
                static const float CC[5][3] = { { 0.85f, 0.85f, 0.86f }, { 0.30f, 0.32f, 0.36f }, { 0.70f, 0.15f, 0.14f }, { 0.20f, 0.35f, 0.62f }, { 0.60f, 0.62f, 0.64f } };
                int c = (int)(frand01(seed) * 5);
                v.r = CC[c][0]; v.g = CC[c][1]; v.b = CC[c][2];
            }
            else if (v.type == 3)
            {
                if (frand01(seed) < 0.5f) { v.r = 0.15f; v.g = 0.45f; v.b = 0.25f; }
                else { v.r = 0.70f; v.g = 0.20f; v.b = 0.18f; }
            }
            else { v.r = 0.25f; v.g = 0.35f; v.b = 0.60f; }
            v.pos = v3(0, 0, 0);
            v.dir = v3(0, 0, -1);
            v.yaw = 0;
            sVehs.push_back(v);
        }
    }

    // parked vehicles for street life
    sParked.clear();
    for (int i = 0; i < 8; i++)
    {
        Veh v;
        v.type = 0; v.speed = 0;
        static const float RC[4][3] = { { 0.80f, 0.20f, 0.16f }, { 0.20f, 0.55f, 0.25f }, { 0.85f, 0.55f, 0.10f }, { 0.15f, 0.40f, 0.65f } };
        int c = (int)(frand01(seed) * 4);
        v.r = RC[c][0]; v.g = RC[c][1]; v.b = RC[c][2];
        float z = -320.0f + i * 85.0f;
        if (i < 5) v.pos = v3(10.6f, 0, z);
        else { v.pos = v3(-10.6f, 0, z - 420.0f); }
        v.yaw = 0;   // parked facing north / south
        if (i >= 5) v.yaw = PI_F;
        sParked.push_back(v);
    }
    for (int i = 0; i < 2; i++)
    {
        Veh v;
        v.type = 1; v.speed = 0;
        v.r = 0.12f; v.g = 0.55f; v.b = 0.25f;
        v.pos = v3(300.0f + i * 60.0f, 0, 3032.0f);
        v.yaw = PI_F * 0.5f;
        sParked.push_back(v);
    }
    for (int i = 0; i < 2; i++)
    {
        Veh v;
        v.type = 2; v.speed = 0;
        v.r = 0.85f; v.g = 0.85f; v.b = 0.86f;
        v.pos = v3(1830.0f + i * 55.0f, 0, 1505.0f);
        v.yaw = PI_F * 0.5f;
        sParked.push_back(v);
    }

    // pedestrians on the pavements
    {
        unsigned wseed = 909u;
        for (int i = 0; i < 24; i++)
        {
            Walker& w = sWalkers[i];
            w.axis = (i < 14) ? 0 : 1;
            w.side = (frand01(wseed) < 0.5f) ? 1.0f : -1.0f;
            w.dir = (frand01(wseed) < 0.5f) ? 1.0f : -1.0f;
            w.speed = 1.0f + frand01(wseed) * 0.8f;
            w.phase = frand01(wseed) * 6.2831853f;
            w.r = 0.25f + frand01(wseed) * 0.60f;
            w.g = 0.25f + frand01(wseed) * 0.60f;
            w.b = 0.25f + frand01(wseed) * 0.60f;
            w.fem = frand01(wseed) < 0.45f;
            if (w.axis == 0)
            {
                w.minPos = -400.0f; w.maxPos = 1800.0f;
                w.pos = w.minPos + frand01(wseed) * (w.maxPos - w.minPos);
            }
            else
            {
                w.minPos = -600.0f; w.maxPos = 1600.0f;
                w.pos = w.minPos + frand01(wseed) * (w.maxPos - w.minPos);
            }
        }
    }

    // traffic driving across Keane Bridge
    {
        unsigned bseed = 3131u;
        for (int i = 0; i < 7; i++)
        {
            BridgeCar& b = sBridge[i];
            b.z = -40.0f + i * 45.0f;
            b.dir = (frand01(bseed) < 0.5f) ? 1.0f : -1.0f;
            b.speed = 6.0f + frand01(bseed) * 9.0f;
            b.lane = (b.dir > 0.0f) ? 3.6f : -3.6f;
            float roll = frand01(bseed);
            b.type = (roll < 0.35f) ? 0 : (roll < 0.65f) ? 2 : (roll < 0.85f) ? 3 : 4;
            if (b.type == 0) { b.r = 0.80f; b.g = 0.20f; b.b = 0.16f; }
            else if (b.type == 2) { b.r = 0.85f; b.g = 0.85f; b.b = 0.87f; }
            else if (b.type == 3) { b.r = 0.15f; b.g = 0.45f; b.b = 0.25f; }
            else { b.r = 0.30f; b.g = 0.35f; b.b = 0.60f; }
        }
    }

    // boats on the Surma
    sBoats.clear();
    for (int i = 0; i < 7; i++)
    {
        Boat b;
        b.s = worldRiverLength() * (float)i / 7.0f + 300.0f;
        b.speed = (i % 2 == 0) ? 2.2f : -1.6f;
        b.lateral = (i % 2 == 0) ? 30.0f : -35.0f;
        b.type = (i == 2 || i == 5) ? 1 : 0;
        b.phase = frand01(seed) * 6.28f;
        b.pos = v3(0, 0, 0); b.dir = v3(0, 0, -1);
        sBoats.push_back(b);
    }

    // AI planes: traffic patterns around the airport
    {
        V3 c = worldAirportCenter();
        V3 d = worldRunwayDir(), r = worldRunwayRight();
        for (int plane = 0; plane < 2; plane++)
        {
            float latOff = (plane == 0) ? 700.0f : -700.0f;
            V3 w[4];
            w[0] = c + d * 1150.0f + r * latOff;
            w[1] = c - d * 1150.0f + r * latOff;
            w[2] = c - d * 1150.0f - r * latOff;
            w[3] = c + d * 1150.0f - r * latOff;
            int start = (plane == 0) ? 0 : 2;
            for (int i = 0; i < 4; i++) sWps[plane][i] = w[(start + i) % 4];
            sAi[plane].pos = sWps[plane][0];
            sAi[plane].alt = (plane == 0) ? 320.0f : 420.0f;
            sAi[plane].wp = 1;
            sAi[plane].speed = 48.0f;
            sAi[plane].roll = 0;
            sAi[plane].yaw = yawFromDir(norm(sWps[plane][1] - sWps[plane][0]));
            if (plane == 0) { sAi[plane].r = 0.85f; sAi[plane].g = 0.25f; sAi[plane].b = 0.20f; }
            else { sAi[plane].r = 0.90f; sAi[plane].g = 0.72f; sAi[plane].b = 0.15f; }
        }
    }

    // birds: ten flocks, most of them low over the water and the city where a
    // pilot actually sees them
    sBirds.clear();
    {
        struct FlockDef { float x, z, radius, y; unsigned char kind; int count; };
        // placed in the corridors a pilot actually flies through: the campus
        // strip, the river and the bridge, the bazaar, the Mazar and the airport
        static const FlockDef FL[16] = {
            {   350.0f,   500.0f,  80.0f,  26.0f, BIRD_EGRET,  7 },   // egrets over the Surma
            {     0.0f,   105.0f,  70.0f,  34.0f, BIRD_EGRET,  6 },   // over Keane Bridge
            {  1750.0f,  1700.0f,  90.0f,  40.0f, BIRD_CROW,   8 },   // crows over the campus
            {  -400.0f,   500.0f,  70.0f,  58.0f, BIRD_CROW,   7 },   // crows over the city
            {   900.0f,  1200.0f,  60.0f,  50.0f, BIRD_PIGEON, 9 },   // pigeons over Zindabazar
            {   300.0f,   400.0f,  55.0f,  44.0f, BIRD_PIGEON, 8 },
            {  1620.0f,  2050.0f, 120.0f,  48.0f, BIRD_GOOSE,  9 },   // geese V over the strip
            {  1500.0f,  1300.0f, 130.0f,  68.0f, BIRD_GOOSE,  8 },   // and along the river
            { -1750.0f, -1150.0f, 100.0f,  42.0f, BIRD_EGRET,  5 },   // egrets over the Mazar
            {   200.0f,  -400.0f, 160.0f, 165.0f, BIRD_KITE,   3 },   // kites soaring above
            // more birds, as asked: two big flights over the strip and the runway,
            // a mob of crows over the bazaar and a second line of geese up river
            {  1600.0f,  2250.0f, 150.0f,  35.0f, BIRD_GOOSE, 12 },
            { -2600.0f, -3200.0f, 130.0f,  28.0f, BIRD_CROW,  10 },
            {   600.0f,   900.0f,  75.0f,  52.0f, BIRD_PIGEON, 12 },
            {  -900.0f,  -600.0f,  95.0f,  60.0f, BIRD_EGRET,  9 },
            {  2100.0f,   700.0f, 110.0f,  46.0f, BIRD_GOOSE,  10 },
            {  -400.0f,  2100.0f, 120.0f,  70.0f, BIRD_KITE,   4 }
        };
        sFlockN = 16;
        for (int f = 0; f < sFlockN; f++)
        {
            sBirdC[f] = v3(FL[f].x, 0.0f, FL[f].z);
            sBirdR[f] = FL[f].radius;
            sBirdY[f] = FL[f].y;
            sBirdKind[f] = FL[f].kind;
            // kites and egrets circle slowly, pigeons hurry
            sBirdSpin[f] = (FL[f].kind == BIRD_PIGEON) ? 0.34f : ((FL[f].kind == BIRD_KITE) ? 0.10f : 0.20f);
            sBirdA[f] = frand01(seed) * 6.28f;
            sFlockCount[f] = FL[f].count;
            for (int b = 0; b < FL[f].count; b++)
            {
                Bird bird;
                bird.phase = frand01(seed) * 6.28f;
                bird.yaw = 0.0f;
                bird.pos = v3(0, 0, 0);
                sBirds.push_back(bird);
            }
        }
    }
    sTime = 0;
}

// ---------------------------------------------------------------------
static bool vehTypeWide(int type);

static void vehicleShadow(float x, float z, float yawRad, float w, float d, float alpha)
{
    glPushMatrix();
    glTranslatef(x - 0.8f, 0.19f, z - 0.7f);
    glRotatef(-rad2deg(yawRad), 0, 1, 0);
    setColorA(0.05f, 0.06f, 0.05f, alpha);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-w, 0, d); glVertex3f(w, 0, d); glVertex3f(w, 0, -d); glVertex3f(-w, 0, -d);
    glEnd();
    glPopMatrix();
}

void trafficUpdate(float dt, double timeSec)
{
    sTime = timeSec;

    // vehicles
    for (size_t i = 0; i < sVehs.size(); i++)
    {
        Veh& v = sVehs[i];
        v.s += v.speed * dt;
        V3 p, d;
        pointOnLoop(sLoops[v.loop], v.s, p, d);
        V3 left = v3(d.z, 0, -d.x);            // keep left, like in Bangladesh
        float lane = 3.6f;
        if (vehTypeWide(v.type)) lane = 4.4f;
        v.pos = p + left * lane;
        v.dir = d;
        v.yaw = yawFromDir(d);
    }

    // boats
    for (size_t i = 0; i < sBoats.size(); i++)
    {
        Boat& b = sBoats[i];
        b.s += b.speed * dt;
        V3 p, d;
        worldRiverFrameAtS(b.s, p, d);
        if (b.speed < 0) d = -d;
        V3 right = v3(-d.z, 0, d.x);
        b.pos = p + right * b.lateral;
        b.pos.y = -0.15f + sinf((float)timeSec * 2.0f + b.phase) * 0.12f;
        b.dir = d;
    }

    // train
    sTrainX += 16.0f * dt;
    if (sTrainX > 5600.0f) sTrainX = -5600.0f;

    // AI planes
    for (int plane = 0; plane < 2; plane++)
    {
        AiPlane& ap = sAi[plane];
        V3 target = sWps[plane][ap.wp];
        V3 to = target - ap.pos;
        float distH = sqrtf(to.x * to.x + to.z * to.z);
        if (distH < 260.0f)
        {
            ap.wp = (ap.wp + 1) % 4;
            target = sWps[plane][ap.wp];
            to = target - ap.pos;
        }
        float desired = yawFromDir(norm(to));
        float dA = wrapPi(desired - ap.yaw);
        float turn = clampf(dA * 1.5f, -0.30f, 0.30f);
        ap.yaw = wrapPi(ap.yaw + turn * dt);
        float rollTarget = clampf(dA * 2.0f, -0.55f, 0.55f);
        ap.roll += (rollTarget - ap.roll) * fminf(1.0f, 2.5f * dt);
        V3 dir = v3(sinf(ap.yaw), 0, -cosf(ap.yaw));
        ap.pos += dir * (ap.speed * dt);
        ap.pos.y += (ap.alt - ap.pos.y) * fminf(1.0f, 0.8f * dt);
    }

    // pedestrians walk along the pavements
    for (int i = 0; i < 24; i++)
    {
        Walker& w = sWalkers[i];
        w.pos += w.dir * w.speed * dt;
        if (w.pos > w.maxPos) w.pos = w.minPos;
        if (w.pos < w.minPos) w.pos = w.maxPos;
    }

    // traffic crossing Keane Bridge
    for (int i = 0; i < 7; i++)
    {
        BridgeCar& b = sBridge[i];
        b.z += b.dir * b.speed * dt;
        if (b.z > 262.0f) b.z = -42.0f;
        if (b.z < -42.0f) b.z = 262.0f;
    }

    // birds circle their flock centre, each in its own track
    for (int f = 0; f < sFlockN; f++) sBirdA[f] += sBirdSpin[f] * dt;
    {
        int idx = 0;
        for (int f = 0; f < sFlockN; f++)
        {
            const BirdKindDef& kd = BIRDS[sBirdKind[f]];
            for (int k = 0; k < sFlockCount[f] && idx < (int)sBirds.size(); k++, idx++)
            {
                Bird& b = sBirds[idx];
                float a = sBirdA[f] + b.phase * 0.30f + (float)k * 0.11f;
                float r = sBirdR[f] + (float)k * 3.0f;
                if (kd.formation == 1)
                {
                    // geese: a loose V behind the leader, alternating sides
                    float side = ((k & 1) ? 1.0f : -1.0f) * (float)((k + 1) / 2);
                    a -= (float)(k / 2) * 0.040f;
                    r += side * 9.0f;
                }
                float y = sBirdY[f] + sinf((float)timeSec * 0.6f + b.phase) * 3.5f + (float)(k % 3) * 2.0f;
                b.pos = v3(sBirdC[f].x + cosf(a) * r, y, sBirdC[f].z + sinf(a) * r);
                b.yaw = a + 1.5707963f;      // face along the circle
            }
        }
    }
}

static bool vehTypeWide(int type) { return type == 3 || type == 4; }

// ---------------------------------------------------------------------
void trafficDraw()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    static const float VW[5] = { 1.4f, 1.6f, 2.3f, 3.0f, 2.9f };
    static const float VD[5] = { 3.2f, 3.2f, 5.2f, 11.0f, 9.2f };

    // vehicles
    for (size_t i = 0; i < sVehs.size(); i++)
    {
        Veh& v = sVehs[i];
        vehicleShadow(v.pos.x, v.pos.z, v.yaw, VW[v.type] * 0.5f + 0.2f, VD[v.type] * 0.5f + 0.3f, 0.18f);
        glPushMatrix();
        glTranslatef(v.pos.x, 0, v.pos.z);
        glRotatef(-rad2deg(v.yaw), 0, 1, 0);
        switch (v.type)
        {
            case 0: drawRickshaw(v.r, v.g, v.b); break;
            case 1: drawCNG(v.r, v.g, v.b); break;
            case 2: drawCar(v.r, v.g, v.b); break;
            case 3: drawBus(v.r, v.g, v.b); break;
            default: drawTruck(v.r, v.g, v.b); break;
        }
        glPopMatrix();
    }
    for (size_t i = 0; i < sParked.size(); i++)
    {
        Veh& v = sParked[i];
        vehicleShadow(v.pos.x, v.pos.z, v.yaw, VW[v.type] * 0.5f + 0.2f, VD[v.type] * 0.5f + 0.3f, 0.18f);
        glPushMatrix();
        glTranslatef(v.pos.x, 0, v.pos.z);
        glRotatef(-rad2deg(v.yaw), 0, 1, 0);
        switch (v.type)
        {
            case 0: drawRickshaw(v.r, v.g, v.b); break;
            case 1: drawCNG(v.r, v.g, v.b); break;
            default: drawCar(v.r, v.g, v.b); break;
        }
        glPopMatrix();
    }

    // boats
    for (size_t i = 0; i < sBoats.size(); i++)
    {
        Boat& b = sBoats[i];
        glPushMatrix();
        glTranslatef(b.pos.x, b.pos.y, b.pos.z);
        glRotatef(-rad2deg(yawFromDir(b.dir)), 0, 1, 0);
        if (b.type == 0)
        {
            setColor(0.55f, 0.42f, 0.28f);
            drawBoxAt(0, 0.35f, 0, 1.6f, 0.55f, 5.6f);
            setColor(0.35f, 0.45f, 0.55f);
            drawBoxAt(0, 0.95f, 0.9f, 1.7f, 0.5f, 2.0f);   // tarp
            setColor(0.30f, 0.30f, 0.32f);
            drawBoxAt(0, 0.75f, -2.35f, 0.5f, 0.7f, 0.5f); // ferryman
        }
        else
        {
            setColor(0.75f, 0.72f, 0.62f);
            drawBoxAt(0, 0.55f, 1.4f, 2.2f, 1.1f, 9.0f);   // hull
            drawBoxAt(0, 0.55f, -4.4f, 2.2f, 1.1f, 2.6f);
            setColor(0.30f, 0.42f, 0.55f);
            drawBoxAt(0, 1.9f, 1.4f, 2.1f, 1.5f, 6.4f);    // deck cabin
            setColor(0.80f, 0.78f, 0.70f);
            drawBoxAt(0, 2.75f, 1.4f, 2.2f, 0.2f, 6.6f);   // roof
        }
        glPopMatrix();
    }

    // train
    {
        for (int i = 0; i < 5; i++)
        {
            float x = sTrainX - i * 24.0f;
            glPushMatrix();
            glTranslatef(x, 0, gRailZ);
            glRotatef(-90.0f, 0, 1, 0);
            if (i == 0)
            {
                setColor(0.20f, 0.50f, 0.30f);
                drawBoxAt(0, 1.9f, 0, 3.0f, 2.6f, 16.0f);
                setColor(0.90f, 0.80f, 0.25f);
                drawBoxAt(0, 2.4f, -7.9f, 2.6f, 0.9f, 0.3f);    // front stripe
                setColor(0.15f, 0.20f, 0.28f);
                drawBoxAt(0, 2.6f, -8.0f, 2.2f, 0.9f, 0.2f);    // windshield
            }
            else
            {
                setColor(0.22f, 0.48f, 0.30f);
                drawBoxAt(0, 1.8f, 0, 2.8f, 2.5f, 20.0f);
                setColor(0.88f, 0.86f, 0.80f);
                drawBoxAt(0, 2.4f, 0, 2.86f, 0.7f, 18.6f);      // window band
            }
            setColor(0.16f, 0.16f, 0.18f);
            drawBoxAt(0, 0.35f, -4.5f, 2.2f, 0.7f, 1.8f);
            drawBoxAt(0, 0.35f, 4.5f, 2.2f, 0.7f, 1.8f);
            glPopMatrix();
        }
    }

    // level crossing booms and lamps
    for (int c = 0; c < 2; c++)
    {
        float cx = gCrossX[c];
        float dist = fabsf(sTrainX - cx);
        bool down = dist < 750.0f;
        float angle = down ? 0.0f : 78.0f;
        bool lampOn = down && (fmod((float)sTime, 0.7f) < 0.35f);
        // north approach boom (pivot west of road, opens to the east)
        glPushMatrix();
        glTranslatef(cx - 13.6f, 1.15f, 2886.0f);
        glRotatef(angle, 0, 0, 1);
        setColor(0.90f, 0.90f, 0.88f);
        drawBoxAt(6.6f, 0, 0, 6.6f, 0.28f, 0.28f);
        setColor(0.75f, 0.15f, 0.12f);
        drawBoxAt(3.3f, 0, 0, 3.2f, 0.30f, 0.30f);
        drawBoxAt(9.9f, 0, 0, 3.2f, 0.30f, 0.30f);
        glPopMatrix();
        // south approach boom (pivot east of road, opens to the west)
        glPushMatrix();
        glTranslatef(cx + 13.6f, 1.15f, 2914.0f);
        glRotatef(-angle, 0, 0, 1);
        setColor(0.90f, 0.90f, 0.88f);
        drawBoxAt(-6.6f, 0, 0, 6.6f, 0.28f, 0.28f);
        setColor(0.75f, 0.15f, 0.12f);
        drawBoxAt(-3.3f, 0, 0, 3.2f, 0.30f, 0.30f);
        drawBoxAt(-9.9f, 0, 0, 3.2f, 0.30f, 0.30f);
        glPopMatrix();
        // lamps on the pivot posts
        for (int s = 0; s < 2; s++)
        {
            float lx = cx + (s == 0 ? -13.6f : 13.6f);
            float lz = 2900.0f + (s == 0 ? -12.0f : 12.0f);
            if (lampOn) setColor(1.0f, 0.15f, 0.10f);
            else setColor(0.35f, 0.08f, 0.06f);
            glPushMatrix();
            glTranslatef(lx, 1.55f, lz);
            drawSphere(0.22f, 6, 4);
            glPopMatrix();
        }
    }

    // birds: body, two beating wings and a tail, oriented along the flight path.
    // Wingspans run from 1.5 m (pigeon) to 5.5 m (kite), so they read from the
    // cockpit instead of being specks.
    {
        int idx = 0;
        for (int f = 0; f < sFlockN && idx < (int)sBirds.size(); f++)
        {
            const BirdKindDef& kd = BIRDS[sBirdKind[f]];
            for (int k = 0; k < sFlockCount[f] && idx < (int)sBirds.size(); k++, idx++)
            {
                Bird& b = sBirds[idx];
                float flap = sinf((float)sTime * kd.flapRate + b.phase) * kd.flapAmp;
                float s = kd.scale;
                setColor(kd.r, kd.g, kd.b);
                glPushMatrix();
                glTranslatef(b.pos.x, b.pos.y, b.pos.z);
                glRotatef(-rad2deg(b.yaw), 0, 1, 0);
                glNormal3f(0.0f, 1.0f, 0.0f);
                glBegin(GL_TRIANGLES);
                // body, nose towards -Z
                glVertex3f(0.0f, 0.0f, -0.60f * s);
                glVertex3f(0.0f, 0.10f * s,  0.45f * s);
                glVertex3f(0.0f, -0.05f * s, 0.35f * s);
                // left wing
                glVertex3f(-0.10f * s, 0.0f, 0.10f * s);
                glVertex3f(-1.05f * s, flap * s, -0.20f * s);
                glVertex3f(-0.05f * s, 0.0f, 0.50f * s);
                // right wing
                glVertex3f(0.10f * s, 0.0f, 0.10f * s);
                glVertex3f(0.05f * s, 0.0f, 0.50f * s);
                glVertex3f(1.05f * s, flap * s, -0.20f * s);
                glEnd();
                glPopMatrix();
            }
        }
    }

    // traffic crossing Keane Bridge
    for (int i = 0; i < 7; i++)
    {
        BridgeCar& b = sBridge[i];
        glPushMatrix();
        glTranslatef(b.lane, 1.58f, b.z);
        glRotatef(b.dir > 0.0f ? 180.0f : 0.0f, 0, 1, 0);
        switch (b.type)
        {
            case 0: drawRickshaw(b.r, b.g, b.b); break;
            case 2: drawCar(b.r, b.g, b.b); break;
            case 3: drawBus(b.r, b.g, b.b); break;
            default: drawTruck(b.r, b.g, b.b); break;
        }
        glPopMatrix();
    }

    // pedestrians on the pavements
    for (int i = 0; i < 24; i++)
    {
        Walker& w = sWalkers[i];
        float x, z, yaw;
        if (w.axis == 0)
        {
            x = w.side * 16.5f;
            z = w.pos;
            yaw = (w.dir > 0.0f) ? 180.0f : 0.0f;
        }
        else
        {
            x = w.pos;
            z = 1200.0f + w.side * 16.5f;
            yaw = (w.dir > 0.0f) ? 90.0f : -90.0f;
        }
        float bob = sinf((float)sTime * 6.0f + w.phase) * 0.035f;
        worldDrawPerson(x, bob, z, yaw, w.r, w.g, w.b, w.fem);
    }

    // AI planes
    for (int i = 0; i < 2; i++)
    {
        vehicleShadow(sAi[i].pos.x, sAi[i].pos.z, sAi[i].yaw, 6.0f, 4.0f, 0.10f);
        craftDrawSimple(sAi[i].pos.x, sAi[i].pos.y, sAi[i].pos.z,
                        sAi[i].yaw, sAi[i].roll, sAi[i].r, sAi[i].g, sAi[i].b);
    }
    glDisable(GL_BLEND);
}
void trafficGetStats(TrafficStats& s)
{
    s.movingVehicles = (int)sVehs.size();
    s.parkedVehicles = (int)sParked.size();
    s.boats = (int)sBoats.size();
    s.birds = (int)sBirds.size();
    s.aiPlanes = 2;
    s.walkers = 24;
    s.bridgeCars = 7;
}