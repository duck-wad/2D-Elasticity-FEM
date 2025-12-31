#pragma once
#include <iostream>
#include <vector>

#include "Material.h"
#include "Solver.h"

/* Model contains the code related to the model. Ex) mesh, elements, etc. */

class Model
{
public:
	Model() {}
	Solver solver;
	std::vector<Material> materials;
};

