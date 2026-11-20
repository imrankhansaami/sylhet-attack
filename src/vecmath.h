// vecmath.h - small 3D math helpers for Sylhet Attack
#pragma once
#include <cmath>

const float PI_F = 3.14159265358979f;

inline float deg2rad(float d) { return d * (PI_F / 180.0f); }
inline float rad2deg(float r) { return r * (180.0f / PI_F); }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float wrapPi(float a) { while (a >  PI_F) a -= 2.0f * PI_F; while (a < -PI_F) a += 2.0f * PI_F; return a; }

// tiny deterministic random generator (so the city looks the same every run)
inline float frand01(unsigned& s) { s = s * 1664525u + 1013904223u; return (float)((s >> 8) & 0xFFFFFF) / 16777216.0f; }

struct V3 { float x, y, z; };

inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
inline V3 operator+(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline V3 operator-(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline V3 operator-(V3 a) { return v3(-a.x, -a.y, -a.z); }
inline V3 operator*(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
inline V3 operator*(float s, V3 a) { return a * s; }
inline V3& operator+=(V3& a, V3 b) { a = a + b; return a; }
inline V3& operator-=(V3& a, V3 b) { a = a - b; return a; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float len(V3 a) { return sqrtf(dot(a, a)); }
inline V3 norm(V3 a) { float l = len(a); if (l < 1e-6f) return v3(0, 0, 0); return a * (1.0f / l); }

// Rotation helpers that match the OpenGL glRotatef convention
inline V3 rotX(V3 v, float a) { float c = cosf(a), s = sinf(a); return v3(v.x, c * v.y - s * v.z, s * v.y + c * v.z); }
inline V3 rotY(V3 v, float a) { float c = cosf(a), s = sinf(a); return v3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z); }
inline V3 rotZ(V3 v, float a) { float c = cosf(a), s = sinf(a); return v3(c * v.x - s * v.y, s * v.x + c * v.y, v.z); }

// Aircraft / object orientation basis.
// Model space: the model faces -Z. yaw > 0 turns right, pitch > 0 is nose up, roll > 0 is a right bank.
struct Basis { V3 f, r, u; };
inline Basis basisFromYPR(float yaw, float pitch, float roll)
{
    Basis b;
    V3 f = v3(0, 0, -1), r = v3(1, 0, 0), u = v3(0, 1, 0);
    f = rotZ(f, -roll); r = rotZ(r, -roll); u = rotZ(u, -roll);
    f = rotX(f, pitch); r = rotX(r, pitch); u = rotX(u, pitch);
    f = rotY(f, -yaw);  r = rotY(r, -yaw);  u = rotY(u, -yaw);
    b.f = f; b.r = r; b.u = u;
    return b;
}

// Heading (radians) whose direction vector points along d (d in the XZ plane)
inline float yawFromDir(V3 d) { return atan2f(d.x, -d.z); }
