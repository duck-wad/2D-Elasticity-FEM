#pragma once
#include <iostream>
#include "Model.h"

/* FileReader includes code to read the INPUT.txt file and set up the model 
Also handles the overall flow of the program */

enum class Section {
    NONE,
    GENERAL,
    MATERIALS,
    NODES,
    ELEMENTS,
    FIXITIES,
    DIST_LOADS
};

class FileReader
{
public:
    FileReader() {}

    void ReadFile(const std::string& filename);

private:
    std::string filename;
    void ReadGeneral(const std::string& line, Model& model);
	void ReadMaterials(const std::string& line, Model& model);
    void ReadNodes(const std::string& line, Model& model);
    void ReadElements(const std::string& line, Model& model);
    void ReadFixities(const std::string& line, Model& model);
    void ReadDistributedLoads(const std::string& line, Model& model);
};

