#include <Eigen/Sparse>
#include <Eigen/IterativeLinearSolvers>

#include "Solver.h"
#include "MatrixOperations.h"

//defaults
Solver::Solver() {
	solverType = SolverType::CHOLESKY_DECOMP;
	tolerance = 0.001;
	maxIterations = 500;
	numStages = 1;
}

Solver::Solver(std::string& type, double tol, int maxiter, int stages) {
	tolerance = tol;
	maxIterations = maxiter;
	numStages = stages;

    if (type == "CHOLESKY_DECOMP") {
		solverType = SolverType::CHOLESKY_DECOMP;
	}
	else if (type == "CONJUGATE_GRADIENT") {
		solverType = SolverType::CONJUGATE_GRADIENT;
	}
	else
		throw std::invalid_argument("Not a valid solver type");
}

void Solver::Solve(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d) {
	if (solverType == SolverType::CHOLESKY_DECOMP) {
		CholeskyDecomposition(A, b, d);
	}
	else if (solverType == SolverType::CONJUGATE_GRADIENT) {
		ConjugateGradient(A, b, d);
	}
	else
		throw std::invalid_argument("Not a valid solver type");
}

//use Cholesky for symmetric A matrix
void Solver::CholeskyDecomposition(
    const std::vector<std::vector<double>>& A,
    const std::vector<double>& b,
    std::vector<double>& d)
{
    int n = A.size();

    typedef Eigen::Triplet<double> T;
    std::vector<T> triplets;
    triplets.reserve(n * 10); // FEM is sparse

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {

            double val = A[i][j];
            if (std::abs(val) > 1e-12) {
                triplets.emplace_back(i, j, val);
            }
        }
    }

    Eigen::SparseMatrix<double> K(n, n);
    K.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::VectorXd F(n);
    for (int i = 0; i < n; i++)
        F(i) = b[i];

    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    solver.compute(K);

    if (solver.info() != Eigen::Success) {
        throw std::runtime_error("Cholesky decomposition failed.");
    }

    Eigen::VectorXd D = solver.solve(F);

    if (solver.info() != Eigen::Success) {
        throw std::runtime_error("Solve failed.");
    }

    d.resize(n);
    for (int i = 0; i < n; i++)
        d[i] = D(i);
}

void Solver::ConjugateGradient(
    const std::vector<std::vector<double>>& A,
    const std::vector<double>& b,
    std::vector<double>& d)
{
    int n = A.size();

    // Convert dense to Eigen dense
    Eigen::MatrixXd K(n, n);
    Eigen::VectorXd F(n);

    for (int i = 0; i < n; i++) {
        F(i) = b[i];
        for (int j = 0; j < n; j++) {
            K(i, j) = A[i][j];
        }
    }

    Eigen::ConjugateGradient<
        Eigen::MatrixXd,
        Eigen::Lower | Eigen::Upper,
        Eigen::IdentityPreconditioner
    > solver;

    solver.setTolerance(tolerance);
    solver.setMaxIterations(maxIterations);

    solver.compute(K);

    if (solver.info() != Eigen::Success)
        throw std::runtime_error("CG setup failed");

    Eigen::VectorXd x = solver.solve(F);

    if (solver.info() != Eigen::Success)
        throw std::runtime_error("CG solve failed");

    d.resize(n);
    for (int i = 0; i < n; i++)
        d[i] = x(i);
}