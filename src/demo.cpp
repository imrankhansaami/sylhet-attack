// demo.cpp - the scripted cinematic fly-through
//
// The demo is a closed camera path with a look-at point for every key. Both the
// camera position and the point it looks at are interpolated with a cubic
// Hermite spline whose tangents take the key times into account, so the speed
// stays continuous even when one shot is long and the next is short. A small
// bank angle is derived from the turn rate, which makes the wide city turns
// lean into the corner like a real chase camera.
//
// Nothing here touches the flight model: the demo is a camera, not an aircraft.

#include "demo.h"
#include "world.h"
#include <cmath>
#include <cstddef>


namespace
{
struct CamKey { float t; V3 cam; V3 look; };

// a point measured in runway coordinates: s metres along the runway from its
// centre, lat metres towards the terminal side, y metres up
V3 runwayPoint(float s, float lat, float y)
{
    V3 p = worldRunwayPoint(s, lat);
    return v3(p.x, y, p.z);
}
struct Caption { float t0, t1; const char* title; const char* body; };

// route: SIU campus -> Zindabazar -> Keane Bridge -> clock tower -> Mazar ->
// Osmani airport runway -> climb out over the city
const CamKey KEYS[] =
{
    {   0.0f, v3( 1560.0f,  70.0f,  2600.0f), v3( 1620.0f,  8.0f, 2000.0f) },
    {   8.0f, v3( 1680.0f,  85.0f,  1900.0f), v3( 1900.0f, 30.0f, 1550.0f) },
    {  15.0f, v3( 1150.0f, 100.0f,  1150.0f), v3(  600.0f, 30.0f,  700.0f) },
    {  22.0f, v3(  450.0f,  40.0f,   480.0f), v3(  100.0f,  8.0f,  105.0f) },
    {  28.0f, v3(  150.0f,  16.0f,   118.0f), v3(    0.0f,  6.0f,  105.0f) },
    {  34.0f, v3(   25.0f,  24.0f,   -35.0f), v3(   90.0f, 13.0f, -100.0f) },
    {  41.0f, v3( -110.0f,  55.0f,  -280.0f), v3(   90.0f, 16.0f, -100.0f) },
    {  48.0f, v3( -700.0f,  82.0f,  -650.0f), v3(-1650.0f, 20.0f,-1100.0f) },
    {  56.0f, v3(-1660.0f,  34.0f, -1350.0f), v3(-1750.0f, 11.0f,-1150.0f) },
    {  64.0f, v3(-2400.0f,  80.0f, -2100.0f), v3(-2300.0f, 10.0f,-2700.0f) },
    {  70.0f, runwayPoint(  -20.0f, 850.0f, 38.0f), runwayPoint( -20.0f, 510.0f, 14.0f) },
    {  75.0f, runwayPoint( -100.0f,   0.0f, 25.0f), runwayPoint( 500.0f,   0.0f,  5.0f) },
    {  79.0f, runwayPoint(  500.0f,   0.0f, 16.0f), runwayPoint(1100.0f,   0.0f,  4.0f) },
    {  83.0f, runwayPoint( 1100.0f,   0.0f, 18.0f), runwayPoint(1700.0f,   0.0f,  4.0f) },
    {  86.0f, runwayPoint( 1500.0f,   0.0f, 45.0f), runwayPoint(2100.0f,   0.0f,  6.0f) },
    {  92.0f, v3( -900.0f, 175.0f, -3050.0f), v3(   50.0f, 20.0f,-1850.0f) },
    {  99.0f, v3( -700.0f, 195.0f, -1850.0f), v3(  200.0f, 20.0f, -550.0f) },
    { 106.0f, v3( -500.0f, 165.0f,  -600.0f), v3(  400.0f, 15.0f,  400.0f) }
};
const int KEY_COUNT = (int)(sizeof(KEYS) / sizeof(KEYS[0]));

const float DEMO_END = 108.0f;       // fades out over the last two seconds
const float FADE_IN = 1.2f;

const Caption CAPTIONS[] =
{
    {   1.2f,   8.0f, "Sylhet International University",
        "The SIU flying strip on the city's eastern edge - home base of the simulator." },
    {   9.0f,  20.5f, "Zindabazar",
        "Rooftops, shops and left-hand traffic in the heart of Sylhet." },
    {  21.5f,  31.0f, "Keane Bridge",
        "The 1936 steel truss bridge carries Zindabazar Road across the Surma." },
    {  33.5f,  42.0f, "Ali Amjad's Clock Tower",
        "A Victorian clock tower on the north bank, still keeping the city's time." },
    {  46.0f,  57.5f, "Hazrat Shahjalal Mazar",
        "The shrine of Shah Jalal - green dome, marble courtyard and minarets." },
    {  58.5f,  74.0f, "Osmani International Airport",
        "Runway 05/23 on the south bank - the landing challenge starts on short final." },
    {  75.0f,  89.0f, "Over the runway",
        "1,400 metres of asphalt, seen from just above the centreline." },
    {  90.0f, 103.0f, "Climbing out",
        "Past the airport, back over the rooftops toward the river." },
    { 103.5f, 106.5f, "Welcome aboard",
        "Free Flight - Flight School - City Tour - Ring Course - Landing Challenge" }
};
const int CAPTION_COUNT = (int)(sizeof(CAPTIONS) / sizeof(CAPTIONS[0]));

// ---------------------------------------------------------------------
// state
// ---------------------------------------------------------------------
float sT = 0.0f;
V3 sPos, sLook;
float sYaw = 0.0f, sPitch = 0.0f, sRoll = 0.0f;

// ---------------------------------------------------------------------
// spline
// ---------------------------------------------------------------------
const CamKey& keyAt(int i)
{
    if (i < 0) i = 0;
    if (i >= KEY_COUNT) i = KEY_COUNT - 1;
    return KEYS[i];
}

float timeAt(int i)
{
    if (i < 0) return KEYS[0].t - 1.0f;
    if (i >= KEY_COUNT) return KEYS[KEY_COUNT - 1].t + 1.0f;
    return KEYS[i].t;
}

// one cubic Hermite segment; the tangents are scaled so that a long segment and
// a short segment still meet with the same velocity
V3 hermite(const V3& p0, const V3& p1, const V3& p2, const V3& p3,
           float t0, float t1, float t2, float t3, float t)
{
    float dt = t2 - t1;
    if (dt < 1e-5f) return p1;
    float u = clampf((t - t1) / dt, 0.0f, 1.0f);
    float u2 = u * u, u3 = u2 * u;
    float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
    float h10 = u3 - 2.0f * u2 + u;
    float h01 = -2.0f * u3 + 3.0f * u2;
    float h11 = u3 - u2;
    V3 m1 = (p2 - p0) * (dt / (t2 - t0));
    V3 m2 = (p3 - p1) * (dt / (t3 - t1));
    return p1 * h00 + m1 * h10 + p2 * h01 + m2 * h11;
}

V3 samplePath(float t, bool look)
{
    int i = 0;
    while (i < KEY_COUNT - 2 && t > KEYS[i + 1].t) i++;

    const CamKey& k0 = keyAt(i - 1);
    const CamKey& k1 = keyAt(i);
    const CamKey& k2 = keyAt(i + 1);
    const CamKey& k3 = keyAt(i + 2);
    V3 p0 = look ? k0.look : k0.cam;
    V3 p1 = look ? k1.look : k1.cam;
    V3 p2 = look ? k2.look : k2.cam;
    V3 p3 = look ? k3.look : k3.cam;
    return hermite(p0, p1, p2, p3, timeAt(i - 1), timeAt(i), timeAt(i + 1), timeAt(i + 2), t);
}

// ---------------------------------------------------------------------
// evaluate the pose for the current time
// ---------------------------------------------------------------------
void evaluate()
{
    sPos = samplePath(sT, false);
    sLook = samplePath(sT, true);

    V3 d = norm(sLook - sPos);
    sYaw = yawFromDir(v3(d.x, 0.0f, d.z));
    sPitch = asinf(clampf(d.y, -1.0f, 1.0f));


    // bank a little into the turn: compare the heading a moment ahead
    float ahead = sT + 0.35f;
    if (ahead > DEMO_END) ahead = DEMO_END;
    V3 c2 = samplePath(ahead, false);
    V3 l2 = samplePath(ahead, true);
    V3 d2 = norm(l2 - c2);
    float yaw2 = yawFromDir(v3(d2.x, 0.0f, d2.z));
    float span = ahead - sT;
    float turnRate = (span > 0.01f) ? wrapPi(yaw2 - sYaw) / span : 0.0f;
    sRoll = clampf(turnRate * 0.32f, -0.14f, 0.14f);
}
}

// ---------------------------------------------------------------------
// public interface
// ---------------------------------------------------------------------
void demoStart()
{
    sT = 0.0f;
    evaluate();
}

void demoUpdate(float dt)
{
    sT += dt;
    if (sT > DEMO_END) sT = DEMO_END;
    evaluate();

}

bool demoFinished()
{
    return sT >= DEMO_END;
}

void demoPose(V3& camPos, float& yaw, float& pitch, float& roll)
{
    camPos = sPos;
    yaw = sYaw;
    pitch = sPitch;
    roll = sRoll;
}

float demoTime() { return sT; }
float demoDuration() { return DEMO_END; }

const char* demoCaptionTitle()
{
    for (int i = 0; i < CAPTION_COUNT; i++)
        if (sT >= CAPTIONS[i].t0 && sT <= CAPTIONS[i].t1)
            return CAPTIONS[i].title;
    return nullptr;
}

const char* demoCaptionBody()
{
    for (int i = 0; i < CAPTION_COUNT; i++)
        if (sT >= CAPTIONS[i].t0 && sT <= CAPTIONS[i].t1)
            return CAPTIONS[i].body;
    return nullptr;
}

float demoCaptionAlpha()
{
    for (int i = 0; i < CAPTION_COUNT; i++)
    {
        if (sT < CAPTIONS[i].t0 || sT > CAPTIONS[i].t1) continue;
        float a = fminf(1.0f, (sT - CAPTIONS[i].t0) / 0.7f);
        a = fminf(a, (CAPTIONS[i].t1 - sT) / 0.7f);
        if (a < 0.0f) a = 0.0f;
        return a * (1.0f - demoFade());
    }
    return 0.0f;
}

float demoFade()
{
    if (sT < FADE_IN) return 1.0f - sT / FADE_IN;
    float out = DEMO_END - 2.0f;
    if (sT > out) return clampf((sT - out) / 2.0f, 0.0f, 1.0f);
    return 0.0f;
}