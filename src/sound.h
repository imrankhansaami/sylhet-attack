// sound.h - procedural sound: every noise in the game is synthesised at run time
//
// There are no audio files. A worker thread mixes an engine tone plus short
// one-shot voices (cannon, missile, explosion, hit, lock) into a waveOut stream.
#pragma once

enum SoundId
{
    SND_CANNON = 0,
    SND_MISSILE,
    SND_BOOM,
    SND_HIT,
    SND_LOCK,
    SND_COUNT
};

bool soundInit();                 // false when the machine has no audio device
void soundShutdown();
void soundSetEngine(float pitch01, float volume01);   // 0..1 each
void soundPlay(SoundId id, float volume);
void soundToggleMute();
bool soundMuted();