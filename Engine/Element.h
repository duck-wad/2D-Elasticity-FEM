#pragma once
#include <iostream>
#include <vector>

#include "Material.h"
#include "Enums.h"

/* This gausspoint class is for GPs within the body of the element */
class GaussPoint {
public:
	GaussPoint() {}
	GaussPoint(double _xi, double _eta, double _weight);

	void ComputeShapeFunction();
	void ComputeShapeFunctionDeriv();

	// functions for calculating stiffness contribution at the GP
	std::vector<std::vector<double>> ComputeStiffnessContribution(const std::vector<std::vector<double>>& coordinates, const std::vector<std::vector<double>>& DMatrix);

private:
	double xi;
	double eta;
	double weight;

	std::vector<double> N;
	std::vector<std::vector<double>> dN;
};

/* This gausspoint class is for GPs on the edge of element for traction calculation 
These don't need to be stored in Element since they are only used for preprocessing */
class GaussPointEdge {
public:
	GaussPointEdge() {}
};

class Element
{
public:
	Element(int _id, std::vector<int> _nodes, std::vector<std::vector<double>> coords, const Material* mat) : id(_id), nodes(_nodes), coordinates(coords), matptr(mat) {}

	void ConstructK(ElementType type);

private:
	int id;
	std::vector<int> nodes;
	std::vector<std::vector<double>> coordinates;
	const Material* matptr; //points to the Material in the map in Model

	std::vector<std::vector<double>> elemKMatrix; //8x8
	std::vector<double> elemFVector; //8x1

	// vector to store gausspoints for later ex) postprocessing
	std::vector<GaussPoint> gaussPoints;
};

