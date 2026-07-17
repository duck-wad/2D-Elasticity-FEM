"""Serialize model state to Engine INPUT.txt format."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Literal, Optional, Tuple

from mesh import RectangleMesh, distributed_load_segments

Assumption = Literal["plane_stress", "plane_strain"]
DynamicMethod = Literal["average_acceleration", "linear_acceleration"]
ScalePreset = Literal["linear"]


@dataclass
class DistributedEdgeMeta:
    is_dynamic: bool = False
    scale_start: float = 0.0
    scale_end: float = 1.0
    scale_preset: ScalePreset = "linear"


@dataclass
class Material:
    slot: int
    name: str
    E: float  # Young's modulus in Pa (file units)
    nu: float
    gamma: float  # unit weight in N/m³


@dataclass
class PointLoad:
    """Engine point load; forces in N."""

    node_id: int
    fx: float
    fy: float
    load_id: int = 0
    is_dynamic: bool = False
    scale_start: float = 0.0
    scale_end: float = 1.0
    scale_preset: ScalePreset = "linear"


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
    # Boundary edge -> (tx_start, tx_end, ty_start, ty_end) kN/m
    distributed_edge_traction_knm: Dict[str, Tuple[float, float, float, float]] = field(
        default_factory=dict
    )
    distributed_edge_meta: Dict[str, DistributedEdgeMeta] = field(default_factory=dict)
    # dynamic analysis (required when any load is dynamic or user enables transient solve)
    is_dynamic: bool = False
    time_step_size: float = 0.1
    num_time_steps: int = 101
    dynamic_method: DynamicMethod = "average_acceleration"
    damping_enabled: bool = False
    damping_alpha: float = 0.0
    damping_beta: float = 0.0


def _fmt_float(x: float) -> str:
    """Round to 4 decimal places; trim trailing zeros for readability."""
    r = round(float(x), 4)
    if abs(r - int(r)) < 1e-12 and abs(r) < 1e12:
        return str(int(r))
    s = f"{r:.4f}".rstrip("0").rstrip(".")
    return s if s else "0"


def linear_scale_factors(num_steps: int, start: float, end: float) -> List[float]:
    """Scale factor at each time step (length num_steps, steps 1..num_steps)."""
    if num_steps <= 0:
        return []
    if num_steps == 1:
        return [float(end)]
    return [start + (end - start) * i / (num_steps - 1) for i in range(num_steps)]


def _assign_point_load_ids(loads: List[PointLoad]) -> None:
    for i, pl in enumerate(loads, start=1):
        pl.load_id = i


def _export_dynamic_history_by_id(
    *,
    dynamic_load_ids: List[int],
    scales_by_load: Dict[int, List[float]],
    time_step_size: float,
) -> List[str]:
    """One time: / id: block per step (all dynamic loads at that step)."""
    if not dynamic_load_ids:
        return []
    num_steps = len(next(iter(scales_by_load.values())))
    lines: List[str] = []
    for step_idx in range(num_steps):
        t = step_idx * time_step_size
        lines.append(f"  time: {_fmt_float(t)} step: {step_idx + 1}")
        for load_id in dynamic_load_ids:
            scale = scales_by_load[load_id][step_idx]
            lines.append(f"   id: {load_id} scale: {_fmt_float(scale)}")
    return lines


def _export_dynamic_history_lines(
    *,
    dynamic_loads: List[PointLoad],
    scales_by_load: Dict[int, List[float]],
    time_step_size: float,
) -> List[str]:
    return _export_dynamic_history_by_id(
        dynamic_load_ids=[pl.load_id for pl in dynamic_loads],
        scales_by_load=scales_by_load,
        time_step_size=time_step_size,
    )


def export_input_text(model: ExportModel) -> str:
    if model.mesh is None:
        raise ValueError("mesh is required before export")

    m = model.mesh
    lines: List[str] = []

    _assign_point_load_ids(model.point_loads)
    if model.is_dynamic:
        dynamic_point_loads = [pl for pl in model.point_loads if pl.is_dynamic]
    else:
        dynamic_point_loads = []
    run_dynamic = model.is_dynamic

    lines.append("general:")
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
    if run_dynamic:
        lines.append(
            f" dynamic: 1 stepsize: {_fmt_float(model.time_step_size)} "
            f"numsteps: {model.num_time_steps} method: {model.dynamic_method}"
        )
        if model.damping_enabled:
            lines.append(
                f" damping: 1 alpha: {_fmt_float(model.damping_alpha)} "
                f"beta: {_fmt_float(model.damping_beta)}"
            )
        else:
            lines.append(" damping: 0")

    lines.append("")
    lines.append("materials:")
    for mat in model.materials:
        lines.append(
            f" material {mat.slot}: {mat.name} formulation: LinearElastic "
            f"E: {_fmt_float(mat.E)} nu: {_fmt_float(mat.nu)} "
            f"gamma: {_fmt_float(mat.gamma)}"
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
    for i, pl in enumerate(model.point_loads, start=1):
        pl.load_id = i
        lines.append(
            f" id: {pl.load_id} node: {pl.node_id} "
            f"fx: {_fmt_float(pl.fx)} fy: {_fmt_float(pl.fy)}"
        )

    if run_dynamic and dynamic_point_loads:
        lines.append("")
        lines.append(f" numdynamicloads: {len(dynamic_point_loads)}")
        lines.append("")
        lines.append(" start")
        scales_by_load: Dict[int, List[float]] = {}
        for pl in dynamic_point_loads:
            if pl.scale_preset == "linear":
                scales_by_load[pl.load_id] = linear_scale_factors(
                    model.num_time_steps, pl.scale_start, pl.scale_end
                )
            else:
                raise ValueError(f"unsupported scale preset {pl.scale_preset!r}")
        lines.extend(
            _export_dynamic_history_lines(
                dynamic_loads=dynamic_point_loads,
                scales_by_load=scales_by_load,
                time_step_size=model.time_step_size,
            )
        )
        lines.append(" stop")

    dist_start_id = len(model.point_loads) + 1
    dist_segments = distributed_load_segments(
        m,
        model.distributed_edge_traction_knm,
        fmt_float=_fmt_float,
        start_load_id=dist_start_id,
    )
    dynamic_dist_ids: List[int] = []
    dist_scales_by_load: Dict[int, List[float]] = {}
    if run_dynamic:
        for seg in dist_segments:
            meta = model.distributed_edge_meta.get(seg.edge)
            if meta is None or not meta.is_dynamic:
                continue
            if meta.scale_preset == "linear":
                dist_scales_by_load[seg.load_id] = linear_scale_factors(
                    model.num_time_steps, meta.scale_start, meta.scale_end
                )
            else:
                raise ValueError(f"unsupported scale preset {meta.scale_preset!r}")
            dynamic_dist_ids.append(seg.load_id)

    if dist_segments:
        lines.append("")
        lines.append("distributed loads:")
        lines.append(f" numloads: {len(dist_segments)}")
        lines.append("")
        lines.extend(seg.file_line for seg in dist_segments)

        if dynamic_dist_ids:
            lines.append("")
            lines.append(f" numdynamicloads: {len(dynamic_dist_ids)}")
            lines.append("")
            lines.append(" start")
            lines.extend(
                _export_dynamic_history_by_id(
                    dynamic_load_ids=dynamic_dist_ids,
                    scales_by_load=dist_scales_by_load,
                    time_step_size=model.time_step_size,
                )
            )
            lines.append(" stop")

    lines.append("")
    lines.append("end")
    return "\n".join(lines) + "\n"
