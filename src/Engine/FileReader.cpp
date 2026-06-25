#include <fstream>
#include <string>
#include <sstream>
#include "FileReader.h"
#include "Solver.h"

/* the ReadFile function will parse the input file and call the appropriate
functions based on the headers in the compute file */

void FileReader::ReadFile(const std::string& filename, Model& model) {
	std::string line;
	std::ifstream infile(filename);
	if (!infile) {
		throw std::invalid_argument("Error: Unable to open file.");
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
		if (line == "point loads:") {
			current = Section::POINT_LOADS;
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
		case Section::POINT_LOADS:
			ReadPointLoads(line, model, infile);
			break;
		case Section::DIST_LOADS:
			ReadDistributedLoads(line, model, infile);
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
	else if (junk == "debug:") {
		int debug;
		ss >> debug;
		model.SetDebug(debug);
	}
	else if (junk == "dynamic:") {
		int isDynamic;
		double stepSize;
		int numStep;
		std::string method;
		ss >> isDynamic >> junk >> stepSize >> junk >> numStep >> junk >> method;
		model.SetIsDynamic(isDynamic);
		model.SetTimeStepSize(stepSize);
		model.SetNumTimeSteps(numStep);
		
		if (method == "average_acceleration")
			model.SetDynamicMethod(DynamicMethod::AVERAGE_ACCEL);
		else if (method == "linear_acceleration")
			model.SetDynamicMethod(DynamicMethod::LINEAR_ACCEL);
		else
			throw std::invalid_argument("Not a valid dynamic method");
	}
	else if (junk == "damping:") {
		int isDamped;
		double alpha, beta;
		ss >> isDamped >> junk;
		if (isDamped) {
			ss >> alpha >> junk >> beta;
		}
		else {
			alpha = 0.0;
			beta = 0.0;
		}
		model.SetDamping(isDamped, alpha, beta);
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
		double gamma;
		ss >> junk >> E >> junk >> nu >> junk >> gamma;
		model.GetMaterials()[matname].SetProperties(E, nu, gamma);

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
	else
		throw std::invalid_argument("Not a valid header");

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
	else
		throw std::invalid_argument("Not a valid header");
}

void FileReader::ReadPointLoads(const std::string& line, Model& model, std::ifstream& infile) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "numloads:") {
		int num;
		ss >> num;

		model.SetNumPointLoads(num);
		return;
	}
	else if (junk == "id:") {
		int id;
		int node;
		std::vector<double> values(2);
		ss >> id >> junk >> node >> junk >> values[0] >> junk >> values[1];

		PointLoad load;
		load.node = node;
		load.xvalue = values[0];
		load.yvalue = values[1];

		if (model.GetNodes().count(node)) {
			model.GetPointLoads().emplace(id, load);
		}
		else
			throw std::invalid_argument("Load not applied to valid node");

		return;
	}
	else if (junk == "numdynamicloads:") {

		int num = 0;
		ss >> num;

		if (!model.IsDynamic() && num != 0) {
			throw std::invalid_argument("Static analysis cannot have a dynamic load");
		}

		model.SetNumDynamicPointLoads(num);
		return;
	}
	else if (junk == "start") {
		std::string templine;
		int currentStep = 0;
		double currentTime = 0.0;
		while (std::getline(infile, templine)) {
			std::stringstream ss(templine);
			ss >> junk;
			if (junk == "stop")
				break;
			else if (junk == "time:") {
				ss >> currentTime >> junk >> currentStep;
				continue;
			}
			else if (junk == "id:") {
				int id;
				double scale;
				ss >> id >> junk >> scale;
				// check if this id exists in the point loads
				if (!model.GetPointLoads().count(id))
					throw std::invalid_argument("Dynamic load does not have a valid ID");
				else
					model.GetPointLoadHistory()[id].push_back(scale);
			}
		}
		// double check
		if (currentStep != model.GetNumTimeSteps())
			throw std::invalid_argument("Time history points do not match the number of time steps");
		if (model.GetNumDynamicPointLoads() != model.GetPointLoadHistory().size())
			throw std::invalid_argument("Number of dynamic loads is invalid");

		// if there is a case where there are some static point loads included with the dynamic point loads
		// to simplify things later, include scalers for those static point loads but make them all 1.0
		// i hope this is not bad idea
		for (const auto& [key, value] : model.GetPointLoads()) {
			if (model.GetPointLoadHistory().find(key) == model.GetPointLoadHistory().end()) {
				model.GetPointLoadHistory()[key] = std::vector<double>(model.GetNumTimeSteps(), 1.0);
			}
		}
	}
	else
		throw std::invalid_argument("Not a valid header");
}

void FileReader::ReadDistributedLoads(const std::string& line, Model& model, std::ifstream& infile) {
	std::stringstream ss(line);
	std::string junk;

	ss >> junk;
	if (junk == "numloads:") {
		int num;
		ss >> num;

		model.SetNumDistLoads(num);

		return;
	}
	else if (junk == "id:") {
		int loadid;
		int elid;
		std::vector<int> nodes(2);
		std::vector<double> xvalues(2);
		std::vector<double> yvalues(2);

		ss >> loadid >> junk >> elid >> junk >> nodes[0] >> junk >> nodes[1] >> junk >> xvalues[0] >> junk >> xvalues[1] >> junk >> yvalues[0] >> junk >> yvalues[1];

		if (model.GetElements().count(elid)) {
			// must check if the edge the load is applied on is an exterior edge
			// i.e) the nodes must be adjacent to each other
			std::vector<int> elemNodes = model.GetElements().at(elid).GetNodes();
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
					load.element = elid;
					load.edgeIndex = static_cast<int>(i);
					load.xvalues = xvalues;
					load.yvalues = yvalues;
					load.nodeIndex = nodes;
					load.scale = 1.0;

					model.GetDistLoads().emplace(loadid, load);

					return;
				}
			}

			throw std::invalid_argument("Load not applied to a valid edge");
		}
		else
			throw std::invalid_argument("Load not applied to a valid element");
	}
	else if (junk == "numdynamicloads:") {
		int num = 0;
		ss >> num;

		if (!model.IsDynamic() && num != 0) {
			throw std::invalid_argument("Static analysis cannot have a dynamic load");
		}

		model.SetNumDynamicDistLoads(num);
		return;
	}
	else if (junk == "start") {
		std::string templine;
		int currentStep = 0;
		double currentTime = 0.0;
		while (std::getline(infile, templine)) {
			std::stringstream ss(templine);
			ss >> junk;
			if (junk == "stop")
				break;
			else if (junk == "time:") {
				ss >> currentTime >> junk >> currentStep;
				continue;
			}
			else if (junk == "id:") {
				int id;
				double scale;
				ss >> id >> junk >> scale;
				// check if this id exists in the distributed loads
				if (!model.GetDistLoads().count(id))
					throw std::invalid_argument("Dynamic load does not have valid ID");
				else
					model.GetDistLoadHistory()[id].push_back(scale);
			}
		}
		// double check
		if (currentStep != model.GetNumTimeSteps())
			throw std::invalid_argument("Time history points do not match the number of time steps");
		if (model.GetNumDynamicDistLoads() != model.GetDistLoadHistory().size())
			throw std::invalid_argument("Number of dynamic loads is invalid");

		for (const auto& [key, value] : model.GetDistLoads()) {
			if (model.GetDistLoadHistory().find(key) == model.GetDistLoadHistory().end()) {
				// for dist loads that are not changing with time, set their history to constant 1
				model.GetDistLoadHistory()[key] = std::vector<double>(model.GetNumTimeSteps(), 1.0);
			}
		}
	}
	else
		throw std::invalid_argument("Not a valid header");
}