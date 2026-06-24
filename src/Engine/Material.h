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
	void SetProperties(double _E, double _nu, double gamma);
	void SetThickness(double thickness);

	void ConstructDMatrix(Assumption assumption);
	const std::vector<std::vector<double>>& GetDMatrix() const { return D; }
	double GetThickness() const { return t; }
	double GetDensity() const { return density; }

private:
	std::string name;
	Formulation formulation;
	double E;
	double nu;
	double unit_weight; // units are N/m3
	double density;
	double G;
	double t;
	bool isOrthotropic = false;

	// constitutive matrix 
	std::vector<std::vector<double>> D;
};

