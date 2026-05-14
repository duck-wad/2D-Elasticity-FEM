#include <fstream>
#include <string>
#include <sstream>
#include "FileReader.h"
#include "Solver.h"

/* the constructor for FileReader will parse the input file and call the appropriate
functions based on the headers in the compute file */

void FileReader::ReadFile(const std::string& filename, Model& model) {
	std::string line;
	std::ifstream infile(filename);
	if (!infile) {
		std::cerr << "Error: Unable to open file." << std::endl;
	}

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

		// construct D matrix here. not sure if this is the correct spot rn
		model.GetMaterials()[matname].ConstructDMatrix(model.GetAssumption());

		model.GetMaterials()[matname].SetThickness(model.GetThickness());

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
		std::vector<std::vector<double>> coords;
		std::string matname;

		if (model.GetElemType() == ElementType::Q4)
			nodes.resize(4);
		else if (model.GetElemType() == ElementType::T3)
			nodes.resize(3);
		else if (model.GetElemType() == ElementType::Q8)
			nodes.resize(8);
		else if (model.GetElemType() == ElementType::T6)
			nodes.resize(6);

		coords.resize(nodes.size());

		ss >> id >> junk;
		// need to parse the nodes, which are presented as (1, 2, 3, 4)
		for (int i = 0; i < nodes.size(); ++i) {
			ss >> nodes[i];
		}
		// store the coordinates in the Element
		for (int i = 0; i < nodes.size(); ++i) {
			std::vector<double> coord = model.GetNodes()[nodes[i]];
			coords[i] = coord;
		}

		ss >> junk >> matname;

		Material* mat = &(model.GetMaterials().at(matname));
		Element el = Element(id, nodes, coords, mat, model.GetElemType());

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

		// must check if it is applied to a valid node number
		if (model.GetNodes().count(id)) {
			model.GetFixities().emplace(id, fixity);
		}
		else
			throw std::invalid_argument("Fixity not applied to a valid node");

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
		std::vector<double> xvalues(2);
		std::vector<double> yvalues(2);

		ss >> id >> junk >> nodes[0] >> junk >> nodes[1] >> junk >> xvalues[0] >> junk >> xvalues[1] >> junk >> yvalues[0] >> junk >> yvalues[1];

		if (model.GetElements().count(id)) {
			// must check if the edge the load is applied on is an exterior edge
			// i.e) the nodes must be adjacent to each other
			std::vector<int> elemNodes = model.GetElements().at(id).GetNodes();
			for (size_t i = 0; i < elemNodes.size(); i++) {

				int node1 = elemNodes[i];
				int node2 = elemNodes[(i + 1) % elemNodes.size()];

				if ((node1 == nodes[0] && node2 == nodes[1]) || (node1 == nodes[1] && node2 == nodes[0])) {
					// if the nodes are swapped need to swap xvalues[0] and xvalues[1]
					// ex. if the element reads local node 3 and 4 as 14 13 but the compute has
					// it written as 13 14
					// this is pretty shit lol
					if ((node1 == nodes[1] && node2 == nodes[0])) {
						double xtemp = xvalues[0];
						xvalues[0] = xvalues[1];
						xvalues[1] = xtemp;
						double ytemp = yvalues[0];
						yvalues[0] = yvalues[1];
						yvalues[1] = ytemp;
					}

					DistributedLoad load;
					load.edgeIndex = static_cast<int>(i);
					load.xvalues = xvalues;
					load.yvalues = yvalues;
					load.nodeIndex = nodes;

					model.GetDistLoads()[id].push_back(load);

					return;
				}
			}

			throw std::invalid_argument("Load not applied to a valid edge");
		}
		else
			throw std::invalid_argument("Load not applied to a valid element");
	}
}