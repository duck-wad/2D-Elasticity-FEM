#include "Model.h"
#include "Utils.h"

#include <Eigen/Sparse>

//default constructor 
Model::Model() {
	std::string default_solver = "CHOLESKY_DECOMP";
	solver = Solver(default_solver);
	// by default assume plane strain assumption
	assumption = Assumption::PLANE_STRAIN;
	debug = 0;
	thickness = 1;

	elemType = ElementType::Q4;
	numNodes = 0;
	numEls = 0;
	numFixities = 0;
	numDistLoads = 0;
}

void Model::SetAssumption(std::string assump) {
	if (assump == "plane_strain")
		assumption = Assumption::PLANE_STRAIN;
	else if (assump == "plane_stress")
		assumption = Assumption::PLANE_STRESS;
	else
		throw std::invalid_argument("Not a valid 2D assumption");
}

void Model::SetElemType(std::string type) {
	if (type == "Q4")
		elemType = ElementType::Q4;
	else if (type == "T3")
		elemType = ElementType::T3;
	else if (type == "Q8")
		elemType = ElementType::Q8;
	else if (type == "T6")
		elemType = ElementType::T6;
	else
		throw std::invalid_argument("Not a valid element type");
}

void Model::Discretize() {

	// loop over list of elements and construct element stiffness and force
	for (auto& pair : elements) {
		int id = pair.first;
		Element& element = pair.second;
		element.ConstructK();
		if (distLoads.count(id)) {
			element.ConstructF(distLoads[id]);
		}
	}
}

void Model::Assemble() {

	// resize global matrices as 2xnumnodes
	globalK.assign(2 * numNodes, std::vector<double>(2 * numNodes, 0.0));
	globalF.assign(2 * numNodes, 0.0);

	for (const auto& pair : elements) {

		const Element& element = pair.second;
		const auto& elemK = element.GetK();
		const auto& elemF = element.GetF();
		const auto& elemNodes = element.GetNodes();

		// store the DOFs in a vector
		// ex. node 1 has DOFs 0 and 1
		std::vector<int> dofMap;

		for (int node : elemNodes) {
			dofMap.push_back(2 * (node - 1));
			dofMap.push_back(2 * (node - 1) + 1);
		}

		for (size_t i = 0; i < dofMap.size(); i++) {

			globalF[dofMap[i]] += elemF[i];

			for (size_t j = 0; j < dofMap.size(); j++) {

				globalK[dofMap[i]][dofMap[j]] += elemK[i][j];
			}
		}
		//writeMatrixToCSV(globalK, "./tempK.csv");
		//writeVectorToCSV(globalF, "./tempF.csv");
	}
}

void Model::ApplyBC() {

	std::vector<int> constraintedDOFs;

	for (auto const& pair : fixities) {
		int dof = 2*(pair.first - 1);
		if (pair.second[0])
			constraintedDOFs.push_back(dof);
		if (pair.second[1])
			constraintedDOFs.push_back(dof + 1);
	}

	for (int i : constraintedDOFs) {
		for (int j = 0; j < 2 * numNodes; j++) {
			// zero out the row and column
			globalK[i][j] = 0;
			globalK[j][i] = 0;
		}
		// set the diagonal position to 1
		globalK[i][i] = 1.0;
		// zero out the force vector to fix the DOF
		globalF[i] = 0.0;
	}
}

void Model::ApplyPointLoads() {

	for (auto const& pair : pointLoads) {
		int dof = 2 * (pair.first - 1);
		double xload = pair.second[0];
		double yload = pair.second[1];

		globalF[dof] += xload;
		globalF[dof + 1] += yload;
	}
}

void Model::Solve() {

	globalD.assign(2 * numNodes, 0.0);
	
	// algorithms like cholesky decomp require matrix to be symmetric
	// there is some tolerance issues causing asymmetry
	// so i will force symmetry by taking averages, this should be overall fine
	// since the stiffness matrix should be symmetric anyway
	for (size_t i = 0; i < globalK.size(); i++) {
		for (size_t j = 0; j < globalK.size(); j++) {
			globalK[i][j] = 0.5 * (globalK[i][j] + globalK[j][i]);
			globalK[j][i] = globalK[i][j];
		}
	}

	solver.Solve(globalK, globalF, globalD);

	// writeVectorToCSV(globalD, "./displacement.csv");
}

void Model::ProcessResults() {

	// convert global displacements to separate x and y vectors
	for (int i = 0; i < numNodes * 2; i++) {
		if (i % 2 == 0)
			globalDX.push_back(globalD[i]);
		else
			globalDY.push_back(globalD[i]);
	}
}