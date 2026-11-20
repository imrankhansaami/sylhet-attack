# Sylhet Attack - design plan

**Pitch.** You fly a modern fighter over Sylhet and you have to defend the city:
enemy strike packages come in over the Surma, ground columns roll up the airport
road, and the landmarks you already built are what is at stake. The flying,
flight model, city, landmarks, HUD, missions, demo and measurement tooling that
already exist all stay - this plan is about turning a sightseeing simulator into
an attack game without throwing the good parts away.

Everything below builds on systems that are already in the project, so each item
is small. Effort is roughly: S = an hour, M = an afternoon, L = a day or more.

---

## 1. Give the player weapons (do this first)

The enemies already have cannon shells, homing missiles, tracers and explosions -
that code just needs to work in the other direction.

| # | Feature | How it builds on what exists | Effort |
| --- | --- | --- | --- |
| 1.1 | **Cannon** - 20 mm, 150 rounds, tracers, recoil nudge, ammo on the HUD | the enemy bullet array, reversed | S |
| 1.2 | **Gunsight** - fixed reticle + a computed lead pipper + range readout | drawStar2D/drawLine2D in the HUD, target vector | S |
| 1.3 | **Air-to-air missiles** - carry four, hold `L` to lock the nearest contact in a 20 degree cone, release, smoke trail | the enemy missile code with a friendly target list | M |
| 1.4 | **Rockets** - salvos of four, unguided, for ground work | bullets with a bigger explosion | S |
| 1.5 | **Bombs** - fall ballistically, blast radius, crater decal | bullets + the existing particle system | M |
| 1.6 | **Kill feedback** - hit sparks, damage smoke, wing shed, explosion, falling wreck, `KILL 2` punch-up | enemy code + particles | M |

## 2. Make the world a battlefield

| # | Feature | How it builds on what exists | Effort |
| --- | --- | --- | --- |
| 2.1 | **Enemy fighters you can kill** (health, smoking when damaged) | enemy.cpp already has health for the ground crash case | S |
| 2.2 | **Ground targets** - a supply convoy on the airport road, a fuel depot on the apron, two AA emplacements | the traffic convoy code, driven along a road loop | M |
| 2.3 | **AA fire** - flak bursts that bracket you (black puffs + shrapnel sound later) | the enemy bullet code with an arcing shell | M |
| 2.4 | **Radar site** - a rotating dish mast that lights you up (lock warning) and can be destroyed | the telecom tower model, plus one target | M |
| 2.5 | **Bomber raids** - three-ship formations heading for a named landmark; if one reaches its release point the landmark takes damage | the AI plane code with a waypoint list and a health flag per landmark | L |
| 2.6 | **Damaged landmarks** - fires, smoke columns, scorch marks, sirens, dimmed city | rotatedQuad decals + the particle system | M |
| 2.7 | **Wreckage** - burnt-out hulks where things died, so the fight leaves marks | the parked-plane model, blackened | S |

## 3. Turn the five missions into attack missions

Keep the flying and the landmarks; change the objectives. Names are suggestions.

| Old | New | Objective |
| --- | --- | --- |
| Free Flight | **Instant Action** | Drop straight into a dogfight over the river - one key, no briefing. Ideal for showing a teacher in 30 seconds. |
| Flight School | **Training Range** | Keep it peaceful: gun practice on towed targets, a missile shot, then land. |
| City Tour | **Defend the City** | Two waves of fighters over the landmarks; you must splash them before they reach the Mazar and the clock tower. |
| Ring Course | **Deck Run** | Four gates low over the water, under Keane Bridge, up the river - with AA firing from the banks. |
| Landing Challenge | **Strike and Return** | Rocket the convoy, then get back to Osmani and land - the landing grade is now the second half of the score. |
| - | **Ace Duel** (new) | One skilled enemy ace, better AI, over the haors at dawn. |

## 4. Systems that make it feel like a game

| # | Feature | Notes | Effort |
| --- | --- | --- | --- |
| 4.1 | **Target list and cycling** (`Tab`, target box, off-screen arrow, distance) | one array of actors with a team and a hit sphere; everything (player, enemies, ground) uses it | M |
| 4.2 | **Radar scope** - a circular PPI next to the minimap with contacts and range rings | 2D circles, cheap and looks the part | S |
| 4.3 | **Player damage states** - smoke trail, engine roughness, controls hardening, then ejection (parachute) and `R` to respawn | aircraft.cpp health + particles | M |
| 4.4 | **Score, combo and ranks** - kill multiplier, accuracy, ranks from Cadet to Ace, high-score file in the game folder | the existing score and stars | S |
| 4.5 | **Rearm and refuel** - land at Osmani to reload; the landing grade already exists | reuse the landing grading | S |
| 4.6 | **Difficulty** - Easy (fewer hostiles, more flares, tougher aircraft), Normal, Ace | one multiplier table | S |
| 4.7 | **Briefing and debrief screens** - map, objectives checklist, then kills / accuracy / time / medal | the results panel, extended | M |
| 4.8 | **Procedural sound** - engine tone that follows RPM, cannon thump, missile whoosh, explosion, lock tone, all synthesised into a buffer and played through `waveOut` | no audio files needed, winmm is already linked | L |

## 5. Presentation

| # | Feature | Effort |
| --- | --- | --- |
| 5.1 | Cinematic intro: three bombers over the haors at dawn, then the title | M |
| 5.2 | Cinematic demo updated to an attack reel (strafing run, missile launch, bridge deck run) with captions | S (the recorder already exists) |
| 5.3 | A one-page "how to play" card on the title screen (already started on the Controls page) | S |
| 5.4 | New screenshots and video for the paper: combat frames, medals, radar scope | S |

---

## Suggested order

**Stage 1 - make it a shooter (about a day of work).**
Cannon and gunsight (1.1, 1.2), killable enemies with proper death effects (1.6,
2.1), kill counter and score (4.4), Instant Action button (3.1), radar scope
(4.2). After this the game is genuinely "Sylhet Attack" and is still the same
project underneath.

**Stage 2 - make the city the objective (two or three days).**
Ground targets and AA (2.2, 2.3), bombs and rockets (1.4, 1.5), the two new
mission types (Defend the City, Strike and Return), landmark damage with fire and
smoke (2.6), briefing/debrief (4.7), player damage states and ejection (4.3).

**Stage 3 - polish (as long as you want to spend).**
Missiles with lock-on (1.3), bomber raids (2.5), radar site (2.4), sound (4.8),
difficulty (4.6), the Ace Duel and the cinematic intro (5.1, 5.2).

## What must not be lost

- The flight model and the headless harness that validates it.
- The measured performance work and the evidence scripts (the paper depends on them).
- The landmarks, the demo, the recorder, and the fact that the whole world is
  generated in code - that is the thesis.
- The licence position: everything stays CC0 or original.

## Notes for the report

If the game becomes an attack game, the thesis chapters do not change: the
contribution is still the procedurally generated city of Sylhet, the physical
flight model and the measured rendering work. The combat layer is extra
gameplay on top, and the new systems (projectiles, hit detection, AI states,
damage) are all candidates for a "systems" section. The measurement story stays
strong because the frame-rate benchmark now also covers the busiest case: a
dogfight over the city with flak, smoke and wrecks.
