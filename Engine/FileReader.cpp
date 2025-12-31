#include <fstream>
#include <string>
#include <sstream>
#include "FileReader.h"
#include "Solver.h"

/* the constructor for FileReader will parse the input file and call the appropriate
functions based on the headers in the compute file */

void FileReader::ReadFile(const std::string& filename) {
	std::string line;
	std::ifstream infile(filename);
	if (!infile) {
		std::cerr << "Error: Unable to open file." << std::endl;
	}

	// create the model object
	Model model = Model();

	Section current = Section::NONE;

	while (std::getline(infile, line)) {
		if (line.empty()) continue;
		// read the section headers if they are there
		if (line == "general:") {
			current = Section::GENERAL;
			continue;
		}
		if (line == "materials:") {
			current = Section::MATERIALS;
			continue;
		}
		if (line == "nodes:") {
			current = Section::NODES;
			continue;
		}
		if (line == "elements:") {
			current = Section::ELEMENTS;
			continue;
		}

		// call the appropriate functions based on the current section
		switch (current) {
		case Section::GENERAL:
			ReadGeneral(line, model);
			break;
		case Section::MATERIALS:
			ReadMaterials(line, model);
			break;
		}

	}

}

void FileReader::ReadGeneral(const std::string& line, Model& model) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "solver:") {
		std::string type;
		double tol;
		int maxiter;
		ss >> type >> junk >> tol >> junk >> maxiter;
		model.solver = Solver(type, tol, maxiter);

		return;
	}
	else if (junk == "stages:") {
		int stages;
		ss >> stages;
		model.solver.numStages = stages;

		return;
	}
	else
		throw std::invalid_argument("Not a valid header");
}

void FileReader::ReadMaterials(const std::string& line, Model& model) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "material") {
		std::string matname;

		ss >> junk >> matname;
		model.materials.push_back(Material(matname));

		return;
	}
	// the rest of the headers, modify the last Material in the vector
	else if (junk == "elasticity") {
		std::string formulation;
		ss >> junk >> formulation;
		model.materials.back().SetFormulation(formulation);

		return;
	}
	else if (junk == "E:") {
		double E;
		double nu;
		ss >> E >> junk >> nu;
		model.materials.back().SetProperties(E, nu);

		return;
	}
	else
		throw std::invalid_argument("Not a valid header");
}