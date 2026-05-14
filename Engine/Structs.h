#pragma once

#include <vector>

struct DistributedLoad {
	std::vector<double> xvalues;
	std::vector<double> yvalues;
	int edgeIndex;
	std::vector<int> nodeIndex;
};