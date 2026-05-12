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
		if (line == "fixities:") {
			current = Section::FIXITIES;
			continue;
		}
		if (line == "distributed loads:") {
			current = Section::DIST_LOADS;
			continue;
		}
		if (line == "end") {
			return;
		}

		// call the appropriate functions based on the current section
		switch (current) {
		case Section::GENERAL:
			ReadGeneral(line, model);
			break;
		case Section::MATERIALS:
			ReadMaterials(line, model);
			break;
		case Section::NODES:
			ReadNodes(line, model);
			break;
		case Section::ELEMENTS:
			ReadElements(line, model);
			break;
		case Section::FIXITIES:
			ReadFixities(line, model);
			break;
		case Section::DIST_LOADS:
			ReadDistributedLoads(line, model);
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
		model.GetSolver() = Solver(type, tol, maxiter);

		return;
	}
	else if (junk == "stages:") {
		int stages;
		ss >> stages;
		model.GetSolver().numStages = stages;

		return;
	}
	else if (junk == "assumption:") {
		std::string assumption;
		double thickness = 1; // default of thickness 1 for plane strain
		ss >> assumption; 
		if (assumption == "plane_stress") {
			ss >> junk >> thickness;
		}
        model.SetAssumption(assumption);
        model.SetThickness(thickness);
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
		model.GetMaterials().emplace(matname, Material(matname));

		std::string formulation;
		ss >> junk >> formulation;
		model.GetMaterials()[matname].SetFormulation(formulation);

		// eventually need to add if else statements for different formulations
		double E;
		double nu;
		ss >> junk >> E >> junk >> nu;
		model.GetMaterials()[matname].SetProperties(E, nu);

		return;
	}
	else
		throw std::invalid_argument("Not a valid header");
}

void FileReader::ReadNodes(const std::string& line, Model& model) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "numnodes:") {
		int num;
		ss >> num;
		model.SetNumNodes(num);

		return;
	}
	else if (junk == "node:") {
		int id;
		std::vector<double> coord(2); // (x, y)
		ss >> id >> junk >> coord[0] >> junk >> coord[1];
		model.GetNodes().emplace(id, coord);

		return;
	}
	else
		throw std::invalid_argument("Not a valid header");
}

void FileReader::ReadElements(const std::string& line, Model& model) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "numelem:") {
		int num;
		std::string type;
		ss >> num >> junk >> type;
		model.SetElemType(type);
		model.SetNumEls(num);

		return;
	}
	else if (junk == "element:") {
		int id;
		std::vector<int> nodes;
		std::string matname;

		if (model.GetElemType() == ElementType::Q4)
			nodes.resize(4);
		else if (model.GetElemType() == ElementType::T3)
			nodes.resize(3);
		else if (model.GetElemType() == ElementType::Q8)
			nodes.resize(8);
		else if (model.GetElemType() == ElementType::T6)
			nodes.resize(6);

		ss >> id >> junk;
		// need to parse the nodes, which are presented as (1, 2, 3, 4)
		for (int i = 0; i < nodes.size(); ++i) {
			ss >> nodes[i];
		}

		ss >> junk >> matname;

		Material* mat = &(model.GetMaterials().at(matname));
		Element el = Element(id, nodes, mat);

		model.GetElements().emplace(id, el);

		return;
	}

}

void FileReader::ReadFixities(const std::string& line, Model& model) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "numfix:") {
		int num;
		ss >> num;
		model.SetNumFixities(num);
		
		return;
	}
	else if (junk == "node:") {
		int id;
		std::vector<int> fixity(2);

		ss >> id >> junk >> fixity[0] >> junk >> fixity[1];

		model.GetFixities().emplace(id, fixity);

		return;
	}
}

void FileReader::ReadDistributedLoads(const std::string& line, Model& model) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "numloads:") {
		int num;
		ss >> num;

		model.SetNumDistLoads(num);

		return;
	}
	else if (junk == "element:") {
		int id;
		std::vector<int> nodes(2);
		std::vector<double> values(2);

		ss >> id >> junk >> nodes[0] >> junk >> nodes[1] >> junk >> values[0] >> junk >> values[1];

		DistributedLoad load;
		load.edgeNodes = nodes;
		load.loadValues = values;

		model.GetDistLoads().emplace(id, load);
	}
}