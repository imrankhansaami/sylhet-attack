// enemy.h - hostile aircraft that hunt the player during a mission
//
// Five or six fighters appear a few seconds into a mission, keep station on the
// player, shoot cannon bursts and launch homing missiles. They are meant to make
// the missions a contest rather than a sightseeing trip, so the player has to
// fly the objectives while dodging - and has flares (X) to break a lock.
#pragma once
#include "vecmath.h"

void enemyReset();                       // clear the sky
void enemyStart(int count);              // send `count` fighters after the player
void enemyRush();                        // bring their arrival forward (Instant Action)
void enemyBeginWaves(int total);         // wave director: bandits arrive in ones and twos
void enemyBeginRaid(V3 target, int count);// raiders that head straight for a landmark
int  enemyRaidersAlive();                // how many of them are still inbound
float landmarkIntegrity();               // average health of the ten landmarks, 0..100
void enemyUpdate(float dt);              // AI, cannon, missiles, explosions
void enemyDraw();                        // 3D pass: aircraft, tracers, missiles, smoke

int  enemyAlive();                       // fighters still flying
int  enemyMissileLock();                 // 1 when a missile is tracking the player
int  enemyFlares();                      // decoys left
bool enemyFireFlare();                   // called by the X key; false when empty

// ---- the player's weapons (Phase 1 of the Sylhet Attack plan) ----------------
void weaponReset();                      // fresh aircraft: full load, no kills yet
void weaponUpdate(float dt);             // cooldowns and shot lifetime
bool weaponFireCannon();                 // hold Z; false when the magazine is dry
bool weaponFireMissile();                // V: needs a lock on the selected bandit
void weaponCycleTarget();                // Tab: next bandit
bool weaponTargetAim(V3& pos, V3& vel, float& dist, float& radius);  // selected contact
int  enemyContacts(V3* out, int max);    // where the bandits are, for the radar scope
int  weaponAmmo();                       // cannon rounds left
int  weaponMissiles();                   // missiles left
int  weaponKills();
int  weaponShotsFired();
int  weaponShotsHit();
float weaponAccuracy();                  // 0..1
const char* weaponRankName();            // Cadet ... Ace

// ---- ground war (Phase 2) --------------------------------------------------
void groundSetup();                      // place the depot, convoy, AA and radar
void groundUpdate(float dt);             // convoy movement, AA fire, fires
void groundDraw();                       // 3D pass
bool weaponDropBomb();                   // B
bool weaponFireRockets();                // G
int  weaponBombs();
int  weaponRockets();
int  groundTargetsAlive();
int  groundTargetsTotal();
bool weaponRearm();                      // refill everything (land at Osmani)
int  landmarkDamageScore();              // how much of Sylhet has been hit, 0..100
void enemySetEnabled(bool on);           // --noenemy, or a presentation switch
bool enemyEnabled();