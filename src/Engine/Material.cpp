#include "Material.h"
#include "Utils.h"

Material::Material(std::string& matname) {
	name = matname;
	// defaults
	formulation = Formulation::LIN_ELASTIC;
	E = 200000;
	nu = 0.3;
	G = E / (2.0 * (1.0 + nu));
}

void Material::SetFormulation(std::string& form) {
	if (form == "LinearElastic") {
		formulation = Formulation::LIN_ELASTIC;
	}
	else if (form == "Plastic") {
		formulation = Formulation::PLASTIC;
	}
	else
		throw std::invalid_argument("Not a valid formulation");
}

void Material::SetProperties(double _E, double _nu) {
	E = _E;
	nu = _nu;
	G = E / (2.0 * (1.0 + nu));
}

void Material::SetThickness(double thickness) {
	t = thickness;
}

void Material::ConstructDMatrix(Assumption assumption) {
	if (assumption == Assumption::PLANE_STRESS) {
		double coef = E / (1.0 - nu * nu);
		D = {
			{1.0, nu, 0.0},
			{nu, 1.0, 0.0},
			{0.0, 0.0, (1.0 - nu) / 2.0}
		};
		D *= coef;		
	}
	else if (assumption == Assumption::PLANE_STRAIN) {
		double coef = E / ((1.0 + nu) * (1.0 - 2.0 * nu));
		D = {
				{1.0 - nu, nu, 0.0},
				{nu, 1.0 - nu, 0.0},
				{0.0, 0.0, (1.0 - nu) / 2.0}
		};
		D *= coef;
	}
}