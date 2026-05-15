#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <cassert>
#include <cmath>

const double LOW_TOL = 1e-8;

/* DEBUGGING FUNCTIONS */

template<typename T>
void writeVectorToCSV(const std::vector<T>& vector, const std::string& filename) {

	// Open the file in append mode
	std::ofstream file(filename);

	if (!file.is_open()) {
		throw std::ios_base::failure("Failed to open file for writing.");
	}

	for (size_t i = 0; i < vector.size(); i++) {
		file << vector[i] << "\n";
	}

	file.close();

	std::cout << "Vector written to " << filename << " succesfully." << std::endl;
}

template<typename T>
void writeMatrixToCSV(const std::vector<std::vector<T>>& matrix, const std::string& filename, const std::vector<std::string>& colTitles = {}) {

	// Open the file stream

	std::ofstream file(filename);

	if (!file.is_open()) {
		throw std::ios_base::failure("Failed to open file for writing.");
	}

	size_t cols = matrix[0].size();
	size_t rows = matrix.size();

	if (!colTitles.empty() && colTitles.size() == cols) {
		for (size_t j = 0; j < cols; j++) {
			file << colTitles[j];
			if (j < cols - 1) {
				file << ",";
			}
		}
		file << "\n";
	}

	// Write the matrix to the file in row-major order
	for (size_t i = 0; i < rows; ++i) {
		for (size_t j = 0; j < cols; ++j) {
			file << matrix[i][j];
			if (j < cols - 1) { // Add a comma unless it's the last column
				file << ",";
			}
		}
		file << "\n"; // Newline after each row
	}

	file.close();

	std::cout << "Matrix written to " << filename << " successfully." << std::endl;
}

template<typename T>
void printMatrix(const std::vector<std::vector<T>>& matrix) {
	size_t cols = matrix[0].size();
	size_t rows = matrix.size();

	for (size_t i = 0; i < rows; ++i) {
		for (size_t j = 0; j < cols; ++j) {
			std::cout << matrix[i][j] << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

template<typename T>
void printVector(const std::vector<T>& vector) {

	for (size_t i = 0; i < vector.size(); ++i) {
		std::cout << vector[i] << std::endl;
	}
	std::cout << std::endl;
}

/* END OF DEBUGGING FUNCTIONS */

/* MATRIX OPERATIONS */

template<typename T>
std::vector<std::vector<T>> operator*(
	const std::vector<std::vector<T>>& mat1,
	const std::vector<std::vector<T>>& mat2)
{
	if (mat1.empty() || mat2.empty()) {
		throw std::invalid_argument("Matrices must not be empty");
	}

	size_t mat1row = mat1.size();
	size_t mat1col = mat1[0].size();

	size_t mat2row = mat2.size();
	size_t mat2col = mat2[0].size();

	// Compatibility condition
	if (mat1col != mat2row) {
		throw std::invalid_argument(
			"Matrix dimensions are not compatible for multiplication");
	}

	// Output is (mat1row x mat2col)
	std::vector<std::vector<T>> output(
		mat1row,
		std::vector<T>(mat2col, T()));

	for (size_t i = 0; i < mat1row; i++) {
		for (size_t j = 0; j < mat2col; j++) {
			for (size_t k = 0; k < mat1col; k++) {
				output[i][j] += mat1[i][k] * mat2[k][j];
			}
		}
	}

	return output;
}

template<typename T>
std::vector<T> operator* (const std::vector<std::vector<T>>& mat, const std::vector<T>& vec) {
	if (mat.empty() || vec.empty()) {
		throw std::invalid_argument("Matrices must not be empty");
	}
	if (mat[0].size() != vec.size()) {
		throw std::invalid_argument("Matrix dimensions are not compatible for multiplication");
	}
	std::vector<T> output(mat.size(), T());

	for (size_t i = 0; i < mat.size(); i++) {
		for (size_t j = 0; j < vec.size(); j++) {
			output[i] += mat[i][j] * vec[j];
		}
	}
	return output;
}

template<typename T>
std::vector<std::vector<T>> operator*(const std::vector<std::vector<T>>& mat, const T c) {
	if (mat.empty()) {
		throw std::invalid_argument("Matrix cannot be empty");
	}

	std::vector<std::vector<T>> output(mat.size(), std::vector<T>(mat[0].size(), 0.0));

	for (size_t i = 0; i < mat.size(); i++) {
		for (size_t j = 0; j < mat[i].size(); j++) {
			output[i][j] = mat[i][j] * c;
		}
	}
	return output;
}

template<typename T>
std::vector<T> operator*(const std::vector<T>& vec, const T c) {
	if (vec.empty()) {
		throw std::invalid_argument("Vector cannot be empty");
	}
	std::vector<T> output(vec.size(), 0.0);
	for (size_t i = 0; i < vec.size(); i++) {
		output[i] += vec[i] * c;
	}
	return output;
}

template<typename T>
std::vector<T> operator/(const std::vector<T>& vec, const T c) {
	if (vec.empty()) {
		throw std::invalid_argument("Vector cannot be empty");
	}
	std::vector<T> output(vec.size());
	for (size_t i = 0; i < vec.size(); i++) {
		output[i] += vec[i] / c;
	}
	return output;
}

template<typename T>
std::vector<std::vector<T>>& operator*= (std::vector<std::vector<T>>& matrix, T scalar) {
	for (auto& row : matrix) {
		for (auto& value : row) {
			value *= scalar;
		}
	}
	return matrix;
}

template<typename T>
std::vector<T>& operator*= (std::vector<T>& vec, T scalar) {
	for (T& elem : vec) {
		elem *= scalar;
	}
	return vec;
}

template<typename T>
std::vector<std::vector<T>>& operator/= (std::vector<std::vector<T>>& matrix, T scalar) {
	for (auto& row : matrix) {
		for (auto& value : row) {
			value /= scalar;
		}
	}
	return matrix;
}

template<typename T>
std::vector<std::vector<T>> operator+(const std::vector<std::vector<T>>& mat1, const std::vector<std::vector<T>>& mat2) {

	if (mat1.empty() || mat2.empty()) {
		throw std::invalid_argument("Matrices must not be empty");
	}
	if (mat1.size() != mat2.size() || mat1[0].size() != mat2[0].size()) {
		throw std::invalid_argument("Matrices must be the same size for addition");
	}

	std::vector<std::vector<T>> output(mat1.size(), std::vector<T>(mat1[0].size(), T()));

	for (size_t i = 0; i < mat1.size(); i++) {
		for (size_t j = 0; j < mat1[0].size(); j++) {
			output[i][j] = mat1[i][j] + mat2[i][j];
		}
	}
	return output;
}

template<typename T>
std::vector<std::vector<T>> operator-(const std::vector<std::vector<T>>& mat1, const std::vector<std::vector<T>>& mat2) {

	if (mat1.empty() || mat2.empty()) {
		throw std::invalid_argument("Matrices must not be empty");
	}
	if (mat1.size() != mat2.size() || mat1[0].size() != mat2[0].size()) {
		throw std::invalid_argument("Matrices must be the same size for addition");
	}

	std::vector<std::vector<T>> output(mat1.size(), std::vector<T>(mat1[0].size(), T()));

	for (size_t i = 0; i < mat1.size(); i++) {
		for (size_t j = 0; j < mat1[0].size(); j++) {
			output[i][j] = mat1[i][j] - mat2[i][j];
		}
	}
	return output;
}

template<typename T>
std::vector<std::vector<T>>& operator+=(std::vector<std::vector<T>>& mat1, const std::vector<std::vector<T>>& mat2) {

	if (mat1.empty() || mat2.empty()) {
		throw std::invalid_argument("Matrices must not be empty");
	}
	if (mat1.size() != mat2.size() || mat1[0].size() != mat2[0].size()) {
		throw std::invalid_argument("Matrices must be the same size for addition");
	}

	for (size_t i = 0; i < mat1.size(); i++) {
		for (size_t j = 0; j < mat1[0].size(); j++) {
			mat1[i][j] += mat2[i][j];
		}
	}
	return mat1;
}

template<typename T>
std::vector<std::vector<T>>& operator-=(std::vector<std::vector<T>>& mat1, const std::vector<std::vector<T>>& mat2) {

	if (mat1.empty() || mat2.empty()) {
		throw std::invalid_argument("Matrices must not be empty");
	}
	if (mat1.size() != mat2.size() || mat1[0].size() != mat2[0].size()) {
		throw std::invalid_argument("Matrices must be the same size for addition");
	}

	for (size_t i = 0; i < mat1.size(); i++) {
		for (size_t j = 0; j < mat1[0].size(); j++) {
			mat1[i][j] -= mat2[i][j];
		}
	}
	return mat1;
}

template<typename T>
std::vector<T>& operator+=(std::vector<T>& v1, const std::vector<T>& v2) {

	if (v1.empty() || v2.empty()) {
		throw std::invalid_argument("Vectors must not be empty");
	}
	if (v1.size() != v2.size()) {
		throw std::invalid_argument("Vectors must be the same size for addition");
	}

	for (size_t i = 0; i < v1.size(); i++) {
		v1[i] += v2[i];
	}
	return v1;
}

template<typename T>
std::vector<T>& operator-=(std::vector<T>& v1, const std::vector<T>& v2) {

	if (v1.empty() || v2.empty()) {
		throw std::invalid_argument("Vectors must not be empty");
	}
	if (v1.size() != v2.size()) {
		throw std::invalid_argument("Vectors must be the same size for addition");
	}

	for (size_t i = 0; i < v1.size(); i++) {
		v1[i] -= v2[i];
	}
	return v1;
}

template<typename T>
std::vector<T> operator+(const std::vector<T>& v1, const std::vector<T>& v2) {

	if (v1.empty() || v2.empty()) {
		throw std::invalid_argument("Vectors must not be empty");
	}
	if (v1.size() != v2.size()) {
		throw std::invalid_argument("Vectors must be the same size for addition");
	}

	std::vector<T> output(v1.size());

	for (size_t i = 0; i < v1.size(); i++) {
		output[i] = v1[i] + v2[i];
	}
	return output;
}

template<typename T>
std::vector<T> operator-(const std::vector<T>& v1, const std::vector<T>& v2) {

	if (v1.empty() || v2.empty()) {
		throw std::invalid_argument("Vectors must not be empty");
	}
	if (v1.size() != v2.size()) {
		throw std::invalid_argument("Vectors must be the same size for addition");
	}

	std::vector<T> output(v1.size());

	for (size_t i = 0; i < v1.size(); i++) {
		output[i] = v1[i] - v2[i];
	}
	return output;
}

//tranpose an nxm matrix
template<typename T>
std::vector<std::vector<T>> transpose(const std::vector<std::vector<T>>& matrix) {

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
double determinant2x2(const std::vector<std::vector<T>>& matrix) {
	assert(matrix.size() == 2 && matrix[0].size() == 2 && "Matrix must be 2x2");
	return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
}

//invert 2x2 matrix and return new matrix
template<typename T>
std::vector<std::vector<T>> inverse2x2(std::vector<std::vector<T>>& matrix) {
	assert(matrix.size() == 2 && matrix[0].size() == 2 && "Matrix must be 2x2");

	std::vector<std::vector<T>> output(2, std::vector<T>(2));

	double det = determinant2x2(matrix);

	output[0][0] = matrix[1][1] / det;
	output[1][1] = matrix[0][0] / det;
	output[0][1] = -1 * matrix[0][1] / det;
	output[1][0] = -1 * matrix[1][0] / det;
	return output;
}

template <typename T>
std::vector<std::vector<T>> outerProduct(const std::vector<T>& vec1, const std::vector<T>& vec2) {
	size_t n = vec1.size();
	size_t m = vec2.size();

	//outer product is an nxm matrix
	std::vector<std::vector<T>> matrix(n, std::vector<T>(m, 0));

	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < m; ++j) {
			matrix[i][j] = vec1[i] * vec2[j];
		}
	}

	return matrix;
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

/* END OF MATRIX OPERATIONS */