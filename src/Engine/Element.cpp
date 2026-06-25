#include <cmath>

#include "Element.h"
#include "Utils.h"

Element::Element(int _id, const std::vector<int>& _nodes, const std::vector<std::vector<double>>& coords, const Material* mat, ElementType _type) : id(_id), nodes(_nodes), coordinates(coords), matptr(mat), type(_type) {

	//compute the area of the element
	ComputeArea();
}

void Element::AddDistributedLoad(const DistributedLoad& load){
	distLoads.push_back(load);
}

void Element::ComputeArea() {
	double temp = 0;
	for (size_t i = 0; i < nodes.size(); i++) {
		if(i == nodes.size()-1)
			temp += (coordinates[i][0] * coordinates[0][1] - coordinates[i][1] * coordinates[0][0]);		
		else
			temp += (coordinates[i][0] * coordinates[i + 1][1] - coordinates[i][1] * coordinates[i + 1][0]);
	}
	area = std::abs(temp) / 2.0;

}

void Element::ConstructK() {
	if (type == ElementType::Q4)
		ConstructKQ4();
	/*else if (type == ElementType::T3)
		ConstructKT3();*/
	else
		throw std::invalid_argument("Element type not supported");
}

void Element::ConstructM() {
	// using lumped mass matrix method (mass divided evenly amongst nodes)
	// compute the mass of the element
	double t = (*matptr).GetThickness();
	double rho = (*matptr).GetDensity();
	double mass = t * rho * area;

	// initialize the mass matrix to an identiy matrix same size of K
	elemMMatrix.assign(elemKMatrix.size(), std::vector<double>(elemKMatrix.size(), 0.0));
	for (size_t i = 0; i < elemMMatrix.size(); i++) {
		elemMMatrix[i][i] = 1.0;
	}
	elemMMatrix *= (mass / 4.0);

}

void Element::ConstructC(double alpha, double beta) {
	elemCMatrix.assign(elemKMatrix.size(), std::vector<double>(elemKMatrix.size(), 0.0));
	// using Rayleigh damping C = alpha * M + beta * K
	elemCMatrix = (elemMMatrix * alpha) + (elemKMatrix * beta);
}

void Element::ConstructF() {
	if (type == ElementType::Q4)
		ConstructFQ4();
	/*else if (type == ElementType::T3)
		ConstructFT3();*/
	else
		throw std::invalid_argument("Element type not supported");
}

/* HELPER FUNCTIONS FOR Q4 ELEMENT */

void Element::ConstructKQ4() {

	std::vector<double> gaussWeights;
	std::vector<std::vector<double>> gaussPointCoords;

	// use 2x2 Gauss Quadrature for the Q4 element
	gaussPointCoords.assign(4, std::vector<double>(2, 0.0));
	gaussWeights.assign(4, 1.0); // all weights are equal to 1.0

	// vector to store the gausspoints 
	gaussPoints.resize(4);

	double temp = 1.0 / sqrt(3.0);
	gaussPointCoords[0] = { -temp, -temp };
	gaussPointCoords[1] = { temp, -temp };
	gaussPointCoords[2] = { temp, temp };
	gaussPointCoords[3] = { -temp, temp };

	elemKMatrix.assign(8, std::vector<double>(8, 0.0));
	elemFVector.assign(8, 0.0);
	const auto& DMatrix = (*matptr).GetDMatrix();

	// for each gausspoint instantiate a GaussPoint struct
	for (size_t i = 0; i < gaussPoints.size(); i++) {

		// initialize the gp object
		GaussPoint gp;
		gp.xi = gaussPointCoords[i][0];
		gp.eta = gaussPointCoords[i][1];
		gp.weight = gaussWeights[i];

		auto N = ComputeNQ4(gp.xi, gp.eta);
		auto dN = ComputedNQ4(gp.xi, gp.eta);

		auto JMatrix = dN * coordinates;
		auto GMatrix = inverse2x2(JMatrix);
		double Jacobian = determinant2x2(JMatrix);

		// 3x8 matrix
		std::vector<std::vector<double>> BMatrix(3, std::vector<double>(8, 0.0));
		// loop over the columns of the BMatrix and make the Bi matrices
		for (int k = 0; k < BMatrix[0].size(); k += 2) {
			size_t j = static_cast<size_t>(k / 2); //??
			BMatrix[0][k] = GMatrix[0][0] * dN[0][j] + GMatrix[0][1] * dN[1][j];
			BMatrix[0][k + 1] = 0.0;
			BMatrix[1][k] = 0.0;
			BMatrix[1][k + 1] = GMatrix[1][0] * dN[0][j] + GMatrix[1][1] * dN[1][j];
			BMatrix[2][k] = GMatrix[1][0] * dN[0][j] + GMatrix[1][1] * dN[1][j];
			BMatrix[2][k + 1] = GMatrix[0][0] * dN[0][j] + GMatrix[0][1] * dN[1][j];
		}

		std::vector<std::vector<double>> BMatrixT = transpose(BMatrix);

		std::vector<std::vector<double>> KProduct = ((BMatrixT * DMatrix) * BMatrix) * Jacobian;

		elemKMatrix += KProduct * (*matptr).GetThickness() * gp.weight;

		gaussPoints[i] = gp;
	}

	//writeMatrixToCSV(elemKMatrix, "./temp.csv");
}

void Element::ConstructFQ4() {
	elemFVector.assign(8, 0.0);

	// loop over the vector of loads applied to the element
	for (size_t i = 0; i < distLoads.size(); i++) {

		const DistributedLoad& load = distLoads[i];
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
			std::vector<double> N = ComputeNQ4(xis[j], etas[j]);

			// load q(x) is defined as linearly varying load b/w node 1 and node 2
			// so use the shape functions to get q(gp) = N1q1+N2q2
			// identify correct shape functions to use based on the edge index
			double loadX = N[edge] * load.xvalues[0] + N[(edge + 1) % N.size()] * load.xvalues[1];
			double loadY = N[edge] * load.yvalues[0] + N[(edge + 1) % N.size()] * load.yvalues[1];

			// multiply the loads by the scale
			loadX *= load.scale;
			loadY *= load.scale;

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

std::vector<double> Element::ComputeNQ4(double xi, double eta) const {
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

std::vector<std::vector<double>> Element::ComputedNQ4(double xi, double eta) const {
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