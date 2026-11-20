// video.h - writes the frame sequence as a playable MJPEG AVI
//
// The game renders with a fixed time step while recording, so every frame is
// exactly 1/fps of flight time and the result is smooth whatever the machine can
// manage in real time. Each frame is a JPEG (stb_image_write) wrapped in a
// standard AVI container, which Windows Media Player, VLC and PowerPoint all
// play with no extra software installed.
#pragma once

bool videoBegin(const char* path, int width, int height, int fps, int maxFrames, int quality);
void videoAddFrame(const unsigned char* rgbTopDown);   // width * height * 3
void videoEnd();
bool videoActive();
int videoFrameCount();