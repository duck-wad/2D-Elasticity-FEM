#include "Material.h"

Material::Material(std::string& matname) {
	name = matname;
	// defaults
	formulation = ElasticityFormulation::LIN_ELASTIC;
	E = 200000;
	nu = 0.3;
	G = E / (2.0 * (1.0 + nu));
}

void Material::SetFormulation(std::string& form) {
	if (form == "LinearElastic") {
		formulation = ElasticityFormulation::LIN_ELASTIC;
	}
	else if (form == "Plastic") {
		formulation = ElasticityFormulation::PLASTIC;
	}
	else
		throw std::invalid_argument("Not a valid formulation");
}

void Material::SetProperties(double _E, double _nu) {
	E = _E;
	nu = _nu;
	G = E / (2.0 * (1.0 + nu));
}