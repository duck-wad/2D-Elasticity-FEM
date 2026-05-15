"""
First-draft 2D elasticity modeler: rectangle + structured Q4 mesh, edge BCs,
point loads, export to Engine INPUT.txt format.
Run from repo:  python Modeler/main.py
Or:            cd Modeler && python main.py
"""

from __future__ import annotations

import subprocess
import sys
from functools import partial
from pathlib import Path
from typing import Literal, cast

from PySide6.QtCore import QPoint, QPointF, Qt, Signal
from PySide6.QtGui import QBrush, QColor, QPainter, QPen, QPolygonF
from PySide6.QtWidgets import (
    QApplication,
    QAbstractSpinBox,
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFormLayout,
    QGridLayout,
    QGraphicsEllipseItem,
    QGraphicsScene,
    QGraphicsView,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSlider,
    QSpinBox,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from engine_results import EngineResults
from export_input import ExportModel, Material, PointLoad, _fmt_float, export_input_text
from mesh import RectangleMesh, nodes_on_edge, structured_rectangle_quad
from run_engine import run_engine


def _spin_no_buttons(box: QDoubleSpinBox | QSpinBox) -> None:
    box.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)


def _scene_xy(nx: float, ny: float) -> QPointF:
    """Map model (x,y) with y up to scene coordinates (y negated for Qt)."""
    return QPointF(nx, -ny)


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

        central = QWidget()
        self.setCentralWidget(central)
        root = QHBoxLayout(central)

        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        # —— Left: controls ——
        scroll_host = QWidget()
        left = QVBoxLayout(scroll_host)
        left.setAlignment(Qt.AlignTop)

        gb_mesh = QGroupBox("Rectangle & mesh")
        fm = QFormLayout(gb_mesh)
        self.sp_width = QDoubleSpinBox()
        self.sp_width.setRange(1e-9, 1e9)
        self.sp_width.setDecimals(4)
        self.sp_width.setValue(1.0)
        self.sp_height = QDoubleSpinBox()
        self.sp_height.setRange(1e-9, 1e9)
        self.sp_height.setDecimals(4)
        self.sp_height.setValue(1.0)
        self.sb_nx = QSpinBox()
        self.sb_nx.setRange(1, 500)
        self.sb_nx.setValue(10)
        self.sb_ny = QSpinBox()
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
        self.cb_solver = QComboBox()
        self.cb_solver.addItem("Cholesky Decomposition", "CHOLESKY_DECOMP")
        self.cb_solver.addItem("Conjugate Gradient", "CONJUGATE_GRADIENT")
        self.cb_solver.currentIndexChanged.connect(self._on_solver_changed)

        self._cg_params = QWidget()
        cg_form = QFormLayout(self._cg_params)
        cg_form.setContentsMargins(0, 0, 0, 0)
        self.sp_tol = QDoubleSpinBox()
        self.sp_tol.setRange(1e-20, 1e10)
        self.sp_tol.setDecimals(4)
        self.sp_tol.setValue(0.001)
        self.sb_maxiter = QSpinBox()
        self.sb_maxiter.setRange(1, 10_000_000)
        self.sb_maxiter.setValue(500)
        _spin_no_buttons(self.sp_tol)
        _spin_no_buttons(self.sb_maxiter)
        cg_form.addRow("Tolerance", self.sp_tol)
        cg_form.addRow("Max iterations", self.sb_maxiter)

        self.cb_assumption = QComboBox()
        self.cb_assumption.addItem("Plane Stress", "plane_stress")
        self.cb_assumption.addItem("Plane Strain", "plane_strain")
        fg.addRow("Linear solver", self.cb_solver)
        fg.addRow(self._cg_params)
        fg.addRow("Plane assumption", self.cb_assumption)
        self._on_solver_changed()
        left.addWidget(gb_gen)

        gb_mat = QGroupBox("Material")
        fmat = QFormLayout(gb_mat)
        self.sp_E = QDoubleSpinBox()
        self.sp_E.setRange(1.0, 1e18)
        self.sp_E.setDecimals(4)
        # 200 GPa → 200e6 kPa (export converts kPa → Pa)
        self.sp_E.setValue(200_000_000.0)
        self.sp_nu = QDoubleSpinBox()
        self.sp_nu.setRange(-0.99, 0.499999)
        self.sp_nu.setDecimals(4)
        self.sp_nu.setValue(0.3)
        self.sp_thickness = QDoubleSpinBox()
        self.sp_thickness.setRange(1e-12, 1e6)
        self.sp_thickness.setDecimals(4)
        self.sp_thickness.setValue(1.0)
        for sp in (self.sp_E, self.sp_nu, self.sp_thickness):
            _spin_no_buttons(sp)
        fmat.addRow("E (kPa)", self.sp_E)
        fmat.addRow("nu", self.sp_nu)
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
            "<span style='color:#e53935;'>Fix X only</span> · "
            "<span style='color:#1e88e5;'>Fix Y only</span> · "
            "<span style='color:#43a047;'>both fixed</span>"
        )
        legend.setTextFormat(Qt.TextFormat.RichText)
        legend.setWordWrap(True)
        legend.setStyleSheet("padding-top: 6px;")
        bc_layout.addWidget(legend)

        for edge in edges:
            self._refresh_bc_checkbox_styles(edge)
        left.addWidget(gb_bc)

        gb_load = QGroupBox("Point loads")
        fl = QVBoxLayout(gb_load)
        self.lbl_pick = QLabel("Selected node: (click mesh)")
        fl.addWidget(self.lbl_pick)
        hl = QHBoxLayout()
        self.sp_lfx = QDoubleSpinBox()
        self.sp_lfx.setRange(-1e20, 1e20)
        self.sp_lfx.setDecimals(4)
        self.sp_lfx.setValue(0.0)
        self.sp_lfy = QDoubleSpinBox()
        self.sp_lfy.setRange(-1e20, 1e20)
        self.sp_lfy.setDecimals(4)
        self.sp_lfy.setValue(0.0)
        _spin_no_buttons(self.sp_lfx)
        _spin_no_buttons(self.sp_lfy)
        hl.addWidget(QLabel("fx (kN)"))
        hl.addWidget(self.sp_lfx)
        hl.addWidget(QLabel("fy (kN)"))
        hl.addWidget(self.sp_lfy)
        fl.addLayout(hl)
        btn_add_load = QPushButton("Add / replace load on selected node")
        btn_add_load.clicked.connect(self._add_point_load)
        fl.addWidget(btn_add_load)

        self.tbl_loads = QTableWidget(0, 3)
        self.tbl_loads.setHorizontalHeaderLabels(["Node", "fx (kN)", "fy (kN)"])
        self.tbl_loads.horizontalHeader().setStretchLastSection(True)
        fl.addWidget(self.tbl_loads)
        btn_rm_load = QPushButton("Remove selected load row")
        btn_rm_load.clicked.connect(self._remove_load_row)
        fl.addWidget(btn_rm_load)
        left.addWidget(gb_load)

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
        left.addWidget(gb_results)

        btn_export = QPushButton("Compute")
        btn_export.clicked.connect(self._export_file)
        left.addWidget(btn_export)
        left.addStretch()

        splitter.addWidget(scroll_host)

        # —— Right: graphics ——
        self.scene = QGraphicsScene(self)
        self.view = MeshGraphicsView()
        self.view.setScene(self.scene)
        self.view.nodePicked.connect(self._on_node_picked)
        splitter.addWidget(self.view)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([360, 900])
        self.view.setBackgroundBrush(QBrush(QColor(252, 252, 255)))

        hint = QLabel(
            "View: left-click node to select · middle-drag pan · wheel zoom · +Y upward"
        )
        hint.setStyleSheet("color: #555; padding: 4px;")
        self.statusBar().addPermanentWidget(hint)
        self.menuBar().setVisible(False)

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
        self.view.set_mesh(self._mesh)
        self._redraw_scene(fit_view=True)
        self._sync_load_table_after_mesh_change()

    def _clear_results(self) -> None:
        self._results = None
        self.chk_show_deformed.setChecked(False)
        self.chk_show_deformed.setEnabled(False)
        self.sl_deform_scale.setEnabled(False)
        self.lbl_deform_scale.setEnabled(False)

    def _deform_scale_factor(self) -> float:
        return self._deform_base_scale * (self.sl_deform_scale.value() / 100.0)

    def _on_deform_display_changed(self, _checked: bool) -> None:
        self._redraw_scene(fit_view=False)

    def _on_deform_slider_changed(self, value: int) -> None:
        self.lbl_deform_scale.setText(f"Deformation scale: {value}%")
        if self.chk_show_deformed.isChecked():
            self._redraw_scene(fit_view=False)

    def _node_model_xy(self, index: int, *, deformed: bool) -> tuple[float, float]:
        assert self._mesh is not None
        nx, ny = self._mesh.nodes[index]
        if not deformed or self._results is None:
            return nx, ny
        scale = self._deform_scale_factor()
        ux = self._results.displacements_x[index]
        uy = self._results.displacements_y[index]
        return nx + scale * ux, ny + scale * uy

    def _apply_results(self, results: EngineResults) -> None:
        if self._mesh is None:
            return
        results.validate_mesh_nodes(self._mesh.num_nodes)
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
        self._redraw_scene(fit_view=False)

    def _sync_load_table_after_mesh_change(self) -> None:
        if self._mesh is None:
            return
        nmax = self._mesh.num_nodes
        rows_to_remove = []
        for r in range(self.tbl_loads.rowCount()):
            it = self.tbl_loads.item(r, 0)
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
            self.tbl_loads.removeRow(r)

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

    def _add_point_load(self) -> None:
        if self._selected_node_1based <= 0:
            QMessageBox.information(self, "Loads", "Select a node on the mesh first.")
            return
        nid = self._selected_node_1based
        fx = self.sp_lfx.value()
        fy = self.sp_lfy.value()
        for r in range(self.tbl_loads.rowCount()):
            it = self.tbl_loads.item(r, 0)
            if it is not None and int(it.text()) == nid:
                self.tbl_loads.removeRow(r)
                break
        row = self.tbl_loads.rowCount()
        self.tbl_loads.insertRow(row)
        self.tbl_loads.setItem(row, 0, QTableWidgetItem(str(nid)))
        self.tbl_loads.setItem(row, 1, QTableWidgetItem(_fmt_float(fx)))
        self.tbl_loads.setItem(row, 2, QTableWidgetItem(_fmt_float(fy)))

    def _remove_load_row(self) -> None:
        r = self.tbl_loads.currentRow()
        if r >= 0:
            self.tbl_loads.removeRow(r)

    def _build_export_model(self) -> ExportModel:
        if self._mesh is None:
            raise ValueError("Generate a mesh before export.")
        name = "material_1"
        mat = Material(
            slot=1,
            name=name,
            E=float(self.sp_E.value()) * 1000.0,
            nu=float(self.sp_nu.value()),
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
        )
        return em

    def _loads_from_table(self) -> list[PointLoad]:
        loads: list[PointLoad] = []
        for r in range(self.tbl_loads.rowCount()):
            try:
                nid = int(self.tbl_loads.item(r, 0).text())
                fx = float(self.tbl_loads.item(r, 1).text())
                fy = float(self.tbl_loads.item(r, 2).text())
            except (AttributeError, ValueError):
                continue
            loads.append(PointLoad(node_id=nid, fx=fx * 1000.0, fy=fy * 1000.0))
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
