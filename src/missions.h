// missions.h - free flight, flight school, city tour, ring course, landing challenge
#pragma once
#include "vecmath.h"

enum MissionId { MIS_FREE = 0, MIS_SCHOOL, MIS_TOUR, MIS_RINGS, MIS_LANDING, MIS_COUNT };

struct MissionTarget
{
    float x, z, y, radius;
    char name[48];
    bool done;
};

struct MissionState
{
    MissionId id;
    bool active;
    bool complete;
    bool failed;
    int step;                 // current objective step
    float time;               // mission clock (seconds)
    float endTimer;           // seconds since completion / failure
    int countDone, countTotal;
    MissionTarget targets[14];
    float score;
    int stars;
    int ringsPassed;
    int lastLandingSeen;      // aircraft landingEvent already processed
    char resultLine[96];      // short summary for the results screen
};

extern MissionState gMission;

const char* missionName(MissionId id);
const char* missionDescription(MissionId id);
MissionState missionCreate(MissionId id);
void missionUpdate(float dt);
void missionDrawWorld();      // beacons and rings, call inside the 3D pass
const char* missionObjective();
bool missionTargetPoint(V3& out, float& radius);
