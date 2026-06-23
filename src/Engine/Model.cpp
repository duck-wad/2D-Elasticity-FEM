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

	isDynamic = 0;
	timeStepSize = 1.0;
	numTimeStep = 0;

	isDamped = 0;
	alphaM = 0;
	betaK = 0;
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

void Model::SetDamping(int d, double alpha, double beta) {
	isDamped = d;
	alphaM = alpha;
	betaK = beta;
}

// logic for assembling the global K, C, and M matrices
void Model::Assemble() {
	DiscretizeK();
	AssembleK();
	if (isDynamic) {
		DiscretizeM();
		DiscretizeC();
		AssembleM();
		AssembleC();
	}

	DiscretizeF();
	AssembleF();

	ApplyPointLoads();
}

/* METHODS FOR CONSTRUCTING ELEMENTAL MATRICES */

void Model::DiscretizeK() {

	// loop over list of elements and construct element stiffness
	for (auto& pair : elements) {
		int id = pair.first;
		Element& element = pair.second;
		element.ConstructK();
	}
}

void Model::DiscretizeF(int currentStep) {
	for (auto& pair : elements) {
		int id = pair.first;
		Element& element = pair.second;
		if (distLoads.count(id)) {


			element.ConstructF(distLoads[id]);
		}
	}
}

void Model::DiscretizeM() {
	//loop over list of elements construct elemental mass if model is dynamic
	for (auto& pair : elements) {
		Element& element = pair.second;
		element.ConstructM();
	}
}

void Model::DiscretizeC() {
	for (auto& pair : elements) {
		Element& element = pair.second;
		element.ConstructC(GetAlphaM(), GetBetaK());
	}
}

/* METHODS FOR GLOBAL ASSEMBLY */

void Model::AssembleK() {

	// resize global matrices as 2xnumnodes
	globalK.assign(2 * numNodes, std::vector<double>(2 * numNodes, 0.0));

	for (const auto& pair : elements) {

		const Element& element = pair.second;
		const auto& elemK = element.GetK();
		const auto& elemNodes = element.GetNodes();

		// store the DOFs in a vector
		// ex. node 1 has DOFs 0 and 1
		std::vector<int> dofMap;

		for (int node : elemNodes) {
			dofMap.push_back(2 * (node - 1));
			dofMap.push_back(2 * (node - 1) + 1);
		}

		for (size_t i = 0; i < dofMap.size(); i++) {
			for (size_t j = 0; j < dofMap.size(); j++) {

				globalK[dofMap[i]][dofMap[j]] += elemK[i][j];
			}
		}
	}
}

void Model::AssembleM() {
	// resize global matrices as 2xnumnodes
	globalM.assign(2 * numNodes, std::vector<double>(2 * numNodes, 0.0));

	for (const auto& pair : elements) {

		const Element& element = pair.second;
		const auto& elemM = element.GetM();
		const auto& elemNodes = element.GetNodes();

		// store the DOFs in a vector
		// ex. node 1 has DOFs 0 and 1
		std::vector<int> dofMap;

		for (int node : elemNodes) {
			dofMap.push_back(2 * (node - 1));
			dofMap.push_back(2 * (node - 1) + 1);
		}

		for (size_t i = 0; i < dofMap.size(); i++) {
			for (size_t j = 0; j < dofMap.size(); j++) {

				globalM[dofMap[i]][dofMap[j]] += elemM[i][j];
			}
		}
	}
}


void Model::AssembleC() {
	// resize global matrices as 2xnumnodes
	globalC.assign(2 * numNodes, std::vector<double>(2 * numNodes, 0.0));

	for (const auto& pair : elements) {

		const Element& element = pair.second;
		const auto& elemC = element.GetC();
		const auto& elemNodes = element.GetNodes();

		// store the DOFs in a vector
		// ex. node 1 has DOFs 0 and 1
		std::vector<int> dofMap;

		for (int node : elemNodes) {
			dofMap.push_back(2 * (node - 1));
			dofMap.push_back(2 * (node - 1) + 1);
		}

		for (size_t i = 0; i < dofMap.size(); i++) {
			for (size_t j = 0; j < dofMap.size(); j++) {

				globalC[dofMap[i]][dofMap[j]] += elemC[i][j];
			}
		}
	}
}

void Model::AssembleF() {
	globalF.assign(2 * numNodes, 0.0);
	for (const auto& pair : elements) {

		const Element& element = pair.second;
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
		}
	}
}

// operate directly on the global force vector
void Model::ApplyPointLoads() {

	for (auto const& pair : pointLoads) {
		int dof = 2 * (pair.first - 1);
		double xload = pair.second[0];
		double yload = pair.second[1];

		globalF[dof] += xload;
		globalF[dof + 1] += yload;
	}
}

void Model::ApplyBCMatrix(std::vector<std::vector<double>>& mat) {

	for (int i : constrainedDOFs) {
		for (int j = 0; j < 2 * numNodes; j++) {
			// zero out the row and column
			mat[i][j] = 0;
			mat[j][i] = 0;
		}
		// set the diagonal position to 1
		mat[i][i] = 1.0;

	}
}

void Model::ApplyBCVector(std::vector<double>& vec) {
	for (int i : constrainedDOFs) {
		// zero out the force vector to fix the DOF
		vec[i] = 0.0;
	}
}

void Model::FindConstrainedDOFs() {
	for (auto const& pair : fixities) {
		int dof = 2 * (pair.first - 1);
		if (pair.second[0])
			constrainedDOFs.push_back(dof);
		if (pair.second[1])
			constrainedDOFs.push_back(dof + 1);
	}
}

void Model::Solve() {

	globalD.assign(2 * numNodes, 0.0);
	
	if (isDynamic)
		SolveDynamic();
	else
		SolveStatic();
}

void Model::SolveStatic() {
	globalD.assign(2 * numNodes, 0.0);

	// algorithms like cholesky decomp require matrix to be symmetric
	// there is some tolerance issues causing asymmetry not sure why
	// so i will force symmetry by taking averages, this should be overall fine
	// since the stiffness matrix should be symmetric anyway
	makeSymmetric(globalK);

	// make an effectiveK matrix so avoid modifying the original K when applying BCs
	std::vector<std::vector<double>> effectiveK(globalK);
	std::vector<double> effectiveF(globalF);

	FindConstrainedDOFs();
	ApplyBCMatrix(effectiveK);
	ApplyBCVector(effectiveF);

	solver.Solve(effectiveK, effectiveF, globalD);
}

void Model::SolveDynamic() {

	/* CREATE THE EFFECTIVE STIFFNESS MATRIX */

	double coefM = 1.0;
	double coefC = 1.0;

	if (dynamicMethod == DynamicMethod::AVERAGE_ACCEL) {
		coefM = 4.0 / std::pow(timeStepSize, 2);
		coefC = 2.0 / timeStepSize;
	}
	else if (dynamicMethod == DynamicMethod::LINEAR_ACCEL) {
		coefM = 6.0 / std::pow(timeStepSize, 2);
		coefC = 3 / timeStepSize;
	}

	// this matrix does not change over time and can be initialized and BC apply once
	std::vector<std::vector<double>> effectiveK = (globalM * coefM) + (globalC * coefC) + globalK;
	ApplyBCMatrix(effectiveK);


	/* INITIALIZE TIME HISTORY CONTAINERS */
	globalDispHistory.assign(numTimeStep, std::vector<double>(2 * numNodes, 0.0));
	globalVeloHistory.assign(numTimeStep, std::vector<double>(2 * numNodes, 0.0));
	globalAccelHistory.assign(numTimeStep, std::vector<double>(2 * numNodes, 0.0));

	// for now assumne initial displacement and velocity are zero
	// want separate vectors for these in case later i want to have non-zero values
	std::vector<double> initialDisp(2 * numNodes, 0.0); 
	std::vector<double> initialVelo(2 * numNodes, 0.0);

	globalDispHistory[0] = initialDisp;
	globalVeloHistory[0] = initialVelo;


	/* FIRST TIME STEP */

	// initialize the force vector for the first time step using the scalers at first index

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