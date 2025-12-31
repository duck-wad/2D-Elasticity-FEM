#pragma once
#include <iostream>

/* Material contains code related to materials Ex) stiffness, Poisson ratio */

enum class ELASTICITY_FORMULATION {
	LIN_ELASTIC,
	PLASTIC
};

class Material
{
public:
	Material(std::string& matname);
	void SetFormulation(std::string& form);
	void SetProperties(double _E, double _nu);

private:
	std::string name;
	ELASTICITY_FORMULATION formulation;
	double E;
	double nu;
	double G;
	bool isOrthotropic = false;
};

