// aircraft.h - the player's light propeller plane: model, flight model, damage
#pragma once
#include "vecmath.h"

enum CraftPhase { PHASE_GROUND = 0, PHASE_AIR, PHASE_CRASHED };

struct Aircraft
{
    V3 pos, vel;                 // metres, metres/second (y = altitude)
    float yaw, pitch, roll;      // radians; +pitch = nose up, +roll = right bank
    float throttle;              // 0 .. 1
    float health;                // 0 .. 100
    bool engineOn;
    bool onGround;
    bool brake;
    float prop;                  // propeller angle (radians)

    // control inputs, set every frame by game.cpp, each -1 .. 1
    float inPitch, inRoll, inYaw;

    // smoothed control-surface positions (the surfaces take a moment to move)
    float cPitch, cRoll, cYaw;
    float cFlap;             // flap deployment, 0 .. 1
    float flapCmd;           // manual flap setting 0..1, or -1 for automatic
    float t;                 // seconds since the flight started
    float trim;              // pitch trim, -1 .. 1
    float altHold;           // altitude the pilot wants the aircraft to hold
    float altI;              // integral term of the altitude hold
    float fuel;              // litres in the tank
    float rpm;               // engine rpm

    // flight state for the HUD
    float alpha;                 // angle of attack (radians)
    float airspeed;              // m/s
    bool stall;
    int phase;

    // landing feedback
    int landingEvent;            // counts touchdowns (missions watch this)
    int landingQuality;          // 3 butter, 2 firm, 1 rough, 0 crash
    char landingText[64];
    float landingMsgTimer;
    float crashTimer;
};

extern const float CRAFT_GEAR_H;   // height of the wheels above ground when parked
extern float gWindSpeed;          // wind speed in m/s
extern float gWindDirRad;         // direction the wind blows towards, radians
extern float gFuelCapacity;       // litres

extern Aircraft gAircraft;         // the player's plane (defined in game.cpp)

void craftReset(Aircraft& a, V3 pos, float yawRad, bool engineOn, float speed, float alt, bool airborne);
void craftStep(Aircraft& a, float dt);
void craftDamage(Aircraft& a, float dmg, const char* msg);   // battle damage, crashes at zero
void craftDraw(const Aircraft& a);
void craftDrawSimple(float x, float y, float z, float yawRad, float rollRad, float r, float g, float b);
void craftDrawCockpitOverlay(float aspect);
