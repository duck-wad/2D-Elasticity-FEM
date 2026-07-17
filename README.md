# 2D Elasticity FEM

A 2D finite element modeler for plane stress / plane strain problems. Build a rectangular mesh, apply supports and loads (including dynamic loads), run the solver, and view deformed shapes or animate dynamic results.

![image-20260716220406257](C:\Users\Nick\AppData\Roaming\Typora\typora-user-images\image-20260716220406257.png)

## Download & run

1. Get a release build (or build once with `.\src\build.ps1`).
2. Open the `dist` folder.
3. Double-click **`fem_modeler.exe`**.

`fem_engine.exe` must sit in the same folder as the modeler (the build script places both there).

## Using the app

1. Set rectangle size and mesh density → **Generate mesh**
2. Choose material and (optional) **Dynamic analysis** settings
3. Apply edge fixities and point / distributed loads
4. Click **Compute**
5. Use **Show deformed shape** (and **Animate** for dynamic runs)

## Source code 

- `src/Engine/` — C++ FEM solver (`fem_engine.exe`). Reads `INPUT.txt`, assembles and solves the system, writes `OUTPUT.json`.
  - `Main.cpp` — program entry point; reads input, runs assembly/solve/results, writes output
  - `FileReader.cpp` — parses `INPUT.txt` into the engine model
  - `FileWriter.cpp` — writes `OUTPUT.json` and optional debug CSV files
  - `Model.cpp` — main analysis workflow: global matrices, loads, boundary conditions, static/dynamic solve
  - `Element.cpp` — Q4 element stiffness, mass, damping, and distributed load calculations
  - `Material.cpp` — material properties and elastic constitutive matrix
  - `Solver.cpp` — linear equation solvers
  - `Structs.h` / `Enums.h` — shared load structs and analysis options
- `src/Modeler/` — Python/Qt GUI (`fem_modeler.exe`).
  - `main.py` — main window, mesh view, loads/supports, results and animation
  - `mesh.py` — rectangular mesh generation
  - `export_input.py` — writes the engine `INPUT.txt`
  - `run_engine.py` — runs `fem_engine.exe`
  - `engine_results.py` — reads `OUTPUT.json`
- `dist/` — packaged app (`fem_modeler.exe` + `fem_engine.exe`)
- `src/build.ps1` — build script that produces `dist/`
