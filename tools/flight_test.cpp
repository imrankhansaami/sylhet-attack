// flight_test.cpp - headless validation of the flight model and the maths.
//
// Runs the real flight model (src/aircraft.cpp) through controlled scenarios and
// compares the results with what the aerodynamics say they should be.
// No OpenGL context is needed: only the physics is exercised, never the drawing.
//
// Build (from the project folder):
//   g++ -O2 -DFREEGLUT_STATIC -Ithird_party/freeglut/include -Isrc tools/flight_test.cpp
//       src/aircraft.cpp src/world.cpp src/glutil.cpp src/textures.cpp -Lthird_party/freeglut/lib
//       -lfreeglut -lopengl32 -lglu32 -lwinmm -o build/flight_test.exe
// Run:  build/flight_test.exe

#include <cstdio>
#include <cmath>
#include "../src/vecmath.h"
#include "../src/aircraft.h"
#include "../src/world.h"

static const float DT = 1.0f / 180.0f;
static const float DEG = 180.0f / 3.14159265358979f;

static void run(Aircraft& a, float seconds, float inP, float inR, float inY)
{
    int n = (int)(seconds / DT);
    for (int i = 0; i < n; i++)
    {
        a.inPitch = inP;
        a.inRoll = inR;
        a.inYaw = inY;
        craftStep(a, DT);
    }
}

static void header(const char* t) { printf("\n---------------- %s ----------------\n", t); }

int main()
{
    printf("Sylhet Flight Simulator - flight model checks\n");

    // ------------------------------------------------------------------
    header("1. maths: basis vectors, signs and units");
    {
        Basis b = basisFromYPR(0.0f, 0.0f, 0.0f);
        printf("level   f=(%+.2f,%+.2f,%+.2f) r=(%+.2f,%+.2f,%+.2f) u=(%+.2f,%+.2f,%+.2f)\n",
               b.f.x, b.f.y, b.f.z, b.r.x, b.r.y, b.r.z, b.u.x, b.u.y, b.u.z);
        float orth = fabsf(dot(b.f, b.r)) + fabsf(dot(b.f, b.u)) + fabsf(dot(b.r, b.u));
        float lenErr = fabsf(len(b.f) - 1.0f) + fabsf(len(b.r) - 1.0f) + fabsf(len(b.u) - 1.0f);
        printf("orthogonality error %.2e   unit-length error %.2e  (both should be ~0)\n", orth, lenErr);

        Basis bp = basisFromYPR(0.0f, 0.30f, 0.0f);
        Basis br = basisFromYPR(0.0f, 0.0f, 0.30f);
        Basis by = basisFromYPR(0.70f, 0.0f, 0.0f);
        printf("pitch +0.30 rad -> nose y = %+.3f  (want > 0, nose up)\n", bp.f.y);
        printf("roll  +0.30 rad -> right wing y = %+.3f  (want < 0, right wing down)\n", br.r.y);
        printf("yaw   +0.70 rad -> forward = (%+.2f,%+.2f,%+.2f), heading back = %.2f rad (want %.2f)\n",
               by.f.x, by.f.y, by.f.z, yawFromDir(by.f), 0.70f);
        printf("yaw convention: heading 0 = north(-Z), 90 = east(+X)\n");
    }

    // ------------------------------------------------------------------
    header("2. trimmed flight: 90 s, no input, cruise power");
    {
        Aircraft a;
        craftReset(a, v3(0, 600, 0), 0.0f, true, 66.0f, 600.0f, true);
        a.throttle = 0.75f;
        float y0 = a.pos.y, v0 = a.airspeed;
        for (int s = 0; s < 18; s++)
        {
            run(a, 5.0f, 0, 0, 0);
            float hs = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
            printf("   t=%3.0fs  y=%6.1f  kt=%5.1f  pitch=%+5.1f  aoa=%+5.1f  gamma=%+5.1f  vy=%+5.2f  flap=%.2f thr=%.2f\n",
                   (s + 1) * 5.0f, a.pos.y, a.airspeed * 1.94384f, a.pitch * DEG, a.alpha * DEG,
                   (hs > 0.1f ? atan2f(a.vel.y, hs) * DEG : 0.0f), a.vel.y, a.cFlap, a.throttle);
        }
        printf("speed  %.1f -> %.1f kt   (%.1f -> %.1f m/s)\n", v0 * 1.94384f, a.airspeed * 1.94384f, v0, a.airspeed);
        printf("height %.0f -> %.0f m   (drift %.0f m in 90 s)\n", y0, a.pos.y, a.pos.y - y0);
        printf("pitch %+.2f deg   roll %+.2f deg   heading %+.2f deg\n",
               a.pitch * DEG, a.roll * DEG, a.yaw * DEG);
    }

    // ------------------------------------------------------------------
    header("3. control authority: full nose-up for 1 s");
    {
        Aircraft a;
        craftReset(a, v3(0, 800, 0), 0.0f, true, 66.0f, 800.0f, true);
        a.throttle = 0.75f;
        run(a, 3.0f, 0, 0, 0);                    // settle first
        float p0 = a.pitch;
        run(a, 1.0f, 1.0f, 0, 0);
        printf("pitch change in 1 s: %+.2f deg  (rate %+.1f deg/s)\n", (a.pitch - p0) * DEG, (a.pitch - p0) * DEG);
        printf("angle of attack now %.2f deg, speed %.1f kt, load factor sense ok\n", a.alpha * DEG, a.airspeed * 1.94384f);
        // and does it recover when the stick is released?
        float p1 = a.pitch;
        run(a, 8.0f, 0, 0, 0);
        printf("after 8 s hands-off: pitch %+.2f deg (was %+.2f), speed %.1f kt\n",
               a.pitch * DEG, p1 * DEG, a.airspeed * 1.94384f);
    }

    // ------------------------------------------------------------------
    header("4. stall: slow flight, full nose-up");
    {
        Aircraft a;
        craftReset(a, v3(0, 1500, 0), 0.0f, true, 42.0f, 1500.0f, true);
        a.throttle = 0.15f;
        bool stalled = false;
        for (int i = 0; i < 180 * 12; i++)
        {
            a.inPitch = 1.0f; a.inRoll = 0; a.inYaw = 0;
            craftStep(a, DT);
            if (a.stall && !stalled)
            {
                stalled = true;
                printf("STALL at %.2f s: airspeed %.1f kt, aoa %.1f deg, height %.0f m\n",
                       i * DT, a.airspeed * 1.94384f, a.alpha * DEG, a.pos.y);
            }
        }
        printf("after holding the stick up: speed %.1f kt, pitch %+.1f deg, altitude %.0f m\n",
               a.airspeed * 1.94384f, a.pitch * DEG, a.pos.y);
    }

    // ------------------------------------------------------------------
    header("5. take-off roll on the Osmani runway");
    {
        V3 rwy = worldRunwayPoint(-1300.0f, 0.0f);
        V3 rdir = worldRunwayDir();
        Aircraft a;
        craftReset(a, v3(rwy.x, 0.0f, rwy.z), yawFromDir(rdir), true, 0.0f, 0.0f, false);
        a.throttle = 1.0f;
        float x0 = a.pos.x, z0 = a.pos.z;
        float liftSpeed = -1.0f, liftDist = 0.0f;
        float pitchAtLift = 0.0f;
        for (int i = 0; i < 180 * 60; i++)
        {
            // rotate once there is speed, then ease off as soon as the wheels leave
            a.inPitch = (a.airspeed > 26.0f) ? 0.30f : 0.0f;
            if (liftSpeed > 0.0f) a.inPitch = 0.10f;
            a.inRoll = 0; a.inYaw = 0;
            craftStep(a, DT);
            if (i % 180 == 0 && i < 180 * 25)
                printf("   t=%4.1fs  kt=%5.1f  y=%6.1f  onGround=%d phase=%d health=%3.0f pitch=%+5.1f\n",
                       i * DT, a.airspeed * 1.94384f, a.pos.y, (int)a.onGround, (int)a.phase, a.health, a.pitch * DEG);
            if (liftSpeed < 0.0f && !a.onGround && i > 360)
            {
                liftSpeed = a.airspeed;
                pitchAtLift = a.pitch;
                liftDist = sqrtf((a.pos.x - x0) * (a.pos.x - x0) + (a.pos.z - z0) * (a.pos.z - z0));
            }
            if (liftSpeed > 0.0f && i > 180 * 8) break;
        }
        printf("lift-off at %.1f kt after %.0f m of runway, pitch %+.1f deg\n",
               liftSpeed * 1.94384f, liftDist, pitchAtLift * DEG);
        // a short pull to get the nose up, then the pilot lets the aircraft fly itself
        run(a, 1.5f, 0.15f, 0, 0);
        printf("after the pull: %.1f kt, altitude %.0f m, pitch %+.1f deg, climb %.1f m/s\n",
               a.airspeed * 1.94384f, a.pos.y, a.pitch * DEG, a.vel.y);
        float yClimb = a.pos.y;
        // and when the stick is released the aircraft has to hold that altitude
        for (int s = 0; s < 20; s++)
        {
            run(a, 0.25f, 0, 0, 0);
            float hs = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
            printf("   +%5.2fs  y=%6.2f  kt=%5.1f  vy=%+5.2f  pitch=%+5.1f  aoa=%+5.1f  gamma=%+5.1f  thr=%.2f  ph=%d\n",
                   (s + 1) * 0.25f, a.pos.y, a.airspeed * 1.94384f, a.vel.y, a.pitch * DEG, a.alpha * DEG,
                   (hs > 0.1f ? atan2f(a.vel.y, hs) * DEG : 0.0f), a.throttle, (int)a.phase);
            if (a.phase == 2) { printf("   ... crashed\n"); break; }
        }
        printf("25 s hands-off from %.0f m: altitude %.0f m (drift %+.0f m), speed %.1f kt, pitch %+.1f deg\n",
               yClimb, a.pos.y, a.pos.y - yClimb, a.airspeed * 1.94384f, a.pitch * DEG);
    }

    // ------------------------------------------------------------------
    header("6. coordinated turn: full aileron for 3 s");
    {
        Aircraft a;
        craftReset(a, v3(0, 900, 0), 0.0f, true, 66.0f, 900.0f, true);
        a.throttle = 0.75f;
        run(a, 3.0f, 0, 0, 0);
        run(a, 3.0f, 0, 1.0f, 0);               // roll right
        float bank = a.roll * DEG;
        float yaw0 = a.yaw;
        float vAt = a.airspeed;
        run(a, 4.0f, 0, 1.0f, 0);               // keep the aileron held
        float rate = (a.yaw - yaw0) * DEG / 4.0f;
        float theory = 9.81f * tanf(a.roll) / fmaxf(vAt, 1.0f) * DEG;
        printf("bank after 3 s of full aileron: %+.1f deg\n", bank);
        printf("measured turn rate %+.1f deg/s, theory (g*tan(phi)/V) %+.1f deg/s\n", rate, theory);
        printf("heading change over the 4 s hold: %+.1f deg\n", (a.yaw - yaw0) * DEG);
        run(a, 8.0f, 0, 0, 0);
        printf("8 s hands-off in the turn: bank %+.1f deg (spiral stability)\n", a.roll * DEG);
    }

    // ------------------------------------------------------------------
    header("7. landing on the runway at Osmani");
    {
        V3 thr = worldRunwayPoint(-900.0f, 0.0f);
        V3 dir = worldRunwayDir();
        Aircraft a;
        craftReset(a, v3(thr.x, 24.0f, thr.z), yawFromDir(dir), true, 33.0f, 24.0f, true);
        a.throttle = 0.45f;
        a.pitch = 0.03f;                 // the nose is up for the approach
        a.vel = v3(dir.x * 33.0f, -1.2f, dir.z * 33.0f);
        int ev0 = a.landingEvent;
        for (int i = 0; i < 180 * 25; i++)
        {
            // a real pilot flies a steady descent and then flares: the flare
            // asks for a much smaller sink rate just above the runway
            float wantVy = (a.pos.y < 6.0f) ? -0.75f : -1.8f;
            a.inPitch = clampf(-(a.vel.y - wantVy) * 0.25f, -0.6f, 0.6f);
            a.inRoll = 0; a.inYaw = 0;
            craftStep(a, DT);
            if (i % 90 == 0 && i < 180 * 26)
                printf("   t=%4.1fs  y=%5.1f  kt=%5.1f  vy=%+5.2f  pitch=%+5.1f  aoa=%+5.1f\n",
                       i * DT, a.pos.y, a.airspeed * 1.94384f, a.vel.y, a.pitch * DEG, a.alpha * DEG);
            if (a.landingEvent != ev0) break;
        }
        printf("touchdown: \"%s\"   health %.0f%%   vertical speed %.2f m/s\n",
               a.landingText, a.health, -a.vel.y);
        printf("ground roll continues, speed %.1f kt\n", a.airspeed * 1.94384f);
    }

    // ------------------------------------------------------------------
    header("8. wind");
    {
        Aircraft a;
        craftReset(a, v3(0, 600, 0), 0.0f, true, 66.0f, 600.0f, true);
        a.throttle = 0.75f;
        run(a, 20.0f, 0, 0, 0);
        float track = atan2f(a.vel.x, -a.vel.z) * DEG;
        printf("wind %.1f kt; nose heading %.1f deg, ground track %.1f deg (difference = drift)\n",
               gWindSpeed * 1.94384f, a.yaw * DEG, track);
    }

    // ------------------------------------------------------------------
    header("9. numerical stability (frame rate independence)");
    {
        Aircraft a, b;
        craftReset(a, v3(0, 800, 0), 0.0f, true, 66.0f, 800.0f, true);
        craftReset(b, v3(0, 800, 0), 0.0f, true, 66.0f, 800.0f, true);
        a.throttle = b.throttle = 0.55f;
        // a: small steps, b: the same physics called with a bigger step
        for (int i = 0; i < 180 * 20; i++)
        {
            a.inPitch = 0.2f; a.inRoll = 0.1f; a.inYaw = 0;
            craftStep(a, DT);
        }
        for (int i = 0; i < 60 * 20; i++)
        {
            b.inPitch = 0.2f; b.inRoll = 0.1f; b.inYaw = 0;
            craftStep(b, 1.0f / 60.0f);
        }
        printf("dt 1/180: pitch %+.2f deg, alt %.0f m, speed %.1f kt\n", a.pitch * DEG, a.pos.y, a.airspeed * 1.94384f);
        printf("dt 1/60 : pitch %+.2f deg, alt %.0f m, speed %.1f kt\n", b.pitch * DEG, b.pos.y, b.airspeed * 1.94384f);
        printf("difference: pitch %.2f deg, height %.1f m\n", (a.pitch - b.pitch) * DEG, a.pos.y - b.pos.y);
    }

    // ------------------------------------------------------------------
    header("10. sanity: no NaNs, and the wheels sit on the ground");
    {
        Aircraft a;
        craftReset(a, v3(0, 0, 0), 0.0f, true, 0.0f, 0.0f, false);
        printf("parked: origin height %.2f m, wheel contact at %.2f m (should match CRAFT_GEAR_H)\n",
               a.pos.y, CRAFT_GEAR_H);
        bool nan = false;
        run(a, 30.0f, 0.3f, 0.4f, 0.2f);
        if (a.pos.x != a.pos.x || a.pos.y != a.pos.y || a.pos.z != a.pos.z) nan = true;
        printf("after 30 s of violent input: position (%.0f, %.0f, %.0f)  NaN=%s\n", a.pos.x, a.pos.y, a.pos.z, nan ? "YES" : "no");
    }

    // ------------------------------------------------------------------
    header("11. the runway has to be on dry, flat ground");
    {
        float worstWater = 1e9f, worstHill = 0.0f;
        float atS = 0.0f, hillS = 0.0f;
        for (int i = -9; i <= 9; i++)
        {
            float s = (float)i * 150.0f;
            V3 p = worldRunwayPoint(s, 0.0f);
            if (worldIsWater(p.x, p.z)) { worstWater = 0.0f; atS = s; }
            float h = worldHillHeightAt(p.x, p.z);
            if (h > worstHill) { worstHill = h; hillS = s; }
        }
        printf("runway length 2900 m, heading %.0f deg, centre (%.0f, %.0f)\n",
               worldRunwayHeadingDeg(), worldAirportCenter().x, worldAirportCenter().z);
        printf("water on the runway: %s", worstWater > 1e8f ? "no\n" : "YES - the river runs under it\n");
        if (worstWater <= 1e8f)
        {
            V3 p = worldRunwayPoint(atS, 0.0f);
            printf("   ... at %.0f m along the runway, position (%.0f, %.0f)\n", atS, p.x, p.z);
        }
        printf("highest hill under the runway: %.1f m", worstHill);
        if (worstHill > 1.0f)
        {
            V3 p = worldRunwayPoint(hillS, 0.0f);
            printf("  at %.0f m along the runway, position (%.0f, %.0f)\n", hillS, p.x, p.z);
        }
        else printf("\n");
        bool ok = (worstWater > 1e8f) && (worstHill < 1.0f);
        printf("runway environment: %s\n", ok ? "OK - dry and flat" : "PROBLEM");
    }

    printf("\nAll checks finished.\n");
    return 0;
}
