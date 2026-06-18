#pragma once
#include <iostream>
#include <vector>
#include <map>

#include "Material.h"
#include "Solver.h"
#include "Element.h"
#include "Enums.h"
#include "Structs.h"

/* Model contains the code related to the model. Ex) mesh, elements, etc. */

class Model
{
public:
	Model();

	/* Getters and setters */
	Solver& GetSolver() { return solver; }
	Assumption GetAssumption() const { return assumption; }
	void SetAssumption(std::string assump);
	double GetThickness() const { return thickness; }
	void SetThickness(double thick) { thickness = thick; }
	void SetDebug(int d) { debug = d; }
	int IsDebug() { return debug; }
	std::map<std::string, Material>& GetMaterials() { return materials; }

	int GetNumNodes() const { return numNodes; }
	void SetNumNodes(int num) { numNodes = num; }
	std::map<int, std::vector<double>>& GetNodes() { return nodes; }

	ElementType GetElemType() const { return elemType; }
	void SetElemType(std::string type);
	int GetNumEls() const { return numEls; }
	void SetNumEls(int num) { numEls = num; }

	std::map<int, Element>& GetElements() { return elements; }

	void SetNumFixities(int num) { numFixities = num; }
	std::map<int, std::vector<int>>& GetFixities(){ return fixities; }

	void SetNumPointLoads(int num) { numPointLoads = num; }
	std::map<int, std::vector<double>>& GetPointLoads() { return pointLoads; }

	void SetNumDistLoads(int num) { numDistLoads = num; }
	std::map<int, std::vector<DistributedLoad>>& GetDistLoads() { return distLoads; }

	// method to perform the initiate creation of Element stiffness matrices
	void Discretize();

	// assemble Element stiffness matrices into the global K
	void Assemble();

	// apply BCs by my modifying K and F
	void ApplyBC();

	// these are applied directly to the nodes
	void ApplyPointLoads();

	void Solve();

	// post processing calculations and reformatting for export
	void ProcessResults();

	const std::vector<double>& const GetDisplacements() { return globalD; }
	const std::vector<double>& const GetDisplacementsX() { return globalDX; }
	const std::vector<double>& const GetDisplacementsY() { return globalDY; }

private:
	Solver solver;
	double thickness;
	Assumption assumption;
	int debug;
	std::map<std::string, Material> materials;

	int numNodes;
	// store the nodes in map where node ID is key and vector(x,y) is value
	std::map<int, std::vector<double>> nodes;

	int numEls;
	ElementType elemType;
	std::map<int, Element> elements;

	int numFixities;
	// store the BCs in map where node ID is key and vector(x, y) is the fixity
	// 1 mean fixed, 0 mean free
	std::map<int, std::vector<int>> fixities;

	int numPointLoads;
	// key is node ID, value is vector for x and y load
	std::map<int, std::vector<double>> pointLoads;

	int numDistLoads;
	// dist loads store in map where element ID is key which maps to a distributed load struct
	// vector of distributed loads allows for multiple loads per element
	std::map<int, std::vector<DistributedLoad>> distLoads;

	// global matrices
	std::vector<std::vector<double>> globalK;
	std::vector<double> globalF;
	std::vector<double> globalD;

	std::vector<double> globalDX;
	std::vector<double> globalDY;
};

