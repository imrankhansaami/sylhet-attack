// aircraft.cpp - flight model and the little Cessna-style plane
#include "aircraft.h"
#include "sound.h"
#include "textures.h"
#include "glutil.h"
#include "world.h"
#include <cstring>
#include <cmath>
#include <cstdio>

const float CRAFT_GEAR_H = 1.28f;   // wheels touch the ground exactly at this height

float gWindSpeed = 7.0f;       // a fresh breeze, as asked: it pushes the aircraft
                               // around and keeps the windsocks streaming
float gWindDirRad = 3.75f;     // blowing towards the south-west
float gFuelCapacity = 45.0f;

static const float MASS = 1000.0f;      // kg
static const float WING_S = 16.0f;      // m^2
static const float RHO = 1.225f;        // air density
// A more powerful engine than the original 2600 N: the aircraft now cruises
// around 130 kt and tops out near 160 kt instead of 110 kt, which also makes the
// ring course and the tour quicker to fly for a demonstration.
static const float MAX_THRUST = 4400.0f;
static const float CL0 = 0.22f, CLA = 4.8f, ALPHA_STALL = 0.26f;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------
static void planeCrash(Aircraft& a, const char* msg);   // defined just below

void craftDamage(Aircraft& a, float dmg, const char* msg)
{
    if (a.phase == PHASE_CRASHED) return;
    a.health = clampf(a.health - dmg, 0.0f, 100.0f);
    soundPlay(SND_HIT, 0.5f);
    if (a.health <= 0.0f) planeCrash(a, msg);
}

static void planeCrash(Aircraft& a, const char* msg)
{
    a.phase = PHASE_CRASHED;
    a.health = 0.0f;
    a.crashTimer = 0.0f;
    a.landingQuality = 0;
    strncpy(a.landingText, msg, 63);
    a.landingText[63] = 0;
    a.landingMsgTimer = 4.0f;
    a.landingEvent++;
    a.throttle = 0.0f;
    a.engineOn = false;
    a.stall = false;
}

static void touchdown(Aircraft& a, float vy, bool onRunway)
{
    float vs = -vy;    // positive = descent rate (m/s)
    float gs = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
    float rollDeg = fabsf(rad2deg(a.roll));
    int q; const char* txt; float dmg = 0.0f;

    if (!onRunway)
    {
        if (vs > 4.0f) { planeCrash(a, "Slammed into rough ground!"); return; }
        q = 1; txt = "Rough field landing - off the runway";
        dmg = (vs > 2.0f) ? 16.0f : 7.0f;
    }
    else if (rollDeg > 18.0f) { planeCrash(a, "Wing strike on landing!"); return; }
    else if (vs <= 0.7f && gs < 45.0f) { q = 3; txt = "BUTTER LANDING!"; dmg = 0.0f; }
    else if (vs <= 1.7f) { q = 2; txt = "Firm landing - good job"; dmg = 1.0f; }
    else if (vs <= 3.0f) { q = 1; txt = "Hard landing"; dmg = 6.0f + (vs - 1.7f) * 4.0f; }
    else if (vs <= 5.0f) { q = 1; txt = "VERY hard landing - check the gear!"; dmg = 30.0f; }
    else { planeCrash(a, "You slammed into the runway!"); return; }

    if (gs > 60.0f) dmg += 15.0f;

    a.health = clampf(a.health - dmg, 0.0f, 100.0f);
    soundPlay(SND_HIT, 0.5f);
    a.landingQuality = q;
    strncpy(a.landingText, txt, 63);
    a.landingText[63] = 0;
    a.landingMsgTimer = 3.5f;
    a.landingEvent++;
    if (a.health <= 0.0f) planeCrash(a, "The plane broke apart on landing!");
}

// ---------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------
void craftReset(Aircraft& a, V3 pos, float yawRad, bool engineOn, float speed, float alt, bool airborne)
{
    a.pos = pos;
    a.pos.y = airborne ? alt : CRAFT_GEAR_H;
    V3 d = v3(sinf(yawRad), 0, -cosf(yawRad));
    a.vel = d * speed;
    a.yaw = yawRad;
    a.pitch = 0.0f;
    a.roll = 0.0f;
    a.throttle = airborne ? 0.55f : 0.0f;
    a.health = 100.0f;
    a.engineOn = engineOn;
    a.onGround = !airborne;
    a.brake = false;
    a.prop = 0.0f;
    a.inPitch = a.inRoll = a.inYaw = 0.0f;
    a.cPitch = a.cRoll = a.cYaw = 0.0f;
    a.cFlap = 0.0f;
    a.flapCmd = -1.0f;       // automatic flaps until the pilot asks for something else
    a.t = 0.0f;
    a.trim = 0.0f;
    a.altHold = a.pos.y;
    a.altI = 0.0f;
    a.fuel = gFuelCapacity;
    a.rpm = 0.0f;
    a.alpha = 0.0f;
    a.airspeed = speed;
    a.stall = false;
    a.phase = airborne ? PHASE_AIR : PHASE_GROUND;
    a.landingEvent = 0;
    a.landingQuality = -1;
    a.landingText[0] = 0;
    a.landingMsgTimer = 0.0f;
    a.crashTimer = 0.0f;
}

void craftStep(Aircraft& a, float dt)
{
    if (a.phase == PHASE_CRASHED)
    {
        a.crashTimer += dt;
        a.prop *= fmaxf(0.0f, 1.0f - 2.0f * dt);
        return;
    }
    if (a.landingMsgTimer > 0) a.landingMsgTimer -= dt;

    Basis b = basisFromYPR(a.yaw, a.pitch, a.roll);

    // wind: a steady breeze with slow gusts
    float gust = 1.0f + 0.38f * sinf(a.t * 0.31f) + 0.20f * sinf(a.t * 0.83f + 1.7f)
                  + 0.10f * sinf(a.t * 2.10f + 0.6f);
    float ws = gWindSpeed * gust;
    V3 wind = v3(sinf(gWindDirRad) * ws, 0.0f, -cosf(gWindDirRad) * ws);

    // airspeed is measured against the moving air
    V3 rel = a.vel - wind;
    float v = len(rel);
    float u = dot(rel, b.f);
    float w = dot(rel, b.u);
    float alpha = (v > 3.0f) ? atan2f(-w, u) : a.pitch;
    float alphaQ = clampf(alpha, -0.5f, 0.5f);

    // the flaps really do add lift (and a little drag)
    float flapCL = a.cFlap * 0.38f;
    float stallAlpha = ALPHA_STALL + a.cFlap * 0.05f;

    bool stalling = false;
    float CL = CL0 + CLA * alphaQ + flapCL;
    if (alphaQ > stallAlpha)
    {
        float ex = alphaQ - stallAlpha;
        CL = (CL0 + CLA * stallAlpha + flapCL) * fmaxf(0.30f, 1.0f - ex * 3.0f);
        stalling = true;
    }
    float q = 0.5f * RHO * v * v * WING_S;

    // ground effect: extra lift close to the ground
    float ge = 1.0f + 0.14f * clampf(1.0f - a.pos.y / 9.0f, 0.0f, 1.0f) * (a.onGround ? 0.0f : 1.0f);

    float lift = q * CL * ge;
    float dragC = 0.036f + 0.035f * CL * CL + a.cFlap * 0.018f + (stalling ? 0.22f : 0.0f);
    float drag = q * dragC;

    V3 F = v3(0, 0, 0);
    if (v > 0.5f) F -= rel * (drag / v);
    // the lift acts perpendicular to the airflow, not to the aircraft's own axes:
    // that is what makes a descending aeroplane gain speed and a climbing one
    // slow down. On the ground the wheels carry the weight, so the wing's lift
    // only pushes straight up.
    if (a.onGround)
    {
        F.y += lift;
    }
    else
    {
        V3 flow = (v > 0.5f) ? v3(rel.x / v, rel.y / v, rel.z / v) : b.f;
        V3 liftDir = b.u - flow * dot(b.u, flow);
        float ll = len(liftDir);
        if (ll > 0.001f) F += liftDir * (lift / ll);
        else F += b.u * lift;
    }
    // propeller thrust falls away as the aircraft speeds up; the trim wheel
    // changes the power, and with it the speed the aircraft wants to cruise at
    if (a.engineOn)
    {
        float thrust = MAX_THRUST * a.throttle * (1.0f - 0.40f * clampf(v / 150.0f, 0.0f, 1.0f));
        thrust *= (1.0f - a.trim * 0.22f);
        if (thrust < 0.0f) thrust = 0.0f;
        F += b.f * thrust;
    }
    F.y -= 9.81f * MASS;

    a.vel += F * (dt / MASS);
    a.pos += a.vel * dt;

    // ----- control response -----
    // the pilot's inputs are filtered, then the control surfaces follow with a
    // little lag - that is what makes a light aircraft feel smooth instead of twitchy
    float lag = 1.0f - expf(-dt / 0.11f);
    a.cPitch += (a.inPitch - a.cPitch) * lag;
    a.cRoll  += (a.inRoll  - a.cRoll)  * lag;
    a.cYaw   += (a.inYaw   - a.cYaw)   * lag;
    a.t += dt;
    // the flaps come out automatically when the aircraft is slow, and they stay
    // out on the take-off roll (which is what makes a short take-off possible)
    float wantFlap = (a.flapCmd >= 0.0f)
                   ? a.flapCmd
                   : ((v < 45.0f || (a.onGround && a.throttle > 0.4f)) ? 1.0f : 0.0f);
    a.cFlap += (wantFlap - a.cFlap) * (1.0f - expf(-dt / 0.7f));

    float auth = clampf(v / 36.0f, 0.0f, 1.0f);
    // extra pull authority close to the ground, so a flare really works
    float gndHelp = (!a.onGround && a.pos.y < 12.0f) ? 1.30f : 1.0f;
    // the elevator is only really effective with airspeed behind it, so the
    // aircraft answers slowly when it is nearly stalled
    float pitchRate = a.cPitch * 1.0f * (0.30f + 0.70f * auth * auth) * gndHelp;
    // the aircraft cannot pull harder than its structural limit, so asking for
    // more and more elevator only slows the response down instead of ballooning
    // the aircraft straight into a stall
    float gLoad = lift / (MASS * 9.81f);
    float gLim = clampf((gLoad - 2.2f) / 1.3f, 0.0f, 1.0f);
    pitchRate *= (1.0f - 0.55f * gLim);
    // and the elevator loses its authority as the wing gets close to the stall
    float stallMargin = clampf((stallAlpha - alpha) / 0.09f, 0.0f, 1.0f);
    pitchRate *= (0.40f + 0.60f * stallMargin);
    // on the runway there is no airflow over the tail until the aircraft is
    // rolling, so it cannot be rotated while it is standing still
    if (a.onGround) pitchRate *= clampf((v - 16.0f) / 20.0f, 0.0f, 1.0f);

    float rollRate = a.cRoll * 1.9f * clampf(v / 24.0f, 0.0f, 1.0f);

    // while the pilot is flying, remember the altitude they want when they let go
    float stickNow = fabsf(a.cPitch) + fabsf(a.cRoll);
    float pilotNow = clampf((stickNow - 0.05f) / 0.10f, 0.0f, 1.0f);
    if (!a.onGround && pilotNow > 0.5f) a.altHold = a.pos.y;
    if (!a.onGround && fabsf(a.cRoll) < 0.05f) rollRate += -a.roll * 1.25f;  // gentle wing leveller
    float yawRate = a.cYaw * 0.5f * (0.4f + 0.6f * auth);
    if (!a.onGround)
    {
        // the rudder is not needed to hold a turn - the aircraft coordinates itself
        yawRate += clampf(9.81f * tanf(clampf(a.roll, -1.2f, 1.2f)) / fmaxf(v, 16.0f), -0.80f, 0.80f);
        // weathervane stability: the nose follows the airflow (weaker in a bank,
        // so a turn stays coordinated instead of slipping)
        float course = atan2f(rel.x, -rel.z);
        yawRate += wrapPi(course - a.yaw) * 0.85f * clampf(v / 30.0f, 0.0f, 1.0f)
                 * (1.0f - 0.6f * fabsf(a.cYaw))
                 * (1.0f - 0.75f * fabsf(sinf(a.roll)));
        // hands-off cruise assist -------------------------------------------------
        // The pilot flies with the stick. Let go of it and the aircraft keeps the
        // altitude it had and flies level, using the same job split a real
        // autopilot uses: the pitch holds the altitude, the power holds the speed.
        //   outer loop : altitude error      -> vertical speed
        //   inner loop : flight path error   -> angle of attack -> pitch attitude
        // As soon as the pilot moves the stick the autopilot gets out of the way
        // completely, so it can never fight a descent or a turn.
        float pilot = pilotNow;
        // with the power back, or already sinking low down, the pilot is on an
        // approach: the assist then only helps with a gentle glide, and it hands
        // the last 30 m over completely so the flare stays the pilot's job
        bool landingCfg = (a.pos.y < 150.0f) && (a.vel.y < 0.5f)
                        && (a.throttle < 0.45f || a.vel.y < -0.8f);
        float altFade = landingCfg ? clampf((a.pos.y - 30.0f) / 60.0f, 0.0f, 1.0f) : 1.0f;
        float autoAuth = (1.0f - pilot)
                       * clampf((v - 18.0f) / 10.0f, 0.0f, 1.0f)       // off when the aircraft is slow
                       * altFade;
        if (autoAuth > 0.02f)
        {
            float hspd = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
            float gamma = (hspd > 1.0f) ? atan2f(a.vel.y, hspd) : 0.0f;   // flight path angle

            float vsCmd;
            if (landingCfg)
            {
                vsCmd = -1.6f;                       // a steady, gentle approach
                a.altI *= fmaxf(0.0f, 1.0f - dt * 0.5f);
            }
            else
            {
                // outer loop: how fast do we have to climb to get back to the altitude?
                float eH = a.altHold - a.pos.y;
                a.altI = clampf(a.altI + eH * 0.05f * dt, -1.5f, 1.5f);
                vsCmd = clampf(eH * 0.09f + a.altI, -3.0f, 3.0f);
                if (a.pos.y < 25.0f && vsCmd < 0.0f) vsCmd = fmaxf(vsCmd, -1.5f);   // never dive into the ground
                // a take-off leaves the aircraft only a few metres up, so the assist
                // climbs to a safe cruising height before the altitude hold takes over
                if (a.altHold < 60.0f && a.pos.y < 60.0f) vsCmd = fmaxf(vsCmd, 1.4f);
            }
            // a slow aeroplane with the throttle wide open is most likely on its
            // way up after a take-off, so keep it flying instead of letting it mush in
            bool keepFlying = (v < 34.0f && a.pos.y < 30.0f && a.throttle > 0.75f);
            if (keepFlying) vsCmd = fmaxf(vsCmd, 1.5f);

            // inner loop: the angle of attack that gives the lift we need, plus a
            // correction for the flight path we want to fly
            float qNow = 0.5f * RHO * v * v * WING_S;
            float cosRoll = fmaxf(0.35f, cosf(a.roll));
            // qNow already contains the wing area, so the lift coefficient is W / qNow
            float clNow = (MASS * 9.81f) / (qNow > 100.0f ? qNow : 100.0f) / cosRoll;
            float alpha1g = clampf((clNow - CL0 - a.cFlap * 0.38f) / CLA, -0.08f, 0.22f);
            float gammaCmd = (v > 8.0f) ? vsCmd / v : 0.0f;
            float alphaCmd = alpha1g + clampf((gammaCmd - gamma) * 1.30f, -0.08f, 0.08f);
            if (keepFlying) alphaCmd = fmaxf(alphaCmd, alpha1g);   // never less lift than it takes to hold the height
            alphaCmd = clampf(alphaCmd, -0.10f, 0.24f);
            float thetaCmd = alphaCmd + gammaCmd;          // attitude = angle of attack + wanted flight path
            pitchRate += clampf((thetaCmd - a.pitch) * 2.0f, -0.75f, 0.75f)
                       * clampf(v / 25.0f, 0.0f, 1.0f) * autoAuth;
        }
        else
        {
            a.altI *= fmaxf(0.0f, 1.0f - dt * 0.5f);       // bleed the integrator off
        }
    }
    // the stall break: the wing lets go just before the critical angle and the
    // nose drops, which is what stops the aircraft from mushing to the ground
    if (!a.onGround && alphaQ > stallAlpha - 0.05f)
    {
        float excess = alphaQ - (stallAlpha - 0.05f);
        a.pitch -= (0.10f + excess * 4.0f) * dt;
        rollRate *= 0.6f;
    }
    a.pitch = clampf(a.pitch + pitchRate * dt, -1.05f, 1.05f);
    a.roll = clampf(a.roll + rollRate * dt, -1.20f, 1.20f);
    a.yaw = wrapPi(a.yaw + yawRate * dt);

    a.stall = stalling && !a.onGround && v > 6.0f;
    a.alpha = alpha;
    a.airspeed = v;

    // engine: spools up, burns fuel and can run dry
    float targetRpm = a.engineOn ? (700.0f + 2000.0f * a.throttle) : 0.0f;
    a.rpm += (targetRpm - a.rpm) * (1.0f - expf(-dt / 0.7f));
    if (a.engineOn)
    {
        // about 14 l/h at cruise power, so a full tank gives a few hours of flying
        a.fuel -= (0.035f + 0.185f * a.throttle) * dt;
        if (a.fuel <= 0.0f) { a.fuel = 0.0f; a.engineOn = false; }
    }
    float propRate = a.rpm * 0.85f;
    if (propRate > 45.0f) propRate = 45.0f;      // capped so the blades do not strobe
    a.prop += dt * propRate;

    // ----- ground -----
    if (a.pos.y <= CRAFT_GEAR_H + 0.002f)
    {
        if (!a.onGround)
        {
            bool onRunway = worldIsOnRunway(a.pos.x, a.pos.z);
            touchdown(a, a.vel.y, onRunway);
            if (a.phase == PHASE_CRASHED) return;
        }
        a.onGround = true;
        a.pos.y = CRAFT_GEAR_H;
        a.altI = 0.0f;
        if (a.vel.y < 0) a.vel.y = 0;

        // settle the attitude while rolling
        a.roll -= a.roll * fminf(1.0f, 5.0f * dt);
        if (a.pitch < 0) a.pitch -= a.pitch * fminf(1.0f, 5.0f * dt);

        V3 hv = v3(a.vel.x, 0, a.vel.z);
        float gs = len(hv);

        // parked or taxiing: the nose settles back to level
        if (gs < 8.0f) a.pitch -= a.pitch * fminf(1.0f, 4.0f * dt);
        // the tailwheel/tail cone would strike the runway at this angle, so this
        // is as far as the nose can come up while the wheels are still down
        a.pitch = clampf(a.pitch, -0.05f, 0.20f);
        float dec = 0.7f + (a.brake ? 5.0f : 0.0f) + (a.engineOn ? 0.0f : 2.0f);
        if (gs > 0.01f)
        {
            float ns = fmaxf(0.0f, gs - dec * dt);
            a.vel.x *= ns / gs;
            a.vel.z *= ns / gs;
            gs = ns;
        }
        if (gs > 0.4f) a.yaw = wrapPi(a.yaw + a.inYaw * clampf(gs / 12.0f, 0.0f, 1.0f) * 0.85f * dt);

        float gs2 = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
        if (lift > MASS * 9.81f * 1.02f && a.pitch > 0.02f && gs2 > 20.0f)
        {
            a.onGround = false;
            a.pos.y = CRAFT_GEAR_H + 0.01f;
        }
    }
    else
    {
        a.onGround = false;
    }

    // ----- water and buildings -----
    if (a.pos.y < 2.2f && worldIsWater(a.pos.x, a.pos.z))
    {
        planeCrash(a, "Ditched in the river!");
        return;
    }
    if (worldHitBuilding(a.pos.x, a.pos.y, a.pos.z))
    {
        planeCrash(a, "You hit a building!");
        return;
    }
    float hill = worldHillHeightAt(a.pos.x, a.pos.z);
    if (hill > 1.0f && a.pos.y < hill + 1.2f)
    {
        planeCrash(a, "You flew into a hill!");
        return;
    }
    if (fabsf(a.pos.x) > 9000.0f || fabsf(a.pos.z) > 9000.0f)
    {
        a.pos.x = clampf(a.pos.x, -9000.0f, 9000.0f);
        a.pos.z = clampf(a.pos.z, -9000.0f, 9000.0f);
    }
}

// ---------------------------------------------------------------------
// model
// ---------------------------------------------------------------------
static void drawCessna(const Aircraft& a, bool detailed,
                       float br, float bg, float bb,
                       float sr, float sg, float sb,
                       bool withFlag, const char* reg)
{
    float propDeg = rad2deg(a.prop);
    float cp = a.cPitch, cr = a.cRoll, cy = a.cYaw, flap = a.cFlap;
    float thr = a.throttle;
    float beacon = (fmodf(a.t, 1.0f) < 0.14f) ? 1.0f : 0.12f;
    // ---------- tapering tail boom ----------
    glPushMatrix();
    glTranslatef(0, 0.0f, 2.0f);
    glRotatef(90.0f, 1, 0, 0);
    setColor(br, bg, bb);
    drawPrism(1.30f, 1.70f, 1.35f, 0.42f);
    glPopMatrix();

    // ---------- cabin ----------
    setColor(br, bg, bb);
    drawBoxAt(0, 0.15f, -0.30f, 1.30f, 1.35f, 4.60f);
    setColor(br * 0.96f, bg * 0.96f, bb * 0.96f);
    drawBoxAt(0, 0.86f, -0.30f, 1.34f, 0.12f, 4.50f);        // roof lip
    setColor(br * 0.90f, bg * 0.90f, bb * 0.90f);
    drawBoxAt(0, -0.55f, -0.30f, 0.90f, 0.22f, 4.30f);        // keel

    // livery: cheat line rising towards the tail plus a gold pinstripe
    setColor(sr, sg, sb);
    drawBoxAt(0, -0.34f, -1.30f, 1.34f, 0.17f, 2.60f);
    drawBoxAt(0, -0.22f, 0.55f, 1.34f, 0.17f, 1.90f);
    drawBoxAt(0, -0.10f, 1.95f, 1.06f, 0.17f, 1.20f);
    setColor(0.92f, 0.78f, 0.25f);
    drawBoxAt(0, -0.46f, -1.30f, 1.35f, 0.07f, 2.60f);
    drawBoxAt(0, -0.34f, 0.55f, 1.35f, 0.07f, 1.90f);

    // ---------- glazing ----------
    setColor(0.16f, 0.21f, 0.28f);
    glPushMatrix();
    glTranslatef(0, 0.45f, -2.35f);
    glRotatef(32.0f, 1, 0, 0);
    drawBox(1.16f, 0.10f, 1.10f);
    glPopMatrix();
    for (int s = 0; s < 2; s++)
    {
        float zz = -1.55f + s * 1.05f;
        drawBoxAt(-0.655f, 0.36f, zz, 0.06f, 0.52f, 0.86f);
        drawBoxAt(0.655f, 0.36f, zz, 0.06f, 0.52f, 0.86f);
    }
    drawBoxAt(0, 0.50f, 1.55f, 0.70f, 0.34f, 0.08f);
    setColor(br * 0.85f, bg * 0.85f, bb * 0.85f);
    drawBoxAt(-0.665f, 0.36f, -1.02f, 0.03f, 0.60f, 0.08f);
    drawBoxAt(0.665f, 0.36f, -1.02f, 0.03f, 0.60f, 0.08f);
    // seats visible through the glass
    setColor(0.45f, 0.16f, 0.16f);
    drawBoxAt(-0.30f, 0.05f, -0.70f, 0.42f, 0.45f, 0.16f);
    drawBoxAt(0.30f, 0.05f, -0.70f, 0.42f, 0.45f, 0.16f);
    // door and step
    setColor(0.45f, 0.45f, 0.47f);
    drawBoxAt(-0.66f, 0.05f, -0.30f, 0.03f, 1.00f, 1.10f);
    setColor(0.30f, 0.30f, 0.32f);
    drawBoxAt(-0.69f, 0.10f, 0.10f, 0.04f, 0.10f, 0.22f);
    setColor(0.40f, 0.40f, 0.42f);
    drawBoxAt(-0.78f, -0.62f, -0.10f, 0.20f, 0.06f, 0.55f);

    // ---------- nose, cowling, exhaust ----------
    setColor(br, bg, bb);
    glPushMatrix();
    glTranslatef(0, 0.15f, -2.60f);
    glRotatef(-90.0f, 1, 0, 0);
    drawCone(0.64f, 1.10f, 10);
    glPopMatrix();
    setColor(sr, sg, sb);
    glPushMatrix();
    glTranslatef(0, -0.18f, -2.72f);
    glRotatef(-90.0f, 1, 0, 0);
    drawCylinderRaw(0.60f, 0.56f, 0.32f, 10, false);
    glPopMatrix();
    setColor(0.22f, 0.22f, 0.24f);
    glPushMatrix();
    glTranslatef(0.30f, -0.45f, -2.55f);
    glRotatef(90.0f, 1, 0, 0);
    drawCylinderRaw(0.10f, 0.08f, 0.70f, 6, true);
    glPopMatrix();

    // propeller: a translucent disc that blurs more as the engine revs up
    glPushMatrix();
    glTranslatef(0, 0.15f, -3.75f);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setColorA(0.88f, 0.90f, 0.94f, 0.08f + 0.20f * thr);
    glPushMatrix(); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(1.85f, 16); glPopMatrix();
    glEnable(GL_LIGHTING);
    setColorA(0.22f, 0.22f, 0.24f, 1.0f - 0.55f * thr);
    glPushMatrix(); glRotatef(propDeg, 0, 0, 1); drawBox(0.15f, 3.50f, 0.09f); glPopMatrix();
    glPushMatrix(); glRotatef(propDeg + 90.0f, 0, 0, 1); drawBox(0.15f, 3.50f, 0.09f); glPopMatrix();
    glDisable(GL_BLEND);
    setColor(0.85f, 0.30f, 0.20f);
    glPushMatrix(); glRotatef(-90.0f, 1, 0, 0); drawCone(0.26f, 0.55f, 8); glPopMatrix();
    glPopMatrix();

    // ---------- wings with dihedral, flaps, ailerons and tip fairings ----------
    for (int s = 0; s < 2; s++)
    {
        float sgn = (s == 0) ? -1.0f : 1.0f;
        glPushMatrix();
        glTranslatef(sgn * 2.72f, 1.02f, -0.30f);
        glRotatef(-sgn * 3.0f, 0, 0, 1);
        setColor(br, bg, bb);
        drawBox(5.35f, 0.26f, 1.78f);
        setColor(sr, sg, sb);
        drawBoxAt(0, 0, -0.90f, 5.35f, 0.20f, 0.26f);
        setColor(br * 0.93f, bg * 0.93f, bb * 0.93f);
        glPushMatrix();
        glTranslatef(sgn * 1.35f, -0.02f, 0.98f);
        glRotatef(sgn * 26.0f * flap, 1, 0, 0);
        drawBox(2.20f, 0.10f, 0.34f);                                 // flap
        glPopMatrix();
        glPushMatrix();
        glTranslatef(sgn * 3.90f, -0.02f, 0.98f);
        glRotatef(-sgn * 24.0f * cr, 1, 0, 0);
        drawBox(1.90f, 0.10f, 0.34f);                                 // aileron
        glPopMatrix();
        setColor(br * 0.95f, bg * 0.95f, bb * 0.95f);
        drawBoxAt(sgn * 2.66f, 0.02f, 0.0f, 0.22f, 0.30f, 1.74f);     // tip fairing
        glPopMatrix();
    }
    setColor(br, bg, bb);
    drawBoxAt(0, 1.00f, -0.30f, 1.70f, 0.34f, 1.95f);                 // root fairing
    // struts and jury struts
    setColor(0.80f, 0.80f, 0.82f);
    for (int s = 0; s < 2; s++)
    {
        float sgn = (s == 0) ? -1.0f : 1.0f;
        glPushMatrix();
        glTranslatef(sgn * 1.55f, 0.20f, -0.30f);
        glRotatef(sgn * 37.5f, 0, 0, 1);
        drawBox(2.50f, 0.09f, 0.14f);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(sgn * 1.78f, 0.48f, -0.20f);
        glRotatef(sgn * 58.0f, 0, 0, 1);
        drawBox(1.05f, 0.06f, 0.08f);
        glPopMatrix();
    }
    // nav lights: red left, green right, landing light in the left wing
    setColor(0.95f, 0.15f, 0.12f);
    glPushMatrix(); glTranslatef(-5.36f, 1.28f, -0.30f); drawSphere(0.11f, 6, 4); glPopMatrix();
    setColor(0.15f, 0.90f, 0.25f);
    glPushMatrix(); glTranslatef(5.36f, 1.28f, -0.30f); drawSphere(0.11f, 6, 4); glPopMatrix();
    setColor(0.95f, 0.95f, 0.90f);
    glPushMatrix(); glTranslatef(-2.10f, 0.92f, -1.05f); drawSphere(0.12f, 6, 4); glPopMatrix();

    // ---------- tail ----------
    setColor(br, bg, bb);
    glPushMatrix();
    glTranslatef(0, 0.05f, 3.05f);
    drawPrism(0.16f, 1.55f, 1.45f, 0.35f);                          // swept fin
    glPopMatrix();
    setColor(sr, sg, sb);
    glPushMatrix();
    glTranslatef(0, 0.05f, 3.15f);
    drawPrism(0.18f, 1.00f, 1.20f, 0.55f);                          // fin stripe
    glPopMatrix();
    // rudder moves with the rudder input
    setColor(br * 0.95f, bg * 0.95f, bb * 0.95f);
    glPushMatrix();
    glTranslatef(0, 0.75f, 3.70f);
    glRotatef(22.0f * cy, 0, 1, 0);
    drawBox(0.14f, 1.20f, 0.36f);
    glPopMatrix();
    setColor(br, bg, bb);
    drawBoxAt(0, 0.52f, 3.35f, 3.90f, 0.17f, 1.05f);                 // stabiliser
    setColor(br * 0.93f, bg * 0.93f, bb * 0.93f);
    glPushMatrix();
    glTranslatef(0, 0.50f, 3.92f);
    glRotatef(20.0f * cp, 1, 0, 0);
    drawBox(3.90f, 0.10f, 0.28f);                                     // elevator
    glPopMatrix();
    setColor(sr, sg, sb);
    drawBoxAt(0, 0.62f, 3.35f, 3.95f, 0.07f, 0.30f);
    // tail beacon and antennas
    setColor(0.95f * beacon + 0.25f, 0.25f * beacon, 0.20f * beacon);
    glPushMatrix(); glTranslatef(0, 1.62f, 3.05f); drawSphere(0.10f, 6, 4); glPopMatrix();
    setColor(0.30f, 0.30f, 0.32f);
    glPushMatrix(); glTranslatef(0, 1.58f, 3.42f); glRotatef(-25.0f, 1, 0, 0); drawBox(0.06f, 0.55f, 0.30f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0.92f, -1.40f); drawBox(0.05f, 0.28f, 0.45f); glPopMatrix();
    // pitot tube under the wing
    glPushMatrix(); glTranslatef(-1.60f, 0.80f, -1.20f); glRotatef(90.0f, 1, 0, 0);
    drawCylinderRaw(0.035f, 0.03f, 0.45f, 5, true);
    glPopMatrix();

    // Bangladeshi flag on the fin (both sides)
    if (withFlag)
    {
        setColor(0.00f, 0.42f, 0.25f);
        drawBoxAt(0, 1.00f, 3.05f, 0.05f, 0.42f, 0.62f);
        setColor(0.85f, 0.13f, 0.15f);
        glPushMatrix(); glTranslatef(0.04f, 1.00f, 3.05f); glRotatef(90.0f, 1, 0, 0); drawDiskXZ(0.12f, 10); glPopMatrix();
        glPushMatrix(); glTranslatef(-0.04f, 1.00f, 3.05f); glRotatef(-90.0f, 1, 0, 0); drawDiskXZ(0.12f, 10); glPopMatrix();
    }
    // registration on the right of the rear fuselage
    if (reg)
    {
        setColor(sr, sg, sb);
        glPushMatrix();
        glTranslatef(0.68f, -0.05f, 1.35f);
        glRotatef(90.0f, 0, 1, 0);
        glLineWidth(1.6f);
        strokeTextCentered(GLUT_STROKE_ROMAN, reg, 0.0055f);
        glLineWidth(1.0f);
        glPopMatrix();
    }

    // ---------- landing gear ----------
    if (detailed)
    {
        for (int s = 0; s < 2; s++)
        {
            float sgn = (s == 0) ? -1.0f : 1.0f;
            setColor(0.55f, 0.56f, 0.58f);
            glPushMatrix();
            glTranslatef(sgn * 0.78f, -0.78f, 0.55f);
            glRotatef(sgn * 30.0f, 0, 0, 1);
            drawBox(0.13f, 0.75f, 0.13f);
            glPopMatrix();
            setColor(0.13f, 0.13f, 0.15f);
            glPushMatrix(); glTranslatef(sgn * 1.05f, -1.02f, 0.55f); glRotatef(90.0f, 0, 0, 1);
            drawCylinderRaw(0.26f, 0.26f, 0.17f, 10, true);
            glPopMatrix();
            setColor(0.72f, 0.72f, 0.74f);
            glPushMatrix(); glTranslatef(sgn * 0.96f, -1.02f, 0.55f); glRotatef(90.0f, 0, 0, 1);
            drawCylinderRaw(0.09f, 0.09f, 0.21f, 8, true);
            glPopMatrix();
        }
        setColor(0.55f, 0.56f, 0.58f);
        drawBoxAt(0, -0.72f, -2.25f, 0.11f, 0.60f, 0.11f);
        drawBoxAt(0, -0.95f, -2.30f, 0.30f, 0.08f, 0.16f);
        setColor(0.13f, 0.13f, 0.15f);
        glPushMatrix(); glTranslatef(0, -1.00f, -2.30f); glRotatef(90.0f, 0, 0, 1);
        drawCylinderRaw(0.21f, 0.21f, 0.14f, 10, true);
        glPopMatrix();
        setColor(0.72f, 0.72f, 0.74f);
        glPushMatrix(); glTranslatef(0, -1.00f, -2.30f); glRotatef(90.0f, 0, 0, 1);
        drawCylinderRaw(0.07f, 0.07f, 0.18f, 8, true);
        glPopMatrix();
    }
}

// ---------------------------------------------------------------------
// The player's aircraft: a modern low-visibility fighter in the style of the
// front-line types the Bangladesh Air Force flies - twin canted fins, swept
// wings with canards, twin afterburning engines and a red-and-green roundel.
// Like every other object in the project it is built from primitives, and it
// uses the same model space as the old aircraft: nose towards -Z, +X to the
// right, +Y up.
// ---------------------------------------------------------------------
static void drawRoundel(float x, float y, float z, float r, float yawDeg)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0, 1, 0);
    glRotatef(90.0f, 1, 0, 0);        // flat, facing up
    setColor(0.10f, 0.42f, 0.22f);    // dark green outer field
    glPushMatrix(); glTranslatef(0, 0, 0.0f); drawDiskXZ(r, 14); glPopMatrix();
    setColor(0.78f, 0.12f, 0.14f);    // red disc, as on the national roundel
    glPushMatrix(); glTranslatef(0, 0, 0.02f); drawDiskXZ(r * 0.55f, 12); glPopMatrix();
    glPopMatrix();
}

// "IMRAN KHAN" in the stroke font on the fuselage side (sgn = -1 left, +1 right)
static void drawJetName(float sgn)
{
    glPushMatrix();
    glTranslatef(sgn * 0.74f, 0.62f, -2.35f);
    glRotatef(sgn * 90.0f, 0, 1, 0);          // the text plane faces outward
    setColor(0.92f, 0.94f, 0.95f);
    strokeTextCentered(GLUT_STROKE_ROMAN, "IMRAN KHAN", 0.011f);
    glPopMatrix();
}

// the national flag as a sticker: a green field with the red disc
static void drawBdFlag(float sgn)
{
    glPushMatrix();
    glTranslatef(sgn * 0.79f, 1.75f, 4.05f);
    glRotatef(sgn * 90.0f, 0, 1, 0);

    const float w = 1.25f, h = 0.75f;
    setColor(0.08f, 0.40f, 0.21f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-w * 0.5f, -h * 0.5f, 0.0f);
    glVertex3f( w * 0.5f, -h * 0.5f, 0.0f);
    glVertex3f( w * 0.5f,  h * 0.5f, 0.0f);
    glVertex3f(-w * 0.5f,  h * 0.5f, 0.0f);
    glEnd();

    setColor(0.86f, 0.13f, 0.16f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 0, 1);
    glVertex3f(0.06f, 0.0f, 0.02f);
    for (int i = 0; i <= 16; i++)
    {
        float a = (float)i / 16.0f * 6.2831853f;
        glVertex3f(0.06f + cosf(a) * 0.24f, sinf(a) * 0.24f, 0.02f);
    }
    glEnd();
    glPopMatrix();
}

static void drawJet(const Aircraft& a)
{
    // panel detail from a photograph, so the airframe is not just flat colour
    bool tex = textureBind(TEX_METAL_SIDING);
    const float camoR = 0.62f, camoG = 0.67f, camoB = 0.60f;   // lighter, so the texture shows
    const float liteR = 0.60f, liteG = 0.64f, liteB = 0.68f;   // pale underside

    // ---------------- fuselage ----------------
    setColor(camoR, camoG, camoB);
    glPushMatrix(); glTranslatef(0, 0.0f, -5.2f); glRotatef(-90.0f, 1, 0, 0);
    drawCone(0.52f, 2.4f, 10); glPopMatrix();                  // radome
    drawBoxAt(0, 0.0f, -3.5f, 1.05f, 1.0f, 3.6f);              // forward fuselage
    setColor(camoR * 1.06f, camoG * 1.06f, camoB * 1.06f);
    drawBoxAt(0, 0.02f, 0.2f, 1.35f, 1.2f, 4.2f);              // centre body
    setColor(camoR, camoG, camoB);
    drawBoxAt(0, 0.0f, 4.4f, 1.15f, 1.05f, 3.2f);              // aft fuselage

    // pale underside panels
    setColor(liteR, liteG, liteB);
    drawBoxAt(0, -0.60f, -3.4f, 0.95f, 0.22f, 3.4f);
    drawBoxAt(0, -0.66f, 0.4f, 1.20f, 0.22f, 4.2f);
    drawBoxAt(0, -0.58f, 4.3f, 1.00f, 0.20f, 3.0f);

    // canopy: a tinted bubble with a frame
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setColorA(0.18f, 0.30f, 0.40f, 0.72f);
    glPushMatrix();
    glTranslatef(0, 0.62f, -2.9f);
    glScalef(0.62f, 0.52f, 1.5f);
    drawSphere(1.0f, 10, 6);
    glPopMatrix();
    glDisable(GL_BLEND);
    setColor(0.26f, 0.28f, 0.26f);
    drawBoxAt(0, 0.55f, -3.9f, 0.9f, 0.16f, 0.4f);             // windscreen frame

    // ---------------- wings ----------------
    // swept, mid-mounted, a little anhedral; the tip sits aft of the root
    for (int s = 0; s < 2; s++)
    {
        float sgn = (s == 0) ? -1.0f : 1.0f;
        // The wing panel is centred on its origin, so the width is the full span
        // of one wing: 10.6 m here, from the root at 0.9 m out to a 6.2 m tip.
        // The panel is also lifted clear of the fuselage and given dihedral, so
        // it is visible from the chase camera instead of edge-on.
        glPushMatrix();
        glTranslatef(sgn * 0.90f, 0.10f, -0.65f);
        glRotatef(sgn * 3.0f, 0, 0, 1);                        // dihedral, tips up
        setColor(0.70f, 0.75f, 0.68f);                         // lighter than the fuselage
        drawPrism(10.6f, 0.40f, 4.4f, 0.66f);                  // the wing itself
        // a pale leading-edge strip across the whole span, to catch the eye
        setColor(0.86f, 0.89f, 0.86f);
        drawBoxAt(0.0f, 0.34f, -2.35f, 10.3f, 0.14f, 0.30f);
        glPopMatrix();

        // leading-edge root extension, the modern fighter look
        setColor(camoR * 0.95f, camoG * 0.95f, camoB * 0.95f);
        glPushMatrix();
        glTranslatef(sgn * 0.75f, 0.02f, -2.6f);
        glRotatef(sgn * 34.0f, 0, 1, 0);
        drawPrism(2.6f, 0.16f, 1.1f, 0.35f);
        glPopMatrix();

        // canard: small foreplane ahead of the wing
        setColor(camoR * 1.05f, camoG * 1.05f, camoB * 1.05f);
        glPushMatrix();
        glTranslatef(sgn * 1.15f, 0.16f, -3.9f);
        glRotatef(sgn * 28.0f, 0, 1, 0);
        drawPrism(1.7f, 0.14f, 1.0f, 0.5f);
        glPopMatrix();

        // wingtip missile rail with a missile under it
        setColor(0.35f, 0.36f, 0.36f);
        drawBoxAt(sgn * 5.4f, 0.02f, 0.2f, 0.18f, 0.14f, 1.6f);
        setColor(0.78f, 0.78f, 0.80f);
        glPushMatrix(); glTranslatef(sgn * 5.4f, -0.24f, 0.2f); glRotatef(-90.0f, 1, 0, 0);
        drawCylinderRaw(0.11f, 0.11f, 2.2f, 8, true); glPopMatrix();
        setColor(0.30f, 0.30f, 0.32f);
        glPushMatrix(); glTranslatef(sgn * 5.4f, -0.24f, 1.15f); glRotatef(-90.0f, 1, 0, 0);
        drawCone(0.11f, 0.5f, 8); glPopMatrix();

        // roundel on the wing, and the navigation light at the tip
        drawRoundel(sgn * 3.4f, 0.50f, 0.2f, 0.46f, 0.0f);
        if (s == 0) setColor(0.85f, 0.12f, 0.12f);
        else setColor(0.15f, 0.80f, 0.25f);
        glPushMatrix(); glTranslatef(sgn * 6.05f, 0.28f, 0.9f); drawSphere(0.14f, 6, 4); glPopMatrix();
    }

    // ---------------- tail ----------------
    for (int s = 0; s < 2; s++)
    {
        float sgn = (s == 0) ? -1.0f : 1.0f;
        // canted vertical fins on the engine nacelles
        glPushMatrix();
        glTranslatef(sgn * 0.72f, 0.45f, 3.6f);
        glRotatef(-sgn * 20.0f, 0, 0, 1);
        setColor(camoR, camoG, camoB);
        drawPrism(0.24f, 2.9f, 3.0f, 0.55f);
        setColor(0.20f, 0.42f, 0.40f);
        drawBoxAt(0.0f, 2.2f, 1.1f, 0.10f, 0.7f, 0.9f);        // fin tip antenna
        glPopMatrix();

        // stabilator
        glPushMatrix();
        glTranslatef(sgn * 0.95f, 0.10f, 4.6f);
        glRotatef(sgn * 24.0f, 0, 1, 0);
        setColor(camoR * 1.05f, camoG * 1.05f, camoB * 1.05f);
        drawPrism(2.9f, 0.16f, 1.5f, 0.55f);
        glPopMatrix();

        // engine nacelle and nozzle
        setColor(camoR * 0.9f, camoG * 0.9f, camoB * 0.9f);
        drawBoxAt(sgn * 0.72f, -0.05f, 4.3f, 1.0f, 1.0f, 3.6f);
        setColor(0.28f, 0.28f, 0.30f);
        glPushMatrix(); glTranslatef(sgn * 0.72f, -0.05f, 6.1f); glRotatef(-90.0f, 1, 0, 0);
        drawCylinderRaw(0.42f, 0.40f, 1.0f, 10, false); glPopMatrix();

        // intake under the LERX
        setColor(0.16f, 0.17f, 0.18f);
        drawBoxAt(sgn * 0.95f, -0.35f, -0.9f, 0.5f, 0.55f, 1.6f);
    }

    // ---------------- afterburner ----------------
    // two cones that grow with the throttle, so a fast pass looks fast
    {
        float ab = clampf((a.throttle - 0.55f) / 0.45f, 0.0f, 1.0f);
        if (ab > 0.02f)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (int s = 0; s < 2; s++)
            {
                float sgn = (s == 0) ? -1.0f : 1.0f;
                float len = 1.6f + 5.6f * ab;
                glPushMatrix();
                glTranslatef(sgn * 0.72f, -0.05f, 6.6f);
                glRotatef(90.0f, 1, 0, 0);      // point aft (+Z)
                setColorA(1.00f, 0.60f * (1.0f - ab * 0.4f), 0.25f, 0.30f + 0.35f * ab);
                drawCone(0.36f + 0.12f * ab, len, 10);
                setColorA(0.60f, 0.85f, 1.00f, 0.35f * ab);
                drawCone(0.20f + 0.10f * ab, len * 1.5f, 8);
                glPopMatrix();
            }
            glDisable(GL_BLEND);
        }
    }

    // ---------------- small details ----------------
    // panel lines along the fuselage and the wing roots
    setColor(0.22f, 0.24f, 0.22f);
    for (int s = 0; s < 2; s++)
    {
        float sg = (s == 0) ? -1.0f : 1.0f;
        drawBoxAt(sg * 0.56f, 0.0f, -2.0f, 0.04f, 0.9f, 3.0f);
        drawBoxAt(sg * 0.56f, 0.0f, 2.2f, 0.04f, 0.8f, 3.6f);
    }
    drawBoxAt(0.0f, 0.61f, 0.2f, 0.10f, 0.10f, 3.4f);

    setColor(0.20f, 0.22f, 0.20f);
    drawBoxAt(0, 0.62f, 1.9f, 0.16f, 0.5f, 0.16f);             // dorsal antenna
    setColor(0.75f, 0.75f, 0.72f);
    glPushMatrix(); glTranslatef(0, 0.05f, -7.3f); glRotatef(-90.0f, 1, 0, 0);
    drawCylinderRaw(0.05f, 0.02f, 1.6f, 5, false); glPopMatrix();   // pitot boom
    // fuselage roundel, both sides
    for (int s = 0; s < 2; s++)
    {
        float sgn = (s == 0) ? -1.0f : 1.0f;
        glPushMatrix();
        glTranslatef(sgn * 0.70f, 0.10f, 0.6f);
        glRotatef(sgn * 90.0f, 0, 1, 0);
        glRotatef(90.0f, 1, 0, 0);
        setColor(0.10f, 0.42f, 0.22f);
        drawDiskXZ(0.40f, 12);
        glTranslatef(0, 0, 0.02f);
        setColor(0.78f, 0.12f, 0.14f);
        drawDiskXZ(0.22f, 10);
        glPopMatrix();
    }
    // the name on both sides of the forward fuselage, and the flag on both fins
    drawJetName(-1.0f);
    drawJetName( 1.0f);
    drawBdFlag(-1.0f);
    drawBdFlag( 1.0f);

    if (tex) textureUnbind();
}

static void drawPlayerCessna(const Aircraft& a)
{
    drawJet(a);
}

void craftDraw(const Aircraft& a)
{
    glPushMatrix();
    glTranslatef(a.pos.x, a.pos.y, a.pos.z);
    glRotatef(-rad2deg(a.yaw), 0, 1, 0);
    glRotatef(rad2deg(a.pitch), 1, 0, 0);
    glRotatef(-rad2deg(a.roll), 0, 0, 1);
    glScalef(1.18f, 1.18f, 1.18f);          // a little bigger, as asked
    drawPlayerCessna(a);
    glPopMatrix();
}

void craftDrawSimple(float x, float y, float z, float yawRad, float rollRad, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(-rad2deg(yawRad), 0, 1, 0);
    glRotatef(-rad2deg(rollRad), 0, 0, 1);
    setColor(r, g, b);
    drawBoxAt(0, 0.15f, 0, 1.1f, 1.1f, 6.2f);
    drawBoxAt(0, 0.9f, -0.2f, 8.6f, 0.22f, 1.5f);
    drawBoxAt(0, 0.9f, 3.0f, 0.14f, 1.2f, 1.1f);
    drawBoxAt(0, 0.45f, 3.05f, 3.0f, 0.18f, 0.8f);
    setColor(0.15f, 0.20f, 0.28f);
    drawBoxAt(0, 0.35f, -0.6f, 1.16f, 0.45f, 1.7f);
    glPopMatrix();
}

void craftDrawCockpitOverlay(float aspect)
{
    // the cockpit gets its own projection: the world is drawn with a 1.5 m near
    // plane, which would clip the instrument panel straight out of the view
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPerspective(72.0, aspect, 0.05, 50.0);   // wide enough to see the whole panel
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);      // the cockpit is lit from inside, not by the sun

    // the cabin around the windscreen: everything outside the opening is dark
    // trim, so the view reads as a cockpit instead of a floating dashboard
    setColor(0.13f, 0.14f, 0.15f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    // roof / headliner above the windscreen
    glVertex3f(-9.0f, 5.0f, -1.56f);  glVertex3f(9.0f, 5.0f, -1.56f);
    glVertex3f(9.0f, 0.46f, -1.56f);  glVertex3f(-9.0f, 0.46f, -1.56f);
    // side trim, left and right of the windscreen
    glVertex3f(-9.0f, 0.46f, -1.56f); glVertex3f(-1.50f, 0.46f, -1.56f);
    glVertex3f(-1.50f, -1.20f, -1.56f); glVertex3f(-9.0f, -1.20f, -1.56f);
    glVertex3f(1.50f, 0.46f, -1.56f); glVertex3f(9.0f, 0.46f, -1.56f);
    glVertex3f(9.0f, -1.20f, -1.56f); glVertex3f(1.50f, -1.20f, -1.56f);
    // lower edge below the glareshield
    glVertex3f(-9.0f, -1.20f, -1.56f); glVertex3f(9.0f, -1.20f, -1.56f);
    glVertex3f(9.0f, -0.42f, -1.56f);  glVertex3f(-9.0f, -0.42f, -1.56f);
    glEnd();

    // roof edge and slim windscreen pillars
    setColor(0.26f, 0.27f, 0.29f);
    drawBoxAt(0, 0.46f, -1.55f, 3.20f, 0.10f, 0.10f);
    drawBoxAt(-1.50f, -0.05f, -1.55f, 0.06f, 1.00f, 0.06f);
    drawBoxAt(1.50f, -0.05f, -1.55f, 0.06f, 1.00f, 0.06f);

    // dashboard top (glareshield)
    setColor(0.26f, 0.24f, 0.22f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-0.72f, -0.12f, -1.00f); glVertex3f(0.72f, -0.12f, -1.00f);
    glVertex3f(0.72f, -0.12f, -0.58f);  glVertex3f(-0.72f, -0.12f, -0.58f);
    glEnd();
    // instrument panel face
    setColor(0.18f, 0.18f, 0.19f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.64f, -0.40f, -0.59f); glVertex3f(0.64f, -0.40f, -0.59f);
    glVertex3f(0.64f, -0.12f, -0.58f);  glVertex3f(-0.64f, -0.12f, -0.58f);
    glEnd();

    // six round instruments
    for (int i = 0; i < 4; i++)
    {
        float gx = -0.33f + i * 0.22f;
        setColor(0.08f, 0.08f, 0.09f);
        glPushMatrix(); glTranslatef(gx, -0.25f, -0.575f); glRotatef(90.0f, 1, 0, 0);
        drawDiskXZ(0.050f, 12);
        glPopMatrix();
        setColor(0.90f, 0.89f, 0.84f);
        glPushMatrix(); glTranslatef(gx, -0.25f, -0.566f); glRotatef(90.0f, 1, 0, 0);
        drawDiskXZ(0.040f, 12);
        glPopMatrix();
        setColor(0.15f, 0.15f, 0.16f);
        glPushMatrix();
        glTranslatef(gx, -0.25f, -0.559f);
        glRotatef(-40.0f + i * 27.0f, 0, 0, 1);
        drawBox(0.008f, 0.032f, 0.008f);
        glPopMatrix();
    }
    // standby compass and radio stack
    setColor(0.10f, 0.10f, 0.11f);
    glPushMatrix(); glTranslatef(-0.47f, -0.19f, -0.575f); glRotatef(90.0f, 1, 0, 0);
    drawDiskXZ(0.032f, 10);
    glPopMatrix();
    setColor(0.16f, 0.17f, 0.18f);
    drawBoxAt(0.47f, -0.25f, -0.575f, 0.14f, 0.20f, 0.04f);
    setColor(0.30f, 0.85f, 0.45f);
    drawBoxAt(0.47f, -0.19f, -0.555f, 0.10f, 0.03f, 0.02f);

    // control yoke
    setColor(0.12f, 0.12f, 0.13f);
    drawBoxAt(0, -0.34f, -0.49f, 0.05f, 0.18f, 0.05f);
    drawBoxAt(-0.14f, -0.30f, -0.49f, 0.24f, 0.04f, 0.04f);
    drawBoxAt(0.14f, -0.30f, -0.49f, 0.24f, 0.04f, 0.04f);
    setColor(0.20f, 0.20f, 0.22f);
    drawBoxAt(-0.26f, -0.29f, -0.49f, 0.05f, 0.06f, 0.06f);
    drawBoxAt(0.26f, -0.29f, -0.49f, 0.05f, 0.06f, 0.06f);
    // throttle and mixture levers
    setColor(0.15f, 0.15f, 0.16f);
    drawBoxAt(0.40f, -0.36f, -0.52f, 0.03f, 0.11f, 0.03f);
    drawBoxAt(0.45f, -0.36f, -0.52f, 0.03f, 0.11f, 0.03f);
    setColor(0.85f, 0.85f, 0.80f);
    drawBoxAt(0.40f, -0.29f, -0.52f, 0.05f, 0.03f, 0.05f);
    drawBoxAt(0.45f, -0.29f, -0.52f, 0.05f, 0.03f, 0.05f);

    glEnable(GL_LIGHTING);
    glEnable(GL_FOG);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
