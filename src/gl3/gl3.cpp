// gl3.cpp - the shader renderer's plumbing: programs, uniforms, meshes
#include "gl3.h"
#include <cstdio>
#include <cstring>

static GLuint compile(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        GLsizei n = 0;
        glGetShaderInfoLog(s, sizeof(log), &n, log);
        printf("[gl3] %s shader failed:\n%s\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool gl3BuildProgram(Gl3Program& p, const char* vsSrc, const char* fsSrc)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return false; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        GLsizei n = 0;
        glGetProgramInfoLog(prog, sizeof(log), &n, log);
        printf("[gl3] link failed:\n%s\n", log);
        glDeleteProgram(prog);
        return false;
    }
    p.id = prog;
    return true;
}

void gl3Use(const Gl3Program& p) { glUseProgram(p.id); }
void gl3SetMat4(const Gl3Program& p, const char* n, const float* m) { glUniformMatrix4fv(glGetUniformLocation(p.id, n), 1, GL_FALSE, m); }
void gl3SetVec3(const Gl3Program& p, const char* n, float x, float y, float z) { glUniform3f(glGetUniformLocation(p.id, n), x, y, z); }
void gl3SetVec2(const Gl3Program& p, const char* n, float x, float y) { glUniform2f(glGetUniformLocation(p.id, n), x, y); }
void gl3SetFloat(const Gl3Program& p, const char* n, float v) { glUniform1f(glGetUniformLocation(p.id, n), v); }
void gl3SetInt(const Gl3Program& p, const char* n, int v) { glUniform1i(glGetUniformLocation(p.id, n), v); }

static void meshFromArrays(Gl3Mesh& m, const float* verts, int floatCount,
                           const unsigned* indices, int indexCount)
{
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(floatCount * sizeof(float)), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    if (indices)
    {
        glGenBuffers(1, &m.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indexCount * sizeof(unsigned)), indices, GL_STATIC_DRAW);
        m.count = indexCount;
    }
    else { m.ibo = 0; m.count = floatCount / 8; }
    glBindVertexArray(0);
}

void gl3MakeIndexedBox(Gl3Mesh& m)
{
    // unit cube: x, z in -0.5..0.5, y in 0..1, face normals
    float v[] = {
        // front (-z)
        -0.5f,0, -0.5f, 0,0,-1, 0,0,  -0.5f,1,-0.5f, 0,0,-1, 0,1,  0.5f,1,-0.5f, 0,0,-1, 1,1,  0.5f,0,-0.5f, 0,0,-1, 1,0,
        // back (+z)
         0.5f,0,  0.5f, 0,0, 1, 0,0,   0.5f,1, 0.5f, 0,0, 1, 0,1, -0.5f,1, 0.5f, 0,0, 1, 1,1, -0.5f,0, 0.5f, 0,0, 1, 1,0,
        // left (-x)
        -0.5f,0,  0.5f,-1,0,0, 0,0,  -0.5f,1, 0.5f,-1,0,0, 0,1,  -0.5f,1,-0.5f,-1,0,0, 1,1,  -0.5f,0,-0.5f,-1,0,0, 1,0,
        // right (+x)
         0.5f,0, -0.5f, 1,0,0, 0,0,   0.5f,1,-0.5f, 1,0,0, 0,1,   0.5f,1, 0.5f, 1,0,0, 1,1,   0.5f,0, 0.5f, 1,0,0, 1,0,
        // top (+y)
        -0.5f,1, -0.5f, 0,1,0, 0,0,  -0.5f,1, 0.5f, 0,1,0, 0,1,   0.5f,1, 0.5f, 0,1,0, 1,1,   0.5f,1,-0.5f, 0,1,0, 1,0,
        // bottom (-y)
        -0.5f,0,  0.5f, 0,-1,0, 0,0,  0.5f,0, 0.5f, 0,-1,0, 1,0,   0.5f,0,-0.5f, 0,-1,0, 1,1,  -0.5f,0,-0.5f, 0,-1,0, 0,1
    };
    unsigned idx[36];
    for (int f = 0; f < 6; f++)
    {
        int b = f * 4;
        idx[f * 6 + 0] = b + 0; idx[f * 6 + 1] = b + 1; idx[f * 6 + 2] = b + 2;
        idx[f * 6 + 3] = b + 0; idx[f * 6 + 4] = b + 2; idx[f * 6 + 5] = b + 3;
    }
    meshFromArrays(m, v, (int)(sizeof(v) / sizeof(float)), idx, 36);
}

static void makeGrid(Gl3Mesh& m, int n, float half)
{
    // (n+1)^2 vertices, n^2 quads, normal up, uv in world units / 4
    float* v = new float[(size_t)(n + 1) * (n + 1) * 8];
    unsigned* idx = new unsigned[(size_t)n * n * 6];
    int vi = 0;
    for (int j = 0; j <= n; j++)
    {
        for (int i = 0; i <= n; i++)
        {
            float x = -half + (2.0f * half) * (float)i / (float)n;
            float z = -half + (2.0f * half) * (float)j / (float)n;
            v[vi++] = x; v[vi++] = 0.0f; v[vi++] = z;
            v[vi++] = 0.0f; v[vi++] = 1.0f; v[vi++] = 0.0f;
            v[vi++] = x * 0.25f; v[vi++] = z * 0.25f;
        }
    }
    int ii = 0;
    for (int j = 0; j < n; j++)
    {
        for (int i = 0; i < n; i++)
        {
            unsigned a = (unsigned)(j * (n + 1) + i);
            unsigned b = a + 1;
            unsigned c = a + (unsigned)(n + 1);
            unsigned d = c + 1;
            idx[ii++] = a; idx[ii++] = c; idx[ii++] = b;
            idx[ii++] = b; idx[ii++] = c; idx[ii++] = d;
        }
    }
    meshFromArrays(m, v, vi, idx, ii);
    delete[] v;
    delete[] idx;
}

void gl3MakeGroundGrid(Gl3Mesh& m, int n, float half) { makeGrid(m, n, half); }
void gl3MakeWaterGrid(Gl3Mesh& m, int n, float half) { makeGrid(m, n, half); }