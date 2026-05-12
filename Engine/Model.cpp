#include "Model.h"

//default constructor 
Model::Model() {
	std::string default_solver = "GAUSSIAN_ELIMINATION";
	solver = Solver(default_solver);
	// by default assume plane strain assumption
	assumption = Assumption::PLANE_STRAIN;
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