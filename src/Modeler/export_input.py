"""Serialize model state to Engine INPUT.txt format."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Literal, Optional, Tuple

from mesh import RectangleMesh, distributed_load_file_lines

Assumption = Literal["plane_stress", "plane_strain"]


@dataclass
class Material:
    slot: int
    name: str
    E: float  # Young's modulus in Pa (file units)
    nu: float


@dataclass
class PointLoad:
    """1-based node id as in the file."""

    node_id: int
    fx: float
    fy: float


@dataclass
class ExportModel:
    solver: str = "CHOLESKY_DECOMP"
    solver_tolerance: float = 0.001
    solver_maxiter: int = 500
    assumption: Assumption = "plane_stress"
    thickness: float = 1.0
    materials: List[Material] = field(default_factory=list)
    mesh: Optional[RectangleMesh] = None
    default_material_name: str = "material_1"
    # 1-based node id -> (fix_x, fix_y) with 0/1
    fixities: Dict[int, Tuple[int, int]] = field(default_factory=dict)
    point_loads: List[PointLoad] = field(default_factory=list)
    debug: int = 0  # 1 = engine writes CSV under debug/
    # Boundary edge -> (tx_start, tx_end, ty_start, ty_end) kN/m; see mesh.distributed_load_file_lines
    distributed_edge_traction_knm: Dict[str, Tuple[float, float, float, float]] = field(
        default_factory=dict
    )


def _fmt_float(x: float) -> str:
    """Round to 4 decimal places; trim trailing zeros for readability."""
    r = round(float(x), 4)
    if abs(r - int(r)) < 1e-12 and abs(r) < 1e12:
        return str(int(r))
    s = f"{r:.4f}".rstrip("0").rstrip(".")
    return s if s else "0"


def export_input_text(model: ExportModel) -> str:
    if model.mesh is None:
        raise ValueError("mesh is required before export")

    m = model.mesh
    lines: List[str] = []

    lines.append("general:")
    # FileReader always parses: solver: <TYPE> tolerance: <tol> maxiter: <n>
    lines.append(
        f" solver: {model.solver} tolerance: {_fmt_float(model.solver_tolerance)} "
        f"maxiter: {model.solver_maxiter}"
    )
    if model.assumption == "plane_stress":
        lines.append(
            f" assumption: plane_stress thickness: {_fmt_float(model.thickness)}"
        )
    else:
        lines.append(" assumption: plane_strain")
    lines.append(f" debug: {int(model.debug)}")

    lines.append("")
    lines.append("materials:")
    for mat in model.materials:
        lines.append(
            f" material {mat.slot}: {mat.name} formulation: LinearElastic "
            f"E: {_fmt_float(mat.E)} nu: {_fmt_float(mat.nu)}"
        )

    lines.append("")
    lines.append("nodes:")
    lines.append(f" numnodes: {m.num_nodes}")
    lines.append("")
    for idx, (x, y) in enumerate(m.nodes):
        nid = idx + 1
        lines.append(f" node: {nid} x: {_fmt_float(x)} y: {_fmt_float(y)}")

    lines.append("")
    lines.append("elements:")
    lines.append(f" numelem: {m.num_elements} type: Q4")
    lines.append("")
    mat_name = model.default_material_name
    for eidx, (a, b, c, d) in enumerate(m.elements):
        eid = eidx + 1
        n1, n2, n3, n4 = a + 1, b + 1, c + 1, d + 1
        lines.append(
            f" element: {eid} nodes: {n1} {n2} {n3} {n4} material: {mat_name}"
        )

    lines.append("")
    lines.append("fixities:")
    fix_items = sorted(model.fixities.items(), key=lambda kv: kv[0])
    lines.append(f" numfix: {len(fix_items)}")
    lines.append("")
    for nid, (fx, fy) in fix_items:
        lines.append(f" node: {nid} x: {fx} y: {fy}")

    lines.append("")
    lines.append("point loads:")
    lines.append(f" numloads: {len(model.point_loads)}")
    lines.append("")
    for pl in model.point_loads:
        lines.append(
            f" node: {pl.node_id} fx: {_fmt_float(pl.fx)} fy: {_fmt_float(pl.fy)}"
        )

    dist_lines = distributed_load_file_lines(
        m,
        model.distributed_edge_traction_knm,
        fmt_float=_fmt_float,
    )
    if dist_lines:
        lines.append("")
        lines.append("distributed loads:")
        lines.append(f" numloads: {len(dist_lines)}")
        lines.append("")
        lines.extend(dist_lines)

    lines.append("")
    lines.append("end")
    return "\n".join(lines) + "\n"
