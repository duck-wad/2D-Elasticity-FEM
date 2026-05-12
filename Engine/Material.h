#pragma once
#include <iostream>
#include "Enums.h"
#include <vector>

/* Material contains code related to materials Ex) stiffness, Poisson ratio */

class Material
{
public:
	Material() {}
	Material(std::string& matname);
	void SetFormulation(std::string& form);
	void SetProperties(double _E, double _nu);

	void ConstructDMatrix(Assumption assumption);
	std::vector<std::vector<double>> GetDMatrix() { return D; }

private:
	std::string name;
	Formulation formulation;
	double E;
	double nu;
	double G;
	bool isOrthotropic = false;

	// constitutive matrix 
	std::vector<std::vector<double>> D;
};

