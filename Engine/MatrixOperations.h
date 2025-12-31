#pragma once

#include <iostream>
#include <vector>
#include <cassert>

/* Helper file for solver algorithms */

const double LOW_TOL = 1e-8;

template<typename T>
void PrintVector(const std::vector<T>& vec) {
	for (size_t i = 0; i < vec.size(); i++) {
		std::cout << vec[i] << std::endl;
	}
	std::cout << std::endl;
}

template<typename T>
void PrintMatrix(const std::vector<std::vector<T>>& mat) {
	for (size_t i = 0; i < mat.size(); i++) {
		for (size_t j = 0; j < mat[i].size(); j++) {
			std::cout << mat[i][j] << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

template<typename T>
std::vector<T> operator* (const std::vector<T> row, const T c) {
	std::vector<T> output(row.size());

	for (size_t i = 0; i < row.size(); i++) {
		output[i] = row[i] * c;
	}
	return output;
}

//subtract row2 from row1
template<typename T>
std::vector<T> operator- (const std::vector<T> row1, const std::vector<T> row2) {

	if (row1.size() != row2.size()) {
		throw std::invalid_argument("Rows must be same size to subtract");
	}

	std::vector<T> output(row1.size());

	for (size_t i = 0; i < row1.size(); i++) {
		output[i] = row1[i] - row2[i];
	}
	return output;
}

//tranpose an nxm matrix
template<typename T>
std::vector<std::vector<T>> Transpose(const std::vector<std::vector<T>>& matrix) {

	size_t rows = matrix.size();
	assert(rows != 0 && "Matrix must be populated to be transposed");
	size_t cols = matrix[0].size();

	std::vector<std::vector<T>> output(cols, std::vector<T>(rows));

	for (size_t i = 0; i < rows; i++) {
		for (size_t j = 0; j < cols; j++) {
			output[j][i] = matrix[i][j];
		}
	}
	return output;
}

template<typename T>
bool isSingular(const std::vector<std::vector<T>>& matrix) {

	for (size_t i = 0; i < matrix.size(); i++) {
		if (std::abs(matrix[i][i]) < LOW_TOL) {
			return true;
		}
	}
	return false;
}

template<typename T>
bool isSymmetric(const std::vector<std::vector<T>>& matrix) {

	if (matrix.size() != matrix[0].size()) {
		return false;
	}

	for (size_t i = 0; i < matrix.size(); i++) {
		for (size_t j = 0; j < matrix.size(); j++) {
			if (matrix[i][j] != matrix[j][i]) {
				return false;
			}
		}
	}
	return true;
}