# Thesis outline (ready to expand)

Every chapter below already has its evidence produced by the project, so writing is
mostly transcription. Figures are named by their file, so the paper and the
repository stay in step. Numbers are the ones in `build\evidence.txt` and
`build\benchmark.txt` from this machine (Intel UHD 620, 1280x720, 20 November 2026
build); re-run the scripts if the numbers must be quoted from another machine.

---

## Abstract (200-250 words)

Answer four questions in order: what was built (a code-generated flight simulator of
Sylhet), with what (C++ and OpenGL/freeglut, fixed-function pipeline, single static
executable), how it was validated (a headless flight-model harness, a screen smoke
test, a measured frame-rate study), and what it achieved (a 15 km city at 21-29 fps
on integrated graphics after a doubling from chunked culling and distance LOD).

## 1. Introduction

- 1.1 Motivation - why Sylhet, why a light aircraft, why a single-file build that
  runs on the lab machines. Mention that the city's landmarks exist in no free 3D
  asset set, so the world had to be generated.
- 1.2 Problem statement: real-time rendering of a detailed city on integrated
  graphics with no GPU, while keeping an aircraft model that behaves physically.
- 1.3 Objectives (the three in `docs/DEFENCE_SCRIPT.md` section 2) and scope limits:
  one city, one aircraft, five missions, no sound, no networking.
- 1.4 Contributions: (a) a reproducible procedural model of Sylhet's landmarks;
  (b) a flight model validated numerically without a window; (c) a measured
  geometry-bound optimisation study on a fixed binary.
- 1.5 Report structure.

## 2. Background and related work

- 2.1 Flight simulation basics: six degrees of freedom, lift = ½ρV²SC_L, induced
  drag, stall, ground effect. Cite an aerodynamics text and a flight-model design
  reference (e.g. the classic Microsoft Flight Simulator flight-model articles).
- 2.2 Real-time rendering in the fixed-function pipeline: immediate mode, display
  lists, fog, per-vertex lighting; why the course pipeline still matters.
- 2.3 Level of detail and visibility culling: view-frustum culling, distance LOD,
  chunked terrain/city streaming - the classical references, then how this project
  applies them in a 2,200 m cull radius and a 900 m LOD switch.
- 2.4 Existing Sylhet/simulation work and why none of it provides the asset set
  (Google Earth imagery is not licensed for redistribution; OSM has footprints but
  no geometry).

## 3. Design

- 3.1 Architecture: 20 source files, 9,289 lines; module responsibilities table
  (world, aircraft, missions, traffic, hud, game, demo, video, glutil, textures).
- 3.2 The world model: same 1:1 coordinate system as the real city (metres, runway
  05/23 on heading 50°, Surma centreline as a polyline); how landmarks were
  dimensioned from reference photographs.
- 3.3 Mission design: the five missions and their scoring (stars, landing grades).
- 3.4 Interface: HUD grid, minimap, caption system, photo mode, cinematic demo.
- Figure: `screenshots/phase3/40_title.png`, `51_photo_mode.png`, `52_controls.png`.

## 4. Implementation

- 4.1 Flight model: state vector, 180 Hz fixed substeps with an accumulator,
  airflow-relative lift and drag, stall break, flap and ground-effect terms, trim
  and altitude hold, damage model. Keep the equations here.
- 4.2 Procedural city: density bands by radius, site exclusion rules, building
  generator (body, plinth, floor bands, balconies, parapets, rooftop clutter,
  solar panels), shops along roads, villages at the fringe.
- 4.3 Landmarks: Keane Bridge, clock tower, Mazar, SIU campus, Osmani terminal,
  stadium, railway, Eidgah; construction technique for each (boxes, cylinders,
  domes, tori, stroke text). Each was refined against photographs of the real
  place: colour, silhouette and materials (the tower's red corrugated cladding
  and pyramid roof, the bridge's red-brown riveted steel, the Eidgah's tiered
  brick terraces and white parapet, the station's concrete shell canopies). The
  references are the author's own photographs of the sites.
- 4.3a A deliberate design decision worth one paragraph: the city spans 4.4 x 4.4 km
  rather than the 8.4 x 8.4 km of the first build, so that every landmark is
  reachable in a short demonstration flight, and a landmark jump (1-9, 0, N) puts
  the aircraft beside any of them in one key press.
- 4.4 Other real-time systems: traffic on road loops with left-hand driving,
  boats, pedestrians, train, two AI aircraft; five species of bird in ten flocks
  (egrets, crows, pigeons, geese in V formation, soaring kites) drawn with
  path-aligned orientation and wing beats and placed in the flight corridors;
  windsocks that point downwind from the same wind vector the flight model uses;
  soft shadows as ground decals.
- 4.4a Signage and the billboard: the 40 m textured panel (unlit front so the
  print stays bright), how the artwork is loaded through the same texture table
  as everything else, and the code-painted fallback that keeps the sign readable
  when no artwork is present.
- 4.5 Photographic textures and their budget: 105 CC0 Poly Haven photographs used
  through a table-driven `TexId` (textures.h/textures.cpp); one wall material per
  chunk and one roof pass per chunk instead of a bind per building (the difference
  measured 15.2 vs 21.3 fps in the city); a 512 px cap applied on load by box
  filtering, which took the decoded texture set from 578 MB to 80 MB and removed
  the intermittent 2-4 fps stalls seen when 2K photos were resident. The river is
  the one generated surface (teal ripple field, scrolled texture matrix) because
  no CC0 water photograph tiled convincingly. This is a nice measured
  memory/performance trade-off to discuss in the optimisation chapter.
- 4.6 Recording and photography: F2/F3, `--shot`, the fixed-step AVI recorder
  (`src/video.cpp`), and what "deterministic recording" buys for a demonstration.
- Figures: `screenshots/phase3/43_demo_bridge.png`, `44_demo_clock.png`,
  `45_demo_mazar.png`, `47_demo_terminal.png`.

## 5. Optimisation study (the chapter examiners like)

- 5.1 Method: measure with the same binary, one switch per configuration -
  `tools\benchmark.ps1`, ten seconds per case, one sample every 0.5 s, per-sample
  CSV in `build\bench\`.
- 5.2 First finding: the frame rate is insensitive to resolution (14.4/14.7/14.3
  fps at 640x360/960x540/1280x720 in the pre-LOD build), so the bottleneck is
  geometry submission, not fill rate. This is what justified geometry work.
- 5.3 Interventions: chunking the city into 196 lists with a 2,200 m cull radius
  aligned to the fog start (so nothing pops), and a second simplified mesh per
  chunk beyond 900 m.
- 5.4 Results - the 2x2 table:

  | Viewpoint | shipped | no culling | no LOD | neither |
  | --- | --- | --- | --- | --- |
  | SIU campus strip | **27.8** | 21.5 | 19.0 | 10.2 |
  | City at 300 m | **18.5** | 17.4 | 10.8 | 9.8 |

  State the texture cost as well: 27-29 fps at the same campus viewpoint before
  the 105 photographs were added, so the realism pass costs about 15%.

  Report the range over two full runs rather than one number: the city cases moved
  by up to about 20% between runs on this machine, which is itself a finding worth
  one sentence in the discussion.
- 5.5 Discussion: the LOD switch is worth +35% to +80% at the city viewpoint, the
  cull radius +3% to +14%, and together they take the same binary from 9-12 fps to
  21-29 fps, i.e. 2.0x to 2.4x the pre-optimisation configuration. The cost is one
  extra display list per chunk in memory (408 lists in total) and no visible change
  to the silhouette, because both LOD meshes are generated from the same recorded
  footprint pass.
- Figure: `screenshots/phase4/61_bench_city.png` (shipped) beside
  `62_bench_city_baseline.png` (the same viewpoint with everything on).
- Optional figure: plot `build\bench\city_culled.csv` and `city_none.csv` with
  time on the x-axis.

## 6. Validation

- 6.1 Flight-model harness (`tools\flight_test.cpp`, 285 lines, no window): eleven
  numeric checks, and what each asserts - trimmed hands-off flight for 90 s,
  elevator authority, stall entry and recovery, take-off roll, coordinated turn
  rate, landing grade, wind drift, frame-rate independence (1/180 vs 1/60 s),
  NaN sanity, runway on dry flat ground.
  Table: check, expected, measured, verdict. Output in `build\flight_test_out.txt`.
- 6.2 Screen smoke test (`tools\smoke_test.ps1`): ten screens, each captured and
  tested for a non-blank frame (brightness mean/spread/colour count) plus the
  mission start position; 10 of 10.
- 6.3 Content statistics from `--stats`: 1,638 buildings, 323 trees, 55 road
  segments, 10 landmarks, 196 chunks in 408 display lists, 25,015 boxes, 1,600
  cylinders, 798 domes and spheres, about 366,000 primitive triangles, 18 textures
  (144 MB decoded), 61 moving vehicles, 7 boats, 34 birds, 2 AI aircraft.
- 6.4 Robustness: runs with `assets\` deleted (flat-colour fallback,
  `screenshots/phase1/no_assets.png`); 1.4 MB static executable with no DLLs.

## 7. Discussion

- What worked: LOD and culling, the fixed-step recorder, the harness as a design
  tool (several flight-model bugs were found by it, not by flying).
- What did not: the river has no convincing CC0 water texture; the detailed ground
  ends at 3.4 km; shadows are decals rather than shadow maps; single-threaded.
- Threats to validity: one machine, one integrated GPU; frame rate varies by a few
  percent between runs (state the observed spread, e.g. 21.1 vs 22.4 fps across
  two full runs of the same case); the demo camera moves, so its average is not a
  fixed-viewpoint measurement.

## 8. Conclusion and future work

Shader path (per-pixel lighting, animated water), instancing for city geometry,
terrain LOD, sound, weather, and a proper waypoint editor for missions.

## Appendices

- A: build instructions (`build.bat`, harness command, tool scripts).
- B: complete control list and command-line switches.
- C: licences (`LICENSES.txt`).
- D: evidence sheets (`build\evidence.txt`, `build\benchmark.txt`,
  `build\smoke_test.txt`).

---

## Figures to include (all already captured)

| Figure | File |
| --- | --- |
| Title screen | `screenshots/phase3/40_title.png` |
| Campus strip opening | `screenshots/phase3/41_demo_campus.png` |
| Keane Bridge | `screenshots/phase3/43_demo_bridge.png` |
| Clock tower | `screenshots/phase3/44_demo_clock.png` |
| Mazar | `screenshots/phase3/45_demo_mazar.png` |
| Airport terminal | `screenshots/phase3/47_demo_terminal.png` |
| Runway centreline | `screenshots/phase3/48_demo_runway.png` |
| Photo mode | `screenshots/phase3/51_photo_mode.png` |
| Controls page | `screenshots/phase3/52_controls.png` |
| Billboard on the strip | `screenshots/phase5/70_billboard_campus.png` |
| Al-Amin panel in the city leg | `screenshots/phase5/71_billboard_alamin.png` |
| Group-photo panel on the apron | `screenshots/phase5/72_billboard_apron.png` |
| Windsocks and birds from the strip | `screenshots/phase5/73_windsocks_birds.png` |
| Benchmark viewpoints | `screenshots/phase4/60-63_*.png` |
| Flat-colour fallback | `screenshots/phase1/no_assets.png` |

## Numbers to re-check before submission

Run these three and copy the fresh output into the paper:

```
powershell -ExecutionPolicy Bypass -File tools\benchmark.ps1
powershell -ExecutionPolicy Bypass -File tools\smoke_test.ps1
powershell -ExecutionPolicy Bypass -File tools\evidence.ps1
```