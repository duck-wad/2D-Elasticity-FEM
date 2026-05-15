"""Load FEM solver results written by the C++ engine."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

OUTPUT_JSON_NAME = "OUTPUT.json"


@dataclass(frozen=True)
class EngineResults:
    displacements_x: list[float]
    displacements_y: list[float]

    @property
    def num_nodes(self) -> int:
        return len(self.displacements_x)

    def validate_mesh_nodes(self, expected: int) -> None:
        if self.num_nodes != expected:
            raise ValueError(
                f"OUTPUT.json has {self.num_nodes} nodes, mesh has {expected}."
            )
        if len(self.displacements_y) != self.num_nodes:
            raise ValueError("displacements_x and displacements_y length mismatch.")

    @classmethod
    def from_path(cls, path: Path) -> EngineResults:
        data = json.loads(path.read_text(encoding="utf-8"))
        try:
            ux = [float(v) for v in data["displacements_x"]]
            uy = [float(v) for v in data["displacements_y"]]
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"Invalid {path.name}: missing or bad displacement arrays.") from exc
        return cls(displacements_x=ux, displacements_y=uy)

    def max_magnitude(self) -> float:
        m = 0.0
        for ux, uy in zip(self.displacements_x, self.displacements_y, strict=True):
            mag = (ux * ux + uy * uy) ** 0.5
            if mag > m:
                m = mag
        return m
