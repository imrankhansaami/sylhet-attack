// tools/gl3_test.cpp - proves the shader pipeline on this machine
//
// Opens a 3.3 core-profile window with freeglut, loads GL through GLAD, compiles
// the sky and lit-surface shaders from src/gl3/gl3_shaders.h, draws one frame
// and writes it to PNG. It does not touch the game; it is the first checkpoint
// of the shader port.
#include <windows.h>
#include <glad/gl.h>
#define FREEGLUT_NO_GL_INCLUDE
#include <GL/freeglut.h>
#include "gl3/gl3.h"
#include "gl3/gl3_shaders.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

static const int W = 1280, H = 720;

static float deg2rad(float d) { return d * 3.14159265358979f / 180.0f; }

struct M4 { float m[16]; };

static M4 identity()
{
    M4 r = { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } };
    return r;
}

static M4 perspective(float fovyDeg, float aspect, float zn, float zf)
{
    float f = 1.0f / tanf(deg2rad(fovyDeg) * 0.5f);
    M4 r = { { 0 } };
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1.0f;
    r.m[14] = 2.0f * zf * zn / (zn - zf);
    return r;
}

struct V { float x, y, z; };
static V sub(V a, V b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static V cross3(V a, V b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
static V norm3(V a) { float l = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); return { a.x / l, a.y / l, a.z / l }; }

static M4 lookAt(V eye, V at, V up)
{
    V f = norm3(sub(at, eye));
    V s = norm3(cross3(f, up));
    V u = cross3(s, f);
    M4 r;
    r.m[0] = s.x; r.m[1] = u.x; r.m[2] = -f.x; r.m[3] = 0;
    r.m[4] = s.y; r.m[5] = u.y; r.m[6] = -f.y; r.m[7] = 0;
    r.m[8] = s.z; r.m[9] = u.z; r.m[10] = -f.z; r.m[11] = 0;
    r.m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    r.m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    r.m[14] = (f.x * eye.x + f.y * eye.y + f.z * eye.z);
    r.m[15] = 1;
    return r;
}

// column-major a * b, matching the shader's column-major mat4
static M4 mul(const M4& a, const M4& b)
{
    M4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
        {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

static Gl3Program sSky, sSurface;
static Gl3Mesh sGround;
static GLuint sSkyVao, sWhiteTex;

// Windows loader: wglGetProcAddress for extensions/core 1.2+, opengl32.dll for 1.1
static GLADapiproc loadGlProc(const char* name)
{
    GLADapiproc p = (GLADapiproc)wglGetProcAddress(name);
    uintptr_t v = (uintptr_t)p;
    if (v == 0 || v == 1 || v == 2 || v == 3 || v == (uintptr_t)-1)
    {
        static HMODULE gl = LoadLibraryA("opengl32.dll");
        p = (GLADapiproc)GetProcAddress(gl, name);
    }
    return p;
}

static void glCheck(const char* what)
{
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) printf("[gl3] error after %s: 0x%x\n", what, e);
}

static void display()
{
    glViewport(0, 0, W, H);
    glClearColor(0.80f, 0.87f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    M4 proj = perspective(60.0f, (float)W / (float)H, 1.5f, 20000.0f);
    V eye = { 0.0f, 90.0f, 700.0f };
    V at = { 0.0f, 20.0f, 0.0f };
    M4 view = lookAt(eye, at, { 0, 1, 0 });
    M4 viewProj = mul(proj, view);

    // sky
    glDisable(GL_DEPTH_TEST);
    glUseProgram(sSky.id);
    V f = norm3(sub(at, eye));
    V r = norm3(cross3(f, { 0, 1, 0 }));
    V u = cross3(r, f);
    gl3SetVec3(sSky, "uFwd", f.x, f.y, f.z);
    gl3SetVec3(sSky, "uRight", r.x, r.y, r.z);
    gl3SetVec3(sSky, "uUp", u.x, u.y, u.z);
    gl3SetFloat(sSky, "uTanHalfFov", tanf(deg2rad(60.0f) * 0.5f));
    gl3SetFloat(sSky, "uAspect", (float)W / (float)H);
    gl3SetVec3(sSky, "uSunDir", 0.40f, 0.55f, -0.35f);
    gl3SetVec3(sSky, "uHorizon", 0.82f, 0.88f, 0.93f);
    gl3SetVec3(sSky, "uZenith", 0.28f, 0.48f, 0.80f);
    glBindVertexArray(sSkyVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glCheck("sky");

    // ground through the lit surface shader
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    gl3Use(sSurface);
    M4 model = identity();
    gl3SetMat4(sSurface, "uModel", model.m);
    gl3SetMat4(sSurface, "uViewProj", viewProj.m);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sWhiteTex);
    gl3SetInt(sSurface, "uAlbedo", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sWhiteTex);
    gl3SetInt(sSurface, "uNormalMap", 1);
    gl3SetInt(sSurface, "uUseNormalMap", 0);
    gl3SetVec3(sSurface, "uTint", 0.42f, 0.55f, 0.32f);
    gl3SetVec3(sSurface, "uSunDir", 0.40f, 0.55f, -0.35f);
    gl3SetVec3(sSurface, "uSunColour", 1.00f, 0.94f, 0.82f);
    gl3SetVec3(sSurface, "uEye", eye.x, eye.y, eye.z);
    gl3SetVec3(sSurface, "uFogColour", 0.80f, 0.87f, 0.92f);
    gl3SetVec2(sSurface, "uFogRange", 2500.0f, 14000.0f);
    gl3SetFloat(sSurface, "uSpecular", 0.0f);
    glBindVertexArray(sGround.vao);
    glDrawElements(GL_TRIANGLES, sGround.count, GL_UNSIGNED_INT, 0);
    glCheck("ground");

    // read the frame and save it
    unsigned char* px = (unsigned char*)malloc((size_t)W * H * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px);
    unsigned char* flip = (unsigned char*)malloc((size_t)W * H * 3);
    for (int y = 0; y < H; y++)
        memcpy(flip + (size_t)y * W * 3, px + (size_t)(H - 1 - y) * W * 3, (size_t)W * 3);
    stbi_write_png("build/gl3_first.png", W, H, 3, flip, W * 3);
    free(px);
    free(flip);

    printf("[gl3] frame written to build/gl3_first.png\n");
    fflush(stdout);
    glutLeaveMainLoop();
}

int main(int argc, char** argv)
{
    FILE* log = fopen("build/gl3_step.txt", "w");
    if (log) { fprintf(log, "enter main\n"); fflush(log); }
    glutInit(&argc, argv);
    if (log) { fprintf(log, "after glutInit\n"); fflush(log); }
    glutInitContextVersion(3, 3);
    glutInitContextFlags(GLUT_FORWARD_COMPATIBLE);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(W, H);
    int win = glutCreateWindow("Sylhet Attack - GL3 test");
    if (log) { fprintf(log, "after create window win=%d\n", win); fflush(log); }

    int version = gladLoadGL((GLADloadfunc)loadGlProc);
    if (log) { fprintf(log, "after gladLoadGL version=%d\n", version); fflush(log); }
    if (!version)
    {
        printf("[gl3] could not load OpenGL 3.3\n");
        if (log) { fprintf(log, "glad failed\n"); fclose(log); }
        return 1;
    }
    printf("[gl3] context reports: %s | %s | %s\n",
           (const char*)glGetString(GL_VERSION),
           (const char*)glGetString(GL_RENDERER),
           (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    if (log) { fprintf(log, "glad ok\n"); fflush(log); }

    if (!gl3BuildProgram(sSky, GL3_SKY_VS, GL3_SKY_FS)) return 1;
    if (!gl3BuildProgram(sSurface, GL3_SURFACE_VS, GL3_SURFACE_FS)) return 1;

    float quad[] = { -1,-1, 1,-1, -1,1, 1,1 };
    glGenVertexArrays(1, &sSkyVao);
    glBindVertexArray(sSkyVao);
    GLuint qv;
    glGenBuffers(1, &qv);
    glBindBuffer(GL_ARRAY_BUFFER, qv);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    glGenTextures(1, &sWhiteTex);
    glBindTexture(GL_TEXTURE_2D, sWhiteTex);
    unsigned char white[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl3MakeGroundGrid(sGround, 64, 4000.0f);

    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}