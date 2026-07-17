"""
First-draft 2D elasticity modeler: rectangle + structured Q4 mesh, edge BCs,
point loads, export to Engine INPUT.txt format.
Run from repo:  python Modeler/main.py
Or:            cd Modeler && python main.py
"""

from __future__ import annotations

import math
import subprocess
import sys
from functools import partial
from pathlib import Path
from typing import Literal, cast

from PySide6.QtCore import QPoint, QPointF, Qt, QTimer, Signal
from PySide6.QtGui import QBrush, QColor, QPainter, QPen, QPolygonF, QWheelEvent
from PySide6.QtWidgets import (
    QApplication,
    QAbstractItemView,
    QAbstractSpinBox,
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFormLayout,
    QGridLayout,
    QGraphicsEllipseItem,
    QGraphicsLineItem,
    QGraphicsScene,
    QGraphicsView,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QSlider,
    QSpinBox,
    QSplitter,
    QTabWidget,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from engine_results import EngineResults
from export_input import (
    DistributedEdgeMeta,
    ExportModel,
    Material,
    PointLoad,
    _fmt_float,
    export_input_text,
)
from mesh import RectangleMesh, nodes_on_edge, structured_rectangle_quad
from run_engine import run_engine


class NoWheelComboBox(QComboBox):
    def wheelEvent(self, event: QWheelEvent) -> None:
        event.ignore()


class NoWheelSpinBox(QSpinBox):
    def wheelEvent(self, event: QWheelEvent) -> None:
        event.ignore()


class NoWheelDoubleSpinBox(QDoubleSpinBox):
    def wheelEvent(self, event: QWheelEvent) -> None:
        event.ignore()


def _spin_no_buttons(box: NoWheelDoubleSpinBox | NoWheelSpinBox) -> None:
    box.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)


def _scene_xy(nx: float, ny: float) -> QPointF:
    """Map model (x,y) with y up to scene coordinates (y negated for Qt)."""
    return QPointF(nx, -ny)


# Outward from the rectangle in scene coordinates (y is negated).
_EDGE_OUTWARD_SCENE: dict[str, QPointF] = {
    "bottom": QPointF(0.0, 1.0),
    "top": QPointF(0.0, -1.0),
    "left": QPointF(-1.0, 0.0),
    "right": QPointF(1.0, 0.0),
}
_EDGE_TANGENT_SCENE: dict[str, QPointF] = {
    "bottom": QPointF(1.0, 0.0),
    "top": QPointF(1.0, 0.0),
    "left": QPointF(0.0, -1.0),
    "right": QPointF(0.0, -1.0),
}


def _boundary_edges_for_node(mesh: RectangleMesh, index: int) -> list[str]:
    x, y = mesh.nodes[index]
    ox, oy = mesh.origin
    tol = min(mesh.width / mesh.nx, mesh.height / mesh.ny) * 0.15
    edges: list[str] = []
    if x - ox <= tol:
        edges.append("left")
    if ox + mesh.width - x <= tol:
        edges.append("right")
    if y - oy <= tol:
        edges.append("bottom")
    if oy + mesh.height - y <= tol:
        edges.append("top")
    return edges


def _pick_symbol_edge(
    edges: list[str],
    *,
    fix_x: bool,
    fix_y: bool,
    active_edges: set[str] | None = None,
) -> str | None:
    """Pick the boundary edge whose UI fixity applies to this node."""
    if not edges:
        return None

    if active_edges:
        candidates = [e for e in edges if e in active_edges]
    else:
        candidates = list(edges)
    if not candidates:
        candidates = list(edges)
    if len(candidates) == 1:
        return candidates[0]

    vertical = [e for e in candidates if e in ("left", "right")]
    horizontal = [e for e in candidates if e in ("bottom", "top")]

    if fix_x and fix_y:
        for e in ("bottom", "top"):
            if e in horizontal:
                return e
        for e in ("left", "right"):
            if e in vertical:
                return e
        return candidates[0]
    elif fix_y and not fix_x:
        for e in ("bottom", "top"):
            if e in horizontal:
                return e
        for e in ("left", "right"):
            if e in vertical:
                return e
    elif fix_x and not fix_y:
        for e in ("left", "right"):
            if e in vertical:
                return e
        for e in ("bottom", "top"):
            if e in horizontal:
                return e
    return candidates[0]


class MeshGraphicsView(QGraphicsView):
    nodePicked = Signal(int)  # 1-based node id, or -1 if none

    # Hit-test in viewport pixels (scene units are model-sized, ~O(1)).
    _PICK_RADIUS_PX = 14.0

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        self._mesh: RectangleMesh | None = None
        # Radius of node dots in scene coordinates (small fraction of element size).
        self._node_marker_radius_scene = 0.002
        self._panning = False
        self._pan_anchor = QPoint()
        self.setDragMode(QGraphicsView.DragMode.NoDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)

    def set_mesh(self, mesh: RectangleMesh | None) -> None:
        self._mesh = mesh
        if mesh is not None:
            dx = mesh.width / mesh.nx
            dy = mesh.height / mesh.ny
            h = min(dx, dy)
            span = max(mesh.width, mesh.height)
            self._node_marker_radius_scene = max(0.09 * h, 0.0008 * span, 1e-9)

    def node_marker_radius_scene(self) -> float:
        return self._node_marker_radius_scene

    def wheelEvent(self, event):  # noqa: N802
        factor = 1.15 if event.angleDelta().y() > 0 else 1 / 1.15
        self.scale(factor, factor)

    def mousePressEvent(self, event):  # noqa: N802
        if event.button() == Qt.MouseButton.MiddleButton:
            self._panning = True
            self._pan_anchor = event.position().toPoint()
        elif event.button() == Qt.MouseButton.LeftButton and self._mesh is not None:
            p_view = event.position().toPoint()
            best = -1
            best_d2 = self._PICK_RADIUS_PX**2
            for i, (nx, ny) in enumerate(self._mesh.nodes):
                q_scene = _scene_xy(nx, ny)
                q_view = self.mapFromScene(q_scene)
                d2 = (p_view.x() - q_view.x()) ** 2 + (p_view.y() - q_view.y()) ** 2
                if d2 < best_d2:
                    best_d2 = d2
                    best = i + 1
            self.nodePicked.emit(best)
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):  # noqa: N802
        if self._panning and event.buttons() & Qt.MouseButton.MiddleButton:
            pos = event.position().toPoint()
            delta = pos - self._pan_anchor
            self._pan_anchor = pos
            self.horizontalScrollBar().setValue(
                self.horizontalScrollBar().value() - delta.x()
            )
            self.verticalScrollBar().setValue(
                self.verticalScrollBar().value() - delta.y()
            )
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):  # noqa: N802
        if event.button() == Qt.MouseButton.MiddleButton:
            self._panning = False
        super().mouseReleaseEvent(event)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("2D Elasticity Modeler")
        self.resize(1280, 780)

        self._mesh: RectangleMesh | None = None
        self._selected_node_1based: int = -1
        self._results: EngineResults | None = None
        self._deform_base_scale: float = 1.0
        self._anim_step: int = 0
        self._anim_timer = QTimer(self)
        self._anim_timer.setInterval(50)
        self._anim_timer.timeout.connect(self._on_anim_tick)
        self._dist_edge_knm: dict[str, tuple[float, float, float, float]] = {}
        self._dist_meta: dict[str, dict[str, float | bool | str]] = {}
        # 1-based node id -> dynamic load metadata for export
        self._load_meta: dict[int, dict[str, float | bool | str]] = {}

        central = QWidget()
        self.setCentralWidget(central)
        root = QHBoxLayout(central)

        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        # —— Left: scrollable controls ——
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        scroll_host = QWidget()
        left = QVBoxLayout(scroll_host)
        left.setAlignment(Qt.AlignTop)
        left.setContentsMargins(2, 2, 6, 2)
        scroll.setWidget(scroll_host)

        gb_mesh = QGroupBox("Rectangle & mesh")
        fm = QFormLayout(gb_mesh)
        self.sp_width = NoWheelDoubleSpinBox()
        self.sp_width.setRange(1e-9, 1e9)
        self.sp_width.setDecimals(4)
        self.sp_width.setValue(1.0)
        self.sp_height = NoWheelDoubleSpinBox()
        self.sp_height.setRange(1e-9, 1e9)
        self.sp_height.setDecimals(4)
        self.sp_height.setValue(1.0)
        self.sb_nx = NoWheelSpinBox()
        self.sb_nx.setRange(1, 500)
        self.sb_nx.setValue(10)
        self.sb_ny = NoWheelSpinBox()
        self.sb_ny.setRange(1, 500)
        self.sb_ny.setValue(10)
        for sp in (
            self.sp_width,
            self.sp_height,
            self.sb_nx,
            self.sb_ny,
        ):
            _spin_no_buttons(sp)
        fm.addRow("Width", self.sp_width)
        fm.addRow("Height", self.sp_height)
        fm.addRow("Quads in X (nx)", self.sb_nx)
        fm.addRow("Quads in Y (ny)", self.sb_ny)
        btn_gen = QPushButton("Generate mesh")
        btn_gen.clicked.connect(self._generate_mesh)
        fm.addRow(btn_gen)
        left.addWidget(gb_mesh)

        gb_gen = QGroupBox("General")
        fg = QFormLayout(gb_gen)
        self.cb_solver = NoWheelComboBox()
        self.cb_solver.addItem("Cholesky Decomposition", "CHOLESKY_DECOMP")
        self.cb_solver.addItem("Conjugate Gradient", "CONJUGATE_GRADIENT")
        self.cb_solver.currentIndexChanged.connect(self._on_solver_changed)

        self._cg_params = QWidget()
        cg_form = QFormLayout(self._cg_params)
        cg_form.setContentsMargins(0, 0, 0, 0)
        self.sp_tol = NoWheelDoubleSpinBox()
        self.sp_tol.setRange(1e-20, 1e10)
        self.sp_tol.setDecimals(4)
        self.sp_tol.setValue(0.001)
        self.sb_maxiter = NoWheelSpinBox()
        self.sb_maxiter.setRange(1, 10_000_000)
        self.sb_maxiter.setValue(500)
        _spin_no_buttons(self.sp_tol)
        _spin_no_buttons(self.sb_maxiter)
        cg_form.addRow("Tolerance", self.sp_tol)
        cg_form.addRow("Max iterations", self.sb_maxiter)

        self.cb_assumption = NoWheelComboBox()
        self.cb_assumption.addItem("Plane Stress", "plane_stress")
        self.cb_assumption.addItem("Plane Strain", "plane_strain")
        fg.addRow("Linear solver", self.cb_solver)
        fg.addRow(self._cg_params)
        fg.addRow("Plane assumption", self.cb_assumption)
        self._on_solver_changed()

        self.chk_dynamic = QCheckBox("Dynamic analysis")
        self.chk_dynamic.toggled.connect(self._on_dynamic_toggled)
        fg.addRow(self.chk_dynamic)

        self._dynamic_params = QWidget()
        dyn_form = QFormLayout(self._dynamic_params)
        dyn_form.setContentsMargins(0, 0, 0, 0)
        self.sb_num_steps = NoWheelSpinBox()
        self.sb_num_steps.setRange(2, 1_000_000)
        self.sb_num_steps.setValue(101)
        self.sp_dt = NoWheelDoubleSpinBox()
        self.sp_dt.setRange(1e-12, 1e6)
        self.sp_dt.setDecimals(6)
        self.sp_dt.setValue(0.1)
        self.cb_dynamic_method = NoWheelComboBox()
        self.cb_dynamic_method.addItem("Average acceleration", "average_acceleration")
        self.cb_dynamic_method.addItem("Linear acceleration", "linear_acceleration")
        self.chk_damping = QCheckBox("Rayleigh damping")
        self.sp_damp_alpha = NoWheelDoubleSpinBox()
        self.sp_damp_alpha.setRange(0.0, 1e12)
        self.sp_damp_alpha.setDecimals(6)
        self.sp_damp_alpha.setValue(0.0)
        self.sp_damp_beta = NoWheelDoubleSpinBox()
        self.sp_damp_beta.setRange(0.0, 1e12)
        self.sp_damp_beta.setDecimals(6)
        self.sp_damp_beta.setValue(0.0)
        for sp in (
            self.sb_num_steps,
            self.sp_dt,
            self.sp_damp_alpha,
            self.sp_damp_beta,
        ):
            _spin_no_buttons(sp)
        dyn_form.addRow("Time steps", self.sb_num_steps)
        dyn_form.addRow("Step size", self.sp_dt)
        dyn_form.addRow("Integration", self.cb_dynamic_method)
        dyn_form.addRow(self.chk_damping)
        dyn_form.addRow("Alpha", self.sp_damp_alpha)
        dyn_form.addRow("Beta", self.sp_damp_beta)
        self.chk_damping.toggled.connect(self._on_damping_toggled)
        self._on_damping_toggled(False)
        fg.addRow(self._dynamic_params)
        self._dynamic_params.setVisible(False)

        left.addWidget(gb_gen)

        gb_mat = QGroupBox("Material")
        fmat = QFormLayout(gb_mat)
        self.sp_E = NoWheelDoubleSpinBox()
        self.sp_E.setRange(1.0, 1e18)
        self.sp_E.setDecimals(4)
        # 200 GPa → 200e6 kPa (export converts kPa → Pa)
        self.sp_E.setValue(200_000_000.0)
        self.sp_nu = NoWheelDoubleSpinBox()
        self.sp_nu.setRange(-0.99, 0.499999)
        self.sp_nu.setDecimals(4)
        self.sp_nu.setValue(0.3)
        self.sp_thickness = NoWheelDoubleSpinBox()
        self.sp_thickness.setRange(1e-12, 1e6)
        self.sp_thickness.setDecimals(4)
        self.sp_thickness.setValue(1.0)
        self.sp_gamma = NoWheelDoubleSpinBox()
        self.sp_gamma.setRange(0.0, 1e9)
        self.sp_gamma.setDecimals(4)
        # kN/m³ (export → N/m³); ~24 for concrete
        self.sp_gamma.setValue(24.0)
        for sp in (self.sp_E, self.sp_nu, self.sp_thickness, self.sp_gamma):
            _spin_no_buttons(sp)
        fmat.addRow("E (kPa)", self.sp_E)
        fmat.addRow("nu", self.sp_nu)
        fmat.addRow("Unit weight γ (kN/m³)", self.sp_gamma)
        fmat.addRow("Thickness (plane stress)", self.sp_thickness)
        self.cb_assumption.currentIndexChanged.connect(self._on_assumption_changed)
        self._on_assumption_changed()
        left.addWidget(gb_mat)

        gb_bc = QGroupBox("Edge fixities (1 = fixed DOF)")
        bc_layout = QVBoxLayout(gb_bc)
        bc_grid = QGridLayout()
        bc_grid.setHorizontalSpacing(16)
        bc_grid.setVerticalSpacing(6)
        bc_grid.setColumnMinimumWidth(0, 64)
        bc_grid.setColumnMinimumWidth(1, 72)
        bc_grid.setColumnMinimumWidth(2, 72)
        self._bc_checks = {}
        edges = ("left", "right", "bottom", "top")
        for row, edge in enumerate(edges):
            lab = QLabel(edge.capitalize())
            lab.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            lab.setMinimumWidth(56)
            cx = QCheckBox("Fix X")
            cy = QCheckBox("Fix Y")
            cx.toggled.connect(partial(self._on_edge_bc_changed, edge))
            cy.toggled.connect(partial(self._on_edge_bc_changed, edge))
            bc_grid.addWidget(lab, row, 0, alignment=Qt.AlignmentFlag.AlignLeft)
            bc_grid.addWidget(
                cx,
                row,
                1,
                alignment=Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
            )
            bc_grid.addWidget(
                cy,
                row,
                2,
                alignment=Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
            )
            self._bc_checks[edge] = (cx, cy)
        bc_grid.setColumnStretch(3, 1)
        bc_layout.addLayout(bc_grid)

        legend = QLabel(
            "<b>Legend</b> (checked state): "
            "<span style='color:#e53935;'>Fix X only</span> (roller, vertical track) · "
            "<span style='color:#1e88e5;'>Fix Y only</span> (roller, horizontal track) · "
            "<span style='color:#43a047;'>both fixed</span> (pin)"
        )
        legend.setTextFormat(Qt.TextFormat.RichText)
        legend.setWordWrap(True)
        legend.setStyleSheet("padding-top: 6px;")
        bc_layout.addWidget(legend)

        for edge in edges:
            self._refresh_bc_checkbox_styles(edge)
        left.addWidget(gb_bc)

        gb_loads = QGroupBox("Loads")
        loads_outer = QVBoxLayout(gb_loads)
        load_tabs = QTabWidget()

        # —— Point loads tab ——
        point_tab = QWidget()
        pt = QVBoxLayout(point_tab)
        pt.setContentsMargins(4, 8, 4, 4)
        pt.setSpacing(8)

        self.lbl_pick = QLabel("Selected node: (click mesh)")
        pt.addWidget(self.lbl_pick)

        load_form = QFormLayout()
        load_form.setFieldGrowthPolicy(
            QFormLayout.FieldGrowthPolicy.ExpandingFieldsGrow
        )
        self.sp_lfx = NoWheelDoubleSpinBox()
        self.sp_lfx.setRange(-1e20, 1e20)
        self.sp_lfx.setDecimals(4)
        self.sp_lfx.setValue(0.0)
        self.sp_lfy = NoWheelDoubleSpinBox()
        self.sp_lfy.setRange(-1e20, 1e20)
        self.sp_lfy.setDecimals(4)
        self.sp_lfy.setValue(0.0)
        _spin_no_buttons(self.sp_lfx)
        _spin_no_buttons(self.sp_lfy)
        load_form.addRow("fx (kN)", self.sp_lfx)
        load_form.addRow("fy (kN)", self.sp_lfy)
        pt.addLayout(load_form)

        self.chk_dynamic_load = QCheckBox("Dynamic load (time-varying scale)")
        self.chk_dynamic_load.toggled.connect(self._on_dynamic_load_toggled)
        pt.addWidget(self.chk_dynamic_load)

        self._dynamic_load_opts = QWidget()
        dyn_form = QFormLayout(self._dynamic_load_opts)
        dyn_form.setContentsMargins(12, 0, 0, 0)
        self.cb_scale_preset = NoWheelComboBox()
        self.cb_scale_preset.addItem("Linear scale", "linear")
        self.sp_scale_start = NoWheelDoubleSpinBox()
        self.sp_scale_start.setRange(-1e12, 1e12)
        self.sp_scale_start.setDecimals(4)
        self.sp_scale_start.setValue(0.0)
        self.sp_scale_end = NoWheelDoubleSpinBox()
        self.sp_scale_end.setRange(-1e12, 1e12)
        self.sp_scale_end.setDecimals(4)
        self.sp_scale_end.setValue(1.0)
        for sp in (self.sp_scale_start, self.sp_scale_end):
            _spin_no_buttons(sp)
        scale_pair = QWidget()
        scale_h = QHBoxLayout(scale_pair)
        scale_h.setContentsMargins(0, 0, 0, 0)
        scale_h.addWidget(QLabel("start"))
        scale_h.addWidget(self.sp_scale_start)
        scale_h.addWidget(QLabel("end"))
        scale_h.addWidget(self.sp_scale_end)
        dyn_form.addRow("Preset", self.cb_scale_preset)
        dyn_form.addRow("Scale", scale_pair)
        pt.addWidget(self._dynamic_load_opts)
        self._on_dynamic_load_toggled(False)

        btn_add_load = QPushButton("Apply to selected node")
        btn_add_load.clicked.connect(self._add_point_load)
        pt.addWidget(btn_add_load)

        self.tbl_loads = QTableWidget(0, 5)
        self.tbl_loads.setHorizontalHeaderLabels(["ID", "Node", "fx", "fy", "Scale"])
        self.tbl_loads.setMinimumHeight(110)
        self.tbl_loads.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
        )
        self.tbl_loads.verticalHeader().setVisible(False)
        self.tbl_loads.verticalHeader().setDefaultSectionSize(24)
        self.tbl_loads.setSelectionBehavior(
            QAbstractItemView.SelectionBehavior.SelectRows
        )
        self.tbl_loads.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        hdr = self.tbl_loads.horizontalHeader()
        hdr.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        hdr.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        hdr.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        hdr.setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch)
        hdr.setSectionResizeMode(4, QHeaderView.ResizeMode.Stretch)
        self.tbl_loads.itemSelectionChanged.connect(self._on_load_row_selected)
        pt.addWidget(self.tbl_loads, stretch=1)

        btn_rm_load = QPushButton("Remove selected row")
        btn_rm_load.clicked.connect(self._remove_load_row)
        pt.addWidget(btn_rm_load)

        load_tabs.addTab(point_tab, "Point")

        # —— Distributed loads tab ——
        dist_tab = QWidget()
        dist_layout = QVBoxLayout(dist_tab)
        dist_layout.setContentsMargins(4, 8, 4, 4)
        dist_layout.setSpacing(8)

        dist_hint = QLabel(
            "Edge traction in kN/m (exported as N/m). "
            "Start/end follow the edge: bottom/top left→right; "
            "left/right bottom→top."
        )
        dist_hint.setWordWrap(True)
        dist_hint.setStyleSheet("color: #555;")
        dist_layout.addWidget(dist_hint)

        dist_grid = QGridLayout()
        dist_grid.setHorizontalSpacing(8)
        dist_grid.setVerticalSpacing(6)
        self.cb_dist_edge = NoWheelComboBox()
        for e in ("bottom", "top", "left", "right"):
            self.cb_dist_edge.addItem(e.capitalize(), e)
        self.sp_dtx_s = NoWheelDoubleSpinBox()
        self.sp_dtx_e = NoWheelDoubleSpinBox()
        self.sp_dty_s = NoWheelDoubleSpinBox()
        self.sp_dty_e = NoWheelDoubleSpinBox()
        for sp in (self.sp_dtx_s, self.sp_dtx_e, self.sp_dty_s, self.sp_dty_e):
            sp.setRange(-1e20, 1e20)
            sp.setDecimals(4)
            sp.setValue(0.0)
            _spin_no_buttons(sp)
        dist_grid.addWidget(QLabel("Edge"), 0, 0)
        dist_grid.addWidget(self.cb_dist_edge, 0, 1, 1, 3)
        dist_grid.addWidget(QLabel("Tx start"), 1, 0)
        dist_grid.addWidget(self.sp_dtx_s, 1, 1)
        dist_grid.addWidget(QLabel("Tx end"), 1, 2)
        dist_grid.addWidget(self.sp_dtx_e, 1, 3)
        dist_grid.addWidget(QLabel("Ty start"), 2, 0)
        dist_grid.addWidget(self.sp_dty_s, 2, 1)
        dist_grid.addWidget(QLabel("Ty end"), 2, 2)
        dist_grid.addWidget(self.sp_dty_e, 2, 3)
        dist_grid.setColumnStretch(1, 1)
        dist_grid.setColumnStretch(3, 1)
        dist_layout.addLayout(dist_grid)

        self.chk_dist_dynamic = QCheckBox("Dynamic load (time-varying scale)")
        self.chk_dist_dynamic.toggled.connect(self._on_dist_dynamic_toggled)
        dist_layout.addWidget(self.chk_dist_dynamic)

        self._dist_dynamic_opts = QWidget()
        dist_dyn_form = QFormLayout(self._dist_dynamic_opts)
        dist_dyn_form.setContentsMargins(12, 0, 0, 0)
        self.cb_dist_scale_preset = NoWheelComboBox()
        self.cb_dist_scale_preset.addItem("Linear scale", "linear")
        self.sp_dist_scale_start = NoWheelDoubleSpinBox()
        self.sp_dist_scale_start.setRange(-1e12, 1e12)
        self.sp_dist_scale_start.setDecimals(4)
        self.sp_dist_scale_start.setValue(0.0)
        self.sp_dist_scale_end = NoWheelDoubleSpinBox()
        self.sp_dist_scale_end.setRange(-1e12, 1e12)
        self.sp_dist_scale_end.setDecimals(4)
        self.sp_dist_scale_end.setValue(1.0)
        for sp in (self.sp_dist_scale_start, self.sp_dist_scale_end):
            _spin_no_buttons(sp)
        dist_scale_pair = QWidget()
        dist_scale_h = QHBoxLayout(dist_scale_pair)
        dist_scale_h.setContentsMargins(0, 0, 0, 0)
        dist_scale_h.addWidget(QLabel("start"))
        dist_scale_h.addWidget(self.sp_dist_scale_start)
        dist_scale_h.addWidget(QLabel("end"))
        dist_scale_h.addWidget(self.sp_dist_scale_end)
        dist_dyn_form.addRow("Preset", self.cb_dist_scale_preset)
        dist_dyn_form.addRow("Scale", dist_scale_pair)
        dist_layout.addWidget(self._dist_dynamic_opts)
        self._on_dist_dynamic_toggled(False)

        hb_d = QHBoxLayout()
        btn_dist_apply = QPushButton("Apply to edge")
        btn_dist_apply.clicked.connect(self._dist_apply_edge)
        hb_d.addWidget(btn_dist_apply)
        dist_layout.addLayout(hb_d)

        self.tbl_dist = QTableWidget(0, 6)
        self.tbl_dist.setHorizontalHeaderLabels(
            ["Edge", "Tx s", "Tx e", "Ty s", "Ty e", "Scale"]
        )
        self.tbl_dist.setMinimumHeight(110)
        self.tbl_dist.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
        )
        self.tbl_dist.verticalHeader().setVisible(False)
        self.tbl_dist.verticalHeader().setDefaultSectionSize(24)
        self.tbl_dist.setSelectionBehavior(
            QAbstractItemView.SelectionBehavior.SelectRows
        )
        self.tbl_dist.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        dist_hdr = self.tbl_dist.horizontalHeader()
        dist_hdr.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        for col in range(1, 6):
            dist_hdr.setSectionResizeMode(col, QHeaderView.ResizeMode.Stretch)
        self.tbl_dist.itemSelectionChanged.connect(self._on_dist_row_selected)
        dist_layout.addWidget(self.tbl_dist, stretch=1)

        dist_btn_row = QHBoxLayout()
        btn_rm_dist = QPushButton("Remove selected row")
        btn_rm_dist.clicked.connect(self._dist_remove_row)
        btn_dist_clear = QPushButton("Clear all")
        btn_dist_clear.clicked.connect(self._dist_clear_all)
        dist_btn_row.addWidget(btn_rm_dist)
        dist_btn_row.addWidget(btn_dist_clear)
        dist_layout.addLayout(dist_btn_row)

        load_tabs.addTab(dist_tab, "Distributed")
        load_tabs.setMinimumHeight(320)
        loads_outer.addWidget(load_tabs)
        left.addWidget(gb_loads)

        gb_results = QGroupBox("Results")
        fr = QFormLayout(gb_results)
        self.chk_engine_debug = QCheckBox("Engine debug output")
        self.chk_show_deformed = QCheckBox("Show deformed shape")
        self.chk_show_deformed.setEnabled(False)
        self.sl_deform_scale = QSlider(Qt.Orientation.Horizontal)
        self.sl_deform_scale.setRange(1, 500)
        self.sl_deform_scale.setValue(100)
        self.sl_deform_scale.setEnabled(False)
        self.lbl_deform_scale = QLabel("Deformation scale: 100%")
        self.lbl_deform_scale.setEnabled(False)
        fr.addRow(self.chk_engine_debug)
        fr.addRow(self.chk_show_deformed)
        fr.addRow(self.lbl_deform_scale, self.sl_deform_scale)
        self.chk_show_deformed.toggled.connect(self._on_deform_display_changed)
        self.sl_deform_scale.valueChanged.connect(self._on_deform_slider_changed)

        self.chk_animate = QCheckBox("Animate (dynamic)")
        self.chk_animate.setEnabled(False)
        self.chk_animate.toggled.connect(self._on_animate_toggled)
        fr.addRow(self.chk_animate)

        self._anim_controls = QWidget()
        anim_form = QFormLayout(self._anim_controls)
        anim_form.setContentsMargins(0, 0, 0, 0)
        self.btn_anim_play = QPushButton("Play")
        self.btn_anim_play.setCheckable(True)
        self.btn_anim_play.toggled.connect(self._on_anim_play_toggled)
        self.sl_anim_step = QSlider(Qt.Orientation.Horizontal)
        self.sl_anim_step.setRange(0, 0)
        self.sl_anim_step.setEnabled(False)
        self.sl_anim_step.valueChanged.connect(self._on_anim_step_changed)
        self.lbl_anim_step = QLabel("Step: —")
        self.cb_anim_speed = NoWheelComboBox()
        self.cb_anim_speed.addItem("Low", 250)
        self.cb_anim_speed.addItem("Medium", 100)
        self.cb_anim_speed.addItem("High", 40)
        self.cb_anim_speed.setCurrentIndex(1)
        self.cb_anim_speed.currentIndexChanged.connect(self._on_anim_speed_changed)
        anim_form.addRow(self.btn_anim_play)
        anim_form.addRow(self.lbl_anim_step, self.sl_anim_step)
        anim_form.addRow("Speed", self.cb_anim_speed)
        self._anim_controls.setVisible(False)
        fr.addRow(self._anim_controls)

        left.addWidget(gb_results)

        btn_export = QPushButton("Compute")
        btn_export.clicked.connect(self._export_file)
        left.addWidget(btn_export)
        left.addStretch()

        splitter.addWidget(scroll)

        # —— Right: graphics ——
        self.scene = QGraphicsScene(self)
        self.view = MeshGraphicsView()
        self.view.setScene(self.scene)
        self.view.nodePicked.connect(self._on_node_picked)
        splitter.addWidget(self.view)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([420, 900])
        self.view.setBackgroundBrush(QBrush(QColor(252, 252, 255)))

        hint = QLabel(
            "View: left-click node to select · middle-drag pan · wheel zoom · +Y upward"
        )
        hint.setStyleSheet("color: #555; padding: 4px;")
        self.statusBar().addPermanentWidget(hint)
        self.menuBar().setVisible(False)
        self._update_dynamic_load_controls_enabled()

    def _solver_token(self) -> str:
        d = self.cb_solver.currentData()
        return str(d) if d else "CHOLESKY_DECOMP"

    def _assumption_token(self) -> str:
        d = self.cb_assumption.currentData()
        return str(d) if d else "plane_stress"

    def _on_assumption_changed(self) -> None:
        ps = self._assumption_token() == "plane_stress"
        self.sp_thickness.setEnabled(ps)

    def _on_solver_changed(self) -> None:
        cg = self._solver_token() == "CONJUGATE_GRADIENT"
        self._cg_params.setVisible(cg)

    def _on_dynamic_toggled(self, checked: bool) -> None:
        self._dynamic_params.setVisible(checked)
        self._update_dynamic_load_controls_enabled()

    def _update_dynamic_load_controls_enabled(self) -> None:
        analysis_on = self.chk_dynamic.isChecked()
        self.chk_dynamic_load.setEnabled(analysis_on)
        self.chk_dist_dynamic.setEnabled(analysis_on)
        if not analysis_on:
            self._remove_dynamic_loads()
            self.chk_dynamic_load.blockSignals(True)
            self.chk_dynamic_load.setChecked(False)
            self.chk_dynamic_load.blockSignals(False)
            self.chk_dist_dynamic.blockSignals(True)
            self.chk_dist_dynamic.setChecked(False)
            self.chk_dist_dynamic.blockSignals(False)
        self._on_dynamic_load_toggled(analysis_on and self.chk_dynamic_load.isChecked())
        self._on_dist_dynamic_toggled(analysis_on and self.chk_dist_dynamic.isChecked())

    def _remove_dynamic_loads(self) -> None:
        """Remove loads marked dynamic from the tables; keep static loads."""
        rows_to_remove: list[int] = []
        for r in range(self.tbl_loads.rowCount()):
            it = self.tbl_loads.item(r, 1)
            if it is None:
                continue
            try:
                nid = int(it.text())
            except ValueError:
                continue
            if self._load_meta.get(nid, {}).get("is_dynamic"):
                rows_to_remove.append(r)
                self._load_meta.pop(nid, None)
        for r in reversed(rows_to_remove):
            self.tbl_loads.removeRow(r)
        self._renumber_point_load_ids()

        edges_to_remove = [
            e
            for e in list(self._dist_edge_knm)
            if self._dist_meta.get(e, {}).get("is_dynamic")
        ]
        for edge in edges_to_remove:
            self._dist_edge_knm.pop(edge, None)
            self._dist_meta.pop(edge, None)
        self._dist_refresh_table()
        self._redraw_scene(fit_view=False)

    def _on_damping_toggled(self, checked: bool) -> None:
        self.sp_damp_alpha.setEnabled(checked)
        self.sp_damp_beta.setEnabled(checked)

    def _on_dynamic_load_toggled(self, checked: bool) -> None:
        enabled = self.chk_dynamic.isChecked() and checked
        self._dynamic_load_opts.setVisible(enabled)
        self.cb_scale_preset.setEnabled(enabled)
        self.sp_scale_start.setEnabled(enabled)
        self.sp_scale_end.setEnabled(enabled)

    def _on_dist_dynamic_toggled(self, checked: bool) -> None:
        enabled = self.chk_dynamic.isChecked() and checked
        self._dist_dynamic_opts.setVisible(enabled)
        self.cb_dist_scale_preset.setEnabled(enabled)
        self.sp_dist_scale_start.setEnabled(enabled)
        self.sp_dist_scale_end.setEnabled(enabled)

    def _dynamic_method_token(self) -> str:
        d = self.cb_dynamic_method.currentData()
        return str(d) if d else "average_acceleration"

    def _load_dynamic_label(self, nid: int) -> str:
        meta = self._load_meta.get(nid)
        if not meta or not meta.get("is_dynamic"):
            return "static"
        s0 = float(meta.get("scale_start", 0.0))
        s1 = float(meta.get("scale_end", 1.0))
        return f"{_fmt_float(s0)}→{_fmt_float(s1)}"

    def _dist_dynamic_label(self, edge: str) -> str:
        meta = self._dist_meta.get(edge)
        if not meta or not meta.get("is_dynamic"):
            return "static"
        s0 = float(meta.get("scale_start", 0.0))
        s1 = float(meta.get("scale_end", 1.0))
        return f"{_fmt_float(s0)}→{_fmt_float(s1)}"

    def _on_load_row_selected(self) -> None:
        rows = self.tbl_loads.selectionModel().selectedRows()
        if len(rows) != 1:
            return
        r = rows[0].row()
        try:
            nid = int(self.tbl_loads.item(r, 1).text())
            fx = float(self.tbl_loads.item(r, 2).text())
            fy = float(self.tbl_loads.item(r, 3).text())
        except (AttributeError, TypeError, ValueError):
            return
        self._selected_node_1based = nid
        self.lbl_pick.setText(f"Selected node: {nid}")
        self.sp_lfx.setValue(fx)
        self.sp_lfy.setValue(fy)
        meta = self._load_meta.get(nid, {})
        is_dyn = bool(meta.get("is_dynamic", False)) and self.chk_dynamic.isChecked()
        self.chk_dynamic_load.blockSignals(True)
        self.chk_dynamic_load.setChecked(is_dyn)
        self.chk_dynamic_load.blockSignals(False)
        self._on_dynamic_load_toggled(is_dyn)
        if is_dyn:
            self.sp_scale_start.setValue(float(meta.get("scale_start", 0.0)))
            self.sp_scale_end.setValue(float(meta.get("scale_end", 1.0)))
            preset = str(meta.get("scale_preset", "linear"))
            idx = self.cb_scale_preset.findData(preset)
            if idx >= 0:
                self.cb_scale_preset.setCurrentIndex(idx)
        self._redraw_scene(fit_view=False)

    def _on_edge_bc_changed(self, edge: str, _checked: bool) -> None:
        self._refresh_bc_checkbox_styles(edge)
        self._rebuild_fixities_preview()

    @staticmethod
    def _bc_checkbox_style(x_on: bool, y_on: bool, *, is_x: bool) -> str:
        if is_x and not x_on:
            return ""
        if not is_x and not y_on:
            return ""
        if x_on and y_on:
            return "QCheckBox { color: #43a047; font-weight: 600; }"
        if is_x:
            return "QCheckBox { color: #e53935; font-weight: 600; }"
        return "QCheckBox { color: #1e88e5; font-weight: 600; }"

    def _refresh_bc_checkbox_styles(self, edge: str) -> None:
        cx, cy = self._bc_checks[edge]
        x_on, y_on = cx.isChecked(), cy.isChecked()
        cx.setStyleSheet(self._bc_checkbox_style(x_on, y_on, is_x=True))
        cy.setStyleSheet(self._bc_checkbox_style(x_on, y_on, is_x=False))

    def _on_node_picked(self, node_1based: int) -> None:
        self._selected_node_1based = node_1based
        if node_1based > 0:
            self.lbl_pick.setText(f"Selected node: {node_1based}")
        else:
            self.lbl_pick.setText("Selected node: (click mesh)")
        self._redraw_scene(fit_view=False)

    def _active_bc_edges(self) -> set[str]:
        """Edges with at least one fixity checkbox enabled in the UI."""
        active: set[str] = set()
        for edge, (cx, cy) in self._bc_checks.items():
            if cx.isChecked() or cy.isChecked():
                active.add(edge)
        return active

    def _collect_fixities(self) -> dict[int, tuple[int, int]]:
        out: dict[int, tuple[int, int]] = {}
        if self._mesh is None:
            return out

        def merge(nid: int, fx: int, fy: int) -> None:
            px, py = out.get(nid, (0, 0))
            out[nid] = (max(px, fx), max(py, fy))

        for edge, (cx, cy) in self._bc_checks.items():
            fx = 1 if cx.isChecked() else 0
            fy = 1 if cy.isChecked() else 0
            if fx == 0 and fy == 0:
                continue
            for idx in nodes_on_edge(self._mesh, edge):
                merge(idx + 1, fx, fy)
        return out

    def _rebuild_fixities_preview(self) -> None:
        self._redraw_scene(fit_view=False)

    def _generate_mesh(self) -> None:
        try:
            self._mesh = structured_rectangle_quad(
                self.sp_width.value(),
                self.sp_height.value(),
                self.sb_nx.value(),
                self.sb_ny.value(),
            )
        except ValueError as e:
            QMessageBox.warning(self, "Mesh", str(e))
            return
        self._clear_results()
        self._dist_edge_knm.clear()
        self._dist_meta.clear()
        self._dist_refresh_table()
        self.view.set_mesh(self._mesh)
        self._redraw_scene(fit_view=True)
        self._sync_load_table_after_mesh_change()

    def _clear_results(self) -> None:
        self._stop_animation()
        self._results = None
        self._anim_step = 0
        self.chk_show_deformed.setChecked(False)
        self.chk_show_deformed.setEnabled(False)
        self.sl_deform_scale.setEnabled(False)
        self.lbl_deform_scale.setEnabled(False)
        self.chk_animate.setChecked(False)
        self.chk_animate.setEnabled(False)
        self._anim_controls.setVisible(False)
        self.sl_anim_step.setEnabled(False)
        self.sl_anim_step.setRange(0, 0)
        self.lbl_anim_step.setText("Step: —")

    def _deform_scale_factor(self) -> float:
        return self._deform_base_scale * (self.sl_deform_scale.value() / 100.0)

    def _on_deform_display_changed(self, _checked: bool) -> None:
        self._redraw_scene(fit_view=False)

    def _on_deform_slider_changed(self, value: int) -> None:
        self.lbl_deform_scale.setText(f"Deformation scale: {value}%")
        if self.chk_show_deformed.isChecked():
            self._redraw_scene(fit_view=False)

    def _current_frame_displacements(self) -> tuple[list[float], list[float]] | None:
        if self._results is None:
            return None
        if self.chk_animate.isChecked() and self._results.has_animation:
            return self._results.frame_displacements(self._anim_step)
        return self._results.displacements_x, self._results.displacements_y

    def _node_model_xy(self, index: int, *, deformed: bool) -> tuple[float, float]:
        assert self._mesh is not None
        nx, ny = self._mesh.nodes[index]
        if not deformed:
            return nx, ny
        frame = self._current_frame_displacements()
        if frame is None:
            return nx, ny
        ux, uy = frame
        scale = self._deform_scale_factor()
        return nx + scale * ux[index], ny + scale * uy[index]

    def _apply_results(self, results: EngineResults) -> None:
        if self._mesh is None:
            return
        results.validate_mesh_nodes(self._mesh.num_nodes)
        self._stop_animation()
        self._results = results
        max_u = results.max_magnitude()
        span = max(self._mesh.width, self._mesh.height)
        if max_u > 1e-20:
            self._deform_base_scale = 0.15 * span / max_u
        else:
            self._deform_base_scale = 1.0
        self.sl_deform_scale.setValue(100)
        self.lbl_deform_scale.setText("Deformation scale: 100%")
        self.chk_show_deformed.setEnabled(True)
        self.sl_deform_scale.setEnabled(True)
        self.lbl_deform_scale.setEnabled(True)
        self.chk_show_deformed.setChecked(True)

        can_anim = results.has_animation
        self.chk_animate.setEnabled(can_anim)
        if can_anim:
            n = len(results.displacement_history_x)
            self.sl_anim_step.setRange(0, n - 1)
            self._anim_step = n - 1
            self.sl_anim_step.blockSignals(True)
            self.sl_anim_step.setValue(self._anim_step)
            self.sl_anim_step.blockSignals(False)
            self._update_anim_step_label()
            self.chk_animate.setChecked(True)
        else:
            self.chk_animate.setChecked(False)
            self._anim_controls.setVisible(False)

        self._redraw_scene(fit_view=False)

    def _stop_animation(self) -> None:
        self._anim_timer.stop()
        self.btn_anim_play.blockSignals(True)
        self.btn_anim_play.setChecked(False)
        self.btn_anim_play.setText("Play")
        self.btn_anim_play.blockSignals(False)

    def _on_animate_toggled(self, checked: bool) -> None:
        self._anim_controls.setVisible(checked and self.chk_animate.isEnabled())
        self.sl_anim_step.setEnabled(checked)
        if not checked:
            self._stop_animation()
        if self.chk_show_deformed.isChecked():
            self._redraw_scene(fit_view=False)

    def _on_anim_play_toggled(self, playing: bool) -> None:
        if playing:
            if self._results is None or not self._results.has_animation:
                self._stop_animation()
                return
            if not self.chk_animate.isChecked():
                self.chk_animate.setChecked(True)
            if not self.chk_show_deformed.isChecked():
                self.chk_show_deformed.setChecked(True)
            self.btn_anim_play.setText("Pause")
            self._on_anim_speed_changed()
            # restart from beginning if at end
            last = self.sl_anim_step.maximum()
            if self._anim_step >= last:
                self._set_anim_step(0)
            self._anim_timer.start()
        else:
            self._anim_timer.stop()
            self.btn_anim_play.setText("Play")

    def _on_anim_speed_changed(self, *_args) -> None:
        interval_ms = self.cb_anim_speed.currentData()
        if interval_ms is None:
            interval_ms = 100
        self._anim_timer.setInterval(int(interval_ms))

    def _on_anim_step_changed(self, step: int) -> None:
        self._anim_step = int(step)
        self._update_anim_step_label()
        if self.chk_show_deformed.isChecked():
            self._redraw_scene(fit_view=False)

    def _set_anim_step(self, step: int) -> None:
        self.sl_anim_step.blockSignals(True)
        self.sl_anim_step.setValue(step)
        self.sl_anim_step.blockSignals(False)
        self._on_anim_step_changed(step)

    def _update_anim_step_label(self) -> None:
        if self._results is None or not self._results.has_animation:
            self.lbl_anim_step.setText("Step: —")
            return
        n = len(self._results.displacement_history_x)
        t = self._anim_step * self._results.time_step_size
        self.lbl_anim_step.setText(
            f"Step: {self._anim_step + 1}/{n}  t={_fmt_float(t)}"
        )

    def _on_anim_tick(self) -> None:
        if self._results is None or not self._results.has_animation:
            self._stop_animation()
            return
        last = self.sl_anim_step.maximum()
        nxt = self._anim_step + 1
        if nxt > last:
            nxt = 0  # loop
        self._set_anim_step(nxt)

    def _sync_load_table_after_mesh_change(self) -> None:
        if self._mesh is None:
            return
        nmax = self._mesh.num_nodes
        rows_to_remove = []
        for r in range(self.tbl_loads.rowCount()):
            it = self.tbl_loads.item(r, 1)
            if it is None:
                continue
            try:
                nid = int(it.text())
            except ValueError:
                rows_to_remove.append(r)
                continue
            if nid < 1 or nid > nmax:
                rows_to_remove.append(r)
        for r in reversed(rows_to_remove):
            it = self.tbl_loads.item(r, 1)
            if it is not None:
                try:
                    self._load_meta.pop(int(it.text()), None)
                except ValueError:
                    pass
            self.tbl_loads.removeRow(r)
        self._renumber_point_load_ids()

    def _renumber_point_load_ids(self) -> None:
        for r in range(self.tbl_loads.rowCount()):
            self.tbl_loads.setItem(r, 0, QTableWidgetItem(str(r + 1)))

    def _redraw_scene(self, *, fit_view: bool = False) -> None:
        self.scene.clear()
        if self._mesh is None:
            return

        m = self._mesh
        show_deformed = self.chk_show_deformed.isChecked() and self._results is not None

        pen_edge = QPen(QColor(60, 60, 80))
        pen_edge.setCosmetic(True)
        pen_edge.setWidthF(1.0)
        brush_el = QBrush(QColor(230, 235, 245, 80))
        if show_deformed:
            pen_edge.setColor(QColor(170, 175, 190))
            brush_el = QBrush(QColor(230, 235, 245, 40))

        pen_def = QPen(QColor(210, 90, 40))
        pen_def.setCosmetic(True)
        pen_def.setWidthF(1.6)
        brush_def = QBrush(QColor(255, 200, 160, 50))
        fixities = self._collect_fixities()
        active_bc_edges = self._active_bc_edges()

        for a, b, c, d in m.elements:
            if show_deformed:
                poly_u = QPolygonF(
                    [
                        _scene_xy(*self._node_model_xy(a, deformed=False)),
                        _scene_xy(*self._node_model_xy(b, deformed=False)),
                        _scene_xy(*self._node_model_xy(c, deformed=False)),
                        _scene_xy(*self._node_model_xy(d, deformed=False)),
                    ]
                )
                gi_u = self.scene.addPolygon(poly_u, pen_edge, brush_el)
                gi_u.setZValue(0)

            poly = QPolygonF(
                [
                    _scene_xy(*self._node_model_xy(a, deformed=show_deformed)),
                    _scene_xy(*self._node_model_xy(b, deformed=show_deformed)),
                    _scene_xy(*self._node_model_xy(c, deformed=show_deformed)),
                    _scene_xy(*self._node_model_xy(d, deformed=show_deformed)),
                ]
            )
            if show_deformed:
                gi = self.scene.addPolygon(poly, pen_def, brush_def)
                gi.setZValue(1)
            else:
                gi = self.scene.addPolygon(poly, pen_edge, brush_el)
                gi.setZValue(0)

        for i, (nx, ny) in enumerate(m.nodes):
            nid = i + 1
            mx, my = self._node_model_xy(i, deformed=show_deformed)
            p = _scene_xy(mx, my)
            fix_x, fix_y = fixities.get(nid, (0, 0))
            selected = nid == self._selected_node_1based
            if selected:
                color = QColor(255, 214, 10)  # selection overrides fixity color
                z = 4
            elif fix_x and fix_y:
                color = QColor(0x43, 0xA0, 0x47)  # both — match legend green
                z = 3
            elif fix_x:
                color = QColor(0xE5, 0x39, 0x35)  # Fix X only — red
                z = 3
            elif fix_y:
                color = QColor(0x1E, 0x88, 0xE5)  # Fix Y only — blue
                z = 3
            else:
                color = QColor(40, 40, 50)
                z = 2
            r = self.view.node_marker_radius_scene()
            el = QGraphicsEllipseItem(p.x() - r, p.y() - r, 2 * r, 2 * r)
            el.setBrush(QBrush(color))
            if selected:
                pen = QPen(QColor(30, 30, 30))
                pen.setWidthF(1.0)
                pen.setCosmetic(True)
                el.setPen(pen)
            else:
                el.setPen(QPen(Qt.PenStyle.NoPen))
            el.setZValue(z)
            self.scene.addItem(el)

            if fix_x or fix_y:
                sym_pen = QPen(QColor(35, 35, 45))
                sym_pen.setCosmetic(True)
                sym_pen.setWidthF(1.25)
                sym_brush = QBrush(QColor(252, 252, 255))
                sym_z = z + 0.5
                sym_size = max(r * 3.4, r + 1e-9)
                edges = _boundary_edges_for_node(m, i)
                edge = _pick_symbol_edge(
                    edges,
                    fix_x=bool(fix_x),
                    fix_y=bool(fix_y),
                    active_edges=active_bc_edges,
                )
                if edge is not None:
                    if fix_x and fix_y:
                        self._draw_pin_symbol(
                            p, edge, sym_size, sym_pen, sym_brush, sym_z
                        )
                    elif fix_y:
                        self._draw_roller_symbol(
                            p,
                            edge,
                            fixed_dof="y",
                            boundary_edges=edges,
                            size=sym_size,
                            pen=sym_pen,
                            brush=sym_brush,
                            z=sym_z,
                        )
                    else:
                        self._draw_roller_symbol(
                            p,
                            edge,
                            fixed_dof="x",
                            boundary_edges=edges,
                            size=sym_size,
                            pen=sym_pen,
                            brush=sym_brush,
                            z=sym_z,
                        )

        self._draw_point_load_arrows(show_deformed)

        pad = max(m.width, m.height) * 0.05 + 1e-6
        if show_deformed:
            xs = [self._node_model_xy(i, deformed=True)[0] for i in range(m.num_nodes)]
            ys = [self._node_model_xy(i, deformed=True)[1] for i in range(m.num_nodes)]
            x0, x1 = min(0.0, min(xs)) - pad, max(m.width, max(xs)) + pad
            y0, y1 = min(0.0, min(ys)) - pad, max(m.height, max(ys)) + pad
            self.scene.setSceneRect(x0, -y1, x1 - x0, y1 - y0)
        else:
            self.scene.setSceneRect(
                -pad, -m.height - pad, m.width + 2 * pad, m.height + 2 * pad
            )
        if fit_view:
            self.view.resetTransform()
            self.view.fitInView(
                self.scene.sceneRect(), Qt.AspectRatioMode.KeepAspectRatio
            )

    def _collect_point_loads_display(self) -> list[tuple[int, float, float]]:
        loads: list[tuple[int, float, float]] = []
        for r in range(self.tbl_loads.rowCount()):
            try:
                nid = int(self.tbl_loads.item(r, 1).text())
                fx = float(self.tbl_loads.item(r, 2).text())
                fy = float(self.tbl_loads.item(r, 3).text())
            except (AttributeError, TypeError, ValueError):
                continue
            loads.append((nid, fx, fy))
        return loads

    def _draw_point_load_arrows(self, show_deformed: bool) -> None:
        if self._mesh is None:
            return
        loads = self._collect_point_loads_display()
        if not loads:
            return

        mags = [math.hypot(fx, fy) for _, fx, fy in loads]
        max_mag = max(mags)
        if max_mag < 1e-12:
            return

        span = max(self._mesh.width, self._mesh.height)
        ref_len = 0.075 * span
        min_len = 0.028 * span
        pen = QPen(QColor(210, 70, 45))
        pen.setCosmetic(True)
        pen.setWidthF(1.8)
        brush = QBrush(QColor(210, 70, 45))

        for nid, fx, fy in loads:
            mag = math.hypot(fx, fy)
            if mag < 1e-12:
                continue
            i = nid - 1
            if i < 0 or i >= self._mesh.num_nodes:
                continue
            mx, my = self._node_model_xy(i, deformed=show_deformed)
            tip = _scene_xy(mx, my)
            length = max(min_len, ref_len * (mag / max_mag))
            dx = fx / mag
            dy = -fy / mag  # model +Y up → scene +Y down
            self._draw_force_arrow(tip, dx, dy, length, pen, brush, z=6.0)

    def _draw_force_arrow(
        self,
        tip: QPointF,
        dx: float,
        dy: float,
        length: float,
        pen: QPen,
        brush: QBrush,
        z: float,
    ) -> None:
        """Arrowhead at the node; shaft extends opposite the applied force."""
        head_frac = 0.22
        head_half = 0.1
        far = QPointF(tip.x() - dx * length, tip.y() - dy * length)
        shaft_end = QPointF(
            tip.x() - dx * length * head_frac,
            tip.y() - dy * length * head_frac,
        )
        shaft = QGraphicsLineItem(far.x(), far.y(), shaft_end.x(), shaft_end.y())
        shaft.setPen(pen)
        shaft.setZValue(z)
        self.scene.addItem(shaft)

        px, py = -dy, dx
        hw = length * head_half
        head = QPolygonF(
            [
                tip,
                QPointF(shaft_end.x() + px * hw, shaft_end.y() + py * hw),
                QPointF(shaft_end.x() - px * hw, shaft_end.y() - py * hw),
            ]
        )
        item = self.scene.addPolygon(head, pen, brush)
        item.setZValue(z + 0.1)

    def _draw_pin_symbol(
        self,
        node_scene: QPointF,
        edge: str,
        size: float,
        pen: QPen,
        brush: QBrush,
        z: float,
    ) -> None:
        """Fixed support: triangle with apex at node, base outside the mesh."""
        outward = _EDGE_OUTWARD_SCENE[edge]
        tangent = _EDGE_TANGENT_SCENE[edge]
        base_center = QPointF(
            node_scene.x() + outward.x() * size,
            node_scene.y() + outward.y() * size,
        )
        half_base = size * 0.58
        poly = QPolygonF(
            [
                node_scene,
                QPointF(
                    base_center.x() + tangent.x() * half_base,
                    base_center.y() + tangent.y() * half_base,
                ),
                QPointF(
                    base_center.x() - tangent.x() * half_base,
                    base_center.y() - tangent.y() * half_base,
                ),
            ]
        )
        item = self.scene.addPolygon(poly, pen, brush)
        item.setZValue(z)

    def _draw_roller_symbol(
        self,
        node_scene: QPointF,
        edge: str,
        *,
        fixed_dof: Literal["x", "y"],
        boundary_edges: list[str] | None = None,
        size: float,
        pen: QPen,
        brush: QBrush,
        z: float,
    ) -> None:
        """Roller: circle between node and a track line tangent to the circle."""
        cr = size * 0.38
        track_half = size * 0.68
        offset = size * 0.62
        edges = boundary_edges or [edge]

        if fixed_dof == "y":
            # Fix Y: same x as node; above on top row, below elsewhere.
            on_top = "top" in edges and "bottom" not in edges
            if on_top:
                center = QPointF(node_scene.x(), node_scene.y() - offset)
                ty = center.y() - cr
            else:
                center = QPointF(node_scene.x(), node_scene.y() + offset)
                ty = center.y() + cr
            p1 = QPointF(center.x() - track_half, ty)
            p2 = QPointF(center.x() + track_half, ty)
        else:
            # Fix X: same y as node; left on bottom/top spans, right on right wall.
            on_horizontal = "bottom" in edges or "top" in edges
            on_right_wall = "right" in edges and not on_horizontal
            if on_right_wall:
                center = QPointF(node_scene.x() + offset, node_scene.y())
                tx = center.x() + cr
            else:
                center = QPointF(node_scene.x() - offset, node_scene.y())
                tx = center.x() - cr
            p1 = QPointF(tx, center.y() - track_half)
            p2 = QPointF(tx, center.y() + track_half)

        line = QGraphicsLineItem(p1.x(), p1.y(), p2.x(), p2.y())
        line.setPen(pen)
        line.setZValue(z)
        self.scene.addItem(line)

        circ = QGraphicsEllipseItem(center.x() - cr, center.y() - cr, 2 * cr, 2 * cr)
        circ.setPen(pen)
        circ.setBrush(brush)
        circ.setZValue(z + 0.1)
        self.scene.addItem(circ)

    def _dist_refresh_table(self) -> None:
        self.tbl_dist.setRowCount(0)
        for edge in ("bottom", "top", "left", "right"):
            if edge not in self._dist_edge_knm:
                continue
            txs, txe, tys, tye = self._dist_edge_knm[edge]
            row = self.tbl_dist.rowCount()
            self.tbl_dist.insertRow(row)
            edge_item = QTableWidgetItem(edge.capitalize())
            edge_item.setData(Qt.ItemDataRole.UserRole, edge)
            self.tbl_dist.setItem(row, 0, edge_item)
            self.tbl_dist.setItem(row, 1, QTableWidgetItem(_fmt_float(txs)))
            self.tbl_dist.setItem(row, 2, QTableWidgetItem(_fmt_float(txe)))
            self.tbl_dist.setItem(row, 3, QTableWidgetItem(_fmt_float(tys)))
            self.tbl_dist.setItem(row, 4, QTableWidgetItem(_fmt_float(tye)))
            self.tbl_dist.setItem(
                row, 5, QTableWidgetItem(self._dist_dynamic_label(edge))
            )

    def _dist_select_edge_row(self, edge: str) -> None:
        for r in range(self.tbl_dist.rowCount()):
            item = self.tbl_dist.item(r, 0)
            if item is None:
                continue
            if item.data(Qt.ItemDataRole.UserRole) == edge:
                self.tbl_dist.selectRow(r)
                return

    def _on_dist_row_selected(self) -> None:
        rows = self.tbl_dist.selectionModel().selectedRows()
        if len(rows) != 1:
            return
        r = rows[0].row()
        item = self.tbl_dist.item(r, 0)
        if item is None:
            return
        edge = item.data(Qt.ItemDataRole.UserRole)
        if not edge or edge not in self._dist_edge_knm:
            return
        idx = self.cb_dist_edge.findData(edge)
        if idx >= 0:
            self.cb_dist_edge.setCurrentIndex(idx)
        txs, txe, tys, tye = self._dist_edge_knm[edge]
        self.sp_dtx_s.setValue(txs)
        self.sp_dtx_e.setValue(txe)
        self.sp_dty_s.setValue(tys)
        self.sp_dty_e.setValue(tye)
        meta = self._dist_meta.get(edge, {})
        is_dyn = bool(meta.get("is_dynamic", False)) and self.chk_dynamic.isChecked()
        self.chk_dist_dynamic.blockSignals(True)
        self.chk_dist_dynamic.setChecked(is_dyn)
        self.chk_dist_dynamic.blockSignals(False)
        self._on_dist_dynamic_toggled(is_dyn)
        if is_dyn:
            self.sp_dist_scale_start.setValue(float(meta.get("scale_start", 0.0)))
            self.sp_dist_scale_end.setValue(float(meta.get("scale_end", 1.0)))
            preset = str(meta.get("scale_preset", "linear"))
            pidx = self.cb_dist_scale_preset.findData(preset)
            if pidx >= 0:
                self.cb_dist_scale_preset.setCurrentIndex(pidx)

    def _dist_apply_edge(self) -> None:
        if self._mesh is None:
            QMessageBox.warning(self, "Distributed loads", "Generate a mesh first.")
            return
        edge = str(self.cb_dist_edge.currentData())
        self._dist_edge_knm[edge] = (
            float(self.sp_dtx_s.value()),
            float(self.sp_dtx_e.value()),
            float(self.sp_dty_s.value()),
            float(self.sp_dty_e.value()),
        )
        is_dyn = self.chk_dynamic.isChecked() and self.chk_dist_dynamic.isChecked()
        self._dist_meta[edge] = {
            "is_dynamic": is_dyn,
            "scale_start": float(self.sp_dist_scale_start.value()),
            "scale_end": float(self.sp_dist_scale_end.value()),
            "scale_preset": str(self.cb_dist_scale_preset.currentData() or "linear"),
        }
        self._dist_refresh_table()
        self._dist_select_edge_row(edge)

    def _dist_remove_row(self) -> None:
        r = self.tbl_dist.currentRow()
        if r < 0:
            return
        item = self.tbl_dist.item(r, 0)
        if item is None:
            return
        edge = item.data(Qt.ItemDataRole.UserRole)
        if edge:
            self._dist_edge_knm.pop(str(edge), None)
            self._dist_meta.pop(str(edge), None)
        self._dist_refresh_table()

    def _dist_clear_all(self) -> None:
        self._dist_edge_knm.clear()
        self._dist_meta.clear()
        self._dist_refresh_table()

    def _add_point_load(self) -> None:
        if self._selected_node_1based <= 0:
            QMessageBox.information(self, "Loads", "Select a node on the mesh first.")
            return
        nid = self._selected_node_1based
        fx = self.sp_lfx.value()
        fy = self.sp_lfy.value()
        is_dyn = self.chk_dynamic.isChecked() and self.chk_dynamic_load.isChecked()
        self._load_meta[nid] = {
            "is_dynamic": is_dyn,
            "scale_start": float(self.sp_scale_start.value()),
            "scale_end": float(self.sp_scale_end.value()),
            "scale_preset": str(self.cb_scale_preset.currentData() or "linear"),
        }
        for r in range(self.tbl_loads.rowCount()):
            it = self.tbl_loads.item(r, 1)
            if it is not None and int(it.text()) == nid:
                self.tbl_loads.removeRow(r)
                break
        row = self.tbl_loads.rowCount()
        self.tbl_loads.insertRow(row)
        self.tbl_loads.setItem(row, 0, QTableWidgetItem(str(row + 1)))
        self.tbl_loads.setItem(row, 1, QTableWidgetItem(str(nid)))
        self.tbl_loads.setItem(row, 2, QTableWidgetItem(_fmt_float(fx)))
        self.tbl_loads.setItem(row, 3, QTableWidgetItem(_fmt_float(fy)))
        self.tbl_loads.setItem(row, 4, QTableWidgetItem(self._load_dynamic_label(nid)))
        self._renumber_point_load_ids()
        self.tbl_loads.selectRow(row)
        self._redraw_scene(fit_view=False)

    def _remove_load_row(self) -> None:
        r = self.tbl_loads.currentRow()
        if r >= 0:
            it = self.tbl_loads.item(r, 1)
            if it is not None:
                try:
                    self._load_meta.pop(int(it.text()), None)
                except ValueError:
                    pass
            self.tbl_loads.removeRow(r)
            self._renumber_point_load_ids()
        self._redraw_scene(fit_view=False)

    def _build_export_model(self) -> ExportModel:
        if self._mesh is None:
            raise ValueError("Generate a mesh before export.")
        name = "material_1"
        mat = Material(
            slot=1,
            name=name,
            E=float(self.sp_E.value()) * 1000.0,
            nu=float(self.sp_nu.value()),
            gamma=float(self.sp_gamma.value()) * 1000.0,
        )
        solver = self._solver_token()
        if solver == "CONJUGATE_GRADIENT":
            tol = float(self.sp_tol.value())
            maxiter = int(self.sb_maxiter.value())
        else:
            tol = 0.001
            maxiter = 500

        em = ExportModel(
            solver=solver,
            solver_tolerance=tol,
            solver_maxiter=maxiter,
            assumption=cast(
                Literal["plane_stress", "plane_strain"], self._assumption_token()
            ),
            thickness=float(self.sp_thickness.value()),
            materials=[mat],
            mesh=self._mesh,
            default_material_name=name,
            fixities=self._collect_fixities(),
            point_loads=self._loads_from_table(),
            debug=1 if self.chk_engine_debug.isChecked() else 0,
            distributed_edge_traction_knm=dict(self._dist_edge_knm),
            distributed_edge_meta=self._distributed_edge_meta_for_export(),
            is_dynamic=self.chk_dynamic.isChecked(),
            time_step_size=float(self.sp_dt.value()),
            num_time_steps=int(self.sb_num_steps.value()),
            dynamic_method=cast(
                Literal["average_acceleration", "linear_acceleration"],
                self._dynamic_method_token(),
            ),
            damping_enabled=self.chk_damping.isChecked(),
            damping_alpha=float(self.sp_damp_alpha.value()),
            damping_beta=float(self.sp_damp_beta.value()),
        )
        return em

    def _distributed_edge_meta_for_export(self) -> dict[str, DistributedEdgeMeta]:
        out: dict[str, DistributedEdgeMeta] = {}
        if not self.chk_dynamic.isChecked():
            return out
        for edge in self._dist_edge_knm:
            meta = self._dist_meta.get(edge, {})
            preset = str(meta.get("scale_preset", "linear"))
            out[edge] = DistributedEdgeMeta(
                is_dynamic=bool(meta.get("is_dynamic", False)),
                scale_start=float(meta.get("scale_start", 0.0)),
                scale_end=float(meta.get("scale_end", 1.0)),
                scale_preset=cast(Literal["linear"], preset),
            )
        return out

    def _loads_from_table(self) -> list[PointLoad]:
        loads: list[PointLoad] = []
        for r in range(self.tbl_loads.rowCount()):
            try:
                nid = int(self.tbl_loads.item(r, 1).text())
                fx = float(self.tbl_loads.item(r, 2).text())
                fy = float(self.tbl_loads.item(r, 3).text())
            except (AttributeError, ValueError):
                continue
            meta = self._load_meta.get(nid, {})
            is_dyn = (
                bool(meta.get("is_dynamic", False)) and self.chk_dynamic.isChecked()
            )
            preset = str(meta.get("scale_preset", "linear"))
            loads.append(
                PointLoad(
                    node_id=nid,
                    fx=fx * 1000.0,
                    fy=fy * 1000.0,
                    load_id=r + 1,
                    is_dynamic=is_dyn,
                    scale_start=float(meta.get("scale_start", 0.0)),
                    scale_end=float(meta.get("scale_end", 1.0)),
                    scale_preset=cast(Literal["linear"], preset),
                )
            )
        return loads

    def _export_file(self) -> None:
        if self._mesh is None:
            QMessageBox.warning(self, "Compute", "Generate a mesh first.")
            return
        try:
            em = self._build_export_model()
            text = export_input_text(em)
        except Exception as e:  # noqa: BLE001
            QMessageBox.critical(self, "Compute", str(e))
            return
        try:
            result = run_engine(text)
        except (FileNotFoundError, OSError) as e:
            QMessageBox.critical(
                self,
                "Compute",
                f"Could not run the FEM engine:\n{e}",
            )
            return
        except subprocess.TimeoutExpired:
            QMessageBox.critical(self, "Compute", "The FEM engine timed out.")
            return

        if result.returncode != 0:
            detail = (result.stderr or result.stdout or "").strip()
            QMessageBox.critical(
                self,
                "Compute",
                f"Engine exited with code {result.returncode}."
                + (f"\n\n{detail}" if detail else ""),
            )
            return

        json_path = result.executable.parent / "OUTPUT.json"
        if not json_path.is_file():
            QMessageBox.warning(
                self,
                "Compute",
                f"Engine finished but {json_path.name} was not found.",
            )
            return
        try:
            results = EngineResults.from_path(json_path)
            self._apply_results(results)
        except ValueError as e:
            QMessageBox.critical(self, "Compute", str(e))
            return

        msg = f"Wrote {result.input_path} and ran {result.executable.name}"
        msg += f"; results in {json_path.name}"
        if self.chk_engine_debug.isChecked():
            msg += " (debug CSV in debug/)"
        self.statusBar().showMessage(msg, 12000)


def main() -> None:
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
