// game.h - game state machine, input and camera (see game.cpp)
#pragma once
#include "vecmath.h"
#include "aircraft.h"
#include "missions.h"

extern int gWinW, gWinH;
extern double gTime;
extern float gMouseSens;    // mouse sensitivity multiplier
extern float gToastTimer;   // seconds a message stays on screen
extern float gFps;          // smoothed frame rate, shown in the HUD
extern bool gShowFps;       // F3: show the frame-rate overlay
extern char gToastText[64]; // the message
extern char gCaptionText[80];   // landmark name card, shown after a jump
extern float gCaptionTimer;     // seconds it stays on screen
extern int gStartLandmark;      // --landmark <n>: jump to gLandmarks[n] on start
extern bool gStartResults;      // --results: show the success screen at once
extern bool gInstantAction;     // the Instant Action sortie
extern bool gStartInstant;      // --instant: start in a dogfight
extern bool gAutoFire;          // --autofire: hold the trigger down
extern int gDifficulty;         // 0 easy, 1 normal, 2 ace
extern bool gStartBriefing;     // show the briefing card before a sortie
extern int gStartWithMission;   // -1 = title screen, otherwise a MissionId (handy for tests/demos)
extern bool gStartCockpit;      // start in the cockpit view instead of the chase view
extern bool gStartDemo;         // --demo: start the cinematic fly-through
extern float gDemoStartTime;    // --demotime <s>: jump into the demo at that second
extern bool gStartPhoto;        // --photo: start in photo mode (capture runs)
extern int gStartPage;          // --help / --credits: open that page at start
extern bool gStatsOnly;         // --stats: print scene counts and quit
extern const char* gBenchPath;  // --benchmark <file.csv>: write fps samples
extern float gBenchSeconds;     // --benchtimed <seconds>: length of that run
extern const char* gVideoPath;  // --video <file.avi>: record the frame sequence
extern int gVideoFps;           // --videofps <n>: frames per second
extern float gVideoSeconds;     // --videotimed <seconds>: how long to record
extern bool gPhotoHud;          // photo mode: H hides the viewfinder
extern bool gPhotoGrid;         // photo mode: G shows the rule-of-thirds grid
extern int gPhotoTime;          // photo mode: slow-motion step, 0 = real time
extern float gPhotoTimeScale;   // photo mode: the multiplier for that step
extern bool gProbe;             // --probe: append flight telemetry to probe.csv for debugging
extern float gAtX, gAtZ, gAtY;  // --at x z alt: start free flight at that point
extern const char* gShotPath;   // --shot <file.bmp>: save one frame and quit (no desktop needed)
extern float gShotDelay;        // --shotdelay <seconds>: wait this long before the shot

void gameInit();
void gameDisplay();
void gameReshape(int w, int h);
void gameKeyDown(unsigned char k, int x, int y);
void gameKeyUp(unsigned char k, int x, int y);
void gameSpecialDown(int k, int x, int y);
void gameSpecialUp(int k, int x, int y);
void gameMouseButton(int button, int state, int x, int y);
void gameMouseMotion(int x, int y);
void gamePassiveMotion(int x, int y);
void gameMouseWheel(int wheel, int dir, int x, int y);
void gameTimerTick();
void gameStartMission(MissionId id);
