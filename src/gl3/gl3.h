// gl3.h - the shader renderer's interface (OpenGL 3.3 core + GLSL)
//
// Founded here, wired behind RENDERER=GL3 in the build: the fixed-function
// renderer stays the shipping path until this one passes the same checks.
#pragma once
#include <glad/gl.h>

struct Gl3Program
{
    GLuint id;
};

// compile and link; prints the compiler log and returns false on error
bool gl3BuildProgram(Gl3Program& p, const char* vertexSrc, const char* fragmentSrc);
void gl3Use(const Gl3Program& p);
void gl3SetMat4(const Gl3Program& p, const char* name, const float* m);
void gl3SetVec3(const Gl3Program& p, const char* name, float x, float y, float z);
void gl3SetVec2(const Gl3Program& p, const char* name, float x, float y);
void gl3SetFloat(const Gl3Program& p, const char* name, float v);
void gl3SetInt(const Gl3Program& p, const char* name, int v);

// a mesh is a VAO plus up to four attributes and an optional index buffer
struct Gl3Mesh
{
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    int count;          // vertices, or indices when ibo is not 0
};

void gl3MakeIndexedBox(Gl3Mesh& m);           // unit cube, 36 indices
void gl3MakeGroundGrid(Gl3Mesh& m, int n, float half);   // flat grid for the terrain
void gl3MakeWaterGrid(Gl3Mesh& m, int n, float half);    // grid for the river surface