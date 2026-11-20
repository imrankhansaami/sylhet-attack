// gl3_post.h - the shader post-processing layer for Sylhet Attack
//
// The game still draws its geometry the way it always has. This layer takes
// that finished 3D image, runs it through GLSL (FXAA, bloom, filmic tone map,
// colour grade and a light sharpen) and puts the crisp 2D HUD on top. It is
// deliberately free of GL headers so game.cpp can call it without disturbing
// the fixed-function renderer.
#pragma once

// Load OpenGL 3.3 through GLAD, compile the post shaders and create the
// off-screen targets. Returns false (and leaves the old renderer alone) if the
// driver cannot do it. Call once, after the GLUT window exists.
bool gl3PostInit(int width, int height);

// Free the GL objects. Safe to call whether or not init succeeded.
void gl3PostShutdown();

// Redirect the 3D scene into the off-screen target. Call before glClear().
bool gl3PostBegin(int width, int height);

// Resolve the scene through the shader chain and draw it to the back buffer.
// Call after the 3D world and before the 2D HUD.
void gl3PostEnd();

bool gl3PostActive();

// Runtime switch for A/B comparison, screenshots and benchmarking.
void gl3PostSetEnabled(bool on);
bool gl3PostEnabled();