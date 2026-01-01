#pragma once
#include <iostream>

/* Material contains code related to materials Ex) stiffness, Poisson ratio */

enum class ElasticityFormulation {
	LIN_ELASTIC,
	PLASTIC
};

class Material
{
public:
	Material() {}
	Material(std::string& matname);
	void SetFormulation(std::string& form);
	void SetProperties(double _E, double _nu);

private:
	std::string name;
	ElasticityFormulation formulation;
	double E;
	double nu;
	double G;
	bool isOrthotropic = false;
};

