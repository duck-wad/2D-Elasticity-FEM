#pragma once

enum class Assumption {
	PLANE_STRAIN,
	PLANE_STRESS
};

enum class ElementType {
	Q4,
	T3,
	Q8,
	T6
};

enum class Formulation {
	LIN_ELASTIC,
	PLASTIC
};

enum class SolverType {
	GAUSSIAN_ELIM,
	LU_DECOMP,
	CHOLESKY_DECOMP,
	GAUSS_SEIDEL
};

enum class Section {
	NONE,
	GENERAL,
	MATERIALS,
	NODES,
	ELEMENTS,
	FIXITIES,
	POINT_LOADS,
	DIST_LOADS
};