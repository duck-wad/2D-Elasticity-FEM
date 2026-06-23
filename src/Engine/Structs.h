#pragma once

#include <vector>

struct DistributedLoad {
	std::vector<double> xvalues;
	std::vector<double> yvalues;
	int edgeIndex;
	std::vector<int> nodeIndex;
	double scale; // optinal scale for the case of dynamic problems
};

struct GaussPoint {
	double xi;
	double eta;
	double weight;
};