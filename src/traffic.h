// traffic.h - everything that moves on its own: vehicles, boats, the train,
// birds and AI planes
#pragma once
#include "vecmath.h"

void trafficInit();
void trafficUpdate(float dt, double timeSec);
void trafficDraw();
struct TrafficStats
{
    int movingVehicles, parkedVehicles, boats, birds, aiPlanes, walkers, bridgeCars;
};
void trafficGetStats(TrafficStats& s);