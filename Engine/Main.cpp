#include <iostream>
#include "FileReader.h"
#include "Model.h"

int main() {

	Model model;
	
	FileReader filereader;

	filereader.ReadFile("Input/INPUT.txt", model);

	model.Discretize(); // construct the elemental K matrices

}