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
	// store the distributed loads within each element
	// clear each elements loads before adding them in case this is called twice
	for (auto& [id, el] : elements) {
		el.ClearDistributedLoads();
	}
	for (auto& [id, load] : distLoads){
		Element& el = elements.at(load.element);
		if (currentStep > 0) {
			double scale = distLoadHistory[id][currentStep - 1];
			load.scale = scale;
		}
		el.AddDistributedLoad(load);
	}

	for (auto& [id, el] : elements){
		el.ConstructF();
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
void Model::ApplyPointLoads(int currentStep) {

	for (auto const& pair : pointLoads) {
		int dof = 2 * (pair.second.node - 1);
		double xload = pair.second.xvalue;
		double yload = pair.second.yvalue;

		double scale = 1.0;
		if (currentStep > 0)
			scale = pointLoadHistory[pair.first][currentStep - 1];

		globalF[dof] += (xload * scale);
		globalF[dof + 1] += (yload * scale);
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
	constrainedDOFs.clear();
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

	FindConstrainedDOFs();
	
	if (isDynamic)
		SolveDynamic();
	else
		SolveStatic();
}

void Model::SolveStatic() {

	DiscretizeF();
	AssembleF();

	ApplyPointLoads();

	// make an effectiveK matrix so avoid modifying the original K when applying BCs
	std::vector<std::vector<double>> effectiveK(globalK);
	std::vector<double> effectiveF(globalF);

	// algorithms like cholesky decomp require matrix to be symmetric
	// there is some tolerance issues causing asymmetry not sure why
	// so i will force symmetry by taking averages, this should be overall fine
	// since the stiffness matrix should be symmetric anyway
	makeSymmetric(effectiveK);

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
	// apply symmetry
	makeSymmetric(effectiveK);
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
	int currentStep = 1;

	// initialize the force vector for the first time step using the scalers at first index
	DiscretizeF(currentStep);
	AssembleF();
	ApplyPointLoads(currentStep);
	ApplyBCVector(globalF);
	std::vector<std::vector<double>> effectiveM(globalM);
	ApplyBCMatrix(effectiveM);

	// compute initial acceleration using M*a = f - C*v - K*d
	std::vector<double> effectiveF = globalF - (globalC * globalVeloHistory[0]) - (globalK * globalDispHistory[0]);
	solver.Solve(effectiveM, effectiveF, globalAccelHistory[0]);
	// now just to make sure, zero out the constrained DOFs in acceleration vector
	ApplyBCVector(globalAccelHistory[0]);

	currentStep += 1;

	/* LOOP OVER REST OF STEPS */
	while (currentStep <= numTimeStep) {
		DiscretizeF(currentStep);
		AssembleF();
		ApplyPointLoads(currentStep);

		if (dynamicMethod == DynamicMethod::AVERAGE_ACCEL) {
			effectiveF = globalF + globalM * (globalDispHistory[currentStep - 2] * (4.0 / std::pow(timeStepSize, 2)) + globalVeloHistory[currentStep - 2] * (4.0 / timeStepSize) + globalAccelHistory[currentStep - 2]) + globalC * (globalDispHistory[currentStep - 2] * (2.0 / timeStepSize) + globalVeloHistory[currentStep - 2]);
		}
		else if (dynamicMethod == DynamicMethod::LINEAR_ACCEL) {
			effectiveF = globalF + globalM * (globalDispHistory[currentStep - 2] * (6.0 / std::pow(timeStepSize, 2)) + globalVeloHistory[currentStep - 2] * (6.0 / timeStepSize) + globalAccelHistory[currentStep - 2] * 2.0) + globalC * (globalDispHistory[currentStep - 2] * (3.0 / timeStepSize) + globalVeloHistory[currentStep - 2] * 2.0 + globalAccelHistory[currentStep - 2] * (timeStepSize / 2.0));
		}
		else
			throw std::invalid_argument("Not a valid dynamic method");

		ApplyBCVector(effectiveF);

		// solve for the displacement at current step
		solver.Solve(effectiveK, effectiveF, globalDispHistory[currentStep - 1]);
		// now compute velocity and accel
		if (dynamicMethod == DynamicMethod::AVERAGE_ACCEL) {
			globalVeloHistory[currentStep - 1] = globalVeloHistory[currentStep - 2] + (globalDispHistory[currentStep - 1] - globalDispHistory[currentStep - 2] - globalVeloHistory[currentStep - 2] * timeStepSize) * (2.0 / timeStepSize);
			globalAccelHistory[currentStep - 1] = (globalDispHistory[currentStep - 1] - globalDispHistory[currentStep - 2] - globalVeloHistory[currentStep - 2] * timeStepSize) * (4.0 / std::pow(timeStepSize, 2)) - globalAccelHistory[currentStep - 2];
		}
		else if (dynamicMethod == DynamicMethod::LINEAR_ACCEL) {
			globalVeloHistory[currentStep - 1] = (globalDispHistory[currentStep - 1] - globalDispHistory[currentStep - 2]) * (3.0 / timeStepSize) - globalVeloHistory[currentStep - 2] * 2.0 - globalAccelHistory[currentStep - 2] * (timeStepSize / 2.0);
			globalAccelHistory[currentStep - 1] = (globalDispHistory[currentStep - 1] - globalDispHistory[currentStep - 2] - globalVeloHistory[currentStep - 2] * timeStepSize - globalAccelHistory[currentStep - 2] * (std::pow(timeStepSize, 2) / 3.0)) * (6.0 / std::pow(timeStepSize, 2));
		}

		// reinforce BC just in case
		ApplyBCVector(globalDispHistory[currentStep - 1]);
		ApplyBCVector(globalVeloHistory[currentStep - 1]);
		ApplyBCVector(globalAccelHistory[currentStep - 1]);

		currentStep += 1;
	}
}

void Model::ProcessResults() {

	if (isDynamic) {
		globalDispHistX.clear();
		globalDispHistY.clear();
		globalVeloHistX.clear();
		globalVeloHistY.clear();
		globalAccelHistX.clear();
		globalAccelHistY.clear();

		const size_t nSteps = globalDispHistory.size();
		globalDispHistX.resize(nSteps);
		globalDispHistY.resize(nSteps);
		globalVeloHistX.resize(nSteps);
		globalVeloHistY.resize(nSteps);
		globalAccelHistX.resize(nSteps);
		globalAccelHistY.resize(nSteps);

		for (size_t t = 0; t < nSteps; ++t) {
			globalDispHistX[t].resize(numNodes);
			globalDispHistY[t].resize(numNodes);
			globalVeloHistX[t].resize(numNodes);
			globalVeloHistY[t].resize(numNodes);
			globalAccelHistX[t].resize(numNodes);
			globalAccelHistY[t].resize(numNodes);

			for (int n = 0; n < numNodes; ++n) {
				const int dof = 2 * n;
				globalDispHistX[t][n] = globalDispHistory[t][dof];
				globalDispHistY[t][n] = globalDispHistory[t][dof + 1];
				globalVeloHistX[t][n] = globalVeloHistory[t][dof];
				globalVeloHistY[t][n] = globalVeloHistory[t][dof + 1];
				globalAccelHistX[t][n] = globalAccelHistory[t][dof];
				globalAccelHistY[t][n] = globalAccelHistory[t][dof + 1];
			}
		}

		// final time step for export (same layout as static globalDX/globalDY)
		globalDX.clear();
		globalDY.clear();
		if (!globalDispHistory.empty()) {
			globalD = globalDispHistory.back();
			for (int n = 0; n < numNodes; ++n) {
				globalDX.push_back(globalD[2 * n]);
				globalDY.push_back(globalD[2 * n + 1]);
			}
		}
	}
	else {
		// clear containers
		globalDX.clear();
		globalDY.clear();

		// convert global displacements to separate x and y vectors
		for (int i = 0; i < numNodes * 2; i++) {
			if (i % 2 == 0)
				globalDX.push_back(globalD[i]);
			else
				globalDY.push_back(globalD[i]);
		}
	}
}