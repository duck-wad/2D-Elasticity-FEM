"""Run the pre-built C++ FEM engine (INPUT.txt must sit beside the executable)."""

from __future__ import annotations

import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

_INPUT_NAME = "INPUT.txt"
_OUTPUT_JSON_NAME = "OUTPUT.json"
_ENGINE_EXE_NAMES = ("fem_engine.exe", "fem_engine", "Engine.exe", "Engine")


@dataclass(frozen=True)
class EngineRunResult:
    executable: Path
    input_path: Path
    returncode: int
    stdout: str
    stderr: str


def _repo_root() -> Path:
    # src/Modeler/run_engine.py -> repo root
    return Path(__file__).resolve().parent.parent.parent


def _engine_search_dirs() -> list[Path]:
    """Directories that may contain fem_engine.exe (packaged app, dist root, dev build)."""
    dirs: list[Path] = []
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        dirs.extend([exe_dir, exe_dir.parent])
    else:
        dirs.append(_repo_root() / "dist")
        dirs.append(Path(__file__).resolve().parent.parent / "Engine" / "bin")
    seen: set[Path] = set()
    unique: list[Path] = []
    for d in dirs:
        resolved = d.resolve()
        if resolved not in seen:
            seen.add(resolved)
            unique.append(resolved)
    return unique


def engine_bin_dir() -> Path:
    exe = find_engine_executable()
    if exe is not None:
        return exe.parent
    return _engine_search_dirs()[0]


def engine_input_path() -> Path:
    return engine_bin_dir() / _INPUT_NAME


def engine_output_json_path() -> Path:
    return engine_bin_dir() / _OUTPUT_JSON_NAME


def find_engine_executable() -> Path | None:
    for directory in _engine_search_dirs():
        for name in _ENGINE_EXE_NAMES:
            candidate = directory / name
            if candidate.is_file():
                return candidate
    return None


def require_engine_executable() -> Path:
    exe = find_engine_executable()
    if exe is not None:
        return exe
    expected = _engine_search_dirs()[0] / "fem_engine.exe"
    raise FileNotFoundError(
        f"FEM engine not found (looked in {_engine_search_dirs()}). "
        "Run src\\build.ps1 to build the distribution."
    )


def write_input(text: str, *, input_path: Path | None = None) -> Path:
    path = input_path or engine_input_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def run_engine(input_text: str, *, timeout: float | None = None) -> EngineRunResult:
    """Write INPUT.txt beside the engine executable and run the solver."""
    exe = require_engine_executable()
    work_dir = exe.parent
    input_path = write_input(input_text, input_path=work_dir / _INPUT_NAME)

    completed = subprocess.run(
        [str(exe)],
        cwd=str(work_dir),
        capture_output=True,
        text=True,
        timeout=timeout,
        env={**os.environ},
    )
    return EngineRunResult(
        executable=exe,
        input_path=input_path,
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
    )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python {Path(__file__).name} <path-to-INPUT.txt>", file=sys.stderr)
        sys.exit(2)
    input_file = Path(sys.argv[1])
    text = input_file.read_text(encoding="utf-8")
    try:
        result = run_engine(text)
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
    if result.stdout:
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
    if result.stderr:
        print(result.stderr, end="" if result.stderr.endswith("\n") else "", file=sys.stderr)
    sys.exit(result.returncode)
