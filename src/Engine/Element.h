#pragma once
#include <iostream>
#include <vector>

#include "Material.h"
#include "Enums.h"
#include "Structs.h"

class Element
{
public:
	Element(int _id, const std::vector<int>& _nodes, const std::vector<std::vector<double>>& coords, const Material* mat, ElementType _type);

	const std::vector<int>& GetNodes() const { return nodes; }

	void ConstructK();
	void ConstructM();
	void ConstructC(double alpha, double beta);
	void ConstructF(const std::vector<DistributedLoad>& loads);

	const std::vector<std::vector<double>>& GetK() const { return elemKMatrix; }
	const std::vector<std::vector<double>>& GetM() const { return elemMMatrix; }
	const std::vector<std::vector<double>>& GetC() const { return elemCMatrix; }
	const std::vector<double>& GetF() const { return elemFVector; }

private:

	void ComputeArea();

	// helper functions for constructing K and F
	void ConstructKQ4();
	void ConstructFQ4(const std::vector<DistributedLoad>& loads);
	// void ConstructKT3();
	// void ConstructFT3();

	// helper functions for shape functions
	std::vector<double> ComputeNQ4(double xi, double eta) const;
	std::vector<std::vector<double>> ComputedNQ4(double xi, double eta) const;

	int id;
	ElementType type;
	std::vector<int> nodes;
	std::vector<std::vector<double>> coordinates;
	const Material* matptr; //points to the Material in the map in Model

	double area;

	std::vector<std::vector<double>> elemKMatrix; 
	std::vector<std::vector<double>> elemMMatrix;
	std::vector<std::vector<double>> elemCMatrix;
	std::vector<double> elemFVector;

	// vector to store gausspoints for later ex) postprocessing
	std::vector<GaussPoint> gaussPoints;
};

