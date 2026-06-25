#include "FileWriter.h"
#include "Model.h"
#include "Utils.h"
#include <filesystem>

void FileWriter::WriteFile(const std::string& filename, Model& model) {
	output["num_nodes"] = model.GetNumNodes();
	output["is_dynamic"] = model.IsDynamic();
	output["displacements_x"] = model.GetDisplacementsX();
	output["displacements_y"] = model.GetDisplacementsY();

	if (model.IsDynamic()) {
		output["num_time_steps"] = model.GetNumTimeSteps();
		output["time_step_size"] = model.GetTimeStepSize();
		output["displacement_history_x"] = model.GetDispHistX();
		output["displacement_history_y"] = model.GetDispHistY();
		output["velocity_history_x"] = model.GetVeloHistX();
		output["velocity_history_y"] = model.GetVeloHistY();
		output["acceleration_history_x"] = model.GetAccelHistX();
		output["acceleration_history_y"] = model.GetAccelHistY();
	}

	if (model.IsDebug()) {
		std::string path = "./debug";
		std::filesystem::create_directories(path);
		writeVectorToCSV(model.GetDisplacementsX(), path + "/displacements_x.csv");
		writeVectorToCSV(model.GetDisplacementsY(), path + "/displacements_y.csv");

		if (model.IsDynamic()) {
			writeMatrixToCSV(model.GetDispHistX(), path + "/displacement_history_x.csv");
			writeMatrixToCSV(model.GetDispHistY(), path + "/displacement_history_y.csv");
			writeMatrixToCSV(model.GetVeloHistX(), path + "/velocity_history_x.csv");
			writeMatrixToCSV(model.GetVeloHistY(), path + "/velocity_history_y.csv");
			writeMatrixToCSV(model.GetAccelHistX(), path + "/acceleration_history_x.csv");
			writeMatrixToCSV(model.GetAccelHistY(), path + "/acceleration_history_y.csv");
		}
	}

	std::ofstream file(filename);
	file << output.dump(4);
}
