// enemy.cpp - hostile fighters: pursuit AI, cannon fire, homing missiles, flares
//
// Everything here is deliberately simple and readable: each fighter steers
// towards the player with a limited turn rate, fires a cannon burst when the
// nose is nearly on target, and launches a missile on a timer. Missiles turn
// towards their target with a limited rate, which is what makes a hard turn or
// a flare break the lock.

#include "enemy.h"
#include "aircraft.h"
#include "glutil.h"
#include "world.h"
#include "sound.h"

extern int gDifficulty;      // 0 easy, 1 normal, 2 ace (game.cpp)
#include "glutil.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
const int MAX_ENEMIES = 8;
const int MAX_BULLETS = 320;
const int MAX_MISSILES = 12;
const int MAX_SMOKE = 220;

// a small state machine, so a bandit patrols, fights, overshoots, runs when hurt
// and goes home when the ammunition is gone
enum EnemyState { ES_PATROL, ES_ENGAGE, ES_OVERSHOOT, ES_EVADE, ES_EGRESS };

struct Enemy
{
    V3 pos, vel;
    float yaw, pitch, roll;
    float speed;
    float health;
    float fireT;          // seconds until the next burst
    float missileT;       // seconds until the next missile
    float delay;          // seconds until it appears
    int ammo;
    int state;
    float stateT;
    bool wingman;
    bool ace;
    bool raider;          // heads straight for a landmark instead of chasing you
    V3 raidTarget;
    bool alive;
};

struct Bullet
{
    V3 p, v;
    float life;
    bool alive;
    int owner;            // 0 = hostile, 1 = the player
};

struct Missile
{
    V3 p, v;
    float life;
    bool alive;
    float armed;          // seconds before the fuse works, so it does not blow up in the tubes
    int owner;            // 0 = hostile, 1 = the player
    int target;           // for player missiles: which enemy it is chasing (-1 = none)
};

struct Smoke
{
    V3 p;
    float age, life;
    float size;
};

Enemy sEnemies[MAX_ENEMIES];
Bullet sBullets[MAX_BULLETS];
Missile sMissiles[MAX_MISSILES];
Smoke sSmoke[MAX_SMOKE];

int sCount = 0;
int sAlive = 0;
int sFlares = 5;
bool sEnabled = true;
float sSmokeNext = 0.0f;

// ---- the player's weapons and score (Phase 1: Sylhet Attack) ----
int sAmmo = 150;             // cannon rounds
int sMissilesLeft = 4;
float sGunCd = 0.0f;
float sMissileCd = 0.0f;
int sKills = 0;
int sShots = 0;
int sHits = 0;
int sTgt = -1;               // selected bandit, -1 = none

struct Wreck { V3 p, v; float yaw, roll, spin, life; bool alive; };
Wreck sWrecks[12];

// ---- ground war (Phase 2) ----
enum GroundKind { GK_DEPOT, GK_TANK, GK_AA, GK_RADAR, GK_TRUCK, GK_KINDS };
struct Ground
{
    V3 p;
    float yaw;
    int kind;
    float health;
    bool alive;
    float fireT;      // AA reload / fire timer
    float phase;      // convoy position along its route, or dish angle
};
static Ground sGround[24];
static int sGroundN = 0;

struct Bomb { V3 p, v; float life; bool alive; };
struct Flak { V3 p, v; float fuse; bool alive; };
static Flak sFlak[20];
static Bomb sBombs[12];
static Bomb sRockets[32];
static int sBombsLeft = 6;
static int sRocketsLeft = 24;

static float sLandmarkHP[10];
static int sLandmarkCount_ = 0;
static float sLandmarkFireT = 0.0f;
static float sPlayerSmokeT = 0.0f;

// the convoy runs along the airport road, between these two points
static const V3 CONVOY_A = { -400.0f, 0.0f, -300.0f };
static const V3 CONVOY_B = { -2500.0f, 0.0f, -1850.0f };

void damageLandmarksNear(V3 p, float radius, float dmg)
{
    (void)p; (void)radius; (void)dmg;
}

// ---------------------------------------------------------------------
float randf()
{
    static unsigned s = 987654321u;
    s = s * 1664525u + 1013904223u;
    return (float)((s >> 8) & 0xFFFFFF) / 16777216.0f;
}


void spawnSmoke(V3 p, float size, float life)
{
    for (int i = 0; i < MAX_SMOKE; i++)
    {
        if (sSmoke[i].life > 0.0f) continue;
        sSmoke[i].p = p;
        sSmoke[i].age = 0.0f;
        sSmoke[i].life = life;
        sSmoke[i].size = size;
        return;
    }
}

void boom(V3 p, float scale)
{
    float d = len(p - gAircraft.pos);
    soundPlay(SND_BOOM, clampf(1.0f - d / 2200.0f, 0.10f, 0.9f));
    // a bigger, brighter fireball than before: more puffs, larger, and a flash
    for (int i = 0; i < 18; i++)
    {
        float a = randf() * 6.2831853f, b = randf() * 3.14159f;
        V3 dir = v3(cosf(a) * cosf(b), sinf(b) * 0.6f, sinf(a) * cosf(b));
        spawnSmoke(p + dir * (3.2f * scale), (6.5f + randf() * 5.0f) * scale, 1.3f + randf() * 1.8f);
    }
    spawnSmoke(p, 12.0f * scale, 0.35f);        // the flash
}

void fireBullet(V3 from, V3 dir, float spread, int owner)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (sBullets[i].alive) continue;
        float sx = (randf() * 2.0f - 1.0f) * spread;
        float sy = (randf() * 2.0f - 1.0f) * spread;
        V3 d = norm(dir + v3(sx, sy, sx * 0.4f));
        sBullets[i].p = from;
        sBullets[i].v = d * (owner ? 620.0f : 430.0f);   // the fighter''s gun is faster
        sBullets[i].life = 2.2f;
        sBullets[i].alive = true;
        sBullets[i].owner = owner;
        return;
    }
}

void fireMissile(V3 from, V3 dir, int owner, int tgt)
{
    for (int i = 0; i < MAX_MISSILES; i++)
    {
        if (sMissiles[i].alive) continue;
        sMissiles[i].p = from;
        sMissiles[i].v = norm(dir) * (owner ? 140.0f : 70.0f);
        sMissiles[i].life = 16.0f;
        sMissiles[i].armed = 0.6f;
        sMissiles[i].alive = true;
        sMissiles[i].owner = owner;
        sMissiles[i].target = tgt;
        return;
    }
}
}

// ---------------------------------------------------------------------
// a bandit comes apart: explosion now, wreck tumbling down with smoke
void killEnemy(int i)
{
    if (i < 0 || i >= sCount) return;
    Enemy& e = sEnemies[i];
    if (!e.alive) return;
    e.alive = false;
    sKills++;
    soundPlay(SND_LOCK, 0.6f);          // a satisfying confirmation chime
    boom(e.pos, 1.8f);

    Basis b = basisFromYPR(e.yaw, e.pitch, e.roll);
    for (int w = 0; w < 12; w++)
    {
        if (sWrecks[w].alive) continue;
        sWrecks[w].alive = true;
        sWrecks[w].p = e.pos;
        sWrecks[w].v = b.f * (e.speed * 0.45f) + v3(0.0f, -3.0f, 0.0f);
        sWrecks[w].yaw = e.yaw;
        sWrecks[w].roll = e.roll;
        sWrecks[w].spin = 2.5f + randf() * 3.0f;
        sWrecks[w].life = 14.0f;
        return;
    }
}

void enemyReset()
{
    sCount = 0;
    sAlive = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) sEnemies[i].alive = false;
    for (int i = 0; i < MAX_BULLETS; i++) sBullets[i].alive = false;
    for (int i = 0; i < MAX_MISSILES; i++) sMissiles[i].alive = false;
    for (int i = 0; i < MAX_SMOKE; i++) sSmoke[i].life = 0.0f;
}

void enemySetEnabled(bool on)
{
    sEnabled = on;
    if (!on) enemyReset();
}

bool enemyEnabled() { return sEnabled; }

int enemyAlive() { return sAlive; }
int enemyFlares() { return sFlares; }

bool enemyFireFlare()
{
    if (sFlares <= 0) return false;
    sFlares--;

    // a flare breaks any missile lock nearby: the missile chases the flare
    V3 flare = gAircraft.pos + v3(0.0f, -6.0f, 0.0f);
    for (int i = 0; i < MAX_MISSILES; i++)
    {
        if (!sMissiles[i].alive) continue;
        float d = len(sMissiles[i].p - gAircraft.pos);
        if (d > 700.0f) continue;
        V3 away = norm(sMissiles[i].p - flare);
        sMissiles[i].v = away * len(sMissiles[i].v);
        spawnSmoke(flare, 3.0f, 1.6f);
        spawnSmoke(flare + v3(randf() * 4 - 2, randf() * 4 - 2, randf() * 4 - 2), 2.6f, 1.2f);
    }
    return true;
}

// one bandit, placed on a ring around the player. The wave director and the
// opening both use this, so there is a single place where a bandit is created.
static void spawnOne(int i, int count)
{
    Enemy& e = sEnemies[i];
    V3 p = gAircraft.pos;
    float a = (float)i / (float)fmaxf(1.0f, (float)count) * 6.2831853f + randf() * 0.5f;
    float dist = 900.0f + randf() * 600.0f;
    e.pos = v3(p.x + cosf(a) * dist, fmaxf(120.0f, p.y + (randf() * 160.0f - 70.0f)), p.z + sinf(a) * dist);
    e.yaw = yawFromDir(norm(p - e.pos));
    e.pitch = 0.0f;
    e.roll = 0.0f;
    e.speed = 88.0f;
    e.health = 60.0f;
    e.ammo = 90;
    e.fireT = 2.5f + randf() * 2.5f;
    e.missileT = 14.0f + randf() * 12.0f;
    e.delay = 4.0f + (float)i * 2.2f;          // they arrive one after another
    e.state = ES_PATROL;
    e.stateT = 0.0f;
    e.wingman = (i & 1) != 0;
    e.ace = false;
    e.raider = false;
    e.raidTarget = v3(0, 0, 0);
    e.alive = true;
    sAlive++;
}

void enemyStart(int count)
{
    if (!sEnabled) return;
    enemyReset();

    // difficulty shapes the fight
    if (gDifficulty == 0) count = (count > 3) ? 3 : count;
    else if (gDifficulty == 2 && count < MAX_ENEMIES) count++;
    if (count > MAX_ENEMIES) count = MAX_ENEMIES;

    sCount = count;
    sFlares = (gDifficulty == 0) ? 8 : (gDifficulty == 2 ? 3 : 5);

    for (int i = 0; i < count; i++) spawnOne(i, count);

    if (gDifficulty == 2 && count > 0)
    {
        sEnemies[0].ace = true;
        sEnemies[0].health = 90.0f;
        sEnemies[0].ammo = 140;
    }
}

// ---------------------------------------------------------------------
// wave director: bandits arrive in ones and twos instead of all at once
// ---------------------------------------------------------------------
static int sPending = 0;
static float sWaveT = 0.0f;

void enemyBeginWaves(int total)
{
    if (!sEnabled) return;
    enemyReset();
    if (gDifficulty == 0) total = (total > 2) ? 2 : total;
    else if (gDifficulty == 1) total = (total > 4) ? 4 : total;
    else if (gDifficulty == 2 && total < MAX_ENEMIES) total++;
    if (total > MAX_ENEMIES) total = MAX_ENEMIES;

    sCount = 0;
    sPending = total;
    sWaveT = 5.0f;
    sFlares = (gDifficulty == 0) ? 8 : (gDifficulty == 2 ? 3 : 5);
}

static void waveDirector(float dt)
{
    if (sPending <= 0) return;
    sWaveT -= dt;
    if (sWaveT > 0.0f) return;
    if (sCount >= MAX_ENEMIES) { sPending = 0; return; }

    spawnOne(sCount, MAX_ENEMIES);
    sEnemies[sCount].delay = 0.3f;
    if (gDifficulty == 2 && sCount == 0)
    {
        sEnemies[0].ace = true;
        sEnemies[0].health = 90.0f;
        sEnemies[0].ammo = 140;
    }
    sCount++;
    sPending--;
    sWaveT = (gDifficulty == 2) ? 9.0f : (gDifficulty == 0 ? 18.0f : 14.0f);
}

// ---------------------------------------------------------------------
// the player''s weapons
// ---------------------------------------------------------------------
void weaponReset()
{
    sAmmo = (gDifficulty == 2) ? 160 : 220;
    sMissilesLeft = 6;
    sGunCd = 0.0f;
    sMissileCd = 0.0f;
    sKills = 0;
    sShots = 0;
    sHits = 0;
    sTgt = -1;
    for (int i = 0; i < 12; i++) sWrecks[i].alive = false;
}

void weaponUpdate(float dt)
{
    if (sGunCd > 0.0f) sGunCd -= dt;
    if (sMissileCd > 0.0f) sMissileCd -= dt;

    for (int i = 0; i < 12; i++)
    {
        Wreck& w = sWrecks[i];
        if (!w.alive) continue;
        w.v.y -= 9.0f * dt;
        w.p += w.v * dt;
        w.roll += w.spin * dt;
        w.life -= dt;
        if (randf() < 0.35f) spawnSmoke(w.p, 3.0f, 1.5f);
        if (w.p.y < 3.0f || w.life <= 0.0f)
        {
            boom(w.p, 2.4f);
            w.alive = false;
        }
    }
}

bool weaponFireCannon()
{
    if (sAmmo <= 0 || sGunCd > 0.0f) return false;
    if (gAircraft.phase == PHASE_CRASHED) return false;
    Basis b = basisFromYPR(gAircraft.yaw, gAircraft.pitch, gAircraft.roll);
    V3 muzzle = gAircraft.pos + b.f * 6.0f + v3(0.0f, -0.25f, 0.0f);
    fireBullet(muzzle, b.f, 0.006f, 1);
    sAmmo--;
    sShots++;
    sGunCd = 0.085f;
    soundPlay(SND_CANNON, 0.55f);
    spawnSmoke(muzzle, 2.2f, 0.09f);                // muzzle flash
    return true;
}

bool weaponFireMissile()
{
    if (sMissilesLeft <= 0 || sMissileCd > 0.0f) return false;
    if (sTgt < 0 || sTgt >= sCount || !sEnemies[sTgt].alive) return false;
    if (gAircraft.phase == PHASE_CRASHED) return false;

    V3 to = sEnemies[sTgt].pos - gAircraft.pos;
    float dist = len(to);
    if (dist > 6000.0f) return false;
    Basis b = basisFromYPR(gAircraft.yaw, gAircraft.pitch, gAircraft.roll);
    if (dist > 1.0f && dot(b.f, norm(to)) < 0.82f) return false;   // roughly ahead

    fireMissile(gAircraft.pos + b.f * 5.0f, b.f, 1, sTgt);
    sMissilesLeft--;
    sMissileCd = 1.4f;
    soundPlay(SND_MISSILE, 0.7f);
    return true;
}

void weaponCycleTarget()
{
    if (sCount <= 0) { sTgt = -1; return; }
    for (int k = 1; k <= sCount; k++)
    {
        int i = ((sTgt < 0 ? 0 : sTgt) + k) % sCount;
        if (sEnemies[i].alive && sEnemies[i].delay <= 0.0f) { sTgt = i; return; }
    }
    sTgt = -1;
}

bool weaponTargetAim(V3& pos, V3& vel, float& dist, float& radius)
{
    if (sTgt < 0 || sTgt >= sCount || !sEnemies[sTgt].alive || sEnemies[sTgt].delay > 0.0f)
    {
        sTgt = -1;
        float best = 1e9f;
        for (int i = 0; i < sCount; i++)
        {
            if (!sEnemies[i].alive || sEnemies[i].delay > 0.0f) continue;
            float d = len(sEnemies[i].pos - gAircraft.pos);
            if (d < best) { best = d; sTgt = i; }
        }
    }
    if (sTgt < 0) return false;

    Enemy& e = sEnemies[sTgt];
    Basis b = basisFromYPR(e.yaw, e.pitch, e.roll);
    pos = e.pos;
    vel = b.f * e.speed;
    dist = len(e.pos - gAircraft.pos);
    radius = 7.0f;
    return true;
}

int enemyContacts(V3* out, int max)
{
    int n = 0;
    for (int i = 0; i < sCount && n < max; i++)
    {
        if (!sEnemies[i].alive || sEnemies[i].delay > 0.0f) continue;
        out[n++] = sEnemies[i].pos;
    }
    return n;
}

int weaponAmmo() { return sAmmo; }
int weaponMissiles() { return sMissilesLeft; }
int weaponKills() { return sKills; }
int weaponShotsFired() { return sShots; }
int weaponShotsHit() { return sHits; }
float weaponAccuracy() { return sShots ? (float)sHits / (float)sShots : 0.0f; }

const char* weaponRankName()
{
    if (sKills >= 10) return "ACE";
    if (sKills >= 8) return "WING COMMANDER";
    if (sKills >= 6) return "SQUADRON LEADER";
    if (sKills >= 4) return "FLIGHT LIEUTENANT";
    if (sKills >= 2) return "FLYING OFFICER";
    return "CADET";
}

// ---------------------------------------------------------------------
// raiders: they ignore the player and fly at a landmark. Shoot them down
// before they arrive, or the landmark burns.
// ---------------------------------------------------------------------
void enemyBeginRaid(V3 target, int count)
{
    if (!sEnabled) return;
    for (int i = 0; i < count && sCount < MAX_ENEMIES; i++)
    {
        spawnOne(sCount, count);
        Enemy& e = sEnemies[sCount];
        float a = randf() * 6.2831853f;
        e.pos = target + v3(cosf(a) * 2400.0f, 170.0f + randf() * 90.0f, sinf(a) * 2400.0f);
        e.yaw = yawFromDir(norm(target - e.pos));
        e.pitch = 0.0f;
        e.roll = 0.0f;
        e.raidTarget = target;
        e.raider = true;
        e.health = 30.0f;
        e.speed = 92.0f;
        e.ammo = 0;
        e.delay = 1.0f + (float)i * 0.6f;
        sCount++;
    }
}

int enemyRaidersAlive()
{
    int n = 0;
    for (int i = 0; i < sCount; i++)
        if (sEnemies[i].alive && sEnemies[i].raider) n++;
    return n;
}

float landmarkIntegrity()
{
    float total = 0.0f;
    for (int i = 0; i < gLandmarkCount && i < 10; i++) total += sLandmarkHP[i];
    return total / fmaxf(1.0f, (float)fminf(10.0f, (float)gLandmarkCount));
}

void enemyRush()
{
    for (int i = 0; i < sCount; i++) sEnemies[i].delay = 0.4f + (float)i * 0.35f;
}

// ---------------------------------------------------------------------
// ground war: depot, convoy, AAA, radar, bombs, rockets, landmark fires
// ---------------------------------------------------------------------
static void addGround(float x, float z, float yaw, int kind, float hp)
{
    if (sGroundN >= 24) return;
    Ground& g = sGround[sGroundN++];
    g.p = v3(x, 0.0f, z);
    g.yaw = yaw;
    g.kind = kind;
    g.health = hp;
    g.alive = true;
    g.fireT = 1.5f + randf() * 1.5f;
    g.phase = 0.0f;
}

void groundSetup()
{
    sGroundN = 0;
    sBombsLeft = 6;
    sRocketsLeft = 24;
    sLandmarkCount_ = gLandmarkCount;
    for (int i = 0; i < 10; i++) sLandmarkHP[i] = 100.0f;
    for (int i = 0; i < 12; i++) sBombs[i].alive = false;
    for (int i = 0; i < 32; i++) sRockets[i].alive = false;
    for (int i = 0; i < 20; i++) sFlak[i].alive = false;

    // fuel depot on the Osmani apron
    V3 ap = worldApronCenter();
    addGround(ap.x + 55.0f, ap.z + 35.0f, 0.0f, GK_TANK, 55.0f);
    addGround(ap.x + 85.0f, ap.z + 35.0f, 0.0f, GK_TANK, 55.0f);
    addGround(ap.x + 70.0f, ap.z + 75.0f, 0.0f, GK_DEPOT, 80.0f);

    // AA covering the river crossing and the campus approach
    addGround(0.0f, 720.0f, 0.0f, GK_AA, 50.0f);
    addGround(1420.0f, 1480.0f, 0.0f, GK_AA, 50.0f);

    // radar mast by the stadium
    addGround(1680.0f, -1430.0f, 0.0f, GK_RADAR, 70.0f);

    // convoy of three trucks on the airport road
    for (int i = 0; i < 3; i++)
    {
        Ground& g = sGround[sGroundN++];
        g.p = CONVOY_A;
        g.yaw = 0.0f;
        g.kind = GK_TRUCK;
        g.health = 45.0f;
        g.alive = true;
        g.fireT = 0.0f;
        g.phase = 0.10f + (float)i * 0.07f;
    }
}

static void explodeGround(V3 p, float radius, float dmg)
{
    boom(p, 1.5f);
    for (int i = 0; i < sGroundN; i++)
    {
        Ground& g = sGround[i];
        if (!g.alive) continue;
        float d = len(g.p - p);
        if (d >= radius) continue;
        g.health -= dmg * (1.0f - d / radius) + 15.0f;
        spawnSmoke(g.p + v3(0.0f, 4.0f, 0.0f), 5.0f, 1.4f);
        if (g.health <= 0.0f)
        {
            g.alive = false;
            boom(g.p, g.kind == GK_TANK ? 2.6f : 1.8f);
        }
    }
    for (int i = 0; i < gLandmarkCount && i < 10; i++)
    {
        V3 lp = v3(gLandmarks[i].x, 0.0f, gLandmarks[i].z);
        float d = len(lp - p);
        if (d < radius + 40.0f)
            sLandmarkHP[i] = fmaxf(0.0f, sLandmarkHP[i] - dmg * 0.35f);
    }
}

int groundTargetsAlive()
{
    int n = 0;
    for (int i = 0; i < sGroundN; i++) if (sGround[i].alive) n++;
    return n;
}
int groundTargetsTotal() { return sGroundN; }

int landmarkDamageScore()
{
    float total = 0.0f;
    for (int i = 0; i < sLandmarkCount_ && i < 10; i++) total += 100.0f - sLandmarkHP[i];
    return (int)(total / fmaxf(1.0f, (float)sLandmarkCount_ * 100.0f) * 100.0f);
}

bool weaponRearm()
{
    bool changed = false;
    if (sAmmo < 150) { sAmmo = 150; changed = true; }
    if (sMissilesLeft < 6) { sMissilesLeft = 6; changed = true; }
    if (sBombsLeft < 6) { sBombsLeft = 6; changed = true; }
    if (sRocketsLeft < 24) { sRocketsLeft = 24; changed = true; }
    if (sFlares < 5) { sFlares = 5; changed = true; }
    return changed;
}

int weaponBombs() { return sBombsLeft; }
int weaponRockets() { return sRocketsLeft; }

bool weaponDropBomb()
{
    if (sBombsLeft <= 0 || gAircraft.phase == PHASE_CRASHED) return false;
    for (int i = 0; i < 12; i++)
    {
        if (sBombs[i].alive) continue;
        Basis b = basisFromYPR(gAircraft.yaw, gAircraft.pitch, gAircraft.roll);
        sBombs[i].alive = true;
        sBombs[i].p = gAircraft.pos + v3(0.0f, -1.6f, 0.0f);
        sBombs[i].v = gAircraft.vel * 0.9f;
        sBombs[i].life = 25.0f;
        sBombsLeft--;
        soundPlay(SND_BOOM, 0.45f);
        return true;
    }
    return false;
}

bool weaponFireRockets()
{
    if (sRocketsLeft < 4 || gAircraft.phase == PHASE_CRASHED) return false;
    Basis b = basisFromYPR(gAircraft.yaw, gAircraft.pitch, gAircraft.roll);
    int fired = 0;
    for (int i = 0; i < 32 && fired < 4; i++)
    {
        if (sRockets[i].alive) continue;
        sRockets[i].alive = true;
        sRockets[i].p = gAircraft.pos + b.f * 4.0f + v3(0.0f, -1.2f, 0.0f);
        V3 d = b.f + v3((randf() - 0.5f) * 0.02f, (randf() - 0.5f) * 0.02f, 0.0f);
        sRockets[i].v = norm(d) * 420.0f + gAircraft.vel * 0.5f;
        sRockets[i].life = 6.0f;
        fired++;
    }
    sRocketsLeft -= fired;
    if (fired) soundPlay(SND_CANNON, 0.8f);
    return fired > 0;
}

void groundUpdate(float dt)
{
    if (sGroundN == 0) return;

    V3 playerPos = gAircraft.pos;
    bool playerUp = (gAircraft.phase != PHASE_CRASHED);

    // convoy rolls up and down the airport road
    for (int i = 0; i < sGroundN; i++)
    {
        Ground& g = sGround[i];
        if (!g.alive) continue;
        if (g.kind == GK_TRUCK)
        {
            g.phase += dt * 0.010f;
            if (g.phase > 1.0f) g.phase = 0.0f;
            V3 d = CONVOY_B - CONVOY_A;
            g.p = CONVOY_A + d * g.phase;
            g.yaw = yawFromDir(norm(d));
        }
        else if (g.kind == GK_RADAR)
        {
            g.phase += dt * 1.2f;          // the dish turns
        }
        else if (g.kind == GK_AA)
        {
            float dist = len(g.p - playerPos);
            if (playerUp && dist < 2800.0f)
            {
                g.fireT -= dt;
                if (g.fireT <= 0.0f)
                {
                    g.fireT = 2.4f + randf() * 1.6f;
                    for (int f = 0; f < 20; f++)
                    {
                        if (sFlak[f].alive) continue;
                        sFlak[f].alive = true;
                        V3 burst = playerPos + v3((randf() - 0.5f) * 120.0f, 30.0f + randf() * 60.0f, (randf() - 0.5f) * 120.0f);
                        float ft = 1.6f;
                        sFlak[f].p = g.p + v3(0.0f, 3.0f, 0.0f);
                        sFlak[f].v = (burst - sFlak[f].p) * (1.0f / ft) + v3(0.0f, 0.5f * 9.8f * ft, 0.0f);
                        sFlak[f].fuse = ft;
                        break;
                    }
                }
            }
        }
    }

    // AA bursts
    for (int f = 0; f < 20; f++)
    {
        Flak& b = sFlak[f];
        if (!b.alive) continue;
        b.v.y -= 9.8f * dt;
        b.p += b.v * dt;
        b.fuse -= dt;
        if (b.fuse <= 0.0f || b.p.y < 1.0f)
        {
            b.alive = false;
            for (int k = 0; k < 6; k++)
                spawnSmoke(b.p + v3((randf() - 0.5f) * 18.0f, (randf() - 0.5f) * 12.0f, (randf() - 0.5f) * 18.0f),
                           4.5f, 1.6f);
            if (playerUp && len(b.p - playerPos) < 38.0f)
                craftDamage(gAircraft, 3.0f, "Hit by anti-aircraft fire!");
        }
    }

    // bombs fall and go off on contact
    for (int i = 0; i < 12; i++)
    {
        Bomb& b = sBombs[i];
        if (!b.alive) continue;
        b.v.y -= 9.8f * dt;
        b.p += b.v * dt;
        b.life -= dt;
        if (b.p.y < 1.0f || b.life <= 0.0f)
        {
            b.alive = false;
            explodeGround(b.p, 55.0f, 65.0f);
        }
    }

    // rockets go off on contact (or when they run out)
    for (int i = 0; i < 32; i++)
    {
        Bomb& r = sRockets[i];
        if (!r.alive) continue;
        r.v.y -= 1.5f * dt;
        r.p += r.v * dt;
        r.life -= dt;
        bool hit = (r.p.y < 1.0f || r.life <= 0.0f);
        if (!hit)
        {
            for (int g = 0; g < sGroundN && !hit; g++)
            {
                if (!sGround[g].alive) continue;
                if (len(sGround[g].p + v3(0.0f, 4.0f, 0.0f) - r.p) < 14.0f) hit = true;
            }
        }
        if (hit)
        {
            r.alive = false;
            explodeGround(r.p, 32.0f, 34.0f);
        }
    }

    // fires on landmarks that have been hit
    sLandmarkFireT -= dt;
    if (sLandmarkFireT <= 0.0f)
    {
        sLandmarkFireT = 0.18f;
        for (int i = 0; i < sLandmarkCount_ && i < 10; i++)
        {
            if (sLandmarkHP[i] > 60.0f) continue;
            V3 lp = v3(gLandmarks[i].x + (randf() - 0.5f) * 40.0f, 6.0f + randf() * 10.0f,
                       gLandmarks[i].z + (randf() - 0.5f) * 40.0f);
            spawnSmoke(lp, 5.0f + (60.0f - sLandmarkHP[i]) * 0.05f, 2.2f);
        }
    }

    // battle damage trailing from the player
    if (playerUp && gAircraft.health < 60.0f)
    {
        sPlayerSmokeT -= dt;
        if (sPlayerSmokeT <= 0.0f)
        {
            sPlayerSmokeT = 0.12f;
            Basis b = basisFromYPR(gAircraft.yaw, gAircraft.pitch, gAircraft.roll);
            spawnSmoke(gAircraft.pos - b.f * 5.0f, gAircraft.health < 30.0f ? 3.2f : 2.2f, 1.1f);
        }
    }
}

void groundDraw()
{
    // convoy, depot, AA and radar
    for (int i = 0; i < sGroundN; i++)
    {
        Ground& g = sGround[i];
        if (!g.alive) continue;
        glPushMatrix();
        glTranslatef(g.p.x, 0.0f, g.p.z);
        glRotatef(-rad2deg(g.yaw), 0, 1, 0);
        switch (g.kind)
        {
            case GK_TANK:
                setColor(0.62f, 0.62f, 0.58f);
                glPushMatrix(); glTranslatef(0.0f, 3.2f, 0.0f); drawCylinderRaw(7.0f, 7.0f, 6.4f, 12, true); glPopMatrix();
                glPushMatrix(); glTranslatef(0.0f, 6.4f, 0.0f); drawDome(7.0f, 12, 3); glPopMatrix();
                setColor(0.55f, 0.36f, 0.22f);
                glPushMatrix(); glTranslatef(0.0f, 6.6f, 0.0f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(6.2f, 10); glPopMatrix();
                break;
            case GK_DEPOT:
                setColor(0.72f, 0.70f, 0.62f);
                drawBoxAt(0.0f, 3.0f, 0.0f, 26.0f, 6.0f, 14.0f);
                setColor(0.55f, 0.35f, 0.25f);
                drawBoxAt(0.0f, 6.4f, 0.0f, 27.0f, 0.6f, 15.0f);
                break;
            case GK_AA:
                setColor(0.42f, 0.44f, 0.38f);
                drawBoxAt(0.0f, 1.2f, 0.0f, 6.0f, 2.4f, 6.0f);
                setColor(0.28f, 0.30f, 0.26f);
                glPushMatrix(); glTranslatef(-0.8f, 0.0f, 0.0f); glRotatef(-55.0f, 1, 0, 0);
                drawCylinderRaw(0.35f, 0.28f, 6.0f, 8, true); glPopMatrix();
                glPushMatrix(); glTranslatef(0.8f, 0.0f, 0.0f); glRotatef(-55.0f, 1, 0, 0);
                drawCylinderRaw(0.35f, 0.28f, 6.0f, 8, true); glPopMatrix();
                break;
            case GK_RADAR:
                setColor(0.50f, 0.52f, 0.48f);
                drawBoxAt(0.0f, 4.0f, 0.0f, 3.0f, 8.0f, 3.0f);
                glPushMatrix();
                glTranslatef(0.0f, 10.0f, 0.0f);
                glRotatef(rad2deg(g.phase), 0, 1, 0);
                setColor(0.74f, 0.76f, 0.72f);
                glRotatef(-35.0f, 1, 0, 0);
                drawDiskXZ(5.5f, 12);
                glPopMatrix();
                break;
            default:   // truck
                setColor(0.46f, 0.48f, 0.32f);
                drawBoxAt(0.0f, 1.6f, 0.0f, 2.6f, 2.0f, 6.0f);
                setColor(0.36f, 0.38f, 0.26f);
                drawBoxAt(0.0f, 2.1f, -2.0f, 2.4f, 1.2f, 1.8f);
                setColor(0.12f, 0.12f, 0.12f);
                for (int w = 0; w < 4; w++)
                    drawBoxAt((w & 1) ? 1.2f : -1.2f, 0.5f, (w & 2) ? 1.9f : -1.9f, 0.4f, 1.0f, 1.0f);
                break;
        }
        glPopMatrix();
    }

    // bombs and rocket motors
    for (int i = 0; i < 12; i++)
    {
        if (!sBombs[i].alive) continue;
        setColor(0.30f, 0.32f, 0.30f);
        glPushMatrix(); glTranslatef(sBombs[i].p.x, sBombs[i].p.y, sBombs[i].p.z);
        drawBox(0.5f, 0.5f, 1.8f); glPopMatrix();
    }
    for (int i = 0; i < 32; i++)
    {
        if (!sRockets[i].alive) continue;
        setColor(0.85f, 0.80f, 0.55f);
        glPushMatrix(); glTranslatef(sRockets[i].p.x, sRockets[i].p.y, sRockets[i].p.z);
        drawSphere(0.35f, 6, 4); glPopMatrix();
    }
}

void enemyUpdate(float dt)
{
    if (!sEnabled || sCount == 0) return;

    const V3 target = gAircraft.pos;
    const bool playerFlying = (gAircraft.phase != PHASE_CRASHED);

    waveDirector(dt);

    // ---------------- fighters ----------------
    sAlive = 0;
    for (int i = 0; i < sCount; i++)
    {
        Enemy& e = sEnemies[i];
        if (!e.alive) continue;

        if (e.delay > 0.0f)
        {
            e.delay -= dt;
            sAlive++;
            continue;
        }

        if (e.raider)
        {
            V3 toT = e.raidTarget - e.pos;
            float dt2 = len(toT);
            if (dt2 < 260.0f)
            {
                // bombs away: the nearest landmark takes the hit
                int best = -1; float bd = 1e9f;
                for (int li = 0; li < gLandmarkCount && li < 10; li++)
                {
                    float d = len(v3(gLandmarks[li].x, 0.0f, gLandmarks[li].z) - e.raidTarget);
                    if (d < bd) { bd = d; best = li; }
                }
                if (best >= 0) sLandmarkHP[best] = fmaxf(0.0f, sLandmarkHP[best] - 35.0f);
                boom(e.pos, 2.0f);
                e.alive = false;
                continue;
            }
            V3 dirT = norm(toT);
            float wy = yawFromDir(v3(dirT.x, 0.0f, dirT.z));
            float wp = asinf(clampf(dirT.y, -1.0f, 1.0f));
            e.yaw += clampf(wrapPi(wy - e.yaw), -1.1f * dt, 1.1f * dt);
            e.pitch += clampf(wp - e.pitch, -1.1f * dt, 1.1f * dt);
            e.roll = 0.0f;
            Basis rb = basisFromYPR(e.yaw, e.pitch, 0.0f);
            e.pos += rb.f * (e.speed * dt);
            sAlive++;
            continue;
        }

        V3 to = target - e.pos;
        float dist = len(to);
        V3 dir = norm(to);

        // ---- the bandit decides what it is doing ----
        switch (e.state)
        {
            case ES_PATROL:
                if (dist < 2600.0f) e.state = ES_ENGAGE;
                break;
            case ES_ENGAGE:
                if (dist < 170.0f) { e.state = ES_OVERSHOOT; e.stateT = 2.2f; }
                else if (e.health < 35.0f && randf() < 0.004f) { e.state = ES_EVADE; e.stateT = 3.5f; }
                else if (e.ammo <= 0) e.state = ES_EGRESS;
                break;
            case ES_OVERSHOOT:
                e.stateT -= dt;
                if (e.stateT <= 0.0f) e.state = ES_ENGAGE;
                break;
            case ES_EVADE:
                e.stateT -= dt;
                if (e.stateT <= 0.0f) e.state = ES_ENGAGE;
                break;
            case ES_EGRESS:
                if (dist > 4200.0f) { e.alive = false; break; }
                break;
        }

        // patrol and egress fly straight and level; the rest chase
        if (e.state == ES_PATROL || e.state == ES_EGRESS)
        {
            dir = norm(v3(sinf(e.yaw) * cosf(e.pitch), sinf(e.pitch), -cosf(e.yaw) * cosf(e.pitch)));
            if (e.state == ES_EGRESS) dir = norm(e.pos - target);
        }
        else if (e.state == ES_EVADE)
        {
            dir = norm(e.pos - target);          // break away from the fight
        }

        // steer: yaw and pitch towards the player, with a turn-rate limit
        float wantYaw = yawFromDir(v3(dir.x, 0.0f, dir.z));
        float wantPitch = asinf(clampf(dir.y, -1.0f, 1.0f));
        float dyaw = wrapPi(wantYaw - e.yaw);
        float dpitch = wantPitch - e.pitch;
        float maxTurn = 0.85f * dt;
        if (e.state == ES_OVERSHOOT) maxTurn *= 0.30f;
        if (e.ace) maxTurn *= 1.35f;
        e.yaw += clampf(dyaw, -maxTurn, maxTurn);
        e.pitch += clampf(dpitch, -maxTurn * 0.8f, maxTurn * 0.8f);
        e.pitch = clampf(e.pitch, -0.7f, 0.7f);
        e.roll = -clampf(dyaw * 1.4f, -0.8f, 0.8f);

        // speed: close the distance when far, ease off when very close
        float wantSpeed = (dist > 500.0f) ? 104.0f : (dist < 180.0f ? 78.0f : 92.0f);
        e.speed += (wantSpeed - e.speed) * (1.0f - expf(-dt * 0.8f));

        Basis b = basisFromYPR(e.yaw, e.pitch, e.roll);
        e.pos += b.f * (e.speed * dt);

        // the ground is hard
        if (e.pos.y < 6.0f)
        {
            boom(e.pos, 1.4f);
            e.alive = false;
            continue;
        }

        float aim = 0.0f;
        if (dist > 1.0f)
        {
            V3 nose = b.f;
            aim = dot(nose, dir);                     // 1 = perfectly on target
        }

        // cannon: short bursts when the nose is nearly on the player
        e.fireT -= dt;
        bool mayFire = (e.state == ES_ENGAGE) && playerFlying && e.ammo > 0;
        if (mayFire && e.fireT <= 0.0f && dist < 800.0f && aim > 0.997f)
        {
            V3 muzzle = e.pos + b.f * 4.0f;
            for (int k = 0; k < 4; k++) fireBullet(muzzle, b.f, 0.012f, 0);
            e.ammo -= 4;
            e.fireT = (e.ace ? 1.2f : 1.7f) + randf() * 1.3f;
        }
        else if (mayFire && e.fireT < -1.5f)
        {
            // hold the trigger a little longer if the shot was not lined up
            if (dist < 600.0f && aim > 0.96f)
            {
                fireBullet(e.pos + b.f * 4.0f, b.f, 0.02f, 0);
                e.ammo--;
                e.fireT = 0.25f;
            }
        }

        // missile: on a timer, when roughly pointed at the player
        e.missileT -= dt;
        if (e.state == ES_ENGAGE && playerFlying && e.missileT <= 0.0f && dist > 250.0f && dist < 1600.0f && aim > 0.90f)
        {
            fireMissile(e.pos + b.f * 5.0f, b.f, 0, -1);
            e.missileT = (e.ace ? 20.0f : 28.0f) + randf() * 14.0f;
        }
        sAlive++;
    }

    // ---------------- cannon shells ----------------
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        Bullet& b = sBullets[i];
        if (!b.alive) continue;
        V3 prev = b.p;
        b.p += b.v * dt;
        b.life -= dt;
        if (b.life <= 0.0f || b.p.y < 0.5f) { b.alive = false; continue; }

        // hit test: the closest the shell came to its target on this step
        V3 seg = b.p - prev;
        float segLen = len(seg);
        if (segLen > 0.01f)
        {
            if (b.owner == 0)
            {
                // hostile shell, aimed at the player
                V3 rel = target - prev;
                float t = clampf(dot(rel, seg) / (segLen * segLen), 0.0f, 1.0f);
                V3 closest = prev + seg * t;
                if (len(closest - target) < 5.0f && playerFlying)
                {
                    b.alive = false;
                    craftDamage(gAircraft, 1.0f, "Shot down by a fighter!");
                    spawnSmoke(closest, 1.2f, 0.5f);
                }
            }
            else
            {
                // the player's cannon: does it cross a bandit on this step?
                for (int ei = 0; ei < sCount; ei++)
                {
                    Enemy& en = sEnemies[ei];
                    if (!en.alive || en.delay > 0.0f) continue;
                    V3 rel = en.pos - prev;
                    float t = clampf(dot(rel, seg) / (segLen * segLen), 0.0f, 1.0f);
                    V3 closest = prev + seg * t;
                    if (len(closest - en.pos) < 6.5f)
                    {
                        b.alive = false;
                        sHits++;
                        en.health -= 20.0f;
                        spawnSmoke(closest, 1.4f, 0.35f);
                        if (en.health <= 0.0f) killEnemy(ei);
                        break;
                    }
                }
            }
        }
    }

    // ---------------- missiles ----------------
    for (int i = 0; i < MAX_MISSILES; i++)
    {
        Missile& m = sMissiles[i];
        if (!m.alive) continue;

        float speed = len(m.v);
        speed = fminf(speed + 120.0f * dt, 230.0f);
        V3 dir = norm(m.v);

        // who is this missile chasing?
        V3 aimPoint = target;
        bool hasTarget = playerFlying;
        if (m.owner == 1)
        {
            hasTarget = (m.target >= 0 && m.target < sCount && sEnemies[m.target].alive);
            if (hasTarget) aimPoint = sEnemies[m.target].pos;
            else { /* target gone: fly on and expire */ }
        }

        if (hasTarget)
        {
            // proportional-ish homing with a limited turn rate
            V3 want = norm(aimPoint - m.p);
            V3 axis = cross(dir, want);
            float sinA = len(axis);
            if (sinA > 1e-4f)
            {
                float cosA = dot(dir, want);
                float ang = atan2f(sinA, cosA);
                float maxAng = 1.15f * dt;
                float turn = clampf(ang, -maxAng, maxAng);
                V3 ax = norm(axis);
                // rotate dir towards want about ax
                float c = cosf(turn), s = sinf(turn);
                V3 nd = dir * c + cross(ax, dir) * s + ax * (dot(ax, dir) * (1.0f - c));
                dir = norm(nd);
            }
        }

        m.v = dir * speed;
        m.p += m.v * dt;
        m.life -= dt;
        if (m.armed > 0.0f) m.armed -= dt;

        // smoke trail
        sSmokeNext -= dt;
        if (sSmokeNext <= 0.0f)
        {
            spawnSmoke(m.p - dir * 3.0f, 2.0f, 1.1f);
            sSmokeNext = 0.03f;
        }

        if (m.p.y < 0.5f) { boom(m.p, 1.2f); m.alive = false; continue; }
        if (m.life <= 0.0f) { m.alive = false; continue; }

        if (m.owner == 0)
        {
            if (m.armed <= 0.0f && playerFlying && len(m.p - target) < 16.0f)
            {
                boom(m.p, 1.6f);
                m.alive = false;
                craftDamage(gAircraft, 12.0f, "Hit by a missile!");
            }
        }
        else
        {
            if (m.armed <= 0.0f && hasTarget && len(m.p - aimPoint) < 22.0f)
            {
                boom(m.p, 1.8f);
                m.alive = false;
                killEnemy(m.target);
            }
        }
    }

    // ---------------- smoke ----------------
    for (int i = 0; i < MAX_SMOKE; i++)
    {
        if (sSmoke[i].life <= 0.0f) continue;
        sSmoke[i].age += dt;
        if (sSmoke[i].age >= sSmoke[i].life) { sSmoke[i].life = 0.0f; continue; }
        sSmoke[i].p.y += 2.0f * dt;
    }
}

int enemyMissileLock()
{
    for (int i = 0; i < MAX_MISSILES; i++)
    {
        if (!sMissiles[i].alive) continue;
        if (len(sMissiles[i].p - gAircraft.pos) < 900.0f) return 1;
    }
    return 0;
}

void enemyDraw()
{
    if (!sEnabled) return;

    // fighters: dark grey-green, so they read as hostile against the sky
    for (int i = 0; i < sCount; i++)
    {
        Enemy& e = sEnemies[i];
        if (!e.alive || e.delay > 0.0f) continue;
        craftDrawSimple(e.pos.x, e.pos.y, e.pos.z, e.yaw, e.roll, 0.32f, 0.36f, 0.30f);
        // a small shadow so they sit on the world
        setColor(0.10f, 0.11f, 0.10f);
        glPushMatrix();
        glTranslatef(e.pos.x, 0.22f, e.pos.z);
        drawDiskXZ(5.0f, 8);
        glPopMatrix();
    }

    // tumbling wrecks
    for (int i = 0; i < 12; i++)
    {
        if (!sWrecks[i].alive) continue;
        craftDrawSimple(sWrecks[i].p.x, sWrecks[i].p.y, sWrecks[i].p.z,
                        sWrecks[i].yaw, sWrecks[i].roll, 0.16f, 0.14f, 0.13f);
    }

    // cannon tracers
    glDisable(GL_LIGHTING);
    glLineWidth(2.6f);
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!sBullets[i].alive) continue;
        V3 d = norm(sBullets[i].v);
        V3 p = sBullets[i].p;
        if (sBullets[i].owner == 1) setColor(1.0f, 0.97f, 0.62f);   // your tracers: bright
        else setColor(1.0f, 0.42f, 0.18f);                          // theirs: red-hot
        glBegin(GL_LINES);
        glVertex3f(p.x, p.y, p.z);
        glVertex3f(p.x - d.x * 26.0f, p.y - d.y * 26.0f, p.z - d.z * 26.0f);
        glEnd();
    }
    glLineWidth(1.0f);

    // missiles: a dark body with a flame, plus their smoke (drawn below)
    for (int i = 0; i < MAX_MISSILES; i++)
    {
        if (!sMissiles[i].alive) continue;
        V3 d = norm(sMissiles[i].v);
        V3 p = sMissiles[i].p;
        setColor(0.92f, 0.92f, 0.94f);
        glPushMatrix();
        glTranslatef(p.x, p.y, p.z);
        glRotatef(-rad2deg(yawFromDir(d)), 0, 1, 0);
        glRotatef(rad2deg(-asinf(clampf(d.y, -1.0f, 1.0f))), 1, 0, 0);
        drawBox(0.45f, 0.45f, 3.6f);
        setColor(0.55f, 0.58f, 0.62f);
        drawBox(0.55f, 0.55f, 0.5f);                 // fins
        glPopMatrix();
        setColor(1.0f, 0.72f, 0.25f);
        glPushMatrix();
        glTranslatef(p.x - d.x * 3.0f, p.y - d.y * 3.0f, p.z - d.z * 3.0f);
        drawDiskXZ(1.0f, 8);
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);

    // smoke puffs
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (int i = 0; i < MAX_SMOKE; i++)
    {
        if (sSmoke[i].life <= 0.0f) continue;
        float f = sSmoke[i].age / sSmoke[i].life;
        float a = (1.0f - f) * 0.55f;
        float sz = sSmoke[i].size * (0.6f + f * 1.6f);
        if (f < 0.25f) setColorA(1.0f, 0.75f, 0.35f, a + 0.25f);   // the flame
        else setColorA(0.55f, 0.55f, 0.58f, a);
        glPushMatrix();
        glTranslatef(sSmoke[i].p.x, sSmoke[i].p.y, sSmoke[i].p.z);
        drawDiskXZ(sz, 6);
        glPopMatrix();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}