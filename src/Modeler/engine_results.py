"""Load FEM solver results written by the C++ engine."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

OUTPUT_JSON_NAME = "OUTPUT.json"


@dataclass(frozen=True)
class EngineResults:
    displacements_x: list[float]
    displacements_y: list[float]
    is_dynamic: bool = False
    num_time_steps: int = 0
    time_step_size: float = 0.0
    # history[step][node] — empty for static runs
    displacement_history_x: list[list[float]] = field(default_factory=list)
    displacement_history_y: list[list[float]] = field(default_factory=list)

    @property
    def num_nodes(self) -> int:
        return len(self.displacements_x)

    @property
    def has_animation(self) -> bool:
        return (
            self.is_dynamic
            and len(self.displacement_history_x) >= 2
            and len(self.displacement_history_y) == len(self.displacement_history_x)
        )

    def validate_mesh_nodes(self, expected: int) -> None:
        if self.num_nodes != expected:
            raise ValueError(
                f"OUTPUT.json has {self.num_nodes} nodes, mesh has {expected}."
            )
        if len(self.displacements_y) != self.num_nodes:
            raise ValueError("displacements_x and displacements_y length mismatch.")
        if self.has_animation:
            for t, (hx, hy) in enumerate(
                zip(
                    self.displacement_history_x,
                    self.displacement_history_y,
                    strict=True,
                )
            ):
                if len(hx) != expected or len(hy) != expected:
                    raise ValueError(
                        f"displacement history step {t} has wrong node count."
                    )

    def frame_displacements(self, step: int) -> tuple[list[float], list[float]]:
        """Return (ux, uy) for a time step index (0-based)."""
        if not self.has_animation:
            return self.displacements_x, self.displacements_y
        n = len(self.displacement_history_x)
        step = max(0, min(int(step), n - 1))
        return self.displacement_history_x[step], self.displacement_history_y[step]

    @classmethod
    def from_path(cls, path: Path) -> EngineResults:
        data = json.loads(path.read_text(encoding="utf-8"))
        try:
            ux = [float(v) for v in data["displacements_x"]]
            uy = [float(v) for v in data["displacements_y"]]
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(
                f"Invalid {path.name}: missing or bad displacement arrays."
            ) from exc

        is_dynamic = bool(data.get("is_dynamic", 0))
        hist_x: list[list[float]] = []
        hist_y: list[list[float]] = []
        num_steps = 0
        dt = 0.0
        if is_dynamic:
            try:
                num_steps = int(data.get("num_time_steps", 0))
                dt = float(data.get("time_step_size", 0.0))
                raw_x = data.get("displacement_history_x", [])
                raw_y = data.get("displacement_history_y", [])
                hist_x = [[float(v) for v in row] for row in raw_x]
                hist_y = [[float(v) for v in row] for row in raw_y]
            except (TypeError, ValueError) as exc:
                raise ValueError(
                    f"Invalid {path.name}: bad dynamic history arrays."
                ) from exc

        return cls(
            displacements_x=ux,
            displacements_y=uy,
            is_dynamic=is_dynamic,
            num_time_steps=num_steps,
            time_step_size=dt,
            displacement_history_x=hist_x,
            displacement_history_y=hist_y,
        )

    def max_magnitude(self) -> float:
        m = 0.0
        if self.has_animation:
            for hx, hy in zip(
                self.displacement_history_x,
                self.displacement_history_y,
                strict=True,
            ):
                for ux, uy in zip(hx, hy, strict=True):
                    mag = (ux * ux + uy * uy) ** 0.5
                    if mag > m:
                        m = mag
            return m
        for ux, uy in zip(self.displacements_x, self.displacements_y, strict=True):
            mag = (ux * ux + uy * uy) ** 0.5
            if mag > m:
                m = mag
        return m
