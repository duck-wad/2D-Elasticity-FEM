#pragma once
#include <iostream>
#include <vector>
#include <map>

#include "Material.h"
#include "Solver.h"
#include "Element.h"

/* Model contains the code related to the model. Ex) mesh, elements, etc. */

enum class Assumption {
	PLANE_STRAIN,
	PLANE_STRESS
};

enum class ElementType {
	Q4,
	T3,
	Q8,
	T6
};

class Model
{
public:
	Model();

	/* Getters and setters */
	Solver& GetSolver() { return solver; }
	Assumption GetAssumption() { return assumption; }
	void SetAssumption(std::string assump);
	double GetThickness() { return thickness; }
	void SetThickness(double thick) { thickness = thick; }
	std::map<std::string, Material>& GetMaterials() { return materials; }

	int GetNumNodes() { return numNodes; }
	void SetNumNodes(int num) { numNodes = num; }
	std::map<int, std::vector<double>>& GetNodes() { return nodes; }

	ElementType GetElemType() { return elemType; }
	void SetElemType(std::string type);
	int GetNumEls() { return numEls; }
	void SetNumEls(int num) { numEls = num; }

	std::map<int, Element>& GetElements() { return elements; }

private:
	Solver solver;
	double thickness;
	Assumption assumption;
	std::map<std::string, Material> materials;

	int numNodes;
	// store the nodes in map where node ID is key and vector(x,y) is value
	std::map<int, std::vector<double>> nodes;

	int numEls;
	ElementType elemType;
	std::map<int, Element> elements;
};

