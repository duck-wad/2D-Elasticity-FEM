"""Structured quadrilateral mesh on an axis-aligned rectangle."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Dict, List, Sequence, Tuple

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


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def distributed_load_file_lines(
    mesh: RectangleMesh,
    edge_traction_knm: Dict[str, Tuple[float, float, float, float]],
    *,
    fmt_float: Callable[[float], str],
) -> List[str]:
    """
    Build INPUT.txt lines for the distributed loads section (no header / numloads).

    edge_traction_knm maps an edge to
      (tx_start, tx_end, ty_start, ty_end) in kN/m (force per unit length in x and y).
    Start/end follow the boundary: horizontal edges left→right; vertical bottom→top.

    Values are scaled by 1000 to N/m before writing (same convention as point kN → N).
    Engine format:
      element: <id> node1: <a> node2: <b> tx1: .. tx2: .. ty1: .. ty2: ..
    """
    nx, ny = mesh.nx, mesh.ny
    scale = 1000.0
    lines: List[str] = []

    def fmt4(tx1: float, tx2: float, ty1: float, ty2: float) -> str:
        return (
            f" tx1: {fmt_float(tx1)} tx2: {fmt_float(tx2)} "
            f"ty1: {fmt_float(ty1)} ty2: {fmt_float(ty2)}"
        )

    edge_order = ("bottom", "top", "left", "right")
    for e in edge_order:
        if e not in edge_traction_knm:
            continue
        txs, txe, tys, tye = edge_traction_knm[e]
        txs_n, txe_n = txs * scale, txe * scale
        tys_n, tye_n = tys * scale, tye * scale

        if e == "bottom":
            for i in range(nx):
                eidx = i
                a, b, c, d = mesh.elements[eidx]
                n1, n2 = a + 1, b + 1
                s0, s1 = i / nx, (i + 1) / nx
                v1x, v2x = _lerp(txs_n, txe_n, s0), _lerp(txs_n, txe_n, s1)
                v1y, v2y = _lerp(tys_n, tye_n, s0), _lerp(tys_n, tye_n, s1)
                lines.append(
                    f" element: {eidx + 1} node1: {n1} node2: {n2}" + fmt4(v1x, v2x, v1y, v2y)
                )
        elif e == "top":
            for i in range(nx):
                eidx = (ny - 1) * nx + i
                a, b, c, d = mesh.elements[eidx]
                n1, n2 = c + 1, d + 1
                s0, s1 = i / nx, (i + 1) / nx
                vx_r, vx_l = _lerp(txs_n, txe_n, s1), _lerp(txs_n, txe_n, s0)
                vy_r, vy_l = _lerp(tys_n, tye_n, s1), _lerp(tys_n, tye_n, s0)
                lines.append(
                    f" element: {eidx + 1} node1: {n1} node2: {n2}" + fmt4(vx_r, vx_l, vy_r, vy_l)
                )
        elif e == "left":
            for j in range(ny):
                eidx = j * nx
                a, b, c, d = mesh.elements[eidx]
                n1, n2 = d + 1, a + 1
                s0, s1 = j / ny, (j + 1) / ny
                vx_t, vx_b = _lerp(txs_n, txe_n, s1), _lerp(txs_n, txe_n, s0)
                vy_t, vy_b = _lerp(tys_n, tye_n, s1), _lerp(tys_n, tye_n, s0)
                lines.append(
                    f" element: {eidx + 1} node1: {n1} node2: {n2}" + fmt4(vx_t, vx_b, vy_t, vy_b)
                )
        else:  # right
            for j in range(ny):
                eidx = j * nx + (nx - 1)
                a, b, c, d = mesh.elements[eidx]
                n1, n2 = b + 1, c + 1
                s0, s1 = j / ny, (j + 1) / ny
                v1x, v2x = _lerp(txs_n, txe_n, s0), _lerp(txs_n, txe_n, s1)
                v1y, v2y = _lerp(tys_n, tye_n, s0), _lerp(tys_n, tye_n, s1)
                lines.append(
                    f" element: {eidx + 1} node1: {n1} node2: {n2}" + fmt4(v1x, v2x, v1y, v2y)
                )

    return lines
