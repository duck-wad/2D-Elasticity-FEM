#include <iostream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif
#include "FileReader.h"
#include "FileWriter.h"
#include "Model.h"

namespace {

std::filesystem::path executable_directory() {
#ifdef _WIN32
	wchar_t buffer[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	if (length == 0 || length == MAX_PATH) {
		return std::filesystem::current_path();
	}
	return std::filesystem::path(buffer).parent_path();
#else
	return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

void use_executable_directory() {
	std::error_code ec;
	std::filesystem::current_path(executable_directory(), ec);
}

}  // namespace

int main() {
	use_executable_directory();

	Model model;

	FileReader filereader;

	filereader.ReadFile("INPUT.txt", model);

	// assemble the system of equations
	model.Assemble();
 
	model.Solve();

	model.ProcessResults();
	


	FileWriter filewriter;

	filewriter.WriteFile("OUTPUT.json", model);
}