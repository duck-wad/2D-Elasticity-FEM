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
	void CholeskyDecomposition(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d);
	void ConjugateGradient(const std::vector<std::vector<double>>& A, const std::vector<double>& b, std::vector<double>& d);
};

