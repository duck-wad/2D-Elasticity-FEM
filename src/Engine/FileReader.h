#pragma once
#include <iostream>
#include "Model.h"
#include "Enums.h"

/* FileReader includes code to read the INPUT.txt file and set up the model */

class FileReader
{
public:
    FileReader() {}

    void ReadFile(const std::string& filename, Model& model);

private:
    std::string filename;
    void ReadGeneral(const std::string& line, Model& model);
	void ReadMaterials(const std::string& line, Model& model);
    void ReadNodes(const std::string& line, Model& model);
    void ReadElements(const std::string& line, Model& model);
    void ReadFixities(const std::string& line, Model& model);
    void ReadPointLoads(const std::string& line, Model& model, std::ifstream& infile);
    void ReadDistributedLoads(const std::string& line, Model& model, std::ifstream& infile);
};

