#pragma once
#include <iostream>
#include <vector>
#include <cmath>
#include "Enums.h"

/* Includes algorithms for solving Ku=x problems */

class Solver
{
public:
	Solver();
	Solver(std::string& type, double tol = 0.001, int maxiter = 500, int stages = 1);
	SolverType solverType;
	double tolerance;
	int maxIterations;
	int numStages;

	void Solve(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d);

// matrix solving algorithms
private:
	void GaussianElimination(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d);
	void LUDecomposition(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d);
	void CholeskyDecomposition(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d);
	void GaussSeidel(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d, const double errorTol = 0.001, const int maxIter = 500);

	//puts the input matrix A into reduced-echelon form
	//if matrix L is passed in as argument, it will be filled as the bottom triangle for LU decomposition
	void ForwardElimination(std::vector<std::vector<double>>& A, std::vector<double>& b, std::vector<std::vector<double>>& L);
	//dummy function if an L matrix is not passed in
	void ForwardElimination(std::vector<std::vector<double>>& A, std::vector<double>& b);

	std::vector<double> ForwardSubstitution(const std::vector<std::vector<double>>& A, const std::vector<double>& b);
	std::vector<double> BackwardSubstitution(const std::vector<std::vector<double>>& A, const std::vector<double>& b);
};

