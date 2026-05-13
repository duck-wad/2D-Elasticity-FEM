#pragma once
#include <iostream>
#include <vector>

#include "Material.h"
#include "Enums.h"

class Element
{
public:
	Element(int _id, std::vector<int> _nodes, std::vector<std::vector<double>> coords, const Material* mat) : id(_id), nodes(_nodes), coordinates(coords), matptr(mat) {}

	void ConstructKandF(ElementType type);

private:
	int id;
	std::vector<int> nodes;
	std::vector<std::vector<double>> coordinates;
	const Material* matptr; //points to the Material in the map in Model

	std::vector<std::vector<double>> elemKMatrix; //8x8
	std::vector<double> elemFVector; //8x1
};

class GaussPoint {
public:
	GaussPoint(double xi, double eta, double weight, std::vector<std::vector<double>> coordinates, std::vector<std::vector<double>> DMatrix);

	void ComputeShapeFunction(double xi, double eta, std::vector<double>& N);
	void ComputeShapeFunctionDeriv(double xi, double eta, std::vector<std::vector<double>>& dN);

	std::vector<std::vector<double>> GetKProduct() { return KProduct; }

private:
	std::vector<std::vector<double>> KProduct;
	std::vector<double> FVector;
};