#include <cmath>

#include "Element.h"
#include "Utils.h"

void Element::ConstructK(ElementType type) {

	std::vector<double> gaussWeights;
	std::vector<std::vector<double>> gaussPointCoords;

	if (type == ElementType::Q4) {
		// use 2x2 Gauss Quadrature for the Q4 element
		gaussPointCoords.resize(4);
		gaussWeights.resize(4);

		// vector to store the gausspoints 
		gaussPoints.resize(4);

		for (auto& iter : gaussPointCoords) {
			iter.resize(2); //each gausspoint xy coord
		}
		// for 2x2 quadrature gaussweights are equal
		gaussWeights[0] = gaussWeights[1] = gaussWeights[2] = gaussWeights[3] = 1.0;
		double temp = 1.0 / sqrt(3.0);
		gaussPointCoords[0] = { -temp, -temp };
		gaussPointCoords[1] = { temp, -temp };
		gaussPointCoords[2] = { temp, temp };
		gaussPointCoords[3] = { -temp, temp };

		elemKMatrix.resize(8, std::vector<double>(8, 0.0));
		std::vector<std::vector<double>> DMatrix = (*matptr).GetDMatrix();
		// for each gausspoint instantiate a GaussPoint object
		for (size_t i = 0; i < gaussPoints.size(); i++) {
			double xi = gaussPointCoords[i][0];
			double eta = gaussPointCoords[i][1];
			double weight = gaussWeights[i];
			// initialize the gp object
			GaussPoint gp(xi, eta, weight);

			std::vector<std::vector<double>> KProduct = gp.ComputeStiffnessContribution(coordinates, DMatrix);

			elemKMatrix += KProduct * (*matptr).GetThickness();

			gaussPoints[i] = gp;
		}

		// writeMatrixToCSV(elemKMatrix, "./temp.csv");
	}
	else {
		throw std::invalid_argument("Element type not supported yet.");
	}
}

/* CODE RELATED TO GAUSSPOINT CALCULATIONS */

GaussPoint::GaussPoint(double _xi, double _eta, double _weight) : xi(_xi), eta(_eta), weight(_weight) {
	// set the shape functions
	ComputeShapeFunction();
	ComputeShapeFunctionDeriv();
}

void GaussPoint::ComputeShapeFunction() {
	N.resize(4, 0.0);

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

void GaussPoint::ComputeShapeFunctionDeriv() {
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

std::vector<std::vector<double>> GaussPoint::ComputeStiffnessContribution(const std::vector<std::vector<double>>& coordinates, const std::vector<std::vector<double>>& DMatrix) {

	std::vector<std::vector<double>> JMatrix = dN * coordinates;
	std::vector<std::vector<double>> GMatrix = inverse2x2(JMatrix);
	double Jacobian = determinant2x2(JMatrix);

	// 3x8 matrix
	std::vector<std::vector<double>> BMatrix(3, std::vector<double>(8, 0.0));
	// loop over the columns of the BMatrix and make the Bi matrices
	for (int i = 0; i < BMatrix[0].size(); i += 2) {
		size_t j = static_cast<size_t>(i / 2); //??
		BMatrix[0][i] = GMatrix[0][0] * dN[0][j] + GMatrix[0][1] * dN[1][j];
		BMatrix[0][i + 1] = 0.0;
		BMatrix[1][i] = 0.0;
		BMatrix[1][i + 1] = GMatrix[1][0] * dN[0][j] + GMatrix[1][1] * dN[1][j];
		BMatrix[2][i] = GMatrix[1][0] * dN[0][j] + GMatrix[1][1] * dN[1][j];
		BMatrix[2][i + 1] = GMatrix[0][0] * dN[0][j] + GMatrix[0][1] * dN[1][j];
	}

	std::vector<std::vector<double>> BMatrixT = transpose(BMatrix);

	std::vector<std::vector<double>> KProduct = ((BMatrixT * DMatrix) * BMatrix) * Jacobian * weight;

	return KProduct;
}