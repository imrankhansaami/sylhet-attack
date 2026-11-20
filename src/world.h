// world.h - the city of Sylhet: rivers, roads, landmarks, airport, railway
#pragma once
#include "vecmath.h"

struct Landmark
{
    const char* name;       // full name (shown on screen labels)
    const char* shortName;  // short name (shown on the mini-map)
    float x, z;             // position
    float labelY;           // height of the floating label
};
extern const Landmark gLandmarks[];
extern const int gLandmarkCount;

void worldBuild();        // build everything once (display lists + collision data)
void worldDrawStatic();   // draw all static geometry
void worldSetCameraY(float y);   // lets the world skip distant detail when high up
extern bool gNoChunk;            // --nochunk: disable city distance culling (benchmark)
extern bool gNoLod;              // --nolod: always draw the detailed chunk list
extern bool gDrawShadows;        // Q: draw the city shadow pass (big frame-rate cost)
extern bool gHideTrees, gHideCity, gHideDetails, gHidePois;   // benchmark switches
void worldSetCameraXZ(float x, float z);   // position used for city chunk culling

struct WorldStats
{
    int buildings;      // city buildings generated
    int trees;          // every tree placed
    int roadSegments;   // straight segments across all roads
    int chunks;         // city chunks (each compiled twice)
    int landmarks;      // named landmarks
    int displayLists;   // static display lists compiled
};
void worldGetStats(WorldStats& s);

// ----- river (Surma) -----
extern const float gRiverPts[][2];
extern const int gRiverPtsCount;
float worldRiverDist(float x, float z);                  // distance to the river centreline
bool worldIsWater(float x, float z);
float worldRiverLength();                                // total centreline length in metres
void worldRiverFrameAtS(float s, V3& p, V3& dirOut);     // point on the river at arc length s (wraps)

// ----- roads (for the mini-map and traffic) -----
extern const int gRoadCount;
const char* worldRoadName(int i);
int worldRoadPointCount(int i);
void worldRoadPoint(int i, int j, float& x, float& z);
float worldRoadWidth(int i);

// ----- airport -----
V3 worldAirportCenter();
float worldRunwayHeadingDeg();
V3 worldRunwayDir();       // unit vector along the runway (heading direction)
V3 worldRunwayRight();     // unit vector to the city side of the runway
V3 worldRunwayPoint(float s, float lat);   // s = metres along runway (0 = centre), lat = metres to the city side
bool worldIsOnRunway(float x, float z);
V3 worldApronCenter();

// ----- railway -----
extern const float gRailZ;
extern const float gCrossX[2];   // x positions of the two level crossings

// ----- terrain: low green hills around the city -----
extern const int gHillCount;
void worldHill(int i, float& x, float& z, float& r);
float worldHillHeightAt(float x, float z);

// ----- people and animated details -----
void worldDrawPerson(float x, float y, float z, float yawDeg, float r, float g, float b, bool female);
void worldDrawWindsocks();   // animated: they point downwind and flutter
void worldDrawClockHands(float hourAngleDeg, float minuteAngleDeg);

// ----- collision -----
bool worldHitBuilding(float x, float y, float z);   // true if the point is inside a tall structure
