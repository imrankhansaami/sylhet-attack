// game.cpp - game states, input handling, cameras and the main update loop
#include "game.h"
#include "glutil.h"
#include "hud.h"
#include "world.h"
#include "traffic.h"
#include "textures.h"
#include "demo.h"
#include "video.h"
#include "enemy.h"
#include "sound.h"
#include <GL/freeglut.h>
#include <windows.h>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#include <mmsystem.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------
// globals
// ---------------------------------------------------------------------
Aircraft gAircraft;
int gWinW = 1280, gWinH = 720;
double gTime = 0.0;
float gMouseSens = 1.0f;
float gToastTimer = 0.0f;
float gFps = 60.0f;
bool gShowFps = false;      // F3: performance overlay
char gToastText[64] = "";
char gCaptionText[80] = "";     // landmark name card, for demonstrating
float gCaptionTimer = 0.0f;
static int sLandmarkIdx = -1;   // last landmark jumped to (N cycles through them)
int gStartLandmark = -1;        // --landmark <n>: jump there on start
bool gStartResults = false;      // --results: show the success screen at once
bool gInstantAction = false;     // one-key dogfight from the title screen
bool gStartInstant = false;      // --instant: start in a dogfight
bool gAutoFire = false;          // --autofire: hold the trigger down (capture runs)
int gDifficulty = 1;             // 0 easy, 1 normal, 2 ace
bool gStartBriefing = true;      // the mission briefing card before a sortie
int gStartWithMission = -1;
bool gStartCockpit = false;
bool gStartDemo = false;       // --demo: start the cinematic fly-through
float gDemoStartTime = 0.0f;   // --demotime <s>: seek before the first frame
bool gStartPhoto = false;      // --photo: start in photo mode (capture runs)
int gStartPage = 0;            // --help / --credits: 1 = help, 2 = credits
bool gStatsOnly = false;       // --stats: print scene counts and quit
const char* gBenchPath = nullptr;   // --benchmark <file.csv>: sample fps to a file
float gBenchSeconds = 12.0f;        // --benchtimed <seconds>
const char* gVideoPath = nullptr;   // --video <file.avi>: record the frame sequence
int gVideoFps = 20;                 // --videofps <n>
float gVideoSeconds = 109.0f;       // --videotimed <seconds>
bool gProbe = false;

// photo mode state (also read by the HUD)
bool gPhotoHud = true;         // H: hide the viewfinder for a clean frame
bool gPhotoGrid = false;       // G: rule-of-thirds guides
int gPhotoTime = 0;            // index into the slow-motion ladder
float gPhotoTimeScale = 1.0f;  // 1, 0.35, 0.12 or 0 (frozen)
static double sProbeNext = 0.0;
bool gShowLabels = true;
int gMapZoom = 1;
bool gInvertPitch = false;     // I: flip the vertical control axis
bool gJoyBrake = false;        // joystick brake button, held
float gAtX = 99999.0f, gAtZ = 0.0f, gAtY = 0.0f;   // --at x z alt: start there

// joystick state (filled in by initJoystick / pollJoystick further down)
static bool sJoyOk = false;
static UINT sJoyId = JOYSTICKID1;
static char sJoyName[80] = "";
static float sJoyPitch = 0.0f, sJoyRoll = 0.0f;
static unsigned sJoyPrevButtons = 0;

enum GameState { GS_TITLE, GS_DEMO, GS_HELP, GS_CREDITS, GS_BRIEF, GS_FLIGHT, GS_PAUSE, GS_RESULTS, GS_PHOTO };
static GameState sState = GS_TITLE;
static GameState sHelpReturn = GS_TITLE;

static float sMouseX = 0, sMouseY = 0;
static bool sPendingClick = false;
static float sClickX = 0, sClickY = 0;

static bool sKeys[256];
static bool sSpec[256];

static float sStickX = 0, sStickY = 0;         // smoothed control stick
static float sStickRawX = 0, sStickRawY = 0;     // raw mouse stick position

static double sLastTime = 0.0;
static float sAccum = 0.0f;                // physics sub-step accumulator

static int sCamMode = 0;                   // 0 chase, 1 cockpit
static float sFov = 60.0f;                 // field of view, opens up with speed
static V3 sCamPos;
static V3 sCamVel;
static float sCamYaw = 0, sCamPitch = 0, sCamRoll = 0;

static V3 sPhotoPos;
static float sPhotoYaw = 0, sPhotoPitch = 0;
static V3 sDemoPos;
static float sDemoYaw = 0, sDemoPitch = 0, sDemoRoll = 0;

struct Particle { V3 p, v; float life; };
static Particle sParts[64];
static bool sPartsActive = false;
static bool sCrashSeen = false;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------
static void cursorForState()
{
    if (sState == GS_FLIGHT || sState == GS_PHOTO) glutSetCursor(GLUT_CURSOR_NONE);
    else glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
}

static void setCamera(V3 pos, float yaw, float pitch, float roll)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(rad2deg(-roll), 0, 0, 1);
    glRotatef(rad2deg(-pitch), 1, 0, 0);
    glRotatef(rad2deg(yaw), 0, 1, 0);
    glTranslatef(-pos.x, -pos.y, -pos.z);
}

// ---------------------------------------------------------------------
// sky: gradient dome, sun and clouds
// ---------------------------------------------------------------------
static V3 sClouds[26];
static float sCloudR[26];

static void drawSky(V3 camPos)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);

    // gradient dome around the camera
    float top[3] = { 0.38f, 0.58f, 0.86f };
    float bot[3] = { 0.80f, 0.87f, 0.92f };
    float R = 16000.0f, yBase = -900.0f;
    for (int band = 0; band < 4; band++)
    {
        float a0 = (float)band * 22.5f;
        float a1 = (float)(band + 1) * 22.5f;
        float c0f = (float)band / 4.0f, c1f = (float)(band + 1) / 4.0f;
        float col0[3], col1[3];
        for (int c = 0; c < 3; c++)
        {
            col0[c] = bot[c] + (top[c] - bot[c]) * c0f;
            col1[c] = bot[c] + (top[c] - bot[c]) * c1f;
        }
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 24; i++)
        {
            float th = (float)i / 24.0f * 2.0f * PI_F;
            float cx = cosf(th), sz = sinf(th);
            glColor3fv(col0);
            glVertex3f(camPos.x + cx * R * cosf(deg2rad(a0)), yBase + R * sinf(deg2rad(a0)), camPos.z + sz * R * cosf(deg2rad(a0)));
            glColor3fv(col1);
            glVertex3f(camPos.x + cx * R * cosf(deg2rad(a1)), yBase + R * sinf(deg2rad(a1)), camPos.z + sz * R * cosf(deg2rad(a1)));
        }
        glEnd();
    }

    // the sun with a soft glow
    {
        V3 sunDir = norm(v3(0.35f, 0.62f, 0.30f));
        V3 sun = camPos + sunDir * 14000.0f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        setColorA(1.0f, 0.92f, 0.60f, 0.20f);
        glPushMatrix(); glTranslatef(sun.x, sun.y, sun.z); drawSphere(900.0f, 10, 6); glPopMatrix();
        setColorA(1.0f, 0.96f, 0.78f, 1.0f);
        glPushMatrix(); glTranslatef(sun.x, sun.y, sun.z); drawSphere(340.0f, 10, 6); glPopMatrix();
        glDisable(GL_BLEND);
    }

    // clouds
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < 26; i++)
    {
        V3 c = sClouds[i];
        float r = sCloudR[i];
        glColor4f(1.0f, 1.0f, 1.0f, 0.80f);
        for (int k = 0; k < 4; k++)
        {
            float ox = (k - 1.5f) * r * 0.55f;
            glPushMatrix();
            glTranslatef(c.x + ox, c.y + (k % 2) * 25.0f, c.z + (k % 2 ? r * 0.2f : -r * 0.2f));
            glScalef(1.0f, 0.32f, 1.0f);
            drawSphere(r * (k == 1 || k == 2 ? 1.0f : 0.75f), 8, 5);
            glPopMatrix();
        }
    }
    glDisable(GL_BLEND);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------------
// the plane's own shadow on the ground
// ---------------------------------------------------------------------
static void drawPlayerShadow()
{
    Aircraft& a = gAircraft;
    if (a.pos.y > 600.0f) return;
    float alpha = 0.26f * clampf(1.0f - a.pos.y / 450.0f, 0.0f, 1.0f);
    float scale = 1.0f + a.pos.y * 0.0022f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    // the shadow is a flat tint, so it must not be lit (otherwise the lighting
    // turns it grey instead of darkening the ground)
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(a.pos.x - a.pos.y * 0.565f, 0.19f, a.pos.z - a.pos.y * 0.484f);
    glRotatef(-rad2deg(a.yaw), 0, 1, 0);
    // a soft elliptical blob, dark in the middle and fading out at the rim, so it
    // reads as a shadow instead of a black rectangle
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.05f, 0.06f, 0.05f, alpha);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= 24; i++)
    {
        float ang = (float)i / 24.0f * 2.0f * PI_F;
        glColor4f(0.05f, 0.06f, 0.05f, 0.0f);
        glVertex3f(cosf(ang) * 6.2f * scale, 0, sinf(ang) * 4.2f * scale);
    }
    glEnd();
    glPopMatrix();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------------
// crash particles
// ---------------------------------------------------------------------
static void spawnParticles(V3 at)
{
    unsigned seed = 991u;
    for (int i = 0; i < 64; i++)
    {
        float a = frand01(seed) * 2.0f * PI_F;
        float sp = 3.0f + frand01(seed) * 16.0f;
        sParts[i].p = at + v3((frand01(seed) - 0.5f) * 2.0f, frand01(seed) * 1.5f, (frand01(seed) - 0.5f) * 2.0f);
        sParts[i].v = v3(cosf(a) * sp, 4.0f + frand01(seed) * 12.0f, sinf(a) * sp);
        sParts[i].life = 1.0f + frand01(seed) * 1.6f;
    }
    sPartsActive = true;
}

static void updateParticles(float dt)
{
    if (!sPartsActive) return;
    bool any = false;
    for (int i = 0; i < 64; i++)
    {
        if (sParts[i].life <= 0) continue;
        any = true;
        sParts[i].v.y -= 14.0f * dt;
        sParts[i].p += sParts[i].v * dt;
        if (sParts[i].p.y < 0.4f) { sParts[i].p.y = 0.4f; sParts[i].v.y *= -0.3f; sParts[i].v.x *= 0.6f; sParts[i].v.z *= 0.6f; }
        sParts[i].life -= dt;
    }
    if (!any) sPartsActive = false;
}

static void drawParticles()
{
    if (!sPartsActive) return;
    glDisable(GL_LIGHTING);
    for (int i = 0; i < 64; i++)
    {
        if (sParts[i].life <= 0) continue;
        float f = clampf(sParts[i].life, 0.0f, 1.0f);
        setColor(0.9f * f + 0.25f * (1 - f), 0.45f * f, 0.15f * f);
        drawBoxAt(sParts[i].p.x, sParts[i].p.y, sParts[i].p.z, 0.7f, 0.7f, 0.7f);
    }
    glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------------
// camera updates
// ---------------------------------------------------------------------
static void updateFlightCamera(float dt, bool snap)
{
    Aircraft& a = gAircraft;
    Basis b = basisFromYPR(a.yaw, a.pitch, a.roll);
    if (sCamMode == 0)
    {
        V3 target = a.pos - b.f * 17.0f + b.u * 4.2f + v3(0, 1.4f, 0);
        if (snap)
        {
            sCamPos = target;
            sCamVel = v3(0, 0, 0);
        }
        else
        {
            // a damped spring: the camera glides instead of snapping
            V3 acc = (target - sCamPos) * 18.0f - sCamVel * 8.0f;
            sCamVel += acc * dt;
            sCamPos += sCamVel * dt;
        }
        // the camera never sinks into the ground or into a hill
        float floorY = worldHillHeightAt(sCamPos.x, sCamPos.z) + 1.8f;
        if (sCamPos.y < floorY) { sCamPos.y = floorY; sCamVel.y = 0.0f; }
        V3 look = a.pos + b.f * 12.0f + b.u * 1.5f + a.vel * 0.10f;
        V3 d = norm(look - sCamPos);
        sCamYaw = yawFromDir(v3(d.x, 0, d.z));
        sCamPitch = asinf(clampf(d.y, -1.0f, 1.0f));
        sCamRoll = 0.0f;
    }
    else
    {
        // seated in the cabin: eye below the wing, looking slightly down over the nose
        sCamPos = a.pos + b.f * 1.15f + b.u * 0.58f;
        sCamYaw = a.yaw;
        sCamPitch = a.pitch - 0.13f;
        sCamRoll = a.roll;
    }
}

// ---------------------------------------------------------------------
// mission start / state switching
// ---------------------------------------------------------------------
void gameStartMission(MissionId id)
{
    Aircraft& a = gAircraft;
    V3 d = worldRunwayDir();
    V3 r = worldRunwayRight();
    V3 apron = worldApronCenter();
    float rwy = deg2rad(worldRunwayHeadingDeg());

    if (id == MIS_FREE || id == MIS_SCHOOL || id == MIS_TOUR)
    {
        if (gAtX < 90000.0f)
        {
            // dev/demo: drop the aircraft at an exact spot (with flying speed if
            // it starts in the air, otherwise it would simply fall)
            bool inAir = gAtY > 1.0f;
            craftReset(a, v3(gAtX, 0, gAtZ), rwy, true, inAir ? 34.0f : 0.0f, 0.0f, inAir);
            a.pos.y = inAir ? gAtY : CRAFT_GEAR_H;
            a.onGround = !inAir;
        }
        else if (id == MIS_SCHOOL)
        {
            // the school needs the real runway and the taxi route
            V3 start = apron + r * 30.0f - d * 140.0f;
            craftReset(a, start, rwy, false, 0.0f, 0.0f, false);
        }
        else
        {
            // home base: the SIU campus flying strip, facing south down the grass
            craftReset(a, v3(1600.0f, 0, 1875.0f), PI_F, true, 0.0f, 0.0f, false);
        }
        a.throttle = 0.0f;
    }
    else if (id == MIS_RINGS)
    {
        // start in the air 500 m before the first ring, pointed at it, having
        // come from Osmani airport
        V3 ring1 = v3(-2400.0f, 180.0f, -1700.0f);
        V3 ap = worldAirportCenter();
        V3 dir = norm(v3(ring1.x - ap.x, 0, ring1.z - ap.z));
        V3 start = ring1 - dir * 500.0f;
        craftReset(a, start, yawFromDir(dir), true, 68.0f, 180.0f, true);
    }
    else if (id == MIS_LANDING)
    {
        // short final: 900 m before the threshold at 60 m, 64 kt - close enough
        // that the runway is in sight and the pilot has time to settle the sink
        V3 start = worldRunwayPoint(-2050.0f, 0.0f);
        craftReset(a, start, rwy, true, 33.0f, 45.0f, true);
        a.throttle = 0.32f;
    }

    gMission = missionCreate(id);

    // Hostiles: the missions are a contest, not a sightseeing trip. Flight
    // School stays peaceful - it is the training mission - and the ring course
    // gets the most, because there is nowhere to hide over the river.
    {
        int hostiles = 0;
        if (id == MIS_RINGS) hostiles = 6;
        else if (id == MIS_LANDING || id == MIS_FREE) hostiles = 5;
        // MIS_TOUR (Defend the City) sends raids instead of a standing patrol
        enemyReset();
        weaponReset();
        groundSetup();
        if (hostiles > 0) enemyBeginWaves(hostiles);
    }

    sState = gStartBriefing ? GS_BRIEF : GS_FLIGHT;
    sCamMode = gStartCockpit ? 1 : 0;
    sStickX = sStickY = 0.0f;
    sPartsActive = false;
    sCrashSeen = false;
    updateFlightCamera(0.0f, true);
    cursorForState();

    // tell the player we found their controller, so they know it is live
    if (sJoyOk)
    {
        snprintf(gToastText, 64, "Joystick: %s", sJoyName);
        gToastTimer = 4.0f;
    }
}

// ---------------------------------------------------------------------
//presentation: jump straight to a landmark
// ---------------------------------------------------------------------
// For showing the city quickly: 1-9 and 0 put the aircraft a quarter of a
// kilometre from the named place, wings level and pointed at it, with its name
// on screen. N walks through them in order.
static void gameJumpToLandmark(int i)
{
    if (i < 0 || i >= gLandmarkCount) return;
    const Landmark& lm = gLandmarks[i];

    float ang = 2.35f;                                  // stand off to the south-east
    float dist = 175.0f;                                // close enough to fill the view
    float alt = lm.labelY + 42.0f;
    V3 pos = v3(lm.x + cosf(ang) * dist, alt, lm.z + sinf(ang) * dist);
    // aim a little to one side, so the chase camera sees the place beside the
    // aircraft instead of straight through it
    float yaw = yawFromDir(v3(lm.x - pos.x, 0.0f, lm.z - pos.z)) + 0.20f;

    craftReset(gAircraft, pos, yaw, true, 34.0f, 0.0f, true);
    gAircraft.pos.y = alt;
    // nose down a little so the place sits in the middle of the screen
    gAircraft.pitch = -atan2f(alt - lm.labelY * 0.6f, dist) * 0.55f;
    gAircraft.roll = 0.0f;
    gAircraft.onGround = false;
    gAircraft.throttle = 0.45f;
    gAircraft.flapCmd = -1.0f;
    sAccum = 0.0f;
    sStickX = sStickY = 0.0f;
    updateFlightCamera(0.0f, true);

    // if the mission has an active step, do not pretend it is complete: the
    // jump is for looking at places, not for scoring
    snprintf(gCaptionText, sizeof(gCaptionText), "%s", lm.name);
    gCaptionTimer = 6.0f;
    sLandmarkIdx = i;
}

// one key from the title screen: airborne over the river with bandits inbound
static void gameStartInstantAction()
{
    gInstantAction = true;
    gameStartMission(MIS_FREE);

    V3 start = v3(900.0f, 0.0f, 1500.0f);
    craftReset(gAircraft, start, deg2rad(15.0f), true, 64.0f, 0.0f, true);
    gAircraft.pos.y = 340.0f;
    gAircraft.throttle = 0.8f;
    gAircraft.pitch = 0.0f;
    gAircraft.roll = 0.0f;
    updateFlightCamera(0.0f, true);

    enemyStart(4);
    enemyRush();                     // they arrive at once, not over half a minute
    weaponReset();

    snprintf(gCaptionText, sizeof(gCaptionText), "INSTANT ACTION  -  FOUR BANDITS");
    gCaptionTimer = 5.0f;
}

static MissionId uiToMission(int ui)
{
    switch (ui)
    {
        case UI_SCHOOL: return MIS_SCHOOL;
        case UI_TOUR: return MIS_TOUR;
        case UI_RINGS: return MIS_RINGS;
        case UI_LANDING: return MIS_LANDING;
    }
    return MIS_FREE;
}

static void leaveToTitle()
{
    sState = GS_TITLE;
    cursorForState();
}

// ---------------------------------------------------------------------
// update
// ---------------------------------------------------------------------
static float expoCurve(float x)
{
    float s = (x < 0.0f) ? -1.0f : 1.0f;
    float ax = fabsf(x);
    return s * (0.42f * ax + 0.58f * ax * ax * ax);
}

// ---------------------------------------------------------------------
// joystick / gamepad
// Windows provides this through winmm, which the build already links, so a
// plugged-in controller works with no extra dependency at all.
// ---------------------------------------------------------------------
static void cycleFlaps();   // defined below

static void initJoystick()
{
    sJoyOk = false;
    if (joyGetNumDevs() == 0) return;
    JOYINFOEX info;
    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(sJoyId, &info) != JOYERR_NOERROR) return;
    JOYCAPS caps;
    if (joyGetDevCaps(sJoyId, &caps, sizeof(caps)) != JOYERR_NOERROR) return;
    sJoyOk = true;
    strncpy(sJoyName, caps.szPname, sizeof(sJoyName) - 1);
    sJoyName[sizeof(sJoyName) - 1] = 0;
}

// returns true while a button is held, and reports the press edge once
static bool joyButton(unsigned now, unsigned bit, bool& pressedNow)
{
    bool held = (now & bit) != 0;
    bool was = (sJoyPrevButtons & bit) != 0;
    pressedNow = held && !was;
    return held;
}

static void pollJoystick()
{
    sJoyPitch = 0.0f;
    sJoyRoll = 0.0f;
    if (!sJoyOk) return;

    JOYINFOEX info;
    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(sJoyId, &info) != JOYERR_NOERROR)
    {
        sJoyOk = false;          // unplugged mid-flight
        return;
    }

    const float c = 32767.5f;
    float jx = (info.dwXpos - c) / c;      // right = +
    float jy = (info.dwYpos - c) / c;      // down (pull back) = +
    if (fabsf(jx) < 0.08f) jx = 0.0f;      // dead zone, sticks are never perfect
    if (fabsf(jy) < 0.08f) jy = 0.0f;
    sJoyRoll  = clampf(jx, -1.0f, 1.0f);
    sJoyPitch = clampf(jy * (gInvertPitch ? -1.0f : 1.0f), -1.0f, 1.0f);

    // buttons: 1 camera, 2 brakes, 3 flaps, 4 engine, 5/6 throttle, 7/8 trim
    bool pressed = false;
    unsigned b = info.dwButtons;

    joyButton(b, 1, pressed);
    if (pressed) { sCamMode = (sCamMode + 1) % 2; }

    gJoyBrake = joyButton(b, 2, pressed);

    joyButton(b, 3, pressed);
    if (pressed) { cycleFlaps(); }

    joyButton(b, 4, pressed);
    if (pressed) gAircraft.engineOn = true;

    joyButton(b, 5, pressed);
    if (pressed) { gAircraft.throttle = clampf(gAircraft.throttle + 0.1f, 0.0f, 1.0f); }
    joyButton(b, 6, pressed);
    if (pressed) { gAircraft.throttle = clampf(gAircraft.throttle - 0.1f, 0.0f, 1.0f); }

    joyButton(b, 7, pressed);
    if (pressed) { gAircraft.trim = clampf(gAircraft.trim - 0.1f, -1.0f, 1.0f); }
    joyButton(b, 8, pressed);
    if (pressed) { gAircraft.trim = clampf(gAircraft.trim + 0.1f, -1.0f, 1.0f); }

    sJoyPrevButtons = b;
}

static void cycleFlaps()
{
    Aircraft& a = gAircraft;
    // AUTO -> 0% -> 50% -> 100% -> AUTO
    if (a.flapCmd < 0.0f) { a.flapCmd = 0.0f; snprintf(gToastText, 64, "Flaps 0%%  (F cycles)"); }
    else if (a.flapCmd < 0.25f) { a.flapCmd = 0.5f; snprintf(gToastText, 64, "Flaps 50%%  (F cycles)"); }
    else if (a.flapCmd < 0.75f) { a.flapCmd = 1.0f; snprintf(gToastText, 64, "Flaps 100%%  (F cycles)"); }
    else { a.flapCmd = -1.0f; snprintf(gToastText, 64, "Flaps AUTO  (F cycles)"); }
    gToastTimer = 2.0f;
}

static void applyInputs(float dt)
{
    Aircraft& a = gAircraft;
    pollJoystick();

    // the mouse stick eases back to the middle on its own
    float decay = expf(-dt * 0.9f);
    sStickRawX *= decay;
    sStickRawY *= decay;

    float keyPitch = (sSpec[GLUT_KEY_UP] ? 1.0f : 0.0f) - (sSpec[GLUT_KEY_DOWN] ? 1.0f : 0.0f);
    float keyRoll = (sSpec[GLUT_KEY_RIGHT] ? 1.0f : 0.0f) - (sSpec[GLUT_KEY_LEFT] ? 1.0f : 0.0f);
    float keyYaw = (sKeys['d'] ? 1.0f : 0.0f) - (sKeys['a'] ? 1.0f : 0.0f);

    // a small dead zone around the centre of the stick
    if (fabsf(sStickRawX) < 0.04f) sStickRawX = 0.0f;
    if (fabsf(sStickRawY) < 0.04f) sStickRawY = 0.0f;

    // the control input itself is smoothed, so small mouse moves do not jolt the aircraft
    float targetX = clampf(sStickRawX + keyRoll + sJoyRoll, -1.2f, 1.2f);
    float targetY = clampf(sStickRawY + keyPitch + sJoyPitch, -1.2f, 1.2f);
    float k = 1.0f - expf(-dt / 0.09f);
    sStickX += (targetX - sStickX) * k;
    sStickY += (targetY - sStickY) * k;

    // an expo curve: fine control around the centre, full authority at the stops
    a.inPitch = clampf(expoCurve(sStickY), -1.0f, 1.0f);
    a.inRoll  = clampf(expoCurve(sStickX), -1.0f, 1.0f);
    a.inYaw   = keyYaw;

    // throttle opens smoothly, and quicker the further it is open
    float thr = (0.26f + 0.55f * a.throttle) * dt;
    if (sKeys['w']) a.throttle += thr;
    if (sKeys['s']) a.throttle -= thr;
    a.throttle = clampf(a.throttle, 0.0f, 1.0f);
    a.brake = (sKeys[' '] != 0) || gJoyBrake;
    if (sKeys['e']) a.engineOn = true;
}

static void updateWorld(float dt)
{
    switch (sState)
    {
        case GS_TITLE:
        case GS_HELP:
        case GS_CREDITS:
            trafficUpdate(dt, gTime);
            break;

        case GS_BRIEF:
            trafficUpdate(dt, gTime);      // the world idles while the briefing is up
            break;

        case GS_DEMO:
            demoUpdate(dt);
            trafficUpdate(dt, gTime);
            if (demoFinished()) leaveToTitle();
            break;

        case GS_FLIGHT:
        {
            Aircraft& a = gAircraft;
            bool over = gMission.complete || gMission.failed;
            if (!over)
            {
                applyInputs(dt);
                const float SUB = 1.0f / 180.0f;
                sAccum += dt;
                int guard = 0;
                while (sAccum >= SUB && guard < 40)
                {
                    craftStep(a, SUB);
                    sAccum -= SUB;
                    guard++;
                }
                if (guard >= 40) sAccum = 0.0f;
            }
            else
            {
                // the flight is over: hold the aircraft still so nothing that
                // happens afterwards (a roll into the river, a crash) can change
                // the landing grade that is about to be shown
                a.brake = true;
                a.inPitch = a.inRoll = a.inYaw = 0.0f;
                sAccum = 0.0f;
            }
            missionUpdate(dt);
            enemyUpdate(dt);
            weaponUpdate(dt);
            groundUpdate(dt);

            // rearm on the apron: slow down on the ground near the terminal
            {
                V3 ap = worldApronCenter();
                float dx = gAircraft.pos.x - ap.x, dz = gAircraft.pos.z - ap.z;
                if (gAircraft.onGround && len(gAircraft.vel) < 3.0f && dx * dx + dz * dz < 320.0f * 320.0f)
                {
                    static float sRearmT = 0.0f;
                    sRearmT -= dt;
                    if (sRearmT <= 0.0f && weaponRearm())
                    {
                        snprintf(gToastText, 64, "Rearmed and refuelled at Osmani");
                        gToastTimer = 2.5f;
                    }
                    if (sRearmT < -3.0f) sRearmT = 0.0f;
                }
            }
            trafficUpdate(dt, gTime);
            updateFlightCamera(dt, false);
            // the field of view opens up with speed - it gives a sense of pace
            float targetFov = 58.0f + 16.0f * clampf(a.airspeed / 95.0f, 0.0f, 1.0f);
            sFov += (targetFov - sFov) * (1.0f - expf(-dt * 2.0f));
            if (gToastTimer > 0.0f) gToastTimer -= dt;
            if (gCaptionTimer > 0.0f) gCaptionTimer -= dt;
            if (sKeys['z'] || gAutoFire) weaponFireCannon();
            // kill feed and the missile warning tone
            {
                static int sKillsSeen = 0;
                static float sBeepT = 0.0f;
                if (weaponKills() > sKillsSeen)
                {
                    sKillsSeen = weaponKills();
                    snprintf(gToastText, 64, "BANDIT DOWN  -  kills %d", sKillsSeen);
                    gToastTimer = 2.4f;
                }
                else if (weaponKills() < sKillsSeen) sKillsSeen = weaponKills();

                sBeepT -= dt;
                if (enemyMissileLock() && sBeepT <= 0.0f)
                {
                    soundPlay(SND_LOCK, 0.45f);
                    sBeepT = 0.45f;
                }
            }

            // engine note follows the revs; quiet on the pad, loud at full power
            {
                float rpm01 = clampf((gAircraft.rpm - 550.0f) / 800.0f, 0.0f, 1.0f);
                bool running = gAircraft.engineOn && gAircraft.phase != PHASE_CRASHED;
                soundSetEngine(rpm01, running ? (0.35f + 0.65f * gAircraft.throttle) : 0.0f);
            }
            if (a.phase == PHASE_CRASHED && !sCrashSeen)
            {
                sCrashSeen = true;
                spawnParticles(a.pos);
            }
            updateParticles(dt);
            if ((gMission.complete || gMission.failed) && gMission.endTimer > 2.6f)
            {
                sState = GS_RESULTS;
                cursorForState();
            }
            break;
        }

        case GS_PAUSE:
            if (gCaptionTimer > 0.0f) gCaptionTimer -= dt;
            if (sKeys['z'] || gAutoFire) weaponFireCannon();
            // kill feed and the missile warning tone
            {
                static int sKillsSeen = 0;
                static float sBeepT = 0.0f;
                if (weaponKills() > sKillsSeen)
                {
                    sKillsSeen = weaponKills();
                    snprintf(gToastText, 64, "BANDIT DOWN  -  kills %d", sKillsSeen);
                    gToastTimer = 2.4f;
                }
                else if (weaponKills() < sKillsSeen) sKillsSeen = weaponKills();

                sBeepT -= dt;
                if (enemyMissileLock() && sBeepT <= 0.0f)
                {
                    soundPlay(SND_LOCK, 0.45f);
                    sBeepT = 0.45f;
                }
            }

            // engine note follows the revs; quiet on the pad, loud at full power
            {
                float rpm01 = clampf((gAircraft.rpm - 550.0f) / 800.0f, 0.0f, 1.0f);
                bool running = gAircraft.engineOn && gAircraft.phase != PHASE_CRASHED;
                soundSetEngine(rpm01, running ? (0.35f + 0.65f * gAircraft.throttle) : 0.0f);
            }
            break;

        case GS_RESULTS:
            missionUpdate(dt);      // advances the end timer and that is all
            updateParticles(dt);
            break;

        case GS_PHOTO:
        {
            float sp = 90.0f * dt;
            V3 fwd = v3(sinf(sPhotoYaw) * cosf(sPhotoPitch), sinf(sPhotoPitch), -cosf(sPhotoYaw) * cosf(sPhotoPitch));
            V3 right = norm(cross(fwd, v3(0, 1, 0)));
            if (sKeys['w']) sPhotoPos += fwd * sp;
            if (sKeys['s']) sPhotoPos -= fwd * sp;
            if (sKeys['a']) sPhotoPos -= right * sp;
            if (sKeys['d']) sPhotoPos += right * sp;
            if (sKeys['q']) sPhotoPos.y -= sp;
            if (sKeys['e']) sPhotoPos.y += sp;
            if (sPhotoPos.y < 1.0f) sPhotoPos.y = 1.0f;

            // The aircraft keeps flying while the camera is free, but at the
            // chosen slow-motion rate, so a photo can catch a moment in motion.
            // The photo keys drive the camera, not the controls, so the stick
            // inputs are cleared for these steps only.
            if (gPhotoTimeScale > 0.001f && !gMission.complete && !gMission.failed)
            {
                Aircraft& a = gAircraft;
                float sp2 = a.inPitch, sr = a.inRoll, sy = a.inYaw;
                a.inPitch = a.inRoll = a.inYaw = 0.0f;
                float scaled = dt * gPhotoTimeScale;
                const float SUB = 1.0f / 180.0f;
                sAccum += scaled;
                int guard = 0;
                while (sAccum >= SUB && guard < 40)
                {
                    craftStep(a, SUB);
                    sAccum -= SUB;
                    guard++;
                }
                if (guard >= 40) sAccum = 0.0f;
                a.inPitch = sp2; a.inRoll = sr; a.inYaw = sy;
                missionUpdate(scaled);
            }
            trafficUpdate(dt, gTime);
            break;
        }
    }
}

// ---------------------------------------------------------------------
// initialisation, resize, display
// ---------------------------------------------------------------------
static void printSceneStats();     // defined below, used by --stats

void gameInit()
{
    sState = GS_TITLE;
    soundInit();
    cursorForState();
    texturesLoad();          // photographic ground / road / runway textures
    initJoystick();          // a plugged-in controller is picked up automatically

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glEnable(GL_MULTISAMPLE);   // smooth edges when the driver offers them

    glClearColor(0.80f, 0.87f, 0.92f, 1.0f);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    {
        GLfloat amb[4] = { 0.55f, 0.56f, 0.54f, 1.0f };
        GLfloat dif[4] = { 0.85f, 0.83f, 0.76f, 1.0f };
        GLfloat pos[4] = { 0.35f, 0.62f, 0.30f, 0.0f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    }
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glFogi(GL_FOG_MODE, GL_LINEAR);
    // the fog starts exactly where the city culling radius ends, so buildings
    // are never seen popping in: they fade out of the haze at the same distance
    glFogf(GL_FOG_START, 2200.0f);
    glFogf(GL_FOG_END, 14500.0f);
    {
        GLfloat fogc[4] = { 0.80f, 0.87f, 0.92f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogc);
    }
    glEnable(GL_FOG);
    glHint(GL_FOG_HINT, GL_NICEST);

    // clouds (fixed, so every launch looks the same)
    unsigned seed = 5150u;
    for (int i = 0; i < 26; i++)
    {
        sClouds[i] = v3((frand01(seed) * 2 - 1) * 6500.0f, 900.0f + frand01(seed) * 800.0f, (frand01(seed) * 2 - 1) * 6500.0f);
        sCloudR[i] = 180.0f + frand01(seed) * 340.0f;
    }

    // reset the modelling counters so they describe the static world only
    gCountBox = gCountCylinder = gCountDome = gCountSphere = 0;
    gCountTriangles = 0;
    worldBuild();
    trafficInit();

    if (gStatsOnly)
    {
        printSceneStats();
        exit(0);
    }

    // park the plane nicely for the title screen
    craftReset(gAircraft, worldApronCenter() + worldRunwayRight() * 30.0f - worldRunwayDir() * 140.0f,
               deg2rad(worldRunwayHeadingDeg()), false, 0.0f, 0.0f, false);
    updateFlightCamera(0.0f, true);
    sLastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0;

    if (gStartWithMission >= 0) gameStartMission((MissionId)gStartWithMission);

    // --instant: straight into the dogfight
    if (gStartInstant) gameStartInstantAction();

    // --results: finish a mission immediately, for capturing the success screen
    if (gStartResults)
    {
        gStartBriefing = false;      // go straight to the debrief, not the briefing
        gameStartMission(MIS_TOUR);
        gMission.complete = true;
        gMission.stars = 3;
        gMission.score = 2140.0f;
        gMission.countDone = gMission.countTotal;
        snprintf(gMission.resultLine, sizeof(gMission.resultLine), "All five landmarks visited");
        gMission.endTimer = 3.0f;
    }

    // --landmark n: free flight, then a jump straight to that place, so a
    // presentation (or a capture script) can go to any landmark in one command
    if (gStartLandmark >= 0)
    {
        gameStartMission(MIS_FREE);
        gameJumpToLandmark(gStartLandmark);
    }

    // the demo wins over a mission flag: it is what a capture run wants
    if (gStartDemo)
    {
        demoStart();
        if (gDemoStartTime > 0.0f) demoUpdate(gDemoStartTime);
        sState = GS_DEMO;
        cursorForState();
    }
    else if (gStartPhoto)
    {
        sState = GS_PHOTO;
        sPhotoPos = sCamPos;
        sPhotoYaw = sCamYaw;
        sPhotoPitch = sCamPitch;
        cursorForState();
    }
    else if (gStartPage == 1)
    {
        sHelpReturn = GS_TITLE;
        sState = GS_HELP;
    }
    else if (gStartPage == 2)
    {
        sState = GS_CREDITS;
    }
}

void gameReshape(int w, int h)
{
    if (h < 1) h = 1;
    gWinW = w;
    gWinH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w / (double)h, 1.5, 20000.0);
    glMatrixMode(GL_MODELVIEW);
}

// ---- self capture: read the finished frame straight out of OpenGL ----------
// Handy for screenshots and for automated checks, and it does not depend on
// the desktop compositor at all (which can be in a bad state on some machines).
const char* gShotPath = nullptr;
float gShotDelay = 0.0f;
static bool sShotDone = false;
static bool sSaveShot = false;      // set by F2, saved at the end of the frame

// stb_image_write gives us PNG files with no extra library to install
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

// ---------------------------------------------------------------------
// measurement: --stats and --benchmark
// ---------------------------------------------------------------------
static const char* stateName(GameState st)
{
    switch (st)
    {
        case GS_TITLE:   return "title";
        case GS_DEMO:    return "demo";
        case GS_HELP:    return "help";
        case GS_CREDITS: return "credits";
        case GS_FLIGHT:  return "flight";
        case GS_PAUSE:   return "pause";
        case GS_RESULTS: return "results";
        case GS_PHOTO:   return "photo";
    }
    return "?";
}

static void printSceneStats()
{
    WorldStats w;
    TrafficStats t;
    worldGetStats(w);
    trafficGetStats(t);

    long mb = texturesDecodedBytes() / (1024L * 1024L);
    printf("\n");
    printf("[stats] world      buildings=%d trees=%d roadSegments=%d landmarks=%d chunks=%d displayLists=%d\n",
           w.buildings, w.trees, w.roadSegments, w.landmarks, w.chunks, w.displayLists);
    printf("[stats] geometry   boxes=%d cylinders=%d domes=%d spheres=%d primitiveTriangles=%ld\n",
           gCountBox, gCountCylinder, gCountDome, gCountSphere, gCountTriangles);
    printf("[stats] textures   loaded=%d decodedMB=%ld\n", texturesLoaded(), mb);
    printf("[stats] traffic    movingVehicles=%d parked=%d boats=%d birds=%d aiPlanes=%d walkers=%d bridgeCars=%d\n",
           t.movingVehicles, t.parkedVehicles, t.boats, t.birds, t.aiPlanes, t.walkers, t.bridgeCars);
    printf("[stats] window     %dx%d\n", gWinW, gWinH);
    fflush(stdout);
}

// one CSV row every half second; the summary line is what the benchmark script
// reads, the per-sample rows are what a graph in the paper is drawn from
static FILE* sBenchFile = nullptr;
static double sBenchStart = -1.0, sBenchLast = 0.0;
static float sBenchNext = 0.5f;
static int sBenchFrames = 0, sBenchSamples = 0;
static float sBenchMin = 0.0f, sBenchMax = 0.0f, sBenchSum = 0.0f;
static bool sBenchDone = false, sBenchShot = false;
static char sBenchPng[300] = "";

static void benchTick(double now)
{
    if (sBenchStart < 0.0)
    {
        sBenchStart = now;
        sBenchLast = now;
        sBenchFile = fopen(gBenchPath, "w");
        if (sBenchFile) fprintf(sBenchFile, "time_s,fps,state,x,y,z\n");
        else printf("[bench] cannot write %s\n", gBenchPath);
        snprintf(sBenchPng, sizeof(sBenchPng), "%s", gBenchPath);
        char* dot = strrchr(sBenchPng, '.');
        if (dot) strcpy(dot, ".png");
        else strcat(sBenchPng, ".png");
    }
    if (sBenchDone) return;

    sBenchFrames++;
    float elapsed = (float)(now - sBenchStart);
    if (elapsed < sBenchNext) return;

    float span = (float)(now - sBenchLast);
    float fps = (span > 1e-4f) ? (float)sBenchFrames / span : 0.0f;
    sBenchFrames = 0;
    sBenchLast = now;

    V3 p = sCamPos;
    if (sState == GS_DEMO) p = sDemoPos;
    else if (sState == GS_PHOTO) p = sPhotoPos;
    if (sBenchFile)
        fprintf(sBenchFile, "%.1f,%.1f,%s,%.0f,%.0f,%.0f\n",
                elapsed, fps, stateName(sState), p.x, p.y, p.z);

    if (sBenchSamples == 0) { sBenchMin = sBenchMax = fps; }
    else
    {
        if (fps < sBenchMin) sBenchMin = fps;
        if (fps > sBenchMax) sBenchMax = fps;
    }
    sBenchSum += fps;
    sBenchSamples++;
    sBenchNext += 0.5f;

    if (elapsed >= gBenchSeconds)
    {
        float avg = sBenchSamples ? sBenchSum / (float)sBenchSamples : 0.0f;
        if (sBenchFile) fclose(sBenchFile);
        sBenchFile = nullptr;
        printf("BENCH_SUMMARY avg=%.1f min=%.1f max=%.1f samples=%d seconds=%.1f\n",
               avg, sBenchMin, sBenchMax, sBenchSamples, elapsed);
        fflush(stdout);
        sBenchDone = true;
    }
}

// ---------------------------------------------------------------------
// recording: --video writes the running game into an MJPEG AVI
// ---------------------------------------------------------------------
static bool sVideoStarted = false, sVideoDone = false;
static int sVideoFrame = 0, sVideoFrames = 0;
static unsigned char* sVideoBuf = nullptr;

static void videoWriteFrame()
{
    const int w = gWinW, h = gWinH;
    const size_t stride = (size_t)w * 3;

    if (!sVideoStarted)
    {
        sVideoStarted = true;
        sVideoFrames = (int)(gVideoSeconds * (float)gVideoFps + 0.5f);
        if (sVideoFrames < 1) sVideoFrames = 1;
        if (!videoBegin(gVideoPath, w, h, gVideoFps, sVideoFrames, 85))
        {
            printf("[video] cannot write %s\n", gVideoPath);
            gVideoPath = nullptr;
            return;
        }
        printf("[video] %dx%d %d fps, %d frames -> %s\n", w, h, gVideoFps, sVideoFrames, gVideoPath);
        fflush(stdout);
    }

    if (!sVideoBuf)
    {
        sVideoBuf = (unsigned char*)malloc(stride * (size_t)h * 2);   // bottom-up read + top-down copy
        if (!sVideoBuf) { gVideoPath = nullptr; return; }
    }

    unsigned char* top = sVideoBuf;
    unsigned char* bot = sVideoBuf + stride * (size_t)h;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, bot);
    for (int y = 0; y < h; y++)
        memcpy(top + (size_t)y * stride, bot + (size_t)(h - 1 - y) * stride, stride);
    videoAddFrame(top);
    sVideoFrame++;

    if (sVideoFrame >= sVideoFrames)
    {
        videoEnd();
        sVideoDone = true;
        printf("VIDEO_DONE frames=%d file=%s\n", sVideoFrame, gVideoPath);
        fflush(stdout);
        return;
    }

    // advance by exactly one video frame, so the recording plays at the right
    // speed however fast or slow the machine renders
    float vdt = 1.0f / (float)gVideoFps;
    gTime += vdt;
    updateWorld(vdt);
}

// save one finished frame as a PNG (or BMP if the name asks for it)
static void writeFrameImage(const char* path, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    unsigned char* px = (unsigned char*)malloc((size_t)w * h * 3);
    if (!px) return;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);

    const char* dot = strrchr(path, '.');
    bool wantPng = dot && (tolower(dot[1]) == 'p');
    if (wantPng)
    {
        // stb writes top-down, OpenGL read bottom-up: flip the rows
        int stride = w * 3;
        unsigned char* flip = (unsigned char*)malloc((size_t)stride * h);
        if (flip)
        {
            for (int y = 0; y < h; y++)
                memcpy(flip + (size_t)y * stride, px + (size_t)(h - 1 - y) * stride, stride);
            stbi_write_png(path, w, h, 3, flip, stride);
            free(flip);
        }
        free(px);
        return;
    }

    FILE* f = fopen(path, "wb");
    if (f)
    {
        int rowSize = w * 3;
        int pad = (4 - (rowSize % 4)) % 4;
        int dataSize = (rowSize + pad) * h;
        int fileSize = 54 + dataSize;
        unsigned char hdr[54] = { 0 };
        hdr[0] = 'B'; hdr[1] = 'M';
        memcpy(hdr + 2, &fileSize, 4);
        int off = 54; memcpy(hdr + 10, &off, 4);
        int hdrSize = 40; memcpy(hdr + 14, &hdrSize, 4);
        memcpy(hdr + 18, &w, 4);
        memcpy(hdr + 22, &h, 4);
        short planes = 1; memcpy(hdr + 26, &planes, 2);
        short bpp = 24; memcpy(hdr + 28, &bpp, 2);
        memcpy(hdr + 34, &dataSize, 4);
        fwrite(hdr, 1, 54, f);
        unsigned char zero[3] = { 0, 0, 0 };
        for (int y = 0; y < h; y++)
        {
            // BMP rows are bottom-up, like glReadPixels returns them, but the
            // byte order has to be BGR
            for (int x = 0; x < w; x++)
            {
                const unsigned char* s = px + ((size_t)y * w + x) * 3;
                unsigned char bgr[3] = { s[2], s[1], s[0] };
                fwrite(bgr, 1, 3, f);
            }
            if (pad) fwrite(zero, 1, pad, f);
        }
        fclose(f);
    }
    free(px);
}

static void writeFrameBMP(const char* path, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    unsigned char* px = (unsigned char*)malloc((size_t)w * h * 3);
    if (!px) return;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // GL_BGR gives the byte order a BMP wants
    glReadPixels(0, 0, w, h, GL_BGR_EXT, GL_UNSIGNED_BYTE, px);

    FILE* f = fopen(path, "wb");
    if (f)
    {
        int rowSize = w * 3;
        int pad = (4 - (rowSize % 4)) % 4;
        int dataSize = (rowSize + pad) * h;
        int fileSize = 54 + dataSize;
        unsigned char hdr[54] = { 0 };
        hdr[0] = 'B'; hdr[1] = 'M';
        memcpy(hdr + 2, &fileSize, 4);
        int off = 54; memcpy(hdr + 10, &off, 4);
        int hdrSize = 40; memcpy(hdr + 14, &hdrSize, 4);
        memcpy(hdr + 18, &w, 4);
        memcpy(hdr + 22, &h, 4);
        short planes = 1; memcpy(hdr + 26, &planes, 2);
        short bpp = 24; memcpy(hdr + 28, &bpp, 2);
        memcpy(hdr + 34, &dataSize, 4);
        fwrite(hdr, 1, 54, f);
        unsigned char zero[3] = { 0, 0, 0 };
        // glReadPixels returns the rows bottom-up, which is exactly BMP order
        for (int y = 0; y < h; y++)
        {
            fwrite(px + (size_t)y * rowSize, 1, rowSize, f);
            if (pad) fwrite(zero, 1, pad, f);
        }
        fclose(f);
    }
    free(px);
}

void gameDisplay()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(sFov, (double)gWinW / (double)gWinH, 1.5, 20000.0);

    bool flightScene = (sState == GS_FLIGHT || sState == GS_PAUSE || sState == GS_RESULTS || sState == GS_PHOTO || sState == GS_BRIEF);
    V3 camPos;

    if (sState == GS_DEMO)
    {
        demoPose(sDemoPos, sDemoYaw, sDemoPitch, sDemoRoll);
        camPos = sDemoPos;
        setCamera(sDemoPos, sDemoYaw, sDemoPitch, sDemoRoll);
    }
    else if (sState == GS_PHOTO)
    {
        camPos = sPhotoPos;
        setCamera(sPhotoPos, sPhotoYaw, sPhotoPitch, 0.0f);
    }
    else if (flightScene)
    {
        camPos = sCamPos;
        setCamera(sCamPos, sCamYaw, sCamPitch, sCamRoll);
    }
    else
    {
        float t = (float)gTime;
        camPos = v3(cosf(t * 0.05f) * 2900.0f, 480.0f + sinf(t * 0.07f) * 50.0f, sinf(t * 0.05f) * 2900.0f);
        V3 d = norm(v3(0.0f, 55.0f, 0.0f) - camPos);
        setCamera(camPos, yawFromDir(v3(d.x, 0, d.z)), asinf(clampf(d.y, -1.0f, 1.0f)), 0.0f);
    }

    drawSky(camPos);
    worldSetCameraY(camPos.y);
    worldSetCameraXZ(camPos.x, camPos.z);
    worldDrawStatic();
    worldDrawWindsocks();       // animated, so they swing in every mode

    // the hands on Ali Amjad's clock tower really move (starting at 10:10)
    {
        double totalMin = 620.0 + gTime / 60.0;
        double hh = fmod(totalMin / 60.0, 12.0);
        double mm = fmod(totalMin, 60.0);
        worldDrawClockHands(-(float)(hh * 30.0 + mm * 0.5), -(float)(mm * 6.0));
    }

    trafficDraw();        // traffic runs in every state, including the demo
    if (sState == GS_FLIGHT || sState == GS_PAUSE || sState == GS_RESULTS)
    {
        enemyDraw();      // hostile fighters, their tracers and missiles
        groundDraw();     // depot, convoy, AAA, radar, bombs and rockets
    }
    if (sState != GS_DEMO)
    {
        // mission beacons, the player's aircraft and any crash debris belong to
        // the missions only - the demo is a camera, not a flight
        missionDrawWorld();
        drawPlayerShadow();
        if (sState == GS_FLIGHT && sCamMode == 1) craftDrawCockpitOverlay((float)gWinW / (float)gWinH);
        else craftDraw(gAircraft);
        drawParticles();
        if (sState == GS_FLIGHT && gShowLabels) hudDrawLabels();
    }

    // the HUD gunsight needs the camera that drew this frame
    hudCaptureProjection();

    // ----- 2D overlays -----
    bool clicked = sPendingClick;
    float mx = sMouseX, my = sMouseY;
    sPendingClick = false;

    switch (sState)
    {
        case GS_TITLE:
        {
            int act = hudTitle(mx, my, clicked);
            if (act == UI_QUIT) glutLeaveMainLoop();
            else if (act == UI_CONTROLS) { sHelpReturn = GS_TITLE; sState = GS_HELP; }
            else if (act == UI_CREDITS) { sState = GS_CREDITS; }
            else if (act == UI_INSTANT) { gameStartInstantAction(); }
            else if (act == UI_DEMO) { demoStart(); sState = GS_DEMO; cursorForState(); }
            else if (act == UI_FREEFLIGHT || act == UI_SCHOOL || act == UI_TOUR || act == UI_RINGS || act == UI_LANDING)
                gameStartMission(uiToMission(act));
            break;
        }
        case GS_BRIEF:
            hudBriefing();
            if (clicked) { sState = GS_FLIGHT; }
            break;
        case GS_DEMO:
            hudDrawDemo();
            if (clicked) leaveToTitle();
            break;
        case GS_HELP:
            hudHelp();
            if (clicked) { sState = sHelpReturn; cursorForState(); }
            break;
        case GS_CREDITS:
            hudCredits();
            if (clicked) { sState = GS_TITLE; cursorForState(); }
            break;
        case GS_FLIGHT:
            hudDrawFlight();
            break;
        case GS_PAUSE:
        {
            int act = hudPause(mx, my, clicked);
            if (act == UI_RESUME) { sState = GS_FLIGHT; cursorForState(); }
            else if (act == UI_RESTART) gameStartMission(gMission.id);
            else if (act == UI_CONTROLS) { sHelpReturn = GS_PAUSE; sState = GS_HELP; }
            else if (act == UI_MAINMENU) leaveToTitle();
            break;
        }
        case GS_RESULTS:
        {
            int act = hudResults(mx, my, clicked);
            if (act == UI_RESTART) gameStartMission(gMission.id);
            else if (act == UI_TOUR) gameStartMission(MIS_TOUR);
            else if (act == UI_MAINMENU) leaveToTitle();
            break;
        }
        case GS_PHOTO:
            hudDrawPhoto();
            break;
    }

    if (gShotPath && !sShotDone && gTime >= gShotDelay)
    {
        sShotDone = true;
        writeFrameImage(gShotPath, gWinW, gWinH);
        // the benchmark reads this line, so a capture doubles as a measurement
        printf("BENCH t=%.1f fps=%.1f x=%.0f y=%.0f z=%.0f\n",
               gTime, gFps, gAircraft.pos.x, gAircraft.pos.y, gAircraft.pos.z);
        fflush(stdout);
        glutLeaveMainLoop();
        return;
    }
    if (sSaveShot)
    {
        sSaveShot = false;
        writeFrameImage("shot_manual.png", gWinW, gWinH);
        snprintf(gToastText, 64, "Screenshot saved to shot_manual.png");
        gToastTimer = 2.5f;
    }
    if (sBenchDone && !sBenchShot)
    {
        // the last measured frame doubles as the picture for the case
        sBenchShot = true;
        writeFrameImage(sBenchPng, gWinW, gWinH);
        glutLeaveMainLoop();
        return;
    }
    if (gVideoPath && !sVideoDone)
    {
        videoWriteFrame();          // draw -> grab -> step one video frame
        if (sVideoDone) { glutLeaveMainLoop(); return; }
    }
    glutSwapBuffers();
}

// ---------------------------------------------------------------------
// input callbacks
// ---------------------------------------------------------------------
void gameTimerTick()
{
    double now = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    float rawDt = (float)(now - sLastTime);
    sLastTime = now;

    // Recording: the world is stepped one frame per drawn frame, inside
    // gameDisplay, so the file plays at the right speed whatever the frame rate.
    if (gVideoPath && !sVideoDone)
    {
        glutPostRedisplay();
        return;
    }

    gTime = now;
    float dt = rawDt;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.08f) dt = 0.08f;

    if (rawDt > 0.0005f) gFps += (1.0f / rawDt - gFps) * 0.08f;
    updateWorld(dt);
    if (gBenchPath) benchTick(now);
    if (gProbe && now >= sProbeNext)
    {
        sProbeNext = now + 0.5;
        FILE* f = fopen("probe.csv", "a");
        if (f)
        {
            fprintf(f, "%.2f,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.2f,%.0f,%.1f,%s\n",
                    now, gAircraft.pos.x, gAircraft.pos.y, gAircraft.pos.z,
                    gAircraft.airspeed * 1.94384f, gAircraft.yaw, gAircraft.pitch, gAircraft.roll,
                    gAircraft.inPitch, gAircraft.inRoll, sStickX, sStickY,
                    (int)gAircraft.stall, (int)gAircraft.onGround, gAircraft.throttle,
                    gAircraft.rpm, gAircraft.fuel, gAircraft.landingText);
            // (frame rate is appended by the benchmark tool through its own parser)
            fclose(f);
        }
    }
    glutPostRedisplay();
}

void gameKeyDown(unsigned char k, int x, int y)
{
    (void)x; (void)y;
    unsigned char c = (unsigned char)tolower(k);
    sKeys[c] = true;

    if (c == 27)   // Esc
    {
        if (sState == GS_FLIGHT) { sState = GS_PAUSE; cursorForState(); }
        else if (sState == GS_PAUSE) { sState = GS_FLIGHT; cursorForState(); }
        else if (sState == GS_PHOTO) { sState = GS_FLIGHT; cursorForState(); }
        else if (sState == GS_HELP) { sState = sHelpReturn; cursorForState(); }
        else if (sState == GS_CREDITS) { sState = GS_TITLE; cursorForState(); }
        else if (sState == GS_DEMO) leaveToTitle();
        else if (sState == GS_RESULTS) leaveToTitle();
        else if (sState == GS_TITLE) glutLeaveMainLoop();
        return;
    }

    // any other key leaves the help and credits pages - nobody should get stuck
    if (sState == GS_HELP) { sState = sHelpReturn; cursorForState(); return; }
    if (sState == GS_CREDITS) { sState = GS_TITLE; cursorForState(); return; }
    if (sState == GS_BRIEF) { sState = GS_FLIGHT; return; }   // any key launches
    if (sState == GS_DEMO) { leaveToTitle(); return; }

    // ---- photo mode -----------------------------------------------------
    // The camera keys (WASD/QE) are already held in sKeys; these are the
    // controls that change how the photo is taken.
    if (sState == GS_PHOTO)
    {
        if (c == 'p' || c == 'c')
        {
            sState = GS_FLIGHT;
            cursorForState();
        }
        else if (c == 'h') gPhotoHud = !gPhotoHud;
        else if (c == 'g') gPhotoGrid = !gPhotoGrid;
        else if (c == 'z')
        {
            static const float SCALES[4] = { 1.0f, 0.35f, 0.12f, 0.0f };
            gPhotoTime = (gPhotoTime + 1) % 4;
            gPhotoTimeScale = SCALES[gPhotoTime];
        }
        return;
    }

    if (sState == GS_TITLE)
    {
        if (c == '1') gameStartMission(MIS_TOUR);        // Defend the City
        else if (c == '2') gameStartMission(MIS_RINGS);  // Deck Run
        else if (c == '3') gameStartMission(MIS_LANDING);// Strike and Return
        else if (c == '4') gameStartMission(MIS_SCHOOL); // Training Range
        else if (c == 'd') { demoStart(); sState = GS_DEMO; cursorForState(); }
        else if (c == '[') { gDifficulty = (gDifficulty + 2) % 3; snprintf(gToastText, 64, "Difficulty: %s", gDifficulty == 0 ? "Easy" : (gDifficulty == 1 ? "Normal" : "Ace")); }
        else if (c == ']') { gDifficulty = (gDifficulty + 1) % 3; snprintf(gToastText, 64, "Difficulty: %s", gDifficulty == 0 ? "Easy" : (gDifficulty == 1 ? "Normal" : "Ace")); }
    }

    if (sState == GS_FLIGHT)
    {
        if (c == 'c') sCamMode = (sCamMode + 1) % 2;
        else if (c == 'm') gMapZoom = (gMapZoom + 1) % 3;
        else if (c == '[') { gMouseSens = clampf(gMouseSens - 0.15f, 0.35f, 3.0f); snprintf(gToastText, 64, "Mouse sensitivity  %.2fx", gMouseSens); gToastTimer = 2.0f; }
        else if (c == ']') { gMouseSens = clampf(gMouseSens + 0.15f, 0.35f, 3.0f); snprintf(gToastText, 64, "Mouse sensitivity  %.2fx", gMouseSens); gToastTimer = 2.0f; }
        else if (c == ',') { gAircraft.trim = clampf(gAircraft.trim - 0.06f, -1.0f, 1.0f); snprintf(gToastText, 64, "Cruise trim  %+.2f", gAircraft.trim); gToastTimer = 1.6f; }
        else if (c == '.') { gAircraft.trim = clampf(gAircraft.trim + 0.06f, -1.0f, 1.0f); snprintf(gToastText, 64, "Cruise trim  %+.2f", gAircraft.trim); gToastTimer = 1.6f; }
        else if (c == 'l') gShowLabels = !gShowLabels;
        else if (c == 'q')
        {
            // quality: the shadow pass is the expensive part, so it toggles
            gDrawShadows = !gDrawShadows;
            snprintf(gToastText, 64, "Shadows %s  (quality %s)", gDrawShadows ? "on" : "off",
                     gDrawShadows ? "high" : "fast");
            gToastTimer = 2.0f;
        }
        else if (c == 'f') cycleFlaps();
        else if (c == 'i')
        {
            gInvertPitch = !gInvertPitch;
            snprintf(gToastText, 64, "Pitch axis: %s", gInvertPitch ? "inverted" : "normal");
            gToastTimer = 2.0f;
        }
        else if (c == 'r') gameStartMission(gMission.id);
        else if (c == 'h') { sHelpReturn = GS_FLIGHT; sState = GS_HELP; cursorForState(); }
        else if (c >= '1' && c <= '9') gameJumpToLandmark(c - '1');
        else if (c == '0') gameJumpToLandmark(9);
        else if (c == 'b')
        {
            if (weaponDropBomb()) { snprintf(gToastText, 64, "Bomb away - %d left", weaponBombs()); }
            else snprintf(gToastText, 64, "No bombs left");
            gToastTimer = 1.6f;
        }
        else if (c == 'g')
        {
            if (weaponFireRockets()) { snprintf(gToastText, 64, "Rockets away - %d left", weaponRockets()); }
            else snprintf(gToastText, 64, "No rockets left");
            gToastTimer = 1.6f;
        }
        else if (c == 'k')
        {
            soundToggleMute();
            snprintf(gToastText, 64, "Sound %s", soundMuted() ? "off" : "on");
            gToastTimer = 1.6f;
        }
        else if (c == 'v')
        {
            if (weaponFireMissile())
            {
                snprintf(gToastText, 64, "Missile away - %d left", weaponMissiles());
                gToastTimer = 2.0f;
            }
            else
            {
                snprintf(gToastText, 64, weaponMissiles() ? "No lock - put the target in the reticle" : "No missiles left");
                gToastTimer = 1.6f;
            }
        }
        else if (c == 9)      // Tab
        {
            weaponCycleTarget();
        }
        else if (c == 'x')
        {
            if (enemyFireFlare())
            {
                soundPlay(SND_MISSILE, 0.45f);
                snprintf(gToastText, 64, "Flare away - %d left", enemyFlares());
            }
            else
                snprintf(gToastText, 64, "No flares left");
            gToastTimer = 2.0f;
        }
        else if (c == 'n')
        {
            sLandmarkIdx = (sLandmarkIdx + 1) % gLandmarkCount;
            gameJumpToLandmark(sLandmarkIdx);
        }
        else if (c == 'p')
        {
            sState = GS_PHOTO;
            sPhotoPos = sCamPos;
            sPhotoYaw = sCamYaw;
            sPhotoPitch = sCamPitch;
            gPhotoHud = true;
            gPhotoGrid = false;
            gPhotoTime = 0;
            gPhotoTimeScale = 1.0f;
        }
    }
    else if (sState == GS_PAUSE && c == 'r')
    {
        gameStartMission(gMission.id);
    }
}

void gameKeyUp(unsigned char k, int x, int y)
{
    (void)x; (void)y;
    sKeys[(unsigned char)tolower(k)] = false;
}

void gameSpecialDown(int k, int x, int y)
{
    (void)x; (void)y;
    if (k >= 0 && k < 256) sSpec[k] = true;
    if (k == GLUT_KEY_F11) glutFullScreenToggle();
    if (k == GLUT_KEY_F2) sSaveShot = true;
    if (k == GLUT_KEY_F3) gShowFps = !gShowFps;
}

void gameSpecialUp(int k, int x, int y)
{
    (void)x; (void)y;
    if (k >= 0 && k < 256) sSpec[k] = false;
}

void gameMouseButton(int button, int state, int x, int y)
{
    sMouseX = (float)x;
    sMouseY = (float)y;
    if (state != GLUT_DOWN) return;

    if (button == GLUT_LEFT_BUTTON)
    {
        if (sState == GS_FLIGHT)
        {
            sStickX = 0.0f;      // recentre the virtual stick
            sStickY = 0.0f;
        }
        else
        {
            sPendingClick = true;
            sClickX = (float)x;
            sClickY = (float)y;
        }
    }
    else if (button == GLUT_RIGHT_BUTTON)
    {
        if (sState == GS_FLIGHT) sCamMode = (sCamMode + 1) % 2;
    }
}

static void motionCommon(int x, int y)
{
    sMouseX = (float)x;
    sMouseY = (float)y;
    if (sState == GS_FLIGHT)
    {
        float cx = gWinW * 0.5f, cy = gWinH * 0.5f;
        float dx = x - cx, dy = y - cy;
        if (fabsf(dx) > 220.0f || fabsf(dy) > 220.0f)
        {
            // a huge jump means the pointer was teleported (window just appeared,
            // alt-tabbed back, etc.) - recentre without jerking the stick
            glutWarpPointer((int)cx, (int)cy);
        }
        else if (dx != 0.0f || dy != 0.0f)
        {
            sStickRawX = clampf(sStickRawX + dx * 0.0035f * gMouseSens, -1.3f, 1.3f);
            sStickRawY = clampf(sStickRawY + dy * 0.0035f * gMouseSens * (gInvertPitch ? -1.0f : 1.0f), -1.3f, 1.3f);
            glutWarpPointer((int)cx, (int)cy);
        }
    }
    else if (sState == GS_PHOTO)
    {
        float cx = gWinW * 0.5f, cy = gWinH * 0.5f;
        float dx = x - cx, dy = y - cy;
        if (fabsf(dx) > 220.0f || fabsf(dy) > 220.0f)
        {
            glutWarpPointer((int)cx, (int)cy);
        }
        else if (dx != 0.0f || dy != 0.0f)
        {
            sPhotoYaw = wrapPi(sPhotoYaw + dx * 0.0032f);
            sPhotoPitch = clampf(sPhotoPitch - dy * 0.0028f, -1.35f, 1.35f);
            glutWarpPointer((int)cx, (int)cy);
        }
    }
}

void gameMouseMotion(int x, int y) { motionCommon(x, y); }
void gamePassiveMotion(int x, int y) { motionCommon(x, y); }

void gameMouseWheel(int wheel, int dir, int x, int y)
{
    (void)wheel; (void)x; (void)y;
    if (sState == GS_FLIGHT || sState == GS_PHOTO)
    {
        gAircraft.throttle = clampf(gAircraft.throttle + 0.06f * dir, 0.0f, 1.0f);
    }
}
