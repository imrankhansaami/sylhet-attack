// hud.cpp - title screen, menus, flight HUD, mini-map, labels, results
#include "hud.h"
#include "glutil.h"
#include "game.h"
#include "world.h"
#include "aircraft.h"
#include "missions.h"
#include "demo.h"
#include "enemy.h"

// ---- the 3D camera, captured so the gunsight can project points ----
static double sProjM[16];
static double sModelM[16];
static int sViewport[4];
static bool sProjectionValid = false;

void hudCaptureProjection()
{
    glGetDoublev(GL_PROJECTION_MATRIX, sProjM);
    glGetDoublev(GL_MODELVIEW_MATRIX, sModelM);
    glGetIntegerv(GL_VIEWPORT, sViewport);
    sProjectionValid = true;
}

bool hudProjectPoint(const V3& p, float& sx, float& sy)
{
    if (!sProjectionValid) return false;
    double x = 0, y = 0, z = 0;
    if (gluProject((double)p.x, (double)p.y, (double)p.z, sModelM, sProjM, sViewport, &x, &y, &z) == GL_FALSE)
        return false;
    if (z < 0.0 || z > 1.0) return false;          // behind the camera
    sx = (float)x;
    sy = (float)(gWinH - y);                        // screen origin is top-left here
    return true;
}
#include <cstdio>
#include <cstring>
#include <cmath>

#define F12 GLUT_BITMAP_HELVETICA_12
#define F18 GLUT_BITMAP_HELVETICA_18
#define F24 GLUT_BITMAP_TIMES_ROMAN_24
#define T24 GLUT_BITMAP_TIMES_ROMAN_24

// ---- fill these three in before the defence -------------------------------
// They are the only place the credits page takes personal details from.
static const char* CREDIT_AUTHOR     = "Imran Khan";
static const char* CREDIT_AUTHOR_ID  = "44008";
static const char* CREDIT_SUPERVISOR = "Arnod Debnat";

struct Btn { float x, y, w, h; const char* label; int action; };

static bool inBtn(const Btn& b, float mx, float my)
{
    return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
}

static void panel(float x, float y, float w, float h, float alpha)
{
    // soft drop shadow, then a body that is slightly lighter at the top
    setColorA(0, 0, 0, alpha * 0.5f);
    fillRect(x + 4.0f, y + 6.0f, w, h);
    gradRect(x, y, w, h,
             0.11f, 0.14f, 0.19f, alpha,
             0.02f, 0.03f, 0.05f, alpha);
    // accent strip along the top edge
    setColorA(0.36f, 0.63f, 0.86f, 0.80f);
    fillRect(x, y, w, 2.5f);
    setColorA(1, 1, 1, 0.20f);
    frameRect(x, y, w, h, 1.0f);
}

static void drawBtn(const Btn& b, float mx, float my)
{
    bool hov = inBtn(b, mx, my);
    setColorA(0, 0, 0, 0.35f);
    fillRect(b.x + 2.0f, b.y + 3.0f, b.w, b.h);
    gradRect(b.x, b.y, b.w, b.h,
             hov ? 0.26f : 0.16f, hov ? 0.43f : 0.28f, hov ? 0.60f : 0.41f, 0.92f,
             hov ? 0.11f : 0.08f, hov ? 0.21f : 0.16f, hov ? 0.33f : 0.25f, 0.92f);
    if (hov)
    {
        setColorA(0.45f, 0.82f, 1.0f, 0.95f);
        fillRect(b.x, b.y, 3.0f, b.h);
    }
    setColorA(1, 1, 1, hov ? 0.55f : 0.26f);
    frameRect(b.x, b.y, b.w, b.h, 1.0f);
    setColorA(1, 1, 1, 1);
    drawTextCentered(F18, b.label, b.x + b.w * 0.5f, b.y + (b.h - 18.0f) * 0.5f);
}

static int handleBtns(const Btn* btns, int n, float mx, float my, bool clicked)
{
    for (int i = 0; i < n; i++) drawBtn(btns[i], mx, my);
    if (clicked)
    {
        for (int i = 0; i < n; i++)
            if (inBtn(btns[i], mx, my)) return btns[i].action;
    }
    return UI_NONE;
}

// =====================================================================
// title screen
// =====================================================================
int hudTitle(float mx, float my, bool clicked)
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;

    setColorA(0, 0, 0, 0.40f);
    fillRect(0, 0, W, 150.0f);
    fillRect(0, H - 40.0f, W, 40.0f);

    // big title, scaled to fit the window
    {
        const char* title = "SYLHET ATTACK";
        float len = strokeTextLen(GLUT_STROKE_ROMAN, title);
        float sc = fminf((W - 90.0f) / len, 0.60f);
        if (sc < 0.15f) sc = 0.15f;
        glLineWidth(3.0f);
        setColorA(1.0f, 0.98f, 0.90f, 1.0f);
        glPushMatrix();
        glTranslatef(W * 0.5f, H - 30.0f - 104.0f * sc, 0);
        strokeTextCentered(GLUT_STROKE_ROMAN, title, sc);
        glPopMatrix();
        glLineWidth(1.0f);
        setColorA(0.85f, 0.95f, 1.0f, 1.0f);
        drawTextCentered(F18, "A computer graphics project - C++ and OpenGL (freeglut)", W * 0.5f, 40.0f + 104.0f * sc);
    }

    // The menu is deliberately short: the three combat modes and the peaceful
    // training range. The button labels come from missionName(), so they can
    // never drift out of step with the missions again.
    float bx = W * 0.5f - 200.0f;
    float by = 235.0f;
    static const MissionId MENU_IDS[4] = { MIS_TOUR, MIS_RINGS, MIS_LANDING, MIS_SCHOOL };
    static const int MENU_UI[4] = { UI_TOUR, UI_RINGS, UI_LANDING, UI_SCHOOL };
    Btn missions[4];
    for (int i = 0; i < 4; i++)
    {
        missions[i].x = bx;
        missions[i].y = by + i * 54.0f;
        missions[i].w = 400.0f;
        missions[i].h = 44.0f;
        missions[i].label = missionName(MENU_IDS[i]);
        missions[i].action = MENU_UI[i];
    }
    int action = handleBtns(missions, 4, mx, my, clicked);

    // description of the hovered mission
    for (int i = 0; i < 4; i++)
    {
        if (inBtn(missions[i], mx, my) && !clicked)
        {
            setColorA(0.8f, 0.9f, 1.0f, 0.95f);
            drawTextCentered(F12, missionDescription(MENU_IDS[i]), W * 0.5f, by + 226.0f);
        }
    }

    Btn demo[2] = {
        { bx, by + 294.0f, 400.0f, 38.0f, "Instant Action  -  dogfight now", UI_INSTANT },
        { bx, by + 338.0f, 400.0f, 34.0f, "Watch the City Demo", UI_DEMO }
    };
    int a2 = handleBtns(demo, 2, mx, my, clicked);

    Btn tools[3] = {
        { bx,            by + 384.0f, 128.0f, 34.0f, "Controls", UI_CONTROLS },
        { bx + 136.0f,   by + 384.0f, 128.0f, 34.0f, "Credits", UI_CREDITS },
        { bx + 272.0f,   by + 384.0f, 128.0f, 34.0f, "Quit", UI_QUIT }
    };
    int a3 = handleBtns(tools, 3, mx, my, clicked);
    if (action == UI_NONE) action = a2;
    if (action == UI_NONE) action = a3;

    setColorA(0.75f, 0.78f, 0.82f, 0.9f);
    drawText(F12, "Click a mission or press 1-4     D: city demo     Esc: quit     F11: fullscreen", 20.0f, H - 28.0f);
    drawText(F12, "Real Sylhet landmarks - geometry generated in code, CC0 photographic textures", W - textW(F12, "Real Sylhet landmarks - geometry generated in code, CC0 photographic textures") - 20.0f, H - 28.0f);

    text2DEnd();
    return action;
}

// =====================================================================
// pause menu
// =====================================================================
int hudPause(float mx, float my, bool clicked)
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    setColorA(0, 0, 0, 0.45f);
    fillRect(0, 0, W, H);

    float px = W * 0.5f - 200.0f, py = H * 0.5f - 190.0f;
    panel(px, py, 400.0f, 360.0f, 0.85f);
    setColorA(1, 1, 1, 1);
    drawTextCentered(F24, "PAUSED", W * 0.5f, py + 22.0f);
    setColorA(0.8f, 0.9f, 1.0f, 0.9f);
    drawTextCentered(F12, missionName(gMission.id), W * 0.5f, py + 58.0f);

    Btn btns[4] = {
        { px + 60.0f, py + 90.0f,  280.0f, 44.0f, "Resume Flight", UI_RESUME },
        { px + 60.0f, py + 144.0f, 280.0f, 44.0f, "Restart Mission", UI_RESTART },
        { px + 60.0f, py + 198.0f, 280.0f, 44.0f, "Controls", UI_CONTROLS },
        { px + 60.0f, py + 252.0f, 280.0f, 44.0f, "Main Menu", UI_MAINMENU }
    };
    int action = handleBtns(btns, 4, mx, my, clicked);
    setColorA(0.75f, 0.78f, 0.82f, 0.9f);
    drawTextCentered(F12, "Press Esc to resume", W * 0.5f, py + 320.0f);
    text2DEnd();
    return action;
}

// =====================================================================
// results screen
// =====================================================================
int hudResults(float mx, float my, bool clicked)
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    setColorA(0, 0, 0, 0.5f);
    fillRect(0, 0, W, H);

    // a little wider than before: the success medal sits in the top-right corner
    float px = W * 0.5f - 290.0f, py = H * 0.5f - 240.0f;
    panel(px, py, 580.0f, 475.0f, 0.88f);

    char buf[160];
    // mission name in the same typeface as the title screen, result word under it
    if (gMission.complete) setColorA(0.45f, 1.0f, 0.55f, 1.0f);
    else setColorA(1.0f, 0.45f, 0.40f, 1.0f);
    {
        float sc = 0.16f;
        float nmW = strokeTextW2D(GLUT_STROKE_ROMAN, missionName(gMission.id), sc);
        drawStrokeText2D(GLUT_STROKE_ROMAN, missionName(gMission.id), W * 0.5f - nmW * 0.5f, py + 16.0f, sc, 2.0f);
        setColorA(gMission.complete ? 0.55f : 1.0f,
                  gMission.complete ? 1.0f : 0.55f,
                  gMission.complete ? 0.65f : 0.50f, 1.0f);
        drawTextCentered(F12, gMission.complete ? "MISSION COMPLETE" : "MISSION FAILED", W * 0.5f, py + 40.0f);
    }

    // stars
    for (int i = 0; i < 3; i++)
    {
        if (i < gMission.stars) setColorA(1.0f, 0.85f, 0.25f, 1.0f);
        else setColorA(0.35f, 0.35f, 0.38f, 1.0f);
        drawStar2D(W * 0.5f - 110.0f + i * 52.0f, py + 84.0f, 20.0f, true);
    }

    // A medal for a successful mission: the tier follows the stars, and it drops
    // in with a shine sweep so the moment lands.
    if (gMission.complete)
    {
        const char* tier = (gMission.stars >= 3) ? "GOLD MEDAL" :
                           (gMission.stars == 2) ? "SILVER MEDAL" :
                           (gMission.stars == 1) ? "BRONZE MEDAL" : "COMPLETED";
        float gold[3] = { 0.96f, 0.80f, 0.30f };
        if (gMission.stars == 2) { gold[0] = 0.84f; gold[1] = 0.86f; gold[2] = 0.90f; }
        else if (gMission.stars == 1) { gold[0] = 0.78f; gold[1] = 0.52f; gold[2] = 0.30f; }
        else { gold[0] = 0.70f; gold[1] = 0.72f; gold[2] = 0.74f; }

        float t = clampf(gMission.endTimer / 0.7f, 0.0f, 1.0f);
        float sc = 0.55f + 0.45f * t;
        float mx = px + 468.0f, my = py + 104.0f;
        float R = 30.0f * sc;

        // ribbon straps
        setColorA(0.62f, 0.16f, 0.20f, 1.0f);
        drawLine2D(mx - 15.0f * sc, my - R - 42.0f * sc, mx - 5.0f * sc, my - R + 8.0f * sc, 12.0f * sc);
        drawLine2D(mx + 15.0f * sc, my - R - 42.0f * sc, mx + 5.0f * sc, my - R + 8.0f * sc, 12.0f * sc);
        // disc, face and inner ring
        setColorA(gold[0] * 0.7f, gold[1] * 0.7f, gold[2] * 0.65f, 1.0f);
        drawCircle2D(mx, my, R, 40, true);
        setColorA(gold[0], gold[1], gold[2], 1.0f);
        drawCircle2D(mx, my - 2.0f * sc, R * 0.86f, 40, true);
        setColorA(gold[0] * 0.5f, gold[1] * 0.5f, gold[2] * 0.45f, 1.0f);
        drawRing2D(mx, my - 2.0f * sc, R * 0.68f, R * 0.58f, 36);
        setColorA(1.0f, 0.97f, 0.88f, 1.0f);
        drawStar2D(mx, my - 2.0f * sc, R * 0.44f, true);
        // tier label under it
        setColorA(1.0f, 1.0f, 1.0f, 1.0f);
        drawTextCentered(F12, tier, mx, my + R + 5.0f);

        // one shine sweep as it appears
        if (gMission.endTimer < 1.7f)
        {
            float k = clampf((gMission.endTimer - 0.4f) / 0.9f, 0.0f, 1.0f);
            if (k > 0.0f && k < 1.0f)
            {
                float sx = mx - R + 2.0f * R * k;
                setColorA(1.0f, 1.0f, 1.0f, 0.40f * (1.0f - fabsf(k - 0.5f) * 2.0f));
                drawLine2D(sx - 7.0f, my - R, sx + 7.0f, my + R, 5.0f);
            }
        }
    }

    // combat debrief: what the sortie actually achieved
    {
        char cb1[128], cb2[128];
        snprintf(cb1, sizeof(cb1), "Kills %d        Gun accuracy %.0f%%", weaponKills(), weaponAccuracy() * 100.0f);
        snprintf(cb2, sizeof(cb2), "Ground targets left %d/%d        Landmark damage %d%%",
                 groundTargetsAlive(), groundTargetsTotal(), landmarkDamageScore());
        setColorA(0.86f, 0.91f, 0.97f, 1.0f);
        drawTextCentered(F12, cb1, W * 0.5f, py + 344.0f);
        drawTextCentered(F12, cb2, W * 0.5f, py + 362.0f);
    }

    // rows
    float ly = py + 152.0f;
    float lx = px + 50.0f, rx = px + 270.0f;
    setColorA(0.8f, 0.85f, 0.9f, 1.0f);
    drawText(F18, "Result", lx, ly);
    setColorA(1, 1, 1, 1);
    {
        // keep the result inside the panel, whatever the message says
        float avail = px + 520.0f - 30.0f - rx;
        char line[160];
        snprintf(line, sizeof(line), "%s", gMission.resultLine);
        if (textW(F18, line) > avail)
        {
            int n = (int)strlen(line);
            while (n > 3)
            {
                n--;
                snprintf(line, sizeof(line), "%.*s...", n, gMission.resultLine);
                if (textW(F18, line) <= avail) break;
            }
        }
        drawText(F18, line, rx, ly);
    }
    ly += 34.0f;
    setColorA(0.8f, 0.85f, 0.9f, 1.0f);
    drawText(F18, "Flight time", lx, ly);
    {
        int t = (int)gMission.time;
        snprintf(buf, 160, "%d:%02d", t / 60, t % 60);
    }
    setColorA(1, 1, 1, 1);
    drawText(F18, buf, rx, ly);
    ly += 34.0f;
    setColorA(0.8f, 0.85f, 0.9f, 1.0f);
    drawText(F18, "Landing", lx, ly);
    {
        const char* q = "not graded";
        if (gAircraft.landingQuality == 3) q = "Butter - perfect";
        else if (gAircraft.landingQuality == 2) q = "Firm and safe";
        else if (gAircraft.landingQuality == 1) q = "Hard - rough";
        else if (gAircraft.landingQuality == 0) q = "Crashed";
        snprintf(buf, 160, "%s", q);
    }
    setColorA(1, 1, 1, 1);
    drawText(F18, buf, rx, ly);
    ly += 34.0f;
    setColorA(0.8f, 0.85f, 0.9f, 1.0f);
    drawText(F18, "Damage", lx, ly);
    snprintf(buf, 160, "%d%%", (int)(100.0f - gAircraft.health + 0.5f));
    setColorA(1, 1, 1, 1);
    drawText(F18, buf, rx, ly);
    ly += 42.0f;
    // score: small label over a big stroke number
    {
        setColorA(0.72f, 0.80f, 0.90f, 1.0f);
        drawTextCentered(F12, "SCORE", W * 0.5f, ly - 4.0f);
        snprintf(buf, 160, "%d", (int)(gMission.score + 0.5f));
        setColorA(0.98f, 0.94f, 0.72f, 1.0f);
        float sw = strokeTextW2D(GLUT_STROKE_ROMAN, buf, 0.26f);
        drawStrokeText2D(GLUT_STROKE_ROMAN, buf, W * 0.5f - sw * 0.5f, ly + 14.0f, 0.24f, 2.2f);
    }

    Btn btns[3];
    int n = 0;
    btns[n].x = px + 60.0f;  btns[n].y = py + 380.0f; btns[n].w = 210.0f; btns[n].h = 44.0f; btns[n].label = "Fly Again"; btns[n].action = UI_RESTART; n++;
    if (gMission.id == MIS_SCHOOL && gMission.complete)
    {
        btns[n].x = px + 300.0f; btns[n].y = py + 380.0f; btns[n].w = 220.0f; btns[n].h = 44.0f; btns[n].label = "Next: City Tour"; btns[n].action = UI_TOUR; n++;
    }
    else
    {
        btns[n].x = px + 300.0f; btns[n].y = py + 380.0f; btns[n].w = 220.0f; btns[n].h = 44.0f; btns[n].label = "Main Menu"; btns[n].action = UI_MAINMENU; n++;
    }
    int action = handleBtns(btns, n, mx, my, clicked);
    if (action == UI_NONE && clicked)
    {
        // click on the left button area also acts as fly again? keep simple: nothing
    }
    text2DEnd();
    return action;
}

// =====================================================================
// help and credits
// =====================================================================
void hudHelp()
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    setColorA(0, 0, 0, 0.60f);
    fillRect(0, 0, W, H);
    float px = W * 0.5f - 430.0f, py = H * 0.5f - 240.0f;
    panel(px, py, 860.0f, 480.0f, 0.9f);
    setColorA(1, 1, 1, 1);
    drawTextCentered(F24, "CONTROLS", W * 0.5f, py + 16.0f);

    static const char* HDR[3] = { "FLYING", "VIEW AND SYSTEM", "PHOTO MODE AND MENUS" };
    static const char* COL[3][15] = {
        {
            "MOUSE  move : steer",
            "LEFT CLICK  : recentre",
            "W / S  : throttle",
            "ARROWS : pitch / roll",
            "A / D  : rudder / taxi",
            "SPACE  : wheel brakes",
            ", / .  : cruise trim",
            "E  : start the engine",
            "F  : flaps AUTO/0/50/100",
            "I  : invert pitch axis",
            ""
        },
        {
            "C / RIGHT CLICK : camera",
            "MOUSE WHEEL : throttle",
            "M / L  : map / labels",
            "Q  : quality (shadows)",
            "[ / ] : mouse sensitivity",
            "F2 : save a screenshot",
            "F3 : frame-rate overlay",
            "H  : this page",
            "R  : restart the mission",
            "X  : flares (break a missile lock)",
            "1-9, 0 : jump to a landmark",
            "N  : next landmark (presenting)",
            "Esc: pause menu",
            "F11: fullscreen"
        },
        {
            "W A S D : fly the camera",
            "Q / E  : down / up",
            "Z  : time 1x / slow / freeze",
            "G  : rule-of-thirds grid",
            "H  : hide the HUD",
            "F2 : save the picture",
            "P or Esc : back to flying",
            "",
            "THE TITLE SCREEN",
            "D  : city demo",
            "1-5 : pick a mission"
        }
    };

    for (int c = 0; c < 3; c++)
    {
        float cx = px + 34.0f + c * 270.0f;
        setColorA(0.45f, 0.80f, 1.0f, 1.0f);
        drawText(F12, HDR[c], cx, py + 52.0f);
        setColorA(1, 1, 1, 0.25f);
        fillRect(cx, py + 68.0f, 240.0f, 1.0f);
        for (int i = 0; i < 15; i++)
        {
            if (!COL[c][i] || !COL[c][i][0]) continue;
            setColorA(0.86f, 0.92f, 1.0f, 1.0f);
            drawText(F12, COL[c][i], cx, py + 82.0f + i * 26.0f);
        }
    }

    setColorA(0.70f, 0.76f, 0.82f, 0.95f);
    drawTextCentered(F12, "A joystick or gamepad is detected automatically - stick flies, buttons handle camera, brakes, flaps and trim.", W * 0.5f, py + 404.0f);
    setColorA(1, 1, 1, 1);
    drawTextCentered(F18, "Press any key (or click) to go back", W * 0.5f, py + 432.0f);
    text2DEnd();
}

void hudCredits()
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    setColorA(0, 0, 0, 0.60f);
    fillRect(0, 0, W, H);
    float px = W * 0.5f - 400.0f, py = H * 0.5f - 245.0f;
    panel(px, py, 800.0f, 490.0f, 0.9f);
    setColorA(1, 1, 1, 1);
    drawTextCentered(F24, "CREDITS", W * 0.5f, py + 16.0f);

    float lx = px + 44.0f, rx = px + 420.0f;
    float ly = py + 62.0f, ry = py + 62.0f;
    char buf[160];

    // ---- left: the project and the people behind it ----
    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "THE PROJECT", lx, ly);
    setColorA(0.30f, 0.55f, 0.75f, 0.7f);
    fillRect(lx, ly + 16.0f, 320.0f, 1.0f);
    ly += 30.0f;

    setColorA(1.0f, 0.98f, 0.90f, 1.0f);
    drawText(F18, "Sylhet Attack", lx, ly);
    ly += 26.0f;
    setColorA(0.86f, 0.90f, 0.95f, 1.0f);
    drawText(F12, "A computer graphics thesis project", lx, ly);
    ly += 26.0f;
    drawText(F12, "Department of Computer Science and Engineering", lx, ly);
    ly += 16.0f;
    drawText(F12, "Sylhet International University", lx, ly);
    ly += 30.0f;

    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "AUTHOR", lx, ly);
    ly += 18.0f;
    snprintf(buf, sizeof(buf), "%s   -   ID %s", CREDIT_AUTHOR, CREDIT_AUTHOR_ID);
    setColorA(1, 1, 1, 1);
    drawText(F18, buf, lx, ly);
    ly += 36.0f;

    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "SUPERVISOR", lx, ly);
    ly += 18.0f;
    setColorA(1, 1, 1, 1);
    drawText(F18, CREDIT_SUPERVISOR, lx, ly);
    ly += 34.0f;

    setColorA(0.72f, 0.78f, 0.85f, 1.0f);
    drawText(F12, "Sylhet, Bangladesh  -  2026", lx, ly);

    // ---- right: what it is built on ----
    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "BUILT WITH", rx, ry);
    setColorA(0.30f, 0.55f, 0.75f, 0.7f);
    fillRect(rx, ry + 16.0f, 336.0f, 1.0f);
    ry += 30.0f;

    static const char* LIBS[] = {
        "C++ and OpenGL",
        "freeglut 3.8.0  -  MIT / X11 licence",
        "stb_image, stb_image_write  -  public domain",
        "Photographic textures  -  Poly Haven, CC0"
    };
    setColorA(0.86f, 0.90f, 0.95f, 1.0f);
    for (int i = 0; i < 4; i++) { drawText(F12, LIBS[i], rx, ry); ry += 18.0f; }
    ry += 14.0f;

    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "THE WORLD", rx, ry);
    ry += 18.0f;
    static const char* WORLD[] = {
        "Every building, road, bridge, tree and vehicle",
        "is generated in code. Ground, walls and roofs",
        "use CC0 photographic textures; the game falls",
        "back to flat colours when assets/ is missing."
    };
    setColorA(0.86f, 0.90f, 0.95f, 1.0f);
    for (int i = 0; i < 4; i++) { drawText(F12, WORLD[i], rx, ry); ry += 17.0f; }
    ry += 14.0f;

    setColorA(0.72f, 0.78f, 0.85f, 1.0f);
    drawText(F12, "Every flight starts and ends at a real place in Sylhet.", rx, ry);
    ry += 18.0f;
    drawText(F12, "Thanks for flying over our city.", rx, ry);

    setColorA(1, 1, 1, 1);
    drawTextCentered(F18, "Click or press Esc to go back", W * 0.5f, py + 448.0f);
    text2DEnd();
}

// =====================================================================
// mini-map
// =====================================================================
static void drawMinimap(float x, float y, float size)
{
    const Aircraft& a = gAircraft;
    float half = (gMapZoom == 0) ? 1200.0f : (gMapZoom == 1 ? 2400.0f : 4800.0f);
    float scale = (size * 0.5f) / half;
    float cx = x + size * 0.5f, cy = y + size * 0.5f;

    setColorA(0.03f, 0.05f, 0.08f, 0.60f);
    fillRect(x, y, size, size);
    setColorA(1, 1, 1, 0.35f);
    frameRect(x, y, size, size, 1.0f);

    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x, (GLint)((float)gWinH - y - size), (GLsizei)size, (GLsizei)size);

#define MAPX(wx) (cx + ((wx) - a.pos.x) * scale)
#define MAPY(wz) (cy + ((wz) - a.pos.z) * scale)

    // hills
    for (int i = 0; i < gHillCount; i++)
    {
        float hx, hz, hr;
        worldHill(i, hx, hz, hr);
        setColorA(0.32f, 0.45f, 0.30f, 0.35f);
        drawCircle2D(MAPX(hx), MAPY(hz), hr * scale, 16, true);
    }

    // river
    setColorA(0.30f, 0.55f, 0.78f, 0.95f);
    for (int i = 0; i + 1 < gRiverPtsCount; i++)
        drawLine2D(MAPX(gRiverPts[i][0]), MAPY(gRiverPts[i][1]),
                   MAPX(gRiverPts[i + 1][0]), MAPY(gRiverPts[i + 1][1]), 4.0f);

    // roads
    setColorA(0.72f, 0.72f, 0.76f, 0.60f);
    for (int r = 0; r < gRoadCount; r++)
    {
        int n = worldRoadPointCount(r);
        float w = worldRoadWidth(r);
        for (int i = 0; i + 1 < n; i++)
        {
            float x0, z0, x1, z1;
            worldRoadPoint(r, i, x0, z0);
            worldRoadPoint(r, i + 1, x1, z1);
            drawLine2D(MAPX(x0), MAPY(z0), MAPX(x1), MAPY(z1), w > 20.0f ? 3.0f : 1.6f);
        }
    }
    // railway
    setColorA(0.45f, 0.42f, 0.40f, 0.8f);
    drawLine2D(MAPX(-5400.0f), MAPY(gRailZ), MAPX(5400.0f), MAPY(gRailZ), 2.0f);
    // runway
    {
        V3 p0 = worldRunwayPoint(-1450.0f, 0);
        V3 p1 = worldRunwayPoint(1450.0f, 0);
        setColorA(0.95f, 0.95f, 0.92f, 0.95f);
        drawLine2D(MAPX(p0.x), MAPY(p0.z), MAPX(p1.x), MAPY(p1.z), 4.0f);
    }
    // the SIU campus flying strip
    {
        setColorA(0.86f, 0.93f, 0.80f, 0.90f);
        drawLine2D(MAPX(1600.0f), MAPY(1850.0f), MAPX(1600.0f), MAPY(2260.0f), 3.0f);
    }

    // ring course: the whole route, with the next gate picked out
    if (gMission.active && gMission.id == MIS_RINGS)
    {
        setColorA(0.95f, 0.85f, 0.35f, 0.45f);
        // the route is exactly as long as the course: countTotal is the ring
        // count, and the loop is clamped so a bad value can never read past the
        // target array again
        int nRings = gMission.countTotal;
        if (nRings > 13) nRings = 13;
        if (nRings < 1) nRings = 1;
        for (int i = 0; i + 1 < nRings; i++)
            drawLine2D(MAPX(gMission.targets[i].x), MAPY(gMission.targets[i].z),
                       MAPX(gMission.targets[i + 1].x), MAPY(gMission.targets[i + 1].z), 1.5f);
        for (int i = 0; i < nRings; i++)
        {
            if (i == gMission.ringsPassed && gMission.step == 0)
            {
                setColorA(1.0f, 0.92f, 0.30f, 1.0f);
                drawCircle2D(MAPX(gMission.targets[i].x), MAPY(gMission.targets[i].z), 4.2f, 10, true);
            }
            else if (gMission.targets[i].done)
            {
                setColorA(0.45f, 0.95f, 0.50f, 0.9f);
                drawCircle2D(MAPX(gMission.targets[i].x), MAPY(gMission.targets[i].z), 2.4f, 8, true);
            }
        }
    }

    // landmarks
    for (int i = 0; i < gLandmarkCount; i++)
    {
        const Landmark& L = gLandmarks[i];
        float mx2 = MAPX(L.x), my2 = MAPY(L.z);
        if (mx2 < x - 20 || mx2 > x + size + 60 || my2 < y - 20 || my2 > y + size + 20) continue;
        setColorA(1, 1, 1, 0.95f);
        drawCircle2D(mx2, my2, 3.0f, 8, true);
        setColorA(0.90f, 0.95f, 1.0f, 0.95f);
        drawText(F12, L.shortName, mx2 + 5.0f, my2 - 7.0f);
    }

    // current mission target
    {
        V3 t;
        float rad;
        if (missionTargetPoint(t, rad))
        {
            float tx = MAPX(t.x), ty = MAPY(t.z);
            bool off = false;
            if (tx < x + 6) { tx = x + 6; off = true; }
            if (tx > x + size - 6) { tx = x + size - 6; off = true; }
            if (ty < y + 6) { ty = y + 6; off = true; }
            if (ty > y + size - 6) { ty = y + size - 6; off = true; }
            float blink = 0.55f + 0.45f * sinf(gMission.time * 6.0f);
            setColorA(1.0f, 0.85f, 0.20f, blink);
            drawCircle2D(tx, ty, off ? 4.0f : 5.0f, 10, true);
            if (off) drawText(F12, "<<", tx + 5.0f, ty - 6.0f);
        }
    }

    // the player
    {
        float dirx = sinf(a.yaw), diry = -cosf(a.yaw);
        float px2 = -diry, py2 = dirx;
        float tipx = cx + dirx * 10.0f, tipy = cy + diry * 10.0f;
        float lx2 = cx - dirx * 5.0f + px2 * 6.0f, ly2 = cy - diry * 5.0f + py2 * 6.0f;
        float rx2 = cx - dirx * 5.0f - px2 * 6.0f, ry2 = cy - diry * 5.0f - py2 * 6.0f;
        setColorA(1, 1, 1, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(tipx, (float)gWinH - tipy);
        glVertex2f(lx2, (float)gWinH - ly2);
        glVertex2f(rx2, (float)gWinH - ry2);
        glEnd();
    }
    glDisable(GL_SCISSOR_TEST);

    // N arrow and zoom label
    setColorA(0.9f, 0.95f, 1.0f, 0.9f);
    drawText(F12, "N", x + size - 16.0f, y + 4.0f);
    drawLine2D(x + size - 11.0f, y + 18.0f, x + size - 11.0f, y + 32.0f, 1.5f);
    drawLine2D(x + size - 11.0f, y + 18.0f, x + size - 14.0f, y + 24.0f, 1.5f);
    drawLine2D(x + size - 11.0f, y + 18.0f, x + size - 8.0f, y + 24.0f, 1.5f);
    char zbuf[32];
    snprintf(zbuf, 32, "%.1f km", half / 1000.0f);
    drawText(F12, zbuf, x + 6.0f, y + size - 18.0f);

#undef MAPX
#undef MAPY
}

// =====================================================================
// 3D labels (landmarks) - call inside the 3D pass
// =====================================================================
void hudDrawLabels()
{
    if (!gShowLabels) return;
    const Aircraft& a = gAircraft;
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < gLandmarkCount; i++)
    {
        const Landmark& L = gLandmarks[i];
        float dx = a.pos.x - L.x, dz = a.pos.z - L.z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist > 9000.0f || dist < 170.0f) continue;
        char buf[80];
        snprintf(buf, 80, "%s  %.1f km", L.name, dist / 1000.0f);
        glColor3f(0.10f, 0.12f, 0.16f);
        glRasterPos3f(L.x + 1.5f, L.labelY, L.z);
        glutBitmapString(F12, (const unsigned char*)buf);
        glColor3f(0.95f, 0.98f, 1.0f);
        glRasterPos3f(L.x, L.labelY, L.z);
        glutBitmapString(F12, (const unsigned char*)buf);
    }
    glDisable(GL_BLEND);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

// =====================================================================
// flight HUD
// =====================================================================
static void drawBar(float x, float y, float w, float h, float frac, float r, float g, float b)
{
    setColorA(0, 0, 0, 0.45f);
    fillRect(x, y, w, h);
    setColorA(r, g, b, 0.95f);
    fillRect(x, y, w * clampf(frac, 0.0f, 1.0f), h);
    setColorA(1, 1, 1, 0.35f);
    frameRect(x, y, w, h, 1.0f);
}

void hudDrawFlight()
{
    Aircraft& a = gAircraft;
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    char buf[192];

    // =================================================================
    // top-left panel: one grid for every row
    //   label at x = 28, bar from x = 100 to 184, value right-aligned at 222
    // =================================================================
    const float LX = 28.0f, BX = 100.0f, BW = 76.0f, VX = 222.0f, ROW = 22.0f;
    panel(16, 14, 220, 200, 0.50f);

    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "AIRSPEED", LX, 18);
    snprintf(buf, 192, "%.0f", a.airspeed * 1.94384f);
    setColorA(1, 1, 1, 1);
    drawStrokeText2D(GLUT_STROKE_ROMAN, buf, LX, 40.0f, 0.28f, 2.2f);
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "kt", LX + strokeTextW2D(GLUT_STROKE_ROMAN, buf, 0.28f) + 8.0f, 57.0f);

    float y = 80.0f;
    // throttle
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "THROTTLE", LX, y);
    drawBar(BX, y + 2.0f, BW, 9.0f, a.throttle, 0.35f, 0.75f, 1.0f);
    snprintf(buf, 192, "%d%%", (int)(a.throttle * 100.0f + 0.5f));
    drawText(F12, buf, VX - textW(F12, buf), y);
    y += ROW;

    // damage
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "DAMAGE", LX, y);
    {
        float hfrac = 1.0f - a.health * 0.01f;
        float br = 0.30f, bg = 0.90f, bb = 0.35f;
        if (hfrac >= 0.40f && hfrac < 0.75f) { br = 1.00f; bg = 0.80f; bb = 0.25f; }
        else if (hfrac >= 0.75f) { br = 1.00f; bg = 0.25f; bb = 0.20f; }
        drawBar(BX, y + 2.0f, BW, 9.0f, hfrac, br, bg, bb);
        snprintf(buf, 192, "%d%%", (int)(100.0f - a.health + 0.5f));
        setColorA(0.90f, 0.92f, 0.95f, 1);
        drawText(F12, buf, VX - textW(F12, buf), y);
    }
    y += ROW;

    // engine rpm
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "RPM", LX, y);
    drawBar(BX, y + 2.0f, BW, 9.0f, a.rpm / 2700.0f, 0.45f, 0.85f, 1.0f);
    snprintf(buf, 192, "%d", (int)(a.rpm + 0.5f));
    setColorA(0.90f, 0.92f, 0.95f, 1);
    drawText(F12, buf, VX - textW(F12, buf), y);
    y += ROW;

    // fuel
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "FUEL", LX, y);
    {
        bool low = a.fuel < gFuelCapacity * 0.2f;
        drawBar(BX, y + 2.0f, BW, 9.0f, a.fuel / gFuelCapacity,
                low ? 1.0f : 0.35f, low ? 0.35f : 0.90f, 0.45f);
        snprintf(buf, 192, "%d L", (int)a.fuel);
        setColorA(low ? 1.0f : 0.90f, low ? 0.45f : 0.92f, low ? 0.35f : 0.95f, 1);
        drawText(F12, buf, VX - textW(F12, buf), y);
    }
    y += ROW;

    // flaps
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "FLAP", LX, y);
    drawBar(BX, y + 2.0f, BW, 9.0f, a.cFlap, 0.55f, 0.80f, 1.0f);
    if (a.flapCmd < 0.0f) snprintf(buf, 192, "AUTO");
    else snprintf(buf, 192, "%d%%", (int)(a.flapCmd * 100.0f + 0.5f));
    setColorA(0.85f, 0.92f, 1.0f, 1);
    drawText(F12, buf, VX - textW(F12, buf), y);
    y += ROW;

    // angle of attack, with the red stall zone marked on the bar
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "AOA", LX, y);
    {
        float bx2 = BX, bw2 = BW, by2 = y + 2.0f;
        setColorA(0, 0, 0, 0.45f);
        fillRect(bx2, by2, bw2, 9.0f);
        // stall zone: alpha above 0.26 rad
        float stallFrac = 0.26f / 0.40f;
        setColorA(0.85f, 0.25f, 0.20f, 0.45f);
        fillRect(bx2 + bw2 * stallFrac, by2, bw2 * (1.0f - stallFrac), 9.0f);
        bool aoaValid = (!a.onGround) && (a.airspeed > 8.0f);
        setColorA(0.98f, 0.85f, 0.30f, 0.95f);
        if (aoaValid) fillRect(bx2, by2, bw2 * clampf(a.alpha / 0.40f, 0.0f, 1.0f), 9.0f);
        setColorA(1, 1, 1, 0.35f);
        frameRect(bx2, by2, bw2, 9.0f, 1.0f);
        if (aoaValid) snprintf(buf, 192, "%+.1f", rad2deg(a.alpha));
        else snprintf(buf, 192, "--");
        setColorA(a.stall ? 1.0f : 0.92f, a.stall ? 0.35f : 0.92f, a.stall ? 0.30f : 0.98f, 1);
        drawText(F12, buf, VX - textW(F12, buf), y);
    }

    

    // =================================================================
    // top-right panel: same grid, values right-aligned at W - 28
    // =================================================================
    const float RX = W - 224.0f, RVX = W - 28.0f;
    panel(W - 236, 14, 220, 168, 0.50f);
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "ALTITUDE", RX, 18);
    snprintf(buf, 192, "%.0f", a.pos.y * 3.28084f);
    setColorA(1, 1, 1, 1);
    drawStrokeText2D(GLUT_STROKE_ROMAN, buf, RX, 40.0f, 0.28f, 2.2f);
    setColorA(0.62f, 0.75f, 0.92f, 1);
    drawText(F12, "ft", RX + strokeTextW2D(GLUT_STROKE_ROMAN, buf, 0.28f) + 8.0f, 57.0f);

    // vertical speed
    drawText(F12, "V/S", RX, 76);
    snprintf(buf, 192, "%+.1f m/s", a.vel.y);
    setColorA(0.90f, 0.94f, 1.0f, 1);
    drawText(F12, buf, RVX - textW(F12, buf), 76);

    // heading
    drawText(F12, "HEADING", RX, 98);
    {
        float deg = fmodf(rad2deg(a.yaw) + 360.0f, 360.0f);
        static const char* card[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
        int ci = (int)((deg + 22.5f) / 45.0f) % 8;
        snprintf(buf, 192, "%03.0f  %s", deg, card[ci]);
        setColorA(1, 1, 1, 1);
        drawText(F18, buf, RVX - textW(F18, buf), 96);
    }

    // wind
    drawText(F12, "WIND", RX, 128);
    {
        float rel = wrapPi(gWindDirRad - a.yaw);
        float cx2 = W - 150.0f, cy2 = 136.0f;
        setColorA(0.75f, 0.90f, 1.0f, 0.95f);
        drawLine2D(cx2, cy2, cx2 + sinf(rel) * 13.0f, cy2 - cosf(rel) * 13.0f, 2.0f);
        drawCircle2D(cx2 + sinf(rel) * 13.0f, cy2 - cosf(rel) * 13.0f, 3.0f, 8, true);
        snprintf(buf, 192, "%.0f kt", gWindSpeed * 1.94384f);
        setColorA(0.85f, 0.92f, 1.0f, 1);
        drawText(F12, buf, RVX - textW(F12, buf), 128);
    }

    // ---- objective banner: one card, mission text on the left, clock on the right ----
    {
        char obuf[300];
        V3 tp; float trad;
        snprintf(obuf, 300, "%s", missionObjective());
        if (missionTargetPoint(tp, trad))
        {
            float dx = a.pos.x - tp.x, dz = a.pos.z - tp.z;
            float dist = sqrtf(dx * dx + dz * dz) / 1000.0f;
            snprintf(obuf + strlen(obuf), 300 - strlen(obuf), "   [%.1f km]", dist);
        }
        int tt = (int)gMission.time;
        char clock[32];
        snprintf(clock, sizeof(clock), "%d:%02d", tt / 60, tt % 60);

        float tw = textW(F18, obuf);
        float cw = textW(F12, clock);
        float cardW = tw + cw + 62.0f;
        float bx = W * 0.5f - cardW * 0.5f;

        setColorA(0, 0, 0, 0.30f);
        fillRect(bx + 3.0f, 17.0f, cardW, 30.0f);
        gradRect(bx, 12.0f, cardW, 30.0f,
                 0.10f, 0.14f, 0.19f, 0.80f,
                 0.03f, 0.05f, 0.08f, 0.80f);
        setColorA(0.36f, 0.63f, 0.86f, 0.9f);
        fillRect(bx, 12.0f, 3.0f, 30.0f);
        setColorA(1, 1, 1, 0.22f);
        frameRect(bx, 12.0f, cardW, 30.0f, 1.0f);

        // objective, left aligned with padding (and clipped to its space)
        setColorA(0.96f, 0.97f, 1.0f, 1);
        drawText(F18, obuf, bx + 16.0f, 18.0f);

        // divider, then the clock right aligned
        float dx2 = bx + cardW - cw - 30.0f;
        setColorA(1, 1, 1, 0.18f);
        fillRect(dx2 - 10.0f, 18.0f, 1.0f, 18.0f);
        setColorA(0.72f, 0.82f, 0.92f, 1);
        drawText(F12, clock, bx + cardW - cw - 14.0f, 22.0f);
    }

    // on-screen message (sensitivity, trim, flaps, ...) - just under the mission banner
    if (gToastTimer > 0.0f)
    {
        float tw = textW(F12, gToastText);
        setColorA(0.0f, 0.0f, 0.0f, 0.45f);
        fillRect(W * 0.5f - tw * 0.5f - 10.0f, 54.0f, tw + 20.0f, 20.0f);
        setColorA(1.0f, 0.95f, 0.75f, 0.95f);
        drawTextCentered(F12, gToastText, W * 0.5f, 58.0f);
    }

    // hostiles: how many are up, and a loud warning when a missile is inbound
    {
        int alive = enemyAlive();
        int lock = enemyMissileLock();
        if (alive > 0 || lock)
        {
            float y = 84.0f, cx3 = W * 0.5f;
            if (lock)
            {
                float pulse = 0.5f + 0.5f * sinf((float)gTime * 12.0f);
                setColorA(0.62f, 0.04f, 0.04f, 0.45f + 0.45f * pulse);
                fillRect(cx3 - 150.0f, y, 300.0f, 26.0f);
                setColorA(1.0f, 0.92f, 0.92f, 1.0f);
                drawTextCentered(F18, "MISSILE INBOUND  -  PRESS X FOR FLARES", cx3, y + 4.0f);
                y += 32.0f;
            }
            char eb[80];
            snprintf(eb, sizeof(eb), "HOSTILES %d     FLARES %d", alive, enemyFlares());
            float ew = textW(F12, eb);
            setColorA(0.0f, 0.0f, 0.0f, 0.45f);
            fillRect(cx3 - ew * 0.5f - 8.0f, y, ew + 16.0f, 19.0f);
            setColorA(1.0f, 0.74f, 0.38f, 0.95f);
            drawTextCentered(F12, eb, cx3, y + 3.0f);
        }
    }

    // ---- gunsight, target brackets and lead pipper (Sylhet Attack, phase 1) ----
    if (a.phase != PHASE_CRASHED)
    {
        // the gun points straight down the nose
        Basis aimB = basisFromYPR(a.yaw, a.pitch, a.roll);
        V3 aimPt = a.pos + aimB.f * 1200.0f;
        float ax, ay;
        if (hudProjectPoint(aimPt, ax, ay))
        {
            setColorA(0.80f, 1.00f, 0.88f, 0.80f);
            drawCircle2D(ax, ay, 12.0f, 22, false);
            drawLine2D(ax - 20.0f, ay, ax - 6.0f, ay, 1.4f);
            drawLine2D(ax + 6.0f, ay, ax + 20.0f, ay, 1.4f);
            drawLine2D(ax, ay - 20.0f, ax, ay - 6.0f, 1.4f);
            drawLine2D(ax, ay + 6.0f, ax, ay + 20.0f, 1.4f);
            setColorA(0.80f, 1.00f, 0.88f, 0.95f);
            drawCircle2D(ax, ay, 1.6f, 8, true);
        }

        V3 tp, tv;
        float td = 0, tr = 0;
        if (weaponTargetAim(tp, tv, td, tr))
        {
            float tx, ty;
            if (hudProjectPoint(tp, tx, ty))
            {
                // target brackets, with the distance under them
                float s2 = 16.0f;
                setColorA(1.0f, 0.55f, 0.35f, 0.95f);
                drawLine2D(tx - s2, ty - s2, tx - s2 + 7, ty - s2, 2.0f);
                drawLine2D(tx - s2, ty - s2, tx - s2, ty - s2 + 7, 2.0f);
                drawLine2D(tx + s2, ty - s2, tx + s2 - 7, ty - s2, 2.0f);
                drawLine2D(tx + s2, ty - s2, tx + s2, ty - s2 + 7, 2.0f);
                drawLine2D(tx - s2, ty + s2, tx - s2 + 7, ty + s2, 2.0f);
                drawLine2D(tx - s2, ty + s2, tx - s2, ty + s2 - 7, 2.0f);
                drawLine2D(tx + s2, ty + s2, tx + s2 - 7, ty + s2, 2.0f);
                drawLine2D(tx + s2, ty + s2, tx + s2, ty + s2 - 7, 2.0f);
                char tb[48];
                snprintf(tb, sizeof(tb), "%.1f km", td / 1000.0f);
                setColorA(1.0f, 0.72f, 0.55f, 0.95f);
                drawText(F12, tb, tx + s2 + 4.0f, ty - 6.0f);

                // lead pipper: where the nose has to point for the shells to arrive
                float tof = td / 620.0f;
                V3 lead = tp + tv * tof;
                float lx, ly;
                if (hudProjectPoint(lead, lx, ly))
                {
                    bool onTarget = (lx > ax - 22.0f && lx < ax + 22.0f && ly > ay - 22.0f && ly < ay + 22.0f);
                    if (onTarget) setColorA(0.55f, 1.0f, 0.55f, 1.0f);
                    else setColorA(1.0f, 0.90f, 0.35f, 0.95f);
                    drawCircle2D(lx, ly, 3.4f, 10, true);
                }
            }
        }
    }

    // ---- a red diamond over every bandit in range, so they are easy to find ----
    if (a.phase != PHASE_CRASHED)
    {
        V3 contacts[12];
        int nc = enemyContacts(contacts, 12);
        for (int i = 0; i < nc; i++)
        {
            float d = len(contacts[i] - a.pos);
            if (d > 3500.0f) continue;
            float px, py;
            if (!hudProjectPoint(contacts[i], px, py)) continue;
            setColorA(1.0f, 0.28f, 0.22f, 0.92f);
            drawLine2D(px, py - 9.0f, px + 7.0f, py, 2.2f);
            drawLine2D(px + 7.0f, py, px, py + 9.0f, 2.2f);
            drawLine2D(px, py + 9.0f, px - 7.0f, py, 2.2f);
            drawLine2D(px - 7.0f, py, px, py - 9.0f, 2.2f);
        }
    }

    // ---- weapons, kills and rank, bottom right ----
    {
        char wb[128];
        snprintf(wb, sizeof(wb), "GUN %3d  MSL %d  BMB %d  RKT %d  KILLS %d",
                 weaponAmmo(), weaponMissiles(), weaponBombs(), weaponRockets(), weaponKills());
        float ww = textW(F12, wb);
        setColorA(0.0f, 0.0f, 0.0f, 0.45f);
        fillRect(W - ww - 40.0f, H - 96.0f, ww + 28.0f, 42.0f);
        setColorA(0.90f, 0.95f, 1.0f, 0.95f);
        drawText(F12, wb, W - ww - 26.0f, H - 90.0f);
        setColorA(0.75f, 0.62f, 0.30f, 0.95f);
        drawText(F12, weaponRankName(), W - textW(F12, weaponRankName()) - 26.0f, H - 72.0f);
        char gb[64];
        snprintf(gb, sizeof(gb), "TARGETS %d/%d", groundTargetsAlive(), groundTargetsTotal());
        setColorA(0.85f, 0.85f, 0.80f, 0.90f);
        drawText(F12, gb, W - textW(F12, gb) - 26.0f, H - 54.0f);
    }

    // ---- radar scope, top right under the altitude panel ----
    {
        const float rr = 66.0f;
        float cx4 = W - 110.0f, cy4 = 200.0f;
        setColorA(0.02f, 0.05f, 0.08f, 0.55f);
        drawCircle2D(cx4, cy4, rr, 36, true);
        setColorA(0.35f, 0.60f, 0.45f, 0.55f);
        drawCircle2D(cx4, cy4, rr, 36, false);
        drawCircle2D(cx4, cy4, rr * 0.66f, 30, false);
        drawCircle2D(cx4, cy4, rr * 0.33f, 24, false);
        drawLine2D(cx4 - rr, cy4, cx4 + rr, cy4, 1.0f);
        drawLine2D(cx4, cy4 - rr, cx4, cy4 + rr, 1.0f);

        // sweep
        float sw = (float)gTime * 1.6f;
        setColorA(0.30f, 0.95f, 0.45f, 0.35f);
        drawLine2D(cx4, cy4, cx4 + cosf(sw) * rr, cy4 - sinf(sw) * rr, 1.5f);

        // contacts: heading-up, 8 km range
        V3 contacts[16];
        int nc = enemyContacts(contacts, 16);
        const float RANGE = 8000.0f;
        float sy2 = sinf(a.yaw), cy2 = cosf(a.yaw);
        for (int i = 0; i < nc; i++)
        {
            float dx = contacts[i].x - a.pos.x;
            float dz = contacts[i].z - a.pos.z;
            // rotate into the aircraft's frame
            float rx = dx * cy2 + dz * sy2;
            float rz = -dx * sy2 + dz * cy2;
            float px = cx4 + (rx / RANGE) * rr;
            float py = cy4 - (rz / RANGE) * rr;
            float ddx = px - cx4, ddy = py - cy4;
            if (ddx * ddx + ddy * ddy > rr * rr) continue;
            setColorA(1.0f, 0.30f, 0.25f, 0.95f);
            drawCircle2D(px, py, 3.0f, 8, true);
        }
        setColorA(0.55f, 1.0f, 0.60f, 1.0f);
        drawCircle2D(cx4, cy4, 2.2f, 8, true);
        setColorA(0.55f, 0.70f, 0.62f, 0.85f);
        drawText(F12, "8 km", cx4 + rr - 26.0f, cy4 + rr + 2.0f);
    }

    // landmark name card (shown for a few seconds after a presentation jump)
    if (gCaptionTimer > 0.0f && gCaptionText[0])
    {
        float a2 = clampf(gCaptionTimer / 1.2f, 0.0f, 1.0f);
        float y0 = H * 0.5f - 120.0f;
        setColorA(0.02f, 0.05f, 0.10f, 0.62f * a2);
        fillRect(W * 0.5f - 250.0f, y0, 500.0f, 74.0f);
        setColorA(0.45f, 0.80f, 1.0f, 0.85f * a2);
        fillRect(W * 0.5f - 250.0f, y0, 500.0f, 2.0f);
        setColorA(1.0f, 1.0f, 1.0f, a2);
        drawTextCentered(F24, gCaptionText, W * 0.5f, y0 + 14.0f);
        setColorA(0.80f, 0.86f, 0.92f, 0.9f * a2);
        drawTextCentered(F12, "1-9 and 0: jump to a landmark        N: walk through them in order", W * 0.5f, y0 + 50.0f);
    }

    // ---- arrow towards the target ----
    {
        V3 tp; float trad;
        if (missionTargetPoint(tp, trad))
        {
            float bearing = atan2f(tp.x - a.pos.x, -(tp.z - a.pos.z));
            float rel = wrapPi(bearing - a.yaw);
            float cy2 = 84.0f, cx2 = W * 0.5f;
            float c = cosf(rel), s = sinf(rel);
            float tipx = cx2 + s * 14.0f, tipy = cy2 - c * 14.0f;
            float bx2 = cx2 - s * 6.0f, by2 = cy2 + c * 6.0f;
            float px = -c * 8.0f, py = -s * 8.0f;
            setColorA(1.0f, 0.85f, 0.2f, 0.95f);
            glBegin(GL_TRIANGLES);
            glVertex2f(tipx, H - tipy);
            glVertex2f(bx2 + px, H - (by2 + py));
            glVertex2f(bx2 - px, H - (by2 - py));
            glEnd();
        }
    }

    // ---- mini-map ----
    drawMinimap(18.0f, H - 250.0f, 232.0f);

    // ---- attitude indicator ----
    {
        float cx2 = W * 0.5f, cy2 = H - 96.0f, r = 50.0f;
        setColorA(0.05f, 0.07f, 0.10f, 0.45f);
        drawCircle2D(cx2, cy2, r + 4.0f, 32, true);
        setColorA(1, 1, 1, 0.85f);
        drawCircle2D(cx2, cy2, r, 32, false);
        // horizon line (rotated by roll, moved by pitch)
        float A2 = -a.roll;
        float ux = cosf(A2), uy = sinf(A2);
        float nx = -uy, ny = ux;
        float d = clampf(rad2deg(a.pitch) * 1.7f, -r, r);
        float halfChord = sqrtf(fmaxf(0.0f, r * r - d * d));
        float qx = cx2 + nx * d, qy = cy2 + ny * d;
        setColorA(0.95f, 0.95f, 0.9f, 0.9f);
        drawLine2D(qx - ux * halfChord, qy - uy * halfChord, qx + ux * halfChord, qy + uy * halfChord, 2.0f);
        // fixed wing symbol
        setColorA(1.0f, 0.85f, 0.2f, 0.95f);
        drawLine2D(cx2 - 22.0f, cy2, cx2 - 7.0f, cy2, 2.5f);
        drawLine2D(cx2 + 7.0f, cy2, cx2 + 22.0f, cy2, 2.5f);
        drawCircle2D(cx2, cy2, 2.5f, 8, true);
        // bank scale ticks
        setColorA(1, 1, 1, 0.6f);
        drawLine2D(cx2, cy2 - r, cx2, cy2 - r + 7.0f, 1.5f);
        drawLine2D(cx2 - r * 0.707f, cy2 - r * 0.707f, cx2 - r * 0.707f + 5.0f, cy2 - r * 0.707f + 5.0f, 1.5f);
        drawLine2D(cx2 + r * 0.707f, cy2 - r * 0.707f, cx2 + r * 0.707f - 5.0f, cy2 - r * 0.707f + 5.0f, 1.5f);
        snprintf(buf, 192, "%+.0f", rad2deg(a.pitch));
        setColorA(0.85f, 0.9f, 0.95f, 0.9f);
        drawText(F12, buf, cx2 + r + 8.0f, cy2 - 7.0f);
        // trim indicator
        {
            float tx = cx2 - r - 26.0f, ty = cy2 - 30.0f;
            setColorA(0, 0, 0, 0.35f);
            fillRect(tx, ty, 7.0f, 60.0f);
            setColorA(0.55f, 0.85f, 1.0f, 0.7f);
            fillRect(tx, ty + 28.0f, 7.0f, 4.0f);
            setColorA(1.0f, 0.85f, 0.30f, 0.95f);
            float tf = clampf(a.trim * 0.5f + 0.5f, 0.0f, 1.0f);
            fillRect(tx - 2.0f, ty + (1.0f - tf) * 60.0f - 1.5f, 11.0f, 3.0f);
        }
    }

        // ---- performance overlay (F3) ----
    if (gShowFps)
    {
        snprintf(buf, 192, "%.0f fps", gFps);
        float fw = textW(F12, buf);
        setColorA(0, 0, 0, 0.45f);
        fillRect(W - fw - 24.0f, H - 28.0f, fw + 14.0f, 18.0f);
        setColorA(0.80f, 0.90f, 1.0f, 0.95f);
        drawText(F12, buf, W - fw - 17.0f, H - 24.0f);
        snprintf(buf, 192, "F3: hide");
        setColorA(0.70f, 0.78f, 0.88f, 0.8f);
        drawText(F12, buf, W - textW(F12, buf) - 17.0f, H - 44.0f);
    }

    // ---- ring course progress: twelve dots, one per gate ----
    if (gMission.active && gMission.id == MIS_RINGS && gMission.step == 0)
    {
        const float dotGap = 22.0f, dotR = 4.5f;
        int nR = gMission.countTotal;
        if (nR < 1) nR = 1;
        if (nR > 12) nR = 12;
        float totalW = nR * dotGap;
        float x0 = W * 0.5f - totalW * 0.5f + dotGap * 0.5f;
        float y0 = H - 186.0f;
        for (int i = 0; i < nR; i++)
        {
            float cx = x0 + i * dotGap;
            if (i < gMission.ringsPassed)
            {
                setColorA(0.45f, 0.95f, 0.50f, 0.95f);
                drawCircle2D(cx, y0, dotR, 10, true);
            }
            else if (i == gMission.ringsPassed)
            {
                float pulse = 1.0f + 0.35f * sinf((float)gTime * 6.0f);
                setColorA(1.0f, 0.90f, 0.30f, 1.0f);
                drawCircle2D(cx, y0, dotR * pulse, 10, true);
            }
            else
            {
                setColorA(0.25f, 0.30f, 0.38f, 0.9f);
                drawCircle2D(cx, y0, dotR * 0.8f, 10, true);
            }
        }
        setColorA(0.80f, 0.86f, 0.95f, 0.9f);
        snprintf(buf, 192, "%d / %d", gMission.ringsPassed, nR);
        drawText(F12, buf, x0 + totalW + 6.0f, y0 - 6.0f);
    }

    // ---- ground proximity warning ----
    if (a.pos.y < 70.0f && a.vel.y < -5.0f && a.phase == PHASE_AIR)
    {
        float pulse = 0.5f + 0.5f * sinf((float)gTime * 10.0f);
        setColorA(0.70f, 0.06f, 0.06f, 0.35f + 0.5f * pulse);
        fillRect(W * 0.5f - 100.0f, H * 0.5f - 150.0f, 200.0f, 26.0f);
        setColorA(1.0f, 0.95f, 0.95f, 1.0f);
        drawTextCentered(F18, "PULL UP", W * 0.5f, H * 0.5f - 146.0f);
    }

    // ---- stall warning ----
    if (a.stall)
    {
        float blink = 0.4f + 0.6f * (sinf((float)gTime * 14.0f) > 0 ? 1.0f : 0.0f);
        setColorA(1.0f, 0.15f, 0.10f, blink);
        drawTextCentered(F24, "!! STALL - LOWER THE NOSE !!", W * 0.5f, H * 0.30f);
        setColorA(1.0f, 0.15f, 0.10f, 0.5f * blink);
        fillRect(0, 0, W, 5.0f);
        fillRect(0, H - 5.0f, W, 5.0f);
        fillRect(0, 0, 5.0f, H);
        fillRect(W - 5.0f, 0, 5.0f, H);
    }

    // ---- landing message ----
    if (a.landingMsgTimer > 0.0f)
    {
        const char* txt = a.landingText;
        float flash = fminf(1.0f, a.landingMsgTimer);
        if (a.landingQuality == 3) setColorA(0.45f, 1.0f, 0.5f, flash);
        else if (a.landingQuality == 2) setColorA(1, 1, 1, flash);
        else if (a.landingQuality == 1) setColorA(1.0f, 0.85f, 0.3f, flash);
        else setColorA(1.0f, 0.4f, 0.35f, flash);
        drawTextCentered(F24, txt, W * 0.5f, H * 0.40f);
    }

    // ---- crash state ----
    if (a.phase == PHASE_CRASHED)
    {
        setColorA(0, 0, 0, 0.35f);
        fillRect(0, 0, W, H);
        setColorA(1.0f, 0.3f, 0.25f, 1.0f);
        drawTextCentered(F24, "CRASHED", W * 0.5f, H * 0.42f);
        setColorA(1, 1, 1, 1);
        drawTextCentered(F18, a.landingText, W * 0.5f, H * 0.42f + 40.0f);
        drawTextCentered(F12, "The flight is ending... you can also press R to restart now", W * 0.5f, H * 0.42f + 68.0f);
    }

    // ---- ground hints ----
    if (a.onGround && a.phase != PHASE_CRASHED)
    {
        if (!a.engineOn)
        {
            setColorA(1, 1, 1, 0.95f);
            drawTextCentered(F18, "Press  E  to start the engine", W * 0.5f, H - 40.0f);
        }
        else if (a.airspeed < 1.0f)
        {
            setColorA(0.85f, 0.9f, 0.95f, 0.9f);
            drawTextCentered(F12, "W: throttle up    A / D: steer    SPACE: brakes    C: camera", W * 0.5f, H - 40.0f);
        }
    }

    text2DEnd();
}

// =====================================================================
// photo mode overlay
// =====================================================================
void hudDrawPhoto()
{
    if (!gPhotoHud) return;      // H: a completely clean frame for the screenshot
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;

    // rule-of-thirds guides
    if (gPhotoGrid)
    {
        setColorA(1, 1, 1, 0.14f);
        fillRect(W / 3.0f, 0, 1.0f, H);
        fillRect(W * 2.0f / 3.0f, 0, 1.0f, H);
        fillRect(0, H / 3.0f, W, 1.0f);
        fillRect(0, H * 2.0f / 3.0f, W, 1.0f);
    }

    // viewfinder corners
    setColorA(1, 1, 1, 0.78f);
    const float m = 24.0f, l = 36.0f;
    fillRect(m, m, l, 2.0f);                 fillRect(m, m, 2.0f, l);
    fillRect(W - m - l, m, l, 2.0f);         fillRect(W - m - 2.0f, m, 2.0f, l);
    fillRect(m, H - m - 2.0f, l, 2.0f);      fillRect(m, H - m - l, 2.0f, l);
    fillRect(W - m - l, H - m - 2.0f, l, 2.0f); fillRect(W - m - 2.0f, H - m - l, 2.0f, l);

    // centre reticle
    setColorA(1, 1, 1, 0.45f);
    fillRect(W * 0.5f - 10.0f, H * 0.5f, 20.0f, 1.0f);
    fillRect(W * 0.5f, H * 0.5f - 10.0f, 1.0f, 20.0f);

    // top row: mode on the left, time scale on the right
    setColorA(0.95f, 0.72f, 0.28f, 1.0f);
    drawText(F18, "PHOTO MODE", 30.0f, 30.0f);
    {
        static const char* SPEED[] = { "TIME  1x", "TIME  0.35x", "TIME  0.12x", "TIME  frozen" };
        const char* s = SPEED[(gPhotoTime >= 0 && gPhotoTime < 4) ? gPhotoTime : 0];
        setColorA(1, 1, 1, 0.9f);
        drawText(F18, s, W - textW(F18, s) - 30.0f, 30.0f);
    }

    // bottom strip with the controls
    setColorA(0, 0, 0, 0.45f);
    fillRect(0, H - 48.0f, W, 48.0f);
    setColorA(0.88f, 0.92f, 0.96f, 0.95f);
    drawTextCentered(F12, "W A S D: move      Q / E: down / up      Z: time scale      G: grid      H: hide HUD      F2: save      P or Esc: fly", W * 0.5f, H - 40.0f);
    setColorA(0.70f, 0.76f, 0.82f, 0.85f);
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "ALT %.0f m     HDG %.0f", gAircraft.pos.y, rad2deg(gAircraft.yaw) < 0 ? rad2deg(gAircraft.yaw) + 360.0f : rad2deg(gAircraft.yaw));
        drawText(F12, buf, W - textW(F12, buf) - 30.0f, H - 22.0f);
    }
    text2DEnd();
}

// =====================================================================
// mission briefing
// =====================================================================
static void drawWrapped(float x, float y, const char* text, float maxChars, void* font)
{
    // a very small word wrapper: enough for a two or three line briefing
    char line[128];
    int n = 0;
    const char* p = text;
    float ly = y;
    while (*p)
    {
        // take a word
        while (*p == ' ') p++;
        const char* w = p;
        while (*p && *p != ' ') p++;
        int wl = (int)(p - w);
        if (n + wl + 1 >= (int)maxChars)
        {
            line[n] = 0;
            setColorA(0.88f, 0.92f, 0.98f, 1.0f);
            drawText(font, line, x, ly);
            ly += 20.0f;
            n = 0;
        }
        if (n) line[n++] = ' ';
        for (int i = 0; i < wl && n < 126; i++) line[n++] = w[i];
    }
    if (n)
    {
        line[n] = 0;
        setColorA(0.88f, 0.92f, 0.98f, 1.0f);
        drawText(font, line, x, ly);
    }
}

void hudBriefing()
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    setColorA(0, 0, 0, 0.45f);
    fillRect(0, 0, W, H);

    float px = W * 0.5f - 330.0f, py = H * 0.5f - 195.0f;
    panel(px, py, 660.0f, 390.0f, 0.92f);

    setColorA(1, 1, 1, 1);
    drawTextCentered(F24, missionName(gMission.id), W * 0.5f, py + 18.0f);
    setColorA(0.95f, 0.72f, 0.28f, 1.0f);
    drawText(F12, "MISSION BRIEFING", px + 40.0f, py + 58.0f);
    setColorA(1, 1, 1, 0.20f);
    fillRect(px + 40.0f, py + 74.0f, 580.0f, 1.0f);

    drawWrapped(px + 40.0f, py + 86.0f, missionDescription(gMission.id), 74.0f, F18);

    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "THREATS", px + 40.0f, py + 160.0f);
    {
        static const char* DIF[3] = { "Easy", "Normal", "Ace" };
        char tb[192];
        snprintf(tb, sizeof(tb), "%s difficulty      Bandits in the air: %d      Ground targets: %d",
                 DIF[(gDifficulty >= 0 && gDifficulty < 3) ? gDifficulty : 1],
                 enemyAlive(), groundTargetsTotal());
        setColorA(0.88f, 0.92f, 0.98f, 1.0f);
        drawText(F18, tb, px + 40.0f, py + 180.0f);
        snprintf(tb, sizeof(tb), "Anti-aircraft sites cover the river and the campus approach. Bandits carry cannon and homing missiles.");
        drawWrapped(px + 40.0f, py + 206.0f, tb, 74.0f, F12);
    }

    setColorA(0.45f, 0.80f, 1.0f, 1.0f);
    drawText(F12, "LOAD", px + 40.0f, py + 248.0f);
    {
        char lb[192];
        snprintf(lb, sizeof(lb), "GUN %d      MSL %d      BOMB %d      ROCKET %d      FLARES %d",
                 weaponAmmo(), weaponMissiles(), weaponBombs(), weaponRockets(), enemyFlares());
        setColorA(0.88f, 0.92f, 0.98f, 1.0f);
        drawText(F18, lb, px + 40.0f, py + 268.0f);
    }

    setColorA(0.75f, 0.80f, 0.85f, 0.95f);
    drawTextCentered(F12, "Z cannon   V missile   B bomb   G rockets   Tab target   X flares   H controls", W * 0.5f, py + 306.0f);
    setColorA(1, 1, 1, 1);
    drawTextCentered(F18, "Press any key (or click) to launch", W * 0.5f, py + 336.0f);
    text2DEnd();
}

// =====================================================================
// cinematic demo overlay
// =====================================================================
void hudDrawDemo()
{
    text2DBegin(gWinW, gWinH);
    float W = (float)gWinW, H = (float)gWinH;
    float fade = demoFade();
    const float bar = 34.0f;

    // letterbox
    setColorA(0, 0, 0, 1.0f);
    fillRect(0, 0, W, bar);
    fillRect(0, H - bar, W, bar);

    setColorA(1, 1, 1, 0.70f * (1.0f - fade));
    drawText(F12, "SYLHET ATTACK", 18.0f, 10.0f);
    {
        const char* tag = "CINEMATIC DEMO";
        drawText(F12, tag, W - textW(F12, tag) - 18.0f, 10.0f);
    }

    // lower-third caption
    {
        float ca = demoCaptionAlpha();
        const char* title = demoCaptionTitle();
        if (title && ca > 0.002f)
        {
            float bandH = 104.0f;
            float y0 = H - bar - bandH;
            setColorA(0, 0, 0, 0.44f * ca);
            fillRect(0, y0, W, bandH);
            setColorA(0.95f, 0.72f, 0.28f, ca);
            fillRect(32.0f, y0 + 26.0f, 3.0f, bandH - 46.0f);
            setColorA(0.02f, 0.05f, 0.08f, 0.55f * ca);
            fillRect(26.0f, y0 + 20.0f, 4.0f, bandH - 40.0f);
            setColorA(1, 1, 1, ca);
            drawText(F24, title, 52.0f, y0 + 18.0f);
            const char* body = demoCaptionBody();
            if (body)
            {
                setColorA(0.86f, 0.90f, 0.95f, ca);
                drawText(F18, body, 52.0f, y0 + 58.0f);
            }
        }
    }

    // bottom bar: hint on the right, progress along the very edge
    // (the hint is dropped while recording, so the video has no instructions in it)
    {
        if (!gVideoPath)
        {
            const char* hint = "any key returns to the title";
            setColorA(1, 1, 1, 0.65f * (1.0f - fade));
            drawText(F12, hint, W - textW(F12, hint) - 18.0f, H - 24.0f);
        }
        setColorA(0.45f, 0.80f, 1.0f, 0.85f * (1.0f - fade));
        fillRect(0, H - 2.0f, W * (demoTime() / demoDuration()), 2.0f);
    }

    if (fade > 0.001f)
    {
        setColorA(0, 0, 0, fade);
        fillRect(0, 0, W, H);
    }
    text2DEnd();
}