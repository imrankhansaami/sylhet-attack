// hud.h - menus, flight HUD, mini-map, labels and results screens
#pragma once
#include "vecmath.h"

enum UiAction
{
    UI_NONE = 0,
    UI_FREEFLIGHT, UI_SCHOOL, UI_TOUR, UI_RINGS, UI_LANDING,
    UI_CONTROLS, UI_CREDITS, UI_QUIT, UI_DEMO, UI_INSTANT,
    UI_RESUME, UI_RESTART, UI_MAINMENU
};

extern bool gShowLabels;   // toggled with L (defined in game.cpp)
extern int gMapZoom;       // 0..2, toggled with M (defined in game.cpp)

int hudTitle(float mx, float my, bool clicked);
int hudPause(float mx, float my, bool clicked);
int hudResults(float mx, float my, bool clicked);
void hudHelp();
void hudCredits();
void hudDrawLabels();      // call inside the 3D pass (uses gluProject)
void hudCaptureProjection();          // remember the 3D camera matrices for the gunsight
bool hudProjectPoint(const V3& p, float& sx, float& sy);   // world point -> screen (2D pass)
void hudDrawFlight();      // the 2D flight HUD
void hudDrawPhoto();       // photo mode frame
void hudDrawDemo();        // cinematic demo: letterbox, captions, fade
void hudBriefing();        // the mission briefing card
