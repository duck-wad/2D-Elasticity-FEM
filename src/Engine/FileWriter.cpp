#include "FileWriter.h"
#include "Model.h"
#include "Utils.h"
#include <filesystem>

void FileWriter::WriteFile(const std::string& filename, Model& model) {
	output["num_nodes"] = model.GetNumNodes();
	output["displacements_x"] = model.GetDisplacementsX();
	output["displacements_y"] = model.GetDisplacementsY();

	if (model.IsDebug()) {
		std::string path = "./debug";
		std::filesystem::create_directories(path);
		writeVectorToCSV(model.GetDisplacementsX(), path + "/displacements_x.csv");
		writeVectorToCSV(model.GetDisplacementsY(), path + "/displacements_y.csv");
	}

	std::ofstream file(filename);
	file << output.dump(4);
}