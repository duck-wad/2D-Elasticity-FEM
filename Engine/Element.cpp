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
		elemFVector.resize(8, 0.0);
		std::vector<std::vector<double>> DMatrix = (*matptr).GetDMatrix();
		// for each gausspoint instantiate a GaussPoint object
		for (size_t i = 0; i < gaussPoints.size(); i++) {
			double xi = gaussPointCoords[i][0];
			double eta = gaussPointCoords[i][1];
			double weight = gaussWeights[i];
			// initialize the gp object
			GaussPoint gp(xi, eta, weight);

			std::vector<std::vector<double>> KProduct = gp.ComputeStiffnessContribution(coordinates, DMatrix);

			elemKMatrix += KProduct * (*matptr).GetThickness() * weight;

			gaussPoints[i] = gp;
		}

		writeMatrixToCSV(elemKMatrix, "./temp.csv");
	}
	else {
		throw std::invalid_argument("Element type not supported yet.");
	}
}

void Element::ConstructF(std::vector<DistributedLoad> loads) {
	elemFVector.resize(8, 0.0);

	// loop over the vector of loads applied to the element
	for (size_t i = 0; i < loads.size(); i++) {

		const DistributedLoad& load = loads[i];
		int edge = load.edgeIndex;

		// based on which edge is loaded, the GP coordinates will be different
		// for instance, edge 0 is the bottom edge, and eta=-1, xi1=-1/sqrt(3) and xi2=1/sqrt(3)
		// for the edge integration, use 2-point gaussian legendre
		std::vector<double> xis(2, 0.0);
		std::vector<double> etas(2, 0.0);
		std::vector<double> weights(2, 1.0); //each gaussweight is 1

		if (edge == 0) {
			etas[0] = etas[1] = -1.0;
			xis[0] = -1. / sqrt(3.);
			xis[1] = 1. / sqrt(3.);
		}
		else if (edge == 1) {
			xis[0] = xis[1] = 1.0;
			etas[0] = -1. / sqrt(3.);
			etas[1] = 1. / sqrt(3.);
		}
		else if (edge == 2) {
			etas[0] = etas[1] = 1.0;
			xis[0] = -1. / sqrt(3.);
			xis[1] = 1. / sqrt(3.);
		}
		else if (edge == 3) {
			xis[0] = xis[1] = -1.0;
			etas[0] = -1. / sqrt(3.);
			etas[1] = 1. / sqrt(3.);
		}
		else
			throw std::invalid_argument("Not a valid edge index");

		// jacobian J is same b/w the GPs and is L/2
		std::vector<double> coord1 = coordinates[edge];
		std::vector<double> coord2 = coordinates[(edge + 1) % 4];
		double L = sqrt(std::pow((coord1[0] - coord2[0]), 2) + std::pow((coord1[1] - coord2[1]), 2));
		double J = L / 2.0;
		
		// loop over each gausspoint to get contribution
		for (size_t j = 0; j < xis.size(); j++) {
			// compute shape functions at gausspoint
			std::vector<double> N = ShapeFunctionsQ4::N(xis[j], etas[j]);

			// load q(x) is defined as linearly varying load b/w node 1 and node 2
			// so use the shape functions to get q(gp) = N1q1+N2q2
			// identify correct shape functions to use based on the edge index
			double loadX = N[edge] * load.xvalues[0] + N[(edge + 1) % N.size()] * load.xvalues[1];
			double loadY = N[edge] * load.yvalues[0] + N[(edge + 1) % N.size()] * load.yvalues[1];

			std::vector<double> tMatrix = { loadX, loadY };

			// need to put shape functions into 2x8 form
			std::vector<std::vector<double>> NMatrix = { 
				{N[0], 0, N[1], 0, N[2], 0, N[3], 0}, 
				{0, N[0], 0, N[1], 0, N[2], 0, N[3]} 
			};

			elemFVector += (transpose(NMatrix) * tMatrix) * J * (*matptr).GetThickness() * weights[j];
		}
	}
	// writeVectorToCSV(elemFVector, "./tempvec.csv");
}

/* CODE RELATED TO GAUSSPOINT CALCULATIONS */

std::vector<double> ShapeFunctionsQ4::N(double xi, double eta) {

	std::vector<double> N(4, 0.0);
	//temp variables
	double pxi = 1.0 + xi;
	double mxi = 1.0 - xi;
	double peta = 1.0 + eta;
	double meta = 1.0 - eta;

	N[0] = 1.0 / 4.0 * mxi * meta;
	N[1] = 1.0 / 4.0 * pxi * meta;
	N[2] = 1.0 / 4.0 * pxi * peta;
	N[3] = 1.0 / 4.0 * mxi * peta;

	return N;
}

std::vector<std::vector<double>> ShapeFunctionsQ4::dN(double xi, double eta) {
	// 2x4 matrix, first row is deriv wrt xi, second deriv wrt eta
	std::vector<std::vector<double>> dN(2, std::vector<double>(4, 0.0));

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

	return dN;
}

GaussPoint::GaussPoint(double _xi, double _eta, double _weight) : xi(_xi), eta(_eta), weight(_weight) {
	// set the shape functions
	N = ShapeFunctionsQ4::N(xi, eta);
	dN = ShapeFunctionsQ4::dN(xi, eta);
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