#pragma once
#include <iostream>
#include <vector>

#include "Material.h"

class Element
{
public:
	Element(int _id, std::vector<int> _nodes, const Material* mat) : id(_id), nodes(_nodes), matptr(mat) {}

private:
	int id;
	std::vector<int> nodes;
	const Material* matptr; //points to the Material in the map in Model
};

