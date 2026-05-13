#include <cmath>

#include "Element.h"
#include "Utils.h"

void Element::ConstructKandF(ElementType type) {

	std::vector<double> gaussWeights;
	std::vector<std::vector<double>> gaussPoints;

	if (type == ElementType::Q4) {
		// use 2x2 Gauss Quadrature for the Q4 element
		gaussPoints.resize(4);
		gaussWeights.resize(4);
		for (auto& iter : gaussPoints) {
			iter.resize(2); //each gausspoint xy coord
		}
		// for 2x2 quadrature gaussweights are equal
		gaussWeights[0] = gaussWeights[1] = gaussWeights[2] = gaussWeights[3] = 1.0;
		double temp = 1.0 / sqrt(3.0);
		gaussPoints[0] = { -temp, -temp };
		gaussPoints[1] = { temp, -temp };
		gaussPoints[2] = { temp, temp };
		gaussPoints[3] = { -temp, temp };

		elemKMatrix.resize(8, std::vector<double>(8, 0.0));
		std::vector<std::vector<double>> DMatrix = (*matptr).GetDMatrix();
		// for each gausspoint instantiate a GaussPoint object
		for (int i = 0; i < gaussPoints.size(); i++) {
			double xi = gaussPoints[i][0];
			double eta = gaussPoints[i][1];
			double weight = gaussWeights[i];
			GaussPoint gp = GaussPoint(xi, eta, weight, coordinates, DMatrix);

			elemKMatrix += gp.GetKProduct() * (*matptr).GetThickness();
		}

		writeMatrixToCSV(elemKMatrix, "./temp.csv");
	}
	else {
		throw std::invalid_argument("Element type not supported yet.");
	}
}

/* CODE RELATED TO GAUSSPOINT CALCULATIONS */

GaussPoint::GaussPoint(double xi, double eta, double weight, std::vector<std::vector<double>> coordinates, std::vector<std::vector<double>> DMatrix) {
	// evaluate shape function and derivatives at the gausspoint
	std::vector<double> shapeFunctions;
	ComputeShapeFunction(xi, eta, shapeFunctions);
	std::vector<std::vector<double>> shapeFunctionDerivs;
	ComputeShapeFunctionDeriv(xi, eta, shapeFunctionDerivs);

	std::vector<std::vector<double>> JMatrix = shapeFunctionDerivs * coordinates;
	std::vector<std::vector<double>> GMatrix = inverse2x2(JMatrix);
	double Jacobian = determinant2x2(JMatrix);

	// 3x8 matrix
	std::vector<std::vector<double>> BMatrix(3, std::vector<double>(8, 0.0));
	// loop over the columns of the BMatrix and make the Bi matrices
	for (int i = 0; i < BMatrix[0].size(); i+=2) {
		size_t j = static_cast<size_t>(i / 2); //??
		BMatrix[0][i] = GMatrix[0][0] * shapeFunctionDerivs[0][j] + GMatrix[0][1] * shapeFunctionDerivs[1][j];
		BMatrix[0][i + 1] = 0.0;
		BMatrix[1][i] = 0.0;
		BMatrix[1][i + 1] = GMatrix[1][0] * shapeFunctionDerivs[0][j] + GMatrix[1][1] * shapeFunctionDerivs[1][j];
		BMatrix[2][i] = GMatrix[1][0] * shapeFunctionDerivs[0][j] + GMatrix[1][1] * shapeFunctionDerivs[1][j];
		BMatrix[2][i+1] = GMatrix[0][0] * shapeFunctionDerivs[0][j] + GMatrix[0][1] * shapeFunctionDerivs[1][j];
	}

	std::vector<std::vector<double>> BMatrixT = transpose(BMatrix);

	KProduct = ((BMatrixT * DMatrix) * BMatrix) * Jacobian * weight;

	// FIGURE OUT HOW TO DO F VECTOR WITH q(x) 
}

void GaussPoint::ComputeShapeFunction(double xi, double eta, std::vector<double>& N) {
	N.resize(4);

	//temp variables
	double pxi = 1.0 + xi;
	double mxi = 1.0 - xi;
	double peta = 1.0 + eta;
	double meta = 1.0 - eta;

	N[0] = 1.0 / 4.0 * mxi * meta;
	N[1] = 1.0 / 4.0 * pxi * meta;
	N[2] = 1.0 / 4.0 * pxi * peta;
	N[3] = 1.0 / 4.0 * mxi * peta;
}

void GaussPoint::ComputeShapeFunctionDeriv(double xi, double eta, std::vector<std::vector<double>>& dN) {
	// 2x4 matrix, first row is deriv wrt xi, second deriv wrt eta
	dN.resize(2, std::vector<double>(4, 0.0));

	double pxi = 1.0 + xi;
	double mxi = 1.0 - xi;
	double peta = 1.0 + eta;
	double meta = 1.0 - eta;

	dN[0][0] = -meta / 4.0;
	dN[0][1] = meta / 4.0;
	dN[0][2] = peta / 4.0;
	dN[0][3] = -peta / 4.0;
	dN[1][0] = -mxi / 4.0;
	dN[1][1] = -pxi / 4.0;
	dN[1][2] = pxi / 4.0;
	dN[1][3] = mxi / 4.0;
}