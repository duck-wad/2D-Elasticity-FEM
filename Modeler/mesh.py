"""Structured quadrilateral mesh on an axis-aligned rectangle."""

from __future__ import annotations

from dataclasses import dataclass
from typing import List, Sequence, Tuple

Vec2 = Tuple[float, float]
Quad = Tuple[int, int, int, int]  # node indices: bottom-left, bottom-right, top-right, top-left (CCW)


@dataclass(frozen=True)
class RectangleMesh:
    """0-based node indices; exporter adds 1 for the engine file."""

    width: float
    height: float
    nx: int
    ny: int
    origin: Vec2
    nodes: List[Vec2]
    elements: List[Quad]

    @property
    def num_nodes(self) -> int:
        return len(self.nodes)

    @property
    def num_elements(self) -> int:
        return len(self.elements)


def structured_rectangle_quad(
    width: float,
    height: float,
    nx: int,
    ny: int,
    origin: Vec2 = (0.0, 0.0),
) -> RectangleMesh:
    """
    Q4 connectivity matches Engine sample: n1 bottom-left, n2 bottom-right,
    n3 top-right, n4 top-left (counter-clockwise in the x–y plane).
    """
    if nx < 1 or ny < 1:
        raise ValueError("nx and ny must be at least 1")
    if width <= 0 or height <= 0:
        raise ValueError("width and height must be positive")

    ox, oy = origin
    dx = width / nx
    dy = height / ny

    nodes: List[Vec2] = []
    for j in range(ny + 1):
        for i in range(nx + 1):
            x = ox + i * dx
            y = oy + j * dy
            nodes.append((x, y))

    elements: List[Quad] = []
    for j in range(ny):
        for i in range(nx):
            bl = j * (nx + 1) + i
            br = bl + 1
            tl = (j + 1) * (nx + 1) + i
            tr = tl + 1
            elements.append((bl, br, tr, tl))

    return RectangleMesh(
        width=width,
        height=height,
        nx=nx,
        ny=ny,
        origin=(ox, oy),
        nodes=nodes,
        elements=elements,
    )


def nodes_on_edge(mesh: RectangleMesh, edge: str) -> List[int]:
    """Return sorted 0-based node indices on the given boundary ('left'|'right'|'bottom'|'top')."""
    nx, ny = mesh.nx, mesh.ny
    edge = edge.lower()
    if edge == "left":
        return [j * (nx + 1) for j in range(ny + 1)]
    if edge == "right":
        return [j * (nx + 1) + nx for j in range(ny + 1)]
    if edge == "bottom":
        return [i for i in range(nx + 1)]
    if edge == "top":
        return [(ny) * (nx + 1) + i for i in range(nx + 1)]
    raise ValueError(f"unknown edge {edge!r}")
