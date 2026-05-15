#pragma once
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Model.h"

using json = nlohmann::json;

/* Handles writing model output to JSON */
class FileWriter
{
public:
	FileWriter() {}

	void WriteFile(const std::string& filename, Model& model);

private:
	json output;
};

