// demo.h - the scripted cinematic fly-through shown from the title screen
#pragma once
#include "vecmath.h"

void demoStart();                 // rewind to the first frame
void demoUpdate(float dt);        // advance the demo clock
bool demoFinished();              // the last frame has been shown
void demoPose(V3& camPos, float& yaw, float& pitch, float& roll);
float demoTime();
float demoDuration();

// lower-third captions; title is null when no caption is on screen
const char* demoCaptionTitle();
const char* demoCaptionBody();
float demoCaptionAlpha();         // 0 .. 1, already faded in and out

float demoFade();                 // 0 = picture, 1 = black (start and end)