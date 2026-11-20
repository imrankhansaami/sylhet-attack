// sound.cpp - a tiny software synthesiser playing through winmm waveOut
//
// The game has no audio files: the engine drone and the weapon noises are all
// generated sample by sample. Four small buffers are kept in flight with the
// device; a worker thread fills whichever one the device has finished with.

#include "sound.h"
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstring>

namespace
{
const int SR = 22050;          // sample rate
const int FRAMES = 512;        // per buffer (~23 ms)
const int BUFFERS = 4;
const float PI2 = 6.2831853f;

HWAVEOUT sDev = nullptr;
WAVEHDR sHdr[BUFFERS];
short sBuf[BUFFERS][FRAMES * 2];
HANDLE sThread = nullptr;
volatile bool sRun = false;
volatile bool sMuted = false;

// engine
volatile float sEnginePitch = 0.0f;
volatile float sEngineVol = 0.0f;
float sPhaseA = 0.0f, sPhaseB = 0.0f;

// one-shot voices
struct Voice
{
    int type;
    float t;          // seconds since it started
    float dur;
    float vol;
    float phase;
    float noise;
    float freq;
    bool active;
};
Voice sVoices[16];

unsigned sSeed = 12345u;
float noise()
{
    sSeed = sSeed * 1664525u + 1013904223u;
    return ((float)((sSeed >> 8) & 0xFFFF) / 32768.0f) - 1.0f;
}

void addVoice(int type, float vol)
{
    for (int i = 0; i < 16; i++)
    {
        if (sVoices[i].active) continue;
        Voice& v = sVoices[i];
        v.active = true;
        v.type = type;
        v.t = 0.0f;
        v.vol = vol;
        v.phase = 0.0f;
        v.noise = 0.0f;
        v.freq = 0.0f;
        switch (type)
        {
            case SND_CANNON: v.dur = 0.11f; v.freq = 95.0f; break;
            case SND_MISSILE: v.dur = 0.65f; v.freq = 180.0f; break;
            case SND_BOOM: v.dur = 0.90f; v.freq = 85.0f; break;
            case SND_HIT: v.dur = 0.07f; v.freq = 1400.0f; break;
            case SND_LOCK: v.dur = 0.10f; v.freq = 1050.0f; break;
        }
        return;
    }
}

float synthesis(float dt)
{
    float mix = 0.0f;

    // ---- engine: a low drone with two partials ----
    if (sEngineVol > 0.001f)
    {
        float f = 42.0f + 110.0f * sEnginePitch;
        sPhaseA += f / SR; if (sPhaseA > 1.0f) sPhaseA -= 1.0f;
        sPhaseB += (f * 2.02f) / SR; if (sPhaseB > 1.0f) sPhaseB -= 1.0f;
        float saw = 2.0f * sPhaseA - 1.0f;
        float drone = sinf(sPhaseA * PI2) * 0.55f + saw * 0.20f + sinf(sPhaseB * PI2) * 0.25f;
        mix += drone * sEngineVol * 0.22f;
    }

    // ---- one shots ----
    for (int i = 0; i < 16; i++)
    {
        Voice& v = sVoices[i];
        if (!v.active) continue;
        v.t += dt;
        float f = v.t / v.dur;
        if (f >= 1.0f) { v.active = false; continue; }

        float env = 1.0f - f;
        env *= env;
        float s = 0.0f;

        switch (v.type)
        {
            case SND_CANNON:
            {
                v.noise = v.noise * 0.55f + noise() * 0.45f;
                v.phase += v.freq / SR; if (v.phase > 1.0f) v.phase -= 1.0f;
                s = v.noise * 0.7f + sinf(v.phase * PI2) * 0.5f;
                v.freq *= 0.985f;
                break;
            }
            case SND_BOOM:
            {
                v.noise = v.noise * 0.85f + noise() * 0.15f;
                v.freq = 85.0f * (1.0f - f * 0.75f);
                v.phase += v.freq / SR; if (v.phase > 1.0f) v.phase -= 1.0f;
                s = sinf(v.phase * PI2) * 0.65f + v.noise * 0.55f;
                break;
            }
            case SND_MISSILE:
            {
                v.noise = v.noise * 0.80f + noise() * 0.20f;
                v.freq = 180.0f + 900.0f * f;
                v.phase += v.freq / SR; if (v.phase > 1.0f) v.phase -= 1.0f;
                s = v.noise * 0.55f + sinf(v.phase * PI2) * 0.25f;
                break;
            }
            default:   // hit / lock: short tones
            {
                v.phase += v.freq / SR; if (v.phase > 1.0f) v.phase -= 1.0f;
                s = sinf(v.phase * PI2) * 0.8f;
                break;
            }
        }
        mix += s * v.vol * env;
    }

    if (mix > 1.0f) mix = 1.0f;
    if (mix < -1.0f) mix = -1.0f;
    return mix;
}

void fillBlock(short* out, int frames)
{
    const float dt = 1.0f / (float)SR;
    for (int i = 0; i < frames; i++)
    {
        float s = sMuted ? 0.0f : synthesis(dt);
        short v = (short)(s * 30000.0f);
        out[i * 2 + 0] = v;
        out[i * 2 + 1] = v;
    }
}

DWORD WINAPI audioThread(LPVOID)
{
    while (sRun)
    {
        bool wrote = false;
        for (int i = 0; i < BUFFERS; i++)
        {
            if (!(sHdr[i].dwFlags & WHDR_DONE)) continue;
            fillBlock(sBuf[i], FRAMES);
            sHdr[i].dwFlags &= ~WHDR_DONE;
            waveOutWrite(sDev, &sHdr[i], sizeof(WAVEHDR));
            wrote = true;
            break;
        }
        if (!wrote) Sleep(2);
    }
    return 0;
}
}

bool soundInit()
{
    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = SR;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    if (waveOutOpen(&sDev, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        sDev = nullptr;
        return false;
    }

    for (int i = 0; i < BUFFERS; i++)
    {
        memset(&sHdr[i], 0, sizeof(WAVEHDR));
        memset(sBuf[i], 0, sizeof(sBuf[i]));
        sHdr[i].lpData = (LPSTR)sBuf[i];
        sHdr[i].dwBufferLength = sizeof(sBuf[i]);
        sHdr[i].dwFlags = WHDR_DONE;
        waveOutPrepareHeader(sDev, &sHdr[i], sizeof(WAVEHDR));
    }

    sRun = true;
    sThread = CreateThread(nullptr, 0, audioThread, nullptr, 0, nullptr);
    return true;
}

void soundShutdown()
{
    if (!sDev) return;
    sRun = false;
    if (sThread)
    {
        WaitForSingleObject(sThread, 1000);
        CloseHandle(sThread);
        sThread = nullptr;
    }
    waveOutReset(sDev);
    for (int i = 0; i < BUFFERS; i++) waveOutUnprepareHeader(sDev, &sHdr[i], sizeof(WAVEHDR));
    waveOutClose(sDev);
    sDev = nullptr;
}

void soundSetEngine(float pitch01, float volume01)
{
    if (!sDev) return;
    if (pitch01 < 0.0f) pitch01 = 0.0f;
    if (pitch01 > 1.0f) pitch01 = 1.0f;
    if (volume01 < 0.0f) volume01 = 0.0f;
    if (volume01 > 1.0f) volume01 = 1.0f;
    sEnginePitch = pitch01;
    sEngineVol = volume01;
}

void soundPlay(SoundId id, float volume)
{
    if (!sDev || sMuted) return;
    addVoice((int)id, volume);
}

void soundToggleMute()
{
    sMuted = !sMuted;
    if (sMuted) sEngineVol = 0.0f;
}

bool soundMuted() { return sMuted; }