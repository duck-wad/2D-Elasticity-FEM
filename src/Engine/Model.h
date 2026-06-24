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
	const double GetThickness() const { return thickness; }
	void SetThickness(double thick) { thickness = thick; }
	void SetDebug(int d) { debug = d; }
	int IsDebug() const { return debug; }

	// related to dynamic settings
	void SetIsDynamic(int d) { isDynamic = d; }
	const int IsDynamic() const { return isDynamic; }
	void SetTimeStepSize(double s) { timeStepSize = s; }
	const int GetTimeStepSize() const { return timeStepSize; }
	void SetNumTimeSteps(int s) { numTimeStep = s; }
	const int GetNumTimeSteps() const { return numTimeStep; }
	void SetDynamicMethod(DynamicMethod d) { dynamicMethod = d; }
	const DynamicMethod GetDynamicMethod() const { return dynamicMethod; }

	// damping settings for dynamic analysis
	void SetDamping(int d, double alpha, double beta);
	int IsDamped() const { return isDamped; }
	double GetAlphaM() const { return alphaM; }
	double GetBetaK() const { return betaK; }

	std::map<std::string, Material>& GetMaterials() { return materials; }

	const int GetNumNodes() const { return numNodes; }
	void SetNumNodes(int num) { numNodes = num; }
	std::map<int, std::vector<double>>& GetNodes() { return nodes; }

	const ElementType GetElemType() const { return elemType; }
	void SetElemType(std::string type);
	int GetNumEls() const { return numEls; }
	void SetNumEls(int num) { numEls = num; }

	std::map<int, Element>& GetElements() { return elements; }

	void SetNumFixities(int num) { numFixities = num; }
	std::map<int, std::vector<int>>& GetFixities(){ return fixities; }

	void SetNumPointLoads(int num) { numPointLoads = num; }
	std::map<int, PointLoad>& GetPointLoads() { return pointLoads; }

	void SetNumDistLoads(int num) { numDistLoads = num; }
	std::map<int, DistributedLoad>& GetDistLoads() { return distLoads; }

	// for the dynamic point and distributed lodas
	void SetNumDynamicPointLoads(int n) { numDynamicPointLoads = n; }
	const int GetNumDynamicPointLoads() const { return numDynamicPointLoads; }
	std::map<int, std::vector<double>>& GetPointLoadHistory() { return pointLoadHistory; }
	void SetNumDynamicDistLoads(int n) { numDynamicDistLoads = n; }
	const int GetNumDynamicDistLoads() const { return numDynamicDistLoads; }
	std::map<int, std::vector<double>>& GetDistLoadHistory() { return distLoadHistory; }

	void Assemble();

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
	std::vector<int> constrainedDOFs;

	int numPointLoads;
	// key is load ID
	std::map<int, PointLoad> pointLoads;

	int numDistLoads;
	// dist loads store in map where load ID is key which maps to a distributed load struct
	std::map<int, DistributedLoad> distLoads;

	// global matrices
	std::vector<std::vector<double>> globalK;
	std::vector<std::vector<double>> globalM;
	std::vector<std::vector<double>> globalC;
	std::vector<double> globalF;
	std::vector<double> globalD;

	std::vector<double> globalDX;
	std::vector<double> globalDY;

	// to store the dynamic results
	std::vector<std::vector<double>> globalDispHistory;
	std::vector<std::vector<double>> globalVeloHistory;
	std::vector<std::vector<double>> globalAccelHistory;

	// dynamic settings
	int isDynamic;
	double timeStepSize;
	int numTimeStep;

	DynamicMethod dynamicMethod;

	int numDynamicPointLoads;
	// in dynamic point loads are scaled by a factor each time step
	// store those scalers in the map below
	// key is node ID (must make sure it is in pointLoads), vector is the scaler with size of numtimestep
	std::map<int, std::vector<double>> pointLoadHistory;

	// same process for the distributed load scalings. element id is key
	int numDynamicDistLoads;
	std::map<int, std::vector<double>> distLoadHistory;

	int isDamped;
	// rayleigh damping parameters
	double alphaM;
	double betaK;

	/* PRIVATE METHODS */

	// method to perform the creation of Element stiffness matrices
	void DiscretizeK();
	//void DiscretizeF(int currentStep = -1);
	void DiscretizeF();
	void DiscretizeM();
	void DiscretizeC();

	// assemble Element stiffness matrices into the global K
	void AssembleK();
	void AssembleF();
	void AssembleM(); 
	void AssembleC();

	// apply BCs by my modifying K and F
	// split into separate functions for dynamic. in dynamic the stiffness matrix only need
	// to have BC applied once, force should apply BC every time step
	void ApplyBCMatrix(std::vector<std::vector<double>>& mat);
	void ApplyBCVector(std::vector<double>& vec);

	void FindConstrainedDOFs();

	// these are applied directly to the nodes
	void ApplyPointLoads();

	void SolveStatic();
	void SolveDynamic();
};

