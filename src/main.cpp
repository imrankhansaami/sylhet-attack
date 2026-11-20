// main.cpp - window creation and GLUT callback wiring
#include <GL/freeglut.h>
#include <cstring>
#include <cstdlib>
#include "game.h"
#include "world.h"
#include "enemy.h"

static void displayCb() { gameDisplay(); }
static void reshapeCb(int w, int h) { gameReshape(w, h); }
static void keyCb(unsigned char k, int x, int y) { gameKeyDown(k, x, y); }
static void keyUpCb(unsigned char k, int x, int y) { gameKeyUp(k, x, y); }
static void specCb(int k, int x, int y) { gameSpecialDown(k, x, y); }
static void specUpCb(int k, int x, int y) { gameSpecialUp(k, x, y); }
static void mouseCb(int b, int s, int x, int y) { gameMouseButton(b, s, x, y); }
static void motionCb(int x, int y) { gameMouseMotion(x, y); }
static void pmotionCb(int x, int y) { gamePassiveMotion(x, y); }
static void wheelCb(int wheel, int dir, int x, int y) { gameMouseWheel(wheel, dir, x, y); }

static void timerCb(int v)
{
    (void)v;
    gameTimerTick();
    glutTimerFunc(15, timerCb, 0);
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--free") == 0) gStartWithMission = MIS_FREE;
        else if (strcmp(argv[i], "--school") == 0) gStartWithMission = MIS_SCHOOL;
        else if (strcmp(argv[i], "--tour") == 0) gStartWithMission = MIS_TOUR;
        else if (strcmp(argv[i], "--rings") == 0) gStartWithMission = MIS_RINGS;
        else if (strcmp(argv[i], "--landing") == 0) gStartWithMission = MIS_LANDING;
        else if (strcmp(argv[i], "--defend") == 0) gStartWithMission = MIS_TOUR;
        else if (strcmp(argv[i], "--deck") == 0) gStartWithMission = MIS_RINGS;
        else if (strcmp(argv[i], "--strike") == 0) gStartWithMission = MIS_LANDING;
        else if (strcmp(argv[i], "--training") == 0) gStartWithMission = MIS_SCHOOL;
        else if (strcmp(argv[i], "--probe") == 0) gProbe = true;
        else if (strcmp(argv[i], "--at") == 0 && i + 3 < argc)
        {
            gAtX = (float)atof(argv[++i]);
            gAtZ = (float)atof(argv[++i]);
            gAtY = (float)atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) gShotPath = argv[++i];
        else if (strcmp(argv[i], "--shotdelay") == 0 && i + 1 < argc) gShotDelay = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--cockpit") == 0) gStartCockpit = true;
        else if (strcmp(argv[i], "--fps") == 0) gShowFps = true;
        else if (strcmp(argv[i], "--demo") == 0) gStartDemo = true;
        else if (strcmp(argv[i], "--demotime") == 0 && i + 1 < argc) gDemoStartTime = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--photo") == 0) gStartPhoto = true;
        else if (strcmp(argv[i], "--help") == 0) gStartPage = 1;
        else if (strcmp(argv[i], "--credits") == 0) gStartPage = 2;
        else if (strcmp(argv[i], "--stats") == 0) gStatsOnly = true;
        else if (strcmp(argv[i], "--landmark") == 0 && i + 1 < argc) gStartLandmark = atoi(argv[++i]);
        else if (strcmp(argv[i], "--noenemy") == 0) enemySetEnabled(false);
        else if (strcmp(argv[i], "--results") == 0) gStartResults = true;
        else if (strcmp(argv[i], "--instant") == 0) gStartInstant = true;
        else if (strcmp(argv[i], "--autofire") == 0) gAutoFire = true;
        else if (strcmp(argv[i], "--diff") == 0 && i + 1 < argc) gDifficulty = atoi(argv[++i]);
        else if (strcmp(argv[i], "--nobrief") == 0) gStartBriefing = false;
        else if (strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) gBenchPath = argv[++i];
        else if (strcmp(argv[i], "--benchtimed") == 0 && i + 1 < argc) gBenchSeconds = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--video") == 0 && i + 1 < argc) gVideoPath = argv[++i];
        else if (strcmp(argv[i], "--videofps") == 0 && i + 1 < argc) gVideoFps = atoi(argv[++i]);
        else if (strcmp(argv[i], "--videotimed") == 0 && i + 1 < argc) gVideoSeconds = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--nochunk") == 0) gNoChunk = true;
        else if (strcmp(argv[i], "--nolod") == 0) gNoLod = true;
        else if (strcmp(argv[i], "--noshadow") == 0) gDrawShadows = false;
        else if (strcmp(argv[i], "--notrees") == 0) gHideTrees = true;
        else if (strcmp(argv[i], "--nocity") == 0) gHideCity = true;
        else if (strcmp(argv[i], "--nodetails") == 0) gHideDetails = true;
        else if (strcmp(argv[i], "--nopois") == 0) gHidePois = true;
        else if (strcmp(argv[i], "--size") == 0 && i + 2 < argc)
        {
            gWinW = atoi(argv[++i]);
            gWinH = atoi(argv[++i]);
        }
    }
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(gWinW, gWinH);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("Sylhet Attack - C++ / OpenGL");

    glutDisplayFunc(displayCb);
    glutReshapeFunc(reshapeCb);
    glutKeyboardFunc(keyCb);
    glutKeyboardUpFunc(keyUpCb);
    glutSpecialFunc(specCb);
    glutSpecialUpFunc(specUpCb);
    glutMouseFunc(mouseCb);
    glutMotionFunc(motionCb);
    glutPassiveMotionFunc(pmotionCb);
    glutMouseWheelFunc(wheelCb);
    glutIgnoreKeyRepeat(0);

    gameInit();

    glutTimerFunc(15, timerCb, 0);
    glutMainLoop();
    return 0;
}
