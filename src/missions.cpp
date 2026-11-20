// missions.cpp - mission logic, goal beacons and the ring course
#include "missions.h"
#include "aircraft.h"
#include "glutil.h"
#include "world.h"
#include "enemy.h"
#include <cmath>
#include <cstdio>
#include <cstring>

MissionState gMission;

// ---------------------------------------------------------------------
// tables
// ---------------------------------------------------------------------
static const char* NAME[MIS_COUNT] = {
    "Patrol",
    "BAF Training Range",
    "Defend the City",
    "Deck Run",
    "Strike and Return"
};

static const char* DESC[MIS_COUNT] = {
    "Patrol the city with four bandits in the air. Land anywhere for a landing rating.",
    "Bangladesh Air Force training range - the peaceful mission. No hostiles: engine start, taxi, take off, fly the pattern and land.",
    "Enemy fighters are working over the city. Show the damage at five landmarks and drive the bandits off while you do it.",
    "Deck Run: three gates low over the Surma and Keane Bridge, with anti-aircraft fire from the banks.",
    "Strike and Return: hit the convoy and the depot on the airport road, then land on runway 05 - the landing grade is half the score."
};

// ring course: (x, z, altitude)
// Three rings instead of twelve: a short, sharp course that can be flown in
// about a minute, which is what a demonstration needs.
#define RING_COUNT 3
static const float RINGS[RING_COUNT][3] = {
    { -1600.0f,  -900.0f, 160.0f },   // over the Surma, west of the bridge
    {  -200.0f,   150.0f, 150.0f },   // straight over Keane Bridge
    {  1400.0f,   300.0f, 190.0f }    // climbing away towards the campus
};

// ---------------------------------------------------------------------
// creation
// ---------------------------------------------------------------------
static MissionTarget mkTarget(float x, float z, float y, float radius, const char* name)
{
    MissionTarget t;
    t.x = x; t.z = z; t.y = y; t.radius = radius;
    strncpy(t.name, name, 47);
    t.name[47] = 0;
    t.done = false;
    return t;
}

// Defend the City: three raids, one after another
static int sRaidsDone = 0;
static bool sRaidActive = false;
static float sRaidWait = 0.0f;

// how many raiders each raid sends: easy 2, normal 3, ace 4
static int difficultyRaiderCount()
{
    extern int gDifficulty;
    return (gDifficulty == 0) ? 2 : (gDifficulty == 2 ? 4 : 3);
}

MissionState missionCreate(MissionId id)
{
    MissionState m;
    memset(&m, 0, sizeof(m));
    m.id = id;
    m.active = true;
    m.step = 0;
    m.time = 0;
    m.endTimer = 0;
    m.score = 0;
    m.stars = 0;
    m.lastLandingSeen = gAircraft.landingEvent;   // ignore landings from before the mission
    m.resultLine[0] = 0;

    if (id == MIS_SCHOOL)
    {
        // the hold point sits at the runway entrance, a short taxi from the apron
        V3 hold = worldRunwayPoint(-200.0f, 26.0f);
        m.targets[0] = mkTarget(hold.x, hold.z, 0, 55.0f, "runway hold point");
        m.countTotal = 0;
    }
    else if (id == MIS_TOUR)
    {
        m.targets[0] = mkTarget(gLandmarks[0].x, gLandmarks[0].z, 200.0f, 200.0f, gLandmarks[0].name);
        m.targets[1] = mkTarget(gLandmarks[1].x, gLandmarks[1].z, 170.0f, 175.0f, gLandmarks[1].name);
        m.targets[2] = mkTarget(gLandmarks[2].x, gLandmarks[2].z, 230.0f, 260.0f, gLandmarks[2].name);
        m.targets[3] = mkTarget(gLandmarks[3].x, gLandmarks[3].z, 200.0f, 230.0f, gLandmarks[3].name);
        m.targets[4] = mkTarget(gLandmarks[5].x, gLandmarks[5].z, 170.0f, 220.0f, gLandmarks[5].name);
        m.countTotal = 5;
    }
    else if (id == MIS_TOUR)
    {
        // three raids, each aimed at one of the first three landmarks
        m.countTotal = 3;
        sRaidsDone = 0;
        sRaidActive = false;
        sRaidWait = 6.0f;
    }
    else if (id == MIS_RINGS)
    {
        for (int i = 0; i < RING_COUNT; i++)
        {
            char nm[48];
            snprintf(nm, 48, "Ring %d", i + 1);
            m.targets[i] = mkTarget(RINGS[i][0], RINGS[i][1], RINGS[i][2], 42.0f, nm);
        }
        m.countTotal = RING_COUNT;
        m.ringsPassed = 0;
    }
    else if (id == MIS_LANDING)
    {
        V3 aim = worldRunwayPoint(-1450.0f, 0.0f);
        m.targets[0] = mkTarget(aim.x, aim.z, 0, 200.0f, "runway 05 threshold");
        m.countTotal = 0;
    }
    return m;
}

const char* missionName(MissionId id) { return NAME[id]; }
const char* missionDescription(MissionId id) { return DESC[id]; }

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------
static float distXZtoTarget(const Aircraft& a, const MissionTarget& t)
{
    float dx = a.pos.x - t.x, dz = a.pos.z - t.z;
    return sqrtf(dx * dx + dz * dz);
}

static void finishMission(const Aircraft& a)
{
    gMission.complete = true;
    gMission.endTimer = 0.0f;
    float score;
    int q = a.landingQuality < 0 ? 2 : a.landingQuality;   // no landing seen: treat as firm
    switch (gMission.id)
    {
        case MIS_SCHOOL:
            score = 500.0f + q * 150.0f + fmaxf(0.0f, 300.0f - gMission.time * 0.6f);
            snprintf(gMission.resultLine, 96, "Pattern completed - landing grade: %s", q == 3 ? "butter" : q == 2 ? "firm" : "rough");
            break;
        case MIS_TOUR:
            score = sRaidsDone * 320.0f + landmarkIntegrity() * 4.0f + q * 140.0f;
            snprintf(gMission.resultLine, 96, "%d of 3 raids stopped, city %d%%", sRaidsDone, (int)landmarkIntegrity());
            break;
        case MIS_RINGS:
            score = gMission.ringsPassed * 100.0f + fmaxf(0.0f, 900.0f - gMission.time * 4.0f) + q * 140.0f;
            snprintf(gMission.resultLine, 96, "%d of %d rings threaded", gMission.ringsPassed, RING_COUNT);
            break;
        case MIS_LANDING:
            score = q * 300.0f + a.health + 100.0f;
            snprintf(gMission.resultLine, 96, "Touchdown at %s", q == 3 ? "butter smooth" : q == 2 ? "a firm rate" : "a heavy rate");
            break;
        default:
            score = 0;
            break;
    }
    gMission.score = score;
    gMission.stars = score >= 1000.0f ? 3 : (score >= 700.0f ? 2 : (score >= 350.0f ? 1 : 0));
}

static void failMission()
{
    if (gMission.complete || gMission.failed) return;
    gMission.failed = true;
    gMission.endTimer = 0.0f;
    gMission.score = 0;
    gMission.stars = 0;
    strncpy(gMission.resultLine, "The plane did not make it back.", 95);
}

// ---------------------------------------------------------------------
// update
// ---------------------------------------------------------------------
void missionUpdate(float dt)
{
    if (!gMission.active) return;
    if (gMission.complete || gMission.failed)
    {
        gMission.endTimer += dt;
        return;
    }
    Aircraft& a = gAircraft;
    gMission.time += dt;

    if (a.phase == PHASE_CRASHED)
    {
        failMission();
        return;
    }

    // detect new touch-downs
    bool landed = false;
    if (a.landingEvent != gMission.lastLandingSeen)
    {
        gMission.lastLandingSeen = a.landingEvent;
        landed = true;
    }

    switch (gMission.id)
    {
        case MIS_FREE:
            break;

        case MIS_SCHOOL:
            if (gMission.step == 0)
            {
                if (a.engineOn) gMission.step = 1;
            }
            else if (gMission.step == 1)
            {
                if (distXZtoTarget(a, gMission.targets[0]) < gMission.targets[0].radius)
                {
                    gMission.targets[0].done = true;
                    gMission.step = 2;
                }
            }
            else if (gMission.step == 2)
            {
                if (!a.onGround && a.pos.y > 80.0f) gMission.step = 3;
            }
            else if (gMission.step == 3)
            {
                if (gMission.countTotal == 0)
                {
                    // lay out the four pattern gates the first time we get here
                    V3 g0 = worldRunwayPoint(1000.0f, 650.0f);
                    V3 g1 = worldRunwayPoint(-1000.0f, 650.0f);
                    V3 g2 = worldRunwayPoint(-1000.0f, -650.0f);
                    V3 g3 = worldRunwayPoint(1000.0f, -650.0f);
                    gMission.targets[1] = mkTarget(g0.x, g0.z, 300.0f, 260.0f, "pattern gate 1");
                    gMission.targets[2] = mkTarget(g1.x, g1.z, 300.0f, 260.0f, "pattern gate 2");
                    gMission.targets[3] = mkTarget(g2.x, g2.z, 300.0f, 260.0f, "pattern gate 3");
                    gMission.targets[4] = mkTarget(g3.x, g3.z, 300.0f, 260.0f, "pattern gate 4");
                    gMission.countTotal = 4;
                }
                int idx = 1 + gMission.countDone;
                MissionTarget& t = gMission.targets[idx];
                if (distXZtoTarget(a, t) < t.radius && a.pos.y > 110.0f && a.pos.y < 560.0f)
                {
                    t.done = true;
                    gMission.countDone++;
                    if (gMission.countDone >= 4) gMission.step = 4;
                }
            }
            else if (gMission.step == 4)
            {
                if (landed) finishMission(a);
            }
            break;

        case MIS_TOUR:
            if (gMission.step == 0)
            {
                if (sRaidsDone >= gMission.countTotal)
                {
                    gMission.step = 1;
                }
                else if (!sRaidActive)
                {
                    sRaidWait -= dt;
                    if (sRaidWait <= 0.0f)
                    {
                        // send the next raid at its landmark
                        int n = (difficultyRaiderCount());
                        V3 aim = v3(gMission.targets[sRaidsDone].x, 40.0f, gMission.targets[sRaidsDone].z);
                        enemyBeginRaid(aim, n);
                        sRaidActive = true;
                    }
                }
                else if (enemyRaidersAlive() == 0)
                {
                    sRaidActive = false;
                    sRaidsDone++;
                    gMission.countDone = sRaidsDone;
                    gMission.targets[sRaidsDone > 0 ? sRaidsDone - 1 : 0].done = true;
                    sRaidWait = 9.0f;
                }

                // the city itself has to survive
                if (landmarkIntegrity() < 45.0f) failMission();
            }
            else if (gMission.step == 1)
            {
                if (landed) finishMission(a);
            }
            break;

        case MIS_RINGS:
            if (gMission.step == 0)
            {
                MissionTarget& t = gMission.targets[gMission.ringsPassed];
                float dx = a.pos.x - t.x, dy = a.pos.y - t.y, dz = a.pos.z - t.z;
                if (sqrtf(dx * dx + dy * dy + dz * dz) < t.radius)
                {
                    t.done = true;
                    gMission.ringsPassed++;
                    if (gMission.ringsPassed >= RING_COUNT) gMission.step = 1;
                }
            }
            else if (gMission.step == 1)
            {
                if (landed) finishMission(a);
            }
            break;

        case MIS_LANDING:
            if (gMission.step == 0)
            {
                if (landed) finishMission(a);
            }
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------
// HUD text
// ---------------------------------------------------------------------
const char* missionObjective()
{
    static char buf[192];
    switch (gMission.id)
    {
        case MIS_FREE:
            return "Free flight - land anywhere for a rating";
        case MIS_SCHOOL:
            switch (gMission.step)
            {
                case 0: return "STEP 1: Start the engine - press E";
                case 1: return "STEP 2: Taxi to the yellow beacon";
                case 2: return "STEP 3: Full throttle (W), pull up past 60 kt";
                case 3: snprintf(buf, 192, "STEP 4: Fly through gate %d of 4", gMission.countDone + 1); return buf;
                default: return "STEP 5: Line up with runway 05 and land";
            }
        case MIS_TOUR:
            if (gMission.step == 0)
            {
                int inbound = enemyRaidersAlive();
                if (inbound > 0)
                    snprintf(buf, 192, "Raid %d of 3 - %d bomber%s inbound to %s  (city %d%%)",
                             sRaidsDone + 1, inbound, inbound == 1 ? "" : "s",
                             gMission.targets[sRaidsDone < 3 ? sRaidsDone : 2].name,
                             (int)landmarkIntegrity());
                else
                    snprintf(buf, 192, "Raid %d of 3 - next raid in %d s  (city %d%%)",
                             sRaidsDone + 1, (int)(sRaidWait > 0.0f ? sRaidWait : 0.0f), (int)landmarkIntegrity());
                return buf;
            }
            return "City saved - now land anywhere to finish";
        case MIS_RINGS:
            if (gMission.step == 0)
            {
                snprintf(buf, 192, "Ring %d of %d - fly through the yellow ring", gMission.ringsPassed + 1, RING_COUNT);
                return buf;
            }
            return "All rings threaded - now land to finish";
        case MIS_LANDING:
            return "Land on runway 05 as gently as you can";
        default: break;
    }
    return "";
}

bool missionTargetPoint(V3& out, float& radius)
{
    switch (gMission.id)
    {
        case MIS_SCHOOL:
            if (gMission.step == 1) { out = v3(gMission.targets[0].x, 0, gMission.targets[0].z); radius = gMission.targets[0].radius; return true; }
            if (gMission.step == 3 && gMission.countTotal > 0)
            {
                MissionTarget& t = gMission.targets[1 + gMission.countDone];
                out = v3(t.x, t.y, t.z); radius = t.radius; return true;
            }
            if (gMission.step == 4)
            {
                V3 p = worldRunwayPoint(-1000.0f, 0.0f);
                out = v3(p.x, 0, p.z); radius = 200.0f; return true;
            }
            return false;
        case MIS_TOUR:
            if (gMission.step == 0)
            {
                // point at the raiders, not at the landmark: that is what you hunt
                V3 cs[12];
                int nc = enemyContacts(cs, 12);
                int best = -1; float bd = 1e9f;
                for (int i = 0; i < nc; i++)
                {
                    float d = len(cs[i] - gAircraft.pos);
                    if (d < bd) { bd = d; best = i; }
                }
                if (best >= 0) { out = cs[best]; radius = 30.0f; return true; }

                MissionTarget& t = gMission.targets[gMission.countDone < 3 ? gMission.countDone : 2];
                out = v3(t.x, t.y, t.z); radius = t.radius; return true;
            }
            {
                V3 p = worldAirportCenter();
                out = p; radius = 800.0f; return true;
            }
        case MIS_RINGS:
            if (gMission.step == 0)
            {
                MissionTarget& t = gMission.targets[gMission.ringsPassed];
                out = v3(t.x, t.y, t.z); radius = t.radius; return true;
            }
            {
                V3 p = worldAirportCenter();
                out = p; radius = 800.0f; return true;
            }
        case MIS_LANDING:
        {
            V3 p = worldRunwayPoint(-1450.0f, 0.0f);
            out = v3(p.x, 0, p.z); radius = 200.0f; return true;
        }
        default: break;
    }
    return false;
}

// ---------------------------------------------------------------------
// world drawing (beacons, rings, gates)
// ---------------------------------------------------------------------
static void beacon(float x, float z, float r, float height, float rr, float gg, float bb, float alpha)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);          // a marker column must never hide what is behind it
    setColorA(rr, gg, bb, alpha);
    glPushMatrix();
    glTranslatef(x, 0, z);
    drawCylinderRaw(r * 0.35f, r, height, 12, false);
    glPopMatrix();
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------
// ring-mode effects: a burst when a ring is threaded, and a fading trail
// ---------------------------------------------------------------------
struct RingPop { V3 p; float age; };
static RingPop sPops[6];
static int sPopCount = 0;
static V3 sTrail[80];
static int sTrailCount = 0;
static float sTrailT = 0.0f;
static int sLastRingsPassed = 0;

static void ringPushPop(V3 p)
{
    if (sPopCount < 6) sPops[sPopCount++] = { p, 0.0f };
    else
    {
        for (int i = 1; i < 6; i++) sPops[i - 1] = sPops[i];
        sPops[5] = { p, 0.0f };
    }
}

static void ringDrawEffects(float t, float dtGuess)
{
    // pops
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (int i = 0; i < sPopCount; i++)
    {
        RingPop& p = sPops[i];
        p.age += dtGuess;
        if (p.age > 0.75f) continue;
        float k = p.age / 0.75f;
        float s = 1.0f + k * 1.9f;
        setColorA(1.0f, 0.90f, 0.35f, (1.0f - k) * 0.75f);
        glPushMatrix();
        glTranslatef(p.p.x, p.p.y, p.p.z);
        glScalef(s, s, s);
        drawTorus(34.0f, 2.0f, 6, 22);
        glPopMatrix();
    }
    // trail
    if (sTrailCount > 2)
    {
        glLineWidth(3.0f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < sTrailCount; i++)
        {
            float f = (float)i / (float)(sTrailCount - 1);
            setColorA(0.45f, 0.85f, 1.0f, 0.45f * f);
            glVertex3f(sTrail[i].x, sTrail[i].y, sTrail[i].z);
        }
        glEnd();
        glLineWidth(1.0f);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void missionDrawWorld()
{
    if (!gMission.active) return;
    float t = gMission.time;

    if (gMission.id == MIS_RINGS)
    {
        // detect a ring just threaded, for the burst effect
        if (gMission.ringsPassed != sLastRingsPassed)
        {
            if (gMission.ringsPassed == 0) sTrailCount = 0;          // mission restarted
            else if (gMission.ringsPassed > sLastRingsPassed)
            {
                int idx = (gMission.ringsPassed - 1) % RING_COUNT;
                ringPushPop(v3(RINGS[idx][0], RINGS[idx][2], RINGS[idx][1]));
            }
            sLastRingsPassed = gMission.ringsPassed;
        }
        // trail: remember where the aircraft has been
        if (t - sTrailT > 0.09f)
        {
            sTrailT = t;
            V3 p = gAircraft.pos;
            if (sTrailCount > 0)
            {
                V3 d = p - sTrail[sTrailCount - 1];
                if (len(d) < 3.0f) { /* too close, skip */ }
                else
                {
                    if (sTrailCount < 80) sTrail[sTrailCount++] = p;
                    else
                    {
                        for (int i = 1; i < 80; i++) sTrail[i - 1] = sTrail[i];
                        sTrail[79] = p;
                    }
                }
            }
            else sTrail[sTrailCount++] = p;
        }

        for (int i = 0; i < RING_COUNT; i++)
        {
            MissionTarget& tg = gMission.targets[i];
            bool current = (i == gMission.ringsPassed) && (gMission.step == 0);
            bool passed = tg.done;
            bool nextUp = (i == gMission.ringsPassed + 1) && (gMission.step == 0);
            if (!current && !passed && !nextUp) continue;

            // orientation: from the previous ring to the next one
            const float* prev = RINGS[(i + RING_COUNT - 1) % RING_COUNT];
            const float* next = RINGS[(i + 1) % RING_COUNT];
            V3 d = norm(v3(next[0] - prev[0], next[2] - prev[2], next[1] - prev[1]));
            float yawDeg = rad2deg(yawFromDir(v3(d.x, 0, d.z)));
            float pitchDeg = rad2deg(asinf(clampf(d.y, -1.0f, 1.0f)));

            float scale = current ? (1.0f + 0.06f * sinf(t * 4.0f)) : 1.0f;
            if (current)      setColor(1.00f, 0.85f, 0.15f);
            else if (passed)  setColor(0.30f, 0.75f, 0.35f);
            else              setColor(0.55f, 0.62f, 0.72f);

            if (current) beacon(tg.x, tg.z, 20.0f, 420.0f, 1.0f, 0.85f, 0.2f, 0.10f);
            else if (nextUp) beacon(tg.x, tg.z, 10.0f, 140.0f, 0.75f, 0.82f, 0.95f, 0.04f);

            glPushMatrix();
            glTranslatef(tg.x, tg.y, tg.z);
            glRotatef(-yawDeg, 0, 1, 0);
            glRotatef(pitchDeg, 1, 0, 0);
            glScalef(scale, scale, scale);

            // faint portal disc inside the gate
            if (current)
            {
                glDisable(GL_LIGHTING);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                setColorA(1.0f, 0.85f, 0.25f, 0.10f + 0.03f * sinf(t * 3.0f));
                glPushMatrix();
                glRotatef(90.0f, 1, 0, 0);
                drawDiskXZ(32.0f, 26);
                glPopMatrix();
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_LIGHTING);
            }

            drawTorus(34.0f, 2.2f, 8, 26);

            // a short tunnel of rings behind the gate: it reads as depth and
            // shows which way to fly through
            if (current)
            {
                glDisable(GL_LIGHTING);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                for (int k = 1; k <= 2; k++)
                {
                    float sc = 1.0f - k * 0.10f;
                    setColorA(1.0f, 0.88f, 0.30f, 0.30f - k * 0.09f);
                    glPushMatrix();
                    glTranslatef(0, 0, -k * 11.0f);
                    glScalef(sc, sc, sc);
                    drawTorus(34.0f, 1.1f, 6, 22);
                    glPopMatrix();
                }
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_LIGHTING);
            }

            // the ring number, big, in the middle of the gate
            {
                char num[8];
                snprintf(num, sizeof(num), "%d", i + 1);
                glDisable(GL_LIGHTING);
                glPushMatrix();
                glTranslatef(0.0f, -13.0f, 0.6f);
                glScalef(0.32f, 0.32f, 0.32f);
                glLineWidth(2.0f);
                setColor(0.12f, 0.12f, 0.14f);
                strokeTextCentered(GLUT_STROKE_ROMAN, num, 1.13f);
                glLineWidth(3.4f);
                if (current) setColor(1.0f, 0.96f, 0.78f);
                else if (passed) setColor(0.55f, 0.92f, 0.60f);
                else setColor(0.85f, 0.90f, 1.0f);
                strokeTextCentered(GLUT_STROKE_ROMAN, num, 1.0f);
                glLineWidth(1.0f);
                glPopMatrix();
                glEnable(GL_LIGHTING);
            }

            glPopMatrix();


        }

        ringDrawEffects(t, 1.0f / 60.0f);
    }
    else if (gMission.id == MIS_TOUR)
    {
        for (int i = 0; i < gMission.countTotal; i++)
        {
            MissionTarget& tg = gMission.targets[i];
            if (i == gMission.countDone && gMission.step == 0)
                beacon(tg.x, tg.z, 70.0f, 420.0f, 0.35f, 0.95f, 0.45f, 0.15f);
            else if (tg.done)
                beacon(tg.x, tg.z, 40.0f, 220.0f, 0.30f, 0.80f, 0.35f, 0.08f);
        }
    }
    else if (gMission.id == MIS_SCHOOL)
    {
        if (gMission.step == 1)
            beacon(gMission.targets[0].x, gMission.targets[0].z, 30.0f, 220.0f, 1.0f, 0.85f, 0.2f, 0.2f);
        else if (gMission.step == 3 && gMission.countTotal > 0)
        {
            for (int i = 0; i < 4; i++)
            {
                MissionTarget& tg = gMission.targets[1 + i];
                if (tg.done) beacon(tg.x, tg.z, 30.0f, 150.0f, 0.30f, 0.80f, 0.35f, 0.08f);
                else if (i == gMission.countDone) beacon(tg.x, tg.z, 60.0f, 300.0f, 0.35f, 0.95f, 0.45f, 0.18f);
            }
        }
    }
    else if (gMission.id == MIS_LANDING)
    {
        V3 p = worldRunwayPoint(-1450.0f, 0.0f);
        beacon(p.x, p.z, 60.0f, 260.0f, 1.0f, 0.85f, 0.2f, 0.16f);
    }
}
