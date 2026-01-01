#include "Solver.h"
#include "MatrixOperations.h"

//defaults
Solver::Solver() {
	solverType = SolverType::GAUSSIAN_ELIM;
	tolerance = 0.001;
	maxIterations = 500;
	numStages = 1;
}

Solver::Solver(std::string& type, double tol, int maxiter, int stages) {
	tolerance = tol;
	maxIterations = maxiter;
	numStages = stages;

	if (type == "GAUSSIAN_ELIMINATION") {
		solverType = SolverType::GAUSSIAN_ELIM;
	}
	else if (type == "LU_DECOMPOSITION") {
		solverType = SolverType::LU_DECOMP;
	}
	else if (type == "CHOLESKY_DECOMP") {
		solverType = SolverType::CHOLESKY_DECOMP;
	}
	else if (type == "GAUSS_SEIDEL") {
		solverType = SolverType::GAUSS_SEIDEL;
	}
	else
		throw std::invalid_argument("Not a valid solver type");
}

/* START OF MATRIX SOLVING ALGORITHM FUNCTIONS */

//gaussian elimination using forward elimination to get A to REF, and then backward substitution to solve for x
void Solver::GaussianElimination(const std::vector<std::vector<double>>& A, const std::vector<double>& b) {

	if (A.size() != b.size() || A[0].size() != b.size()) {
		throw std::invalid_argument("Matrix and vector must have same dimensions");
	}

	std::vector<std::vector<double>> A_REF = A;
	std::vector<double> b_REF = b;

	ForwardElimination(A_REF, b_REF);

	//check if A_REF is singular (zero exists on the diagonal)
	if (isSingular(A_REF)) {
		throw std::invalid_argument("Matrix is singular, system cannot be solved for a unique solution");
	}

	std::vector<double> x = BackwardSubstitution(A_REF, b_REF);

	std::cout << "Results from Gaussian Elimination: " << std::endl;
	PrintVector(x);
}

//LU decomposition uses forward elimination to decompose A into L and U, forward substitution to get D, then backward substitution to get x
void Solver::LUDecomposition(const std::vector<std::vector<double>>& A, const std::vector<double>& b) {

	if (A.size() != b.size() || A[0].size() != b.size()) {
		throw std::invalid_argument("Matrix and vector must have same dimensions");
	}

	std::vector<std::vector<double>> U = A;
	std::vector<double> b_REF = b;
	std::vector<std::vector<double>> L(A.size(), std::vector<double>(A.size()));

	ForwardElimination(U, b_REF, L);

	//check if A_REF is singular (zero exists on the diagonal)
	if (isSingular(U)) {
		throw std::invalid_argument("Matrix is singular, system cannot be solved for a unique solution");
	}

	//perform forward substitution using L and b to get D
	//pass b, not b_REF for forwardsub
	std::vector<double> D = ForwardSubstitution(L, b);
	//then use backward substitution to get x
	std::vector<double> x = BackwardSubstitution(U, D);

	std::cout << "Results from LU Decomposition: " << std::endl;
	PrintVector(x);
}

//use Cholesky for symmetric A matrix
void Solver::CholeskyDecomposition(const std::vector<std::vector<double>>& A, const std::vector<double>& b) {

	if (A.size() != b.size() || A[0].size() != b.size()) {
		throw std::invalid_argument("Matrix and vector must have same dimensions");
	}
	//make sure matrix is symmetric
	if (!isSymmetric(A)) {
		throw std::invalid_argument("Matrix must be symmetric for Cholesky decomposition");
	}

	//fill in L
	std::vector<std::vector<double>> L(A.size(), std::vector<double>(A.size(), 0.0));

	for (size_t k = 0; k < A.size(); k++) {
		for (size_t i = 0; i < k; i++) {
			double sum = 0.0;
			for (size_t j = 0; j < i; j++) {
				sum += L[i][j] * L[k][j];
			}
			L[k][i] = (A[k][i] - sum) / L[i][i];
		}
		//compute the diagonal
		double sum = 0.0;
		for (size_t j = 0; j < k; j++) {
			sum += L[k][j] * L[k][j];
		}
		L[k][k] = std::sqrt(A[k][k] - sum);
	}

	if (isSingular(L)) {
		throw std::invalid_argument("Matrix is singular, system cannot be solved for a unique solution");
	}

	std::vector<std::vector<double>> LT = Transpose(L);

	//perform forward substitution using L and b to get D
	std::vector<double> D = ForwardSubstitution(L, b);
	//then use backward substitution to get x
	std::vector<double> x = BackwardSubstitution(LT, D);

	std::cout << "Results from Cholesky Decomposition: " << std::endl;
	PrintVector(x);
}

void Solver::GaussSeidel(const std::vector<std::vector<double>>& A, const std::vector<double>& b, const double errorTol, const int maxIter) {
	if (A.size() != b.size() || A[0].size() != b.size()) {
		throw std::invalid_argument("Matrix and vector must have same dimensions");
	}
	//initialize guess to zero
	std::vector<double> x(A.size(), 0.0);
	int numIter = 0;

	for (size_t iter = 0; iter < maxIter; iter++) {
		numIter = iter + 1;
		//store results from previous iteration
		std::vector<double> x_old = x;
		//compute the solution for each row sequentially starting with the first row
		for (size_t i = 0; i < A.size(); i++) {
			double sum = 0.0;
			for (size_t j = 0; j < i; j++) {
				sum += A[i][j] * x[j];
			}
			for (size_t j = i + 1; j < A.size(); j++) {
				sum += A[i][j] * x_old[j];
			}
			//update result for current iteration
			x[i] = (b[i] - sum) / A[i][i];
		}
		//check convergence. error for each value must be below tolerance
		bool errorFlag = false;
		for (size_t i = 0; i < A.size(); i++) {
			double error = (x[i] - x_old[i]) / x[i];
			if (error > errorTol) errorFlag = true;
		}
		if (!errorFlag) break;

	}

	if (numIter == maxIter) {
		throw std::runtime_error("Gauss-Seidel method failed to converge within maximum iterations");
	}

	std::cout << "Results from Gauss-Seidel iterative method: " << std::endl;
	PrintVector(x);
	std::cout << "Iterations required: " << numIter << std::endl;
}



/* START OF HELPER FUNCTIONS */

//if L needs to be filled, the function expects L to be sized before being passed in
void Solver::ForwardElimination(std::vector<std::vector<double>>& A, std::vector<double>& b, std::vector<std::vector<double>>& L) {

	if (A.size() != b.size() || A[0].size() != b.size()) {
		throw std::invalid_argument("Matrix and vector must have same dimensions");
	}

	bool fillL = !L.empty();
	if (fillL) {
		if (L.size() != A.size() || L[0].size() != A[0].size()) {
			throw std::invalid_argument("L is not sized correctly");
		}
		for (size_t i = 0; i < L.size(); i++) {
			L[i][i] = 1.0;
		}
	}

	//append b to last column of A matrix to put into augmented form to streamline row operations
	std::vector<std::vector<double>> temp = Transpose(A);
	temp.push_back(b);
	std::vector<std::vector<double>> aug_A = Transpose(temp);

	//perform row operations to bring aug_A to REF
	for (size_t piv = 0; piv < aug_A.size(); piv++) {
		for (size_t i = piv + 1; i < aug_A.size(); i++) {
			double f = aug_A[i][piv] / aug_A[piv][piv];
			std::vector<double> pivRow = aug_A[piv] * f;
			aug_A[i] = aug_A[i] - pivRow;
			if (fillL) {
				L[i][piv] = f;
			}
		}
	}
	//set b to last column of augmented matrix
	//un-augment to get the REF for A
	temp = Transpose(aug_A);
	b = temp[A.size()];
	temp.pop_back();
	A = Transpose(temp);
}

//if L is not passed in, dummy function is called which then calls actual function with an empty L
void Solver::ForwardElimination(std::vector<std::vector<double>>& A, std::vector<double>& b) {
	std::vector<std::vector<double>> L = {};
	ForwardElimination(A, b, L);
}

std::vector<double> Solver::ForwardSubstitution(const std::vector<std::vector<double>>& A, const std::vector<double>& b) {
	std::vector<double> x(b.size());

	for (size_t i = 0; i < A.size(); i++) {
		if (i == 0) {
			x[i] = b[i] / A[i][i];
		}
		else {
			double sum = 0.0;
			for (size_t j = 0; j < i; j++) {
				sum += A[i][j] * x[j];
			}
			x[i] = (b[i] - sum) / A[i][i];
		}
	}
	return x;
}

std::vector<double> Solver::BackwardSubstitution(const std::vector<std::vector<double>>& A, const std::vector<double>& b) {

	std::vector<double> x(b.size());
	for (size_t i = A.size(); i-- > 0; ) {
		if (i == A.size() - 1) {
			x[i] = b[i] / A[i][i];
		}
		else {
			double sum = 0.0;
			for (size_t j = i + 1; j < A.size(); j++) {
				sum += A[i][j] * x[j];
			}
			x[i] = (b[i] - sum) / A[i][i];
		}
	}
	return x;
}

