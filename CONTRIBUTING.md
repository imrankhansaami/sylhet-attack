# Contributing to Sylhet Attack

Thanks for taking a look. The project is MIT licensed, builds with one batch file and has no
private assets or online services, so a fork is the expected way to experiment.

## Before you start

- Read the [README](README.md), especially the build and controls sections.
- Build the unmodified game once and run the smoke test. That tells you whether a later
  failure is yours or the machine's.
- Open an issue before a large feature, a renderer change or a new engine port, so the scope
  can be agreed first.
- Small fixes — a typo, a crash, a wrong HUD value, a missing build file — can go straight to
  a pull request.

## Set up a fork

```bat
git clone https://github.com/<your-name>/sylhet-attack.git
cd sylhet-attack
git remote add upstream https://github.com/imrankhansaami/sylhet-attack.git
build.bat
run.bat
```

Use a branch per change:

```bat
git checkout -b fix/landing-grade
```

## Build and test

Requirements: Windows 10/11 and a MinGW `g++` (the compiler bundled with Code::Blocks works;
Dev-C++ MinGW64 and a `g++` on `PATH` are also detected).

```bat
build.bat
powershell -ExecutionPolicy Bypass -File tools\smoke_test.ps1
```

`build.bat debug` shows compiler output in a console. If linking fails with “Permission
denied”, close any running `SylhetAttack.exe` first.

Before opening a pull request, please run:

```bat
build.bat
powershell -ExecutionPolicy Bypass -File tools\smoke_test.ps1
```

For a gameplay or performance change, also run the relevant tool:

```bat
powershell -ExecutionPolicy Bypass -File tools\benchmark.ps1
powershell -ExecutionPolicy Bypass -File tools\takeoff_test.ps1
powershell -ExecutionPolicy Bypass -File tools\control_test.ps1
```

The smoke test should report **10 of 10 screens passed**. Build output (`build\`, `dist\`,
`SylhetAttack.exe`, logs and generated screenshots) must not be committed; the `.gitignore`
handles the usual cases.

## What we accept

Good contributions include:

- bug fixes with clear reproduction steps;
- performance work backed by a before/after benchmark;
- flight-model, AI or weapon tuning that keeps the game winnable;
- new landmarks, missions, HUD details or visual improvements in the existing style;
- build, documentation and accessibility improvements;
- a Godot or Unity port under `ports/` (keep the C++ game working).

Please keep these rules:

- **The game stays easy to win.** Easy, Normal and Ace should all be finishable by a new
  player.
- **BAF Training Range stays peaceful.** No hostiles, ever.
- **No civilian targets, gore or real-world political claims.** Landmarks are evacuated;
  military targets only.
- **No copyrighted assets.** Do not add models, textures, music, fonts or images you do not
  have the right to distribute. CC0 is preferred; generated content is best.
- **No new runtime dependencies** unless there is a strong reason. The Windows build should
  remain a static exe that runs without installing DLLs.
- **Do not change the old C++ game when adding a port.** Put Godot/Unity work in its own
  folder and leave the original build green.

## Code style

The codebase is C++11, compiled with MinGW. Match the surrounding style:

- four-space indentation, opening brace on its own line for functions;
- `static` for file-local helpers; small functions with a short comment above non-obvious
  maths;
- keep comments useful and in the same plain-English voice as the rest of the project;
- no `using namespace std;` in headers;
- no new third-party library unless it is vendored with its licence and added to
  `LICENSES.txt`;
- if you add a `.cpp` file, add it to `build.bat` — otherwise it will not be compiled;
- if you change a control, a mission or the HUD, update the in-game controls page and the
  README.

## Pull request checklist

- [ ] One logical change, with a clear title.
- [ ] `build.bat` succeeds.
- [ ] `tools\smoke_test.ps1` reports 10 of 10 screens.
- [ ] Gameplay changes include a screenshot or short recording.
- [ ] Performance changes include the benchmark CSV or a table.
- [ ] README/controls updated if behaviour or inputs changed.
- [ ] No build output, generated video or screenshot dumps added.
- [ ] No copyrighted assets added.
- [ ] The game is still winnable on Easy, Normal and Ace.

In the pull request, say what changed, why, how you tested it, and anything you could not
test. A small, honest PR is much easier to review than a large one with no evidence.

## Reporting a bug

Please include:

- Windows version and GPU;
- the exact steps to reproduce;
- what you expected and what happened;
- the mission, difficulty and controls used;
- a screenshot if it is visual;
- `build\smoke_test.txt` or console output if a test failed.

Do not attach the whole build folder. The source, the diff and the relevant log are enough.

## Licence of contributions

By contributing, you agree that your contribution is licensed under the project's MIT
licence (see [LICENSE](LICENSE)) and that you have the right to submit it. Third-party
assets keep their own licences and must be recorded in [LICENSES.txt](LICENSES.txt).