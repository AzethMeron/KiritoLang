#ifndef KIRITO_TEMPLATE_MATRIX_HPP
#define KIRITO_TEMPLATE_MATRIX_HPP

#include <vector>
#include <exception>
#include <algorithm>
#include <numeric>
#include <initializer_list>
#include <execution>
#include <vector>
#include <string>
#include <functional>

#include "Bool.hpp"
#include "Number.hpp"
#include "Math.hpp"
#include "Misc.hpp"

namespace Kirito
{
	// Implementing this class as a template although swapping TYPE=Number with something else requires some overloads in Math namespace
	// TYPE must also have method CastString returning Kirito's String
	// Also Matrix is used to model both matrices and vectors
	template<typename TYPE>
	class TemplateMatrix
	{
		private:
			std::vector<std::vector<TYPE>> _data; // [row][col]
			size_t _rows;
			size_t _cols;
			static void EnsureInteger(const std::initializer_list<Number>& list)
			{
				for(const Number& i : list)
				{
					if (!i.IsInteger()) // TODO
					{
						throw std::exception( Misc::StdStringFormat("Index must be an integer, got %s", i.CastString().c_str()).c_str() );
					}
				}
			}
			TYPE  At(const size_t& r, const size_t& c) const { return this->_data.at(r).at(c); } // Just to silence some warnings
			TYPE& At(const size_t& r, const size_t& c) { return this->_data.at(r).at(c); } // Just to silence some warnings

			void _ForEach(const std::function<void(const size_t& row, const size_t& col)>& func) const
			{
				std::vector<size_t> rows(this->_rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(this->_cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								func(row, col);
							}
						);
					}
				);
			}
		public:
			TemplateMatrix(const Number& r, const Number& c, const TYPE& val = 0)
			{
				TemplateMatrix::EnsureInteger({ r,c });
				if ((r.IsZero() && !c.IsZero()) || (!r.IsZero() && c.IsZero())) { throw std::exception("Matrix can't have zero rows/columns, unless it's completely empty matrix."); }
				this->_rows = (double)r.Round(); // No, data loss is impossible
				this->_cols = (double)c.Round(); // No, data loss is impossible
				this->_data = std::vector<std::vector<TYPE>>(this->_rows);
				// Shorting very long commands in next statement
				auto& cols = this->_cols;
				auto& ref = this->_data; 
				// Parallel initialisation
				std::for_each(
					std::execution::par,
					std::begin(ref),
					std::end(ref),
					[&](std::vector<TYPE>& i) {
						i = std::vector<TYPE>(cols,val);
					}
				);
			}
			TemplateMatrix(const std::vector<std::vector<TYPE>>& list)
			{
				this->_rows = list.size();
				this->_cols = 0;
				bool cols_initialized = false;
				for (const auto& i : list)
				{
					if (cols_initialized)
					{
						if (i.size() != this->_cols)
						{
							throw std::exception(Misc::StdStringFormat("Inconsident dimensions in Matrix bracket initialisation, expected %ld columns, got %ld", this->_cols, i.size()).c_str());
						}
					}
					else
					{
						this->_cols = i.size();
						cols_initialized = true;
					}
					this->_data.push_back(i);
				}
			}
			static TemplateMatrix Diagonal(const Number& size, const TYPE& val = 1.0)
			{
				TemplateMatrix::EnsureInteger({ size });
				auto output = TemplateMatrix(size, size);
				for (size_t i = 0; i < (double)size.Round(); ++i)
				{
					output.At(i, i) = val;
				}
				return output;
			}
			TemplateMatrix Transpose() const
			{
				auto output = TemplateMatrix(this->Columns(), this->Rows());
				std::vector<size_t> rows(this->_rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(this->_cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(col, row) = this->At(row, col);
							}
						);
					}
				);
				return output;
			}
			Number Rows() const { return (double) this->_rows; }
			Number Columns() const { return (double) this->_cols; }
			Number Size() const { return this->_cols * this->_rows; }
			TYPE  At(const Number& r, const Number& c) const { TemplateMatrix::EnsureInteger({ r,c }); return this->_data.at((double)r).at((double)c); }
			TYPE& At(const Number& r, const Number& c) { TemplateMatrix::EnsureInteger({r,c}); return this->_data.at((double)r).at((double)c); }
			Bool Exists(const Number& r, const Number& c) const
			{
				TemplateMatrix::EnsureInteger({ r,c });
				if (r >= 0 && r < this->Rows())
					if (c >= 0 && c < this->Columns())
						return true;
				return false;
			}
			Bool IsSquare() const
			{
				return (this->_rows == this->_cols);
			}
			Bool IsEmpty() const
			{
				return (this->_rows == 0) || (this->_cols == 0);
			}
			Bool IsVector() const
			{
				return (this->_rows == 1) || (this->_cols == 1);
			}
			TemplateMatrix RowVector() const
			{
				if (!this->IsVector()) { throw std::exception(Misc::StdStringFormat("Can't convert matrix of shape (%ld,%ld) into row vector", this->_rows, this->_cols).c_str()); }
				if (this->_rows == 1) return *this;
				return this->Transpose();
			}
			TemplateMatrix ColumnVector() const
			{
				if (!this->IsVector()) { throw std::exception(Misc::StdStringFormat("Can't convert matrix of shape (%ld,%ld) into column vector", this->_rows, this->_cols).c_str()); }
				if (this->_cols == 1) return *this;
				return this->Transpose();
			}
			String CastString() const
			{
				std::string output = "";
				for (size_t row = 0; row < this->_rows; ++row)
				{
					std::string string_row = "[ ";
					for (size_t col = 0; col < this->_cols; ++col)
					{
						string_row += this->At(row, col).CastString().Value() + std::string(" ");
					}
					output += string_row + std::string("]\n");
				}
				return output;
			}
			TemplateMatrix operator + (const TemplateMatrix& m) const
			{
				if ((this->_rows != m._rows) || (this->_cols != m._cols)) {
					throw std::exception(Misc::StdStringFormat("Wrong dimension for matrix addition: (%ld,%ld) and (%ld,%ld)", this->_rows, this->_cols, m._rows, m._cols).c_str());
				}
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(row, col) = this->At(row, col) + m.At(row,col);
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix operator - (const TemplateMatrix& m) const
			{
				if ((this->_rows != m._rows) || (this->_cols != m._cols)) {
					throw std::exception(Misc::StdStringFormat("Wrong dimension for matrix subtraction: (%ld,%ld) and (%ld,%ld)", this->_rows, this->_cols, m._rows, m._cols).c_str());
				}
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(row, col) = this->At(row, col) - m.At(row, col);
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix operator * (const TemplateMatrix& m) const
			{
				if ((this->_cols != m._rows)) {
					throw std::exception(Misc::StdStringFormat("Wrong dimension for matrix multiplication: (%ld,%ld) and (%ld,%ld)", this->_rows, this->_cols, m._rows, m._cols).c_str());
				}
				auto output = TemplateMatrix(this->Rows(), m.Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0); 
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0); 
				std::for_each( 
					std::execution::par, 
					std::begin(rows),
					std::end(rows), 
					[&](const size_t& row) {
						std::for_each(
							std::execution::par, 
							std::begin(cols),
							std::end(cols), 
							[&](const size_t& col) { 
								TYPE sum = 0;
								for (size_t i = 0; i < this->_cols; ++i)
								{
									sum = sum + this->At(row, i) * m.At(i, col);
								}
								output.At(row, col) = sum;
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix operator + (const TYPE& val) const
			{
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(row, col) = this->At(row, col) + val;
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix operator - (const TYPE& val) const
			{
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(row, col) = this->At(row, col) - val;
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix operator * (const TYPE& val) const
			{
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(row, col) = this->At(row, col) * val;
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix operator / (const TYPE& val) const
			{
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								output.At(row, col) = this->At(row, col) / val;
							}
						);
					}
				);
				return output;
			}
			// TODO
			TYPE Determinant() const
			{
				if (!this->IsSquare()) { throw std::exception("Can't compute determinant of non-square matrix."); }
				// Calculation of determinant using gaussian elimination requires modifications to matrix
				// We don't want to modify real object, so we create a copy
				TemplateMatrix copy = *this;
				// Row swap causes reversal of sign, so we need to store info about it somewhere
				TYPE sign = 1;
				// Main loop: i goes through trail (diagonal)
				for (size_t i = 0; i < copy._rows; ++i)
				{
					// If 0 on diagonal, we need to swap rows
					if (copy.At(i, i).IsZero())
					{
						// Looking for non-zero rows below our currently analysed row
						for (size_t j = i+1; j < copy._rows; ++j)
						{
							// Is it different than 0 ?
							if (!copy.At(j, i).IsZero())
							{
								// Swap, flip sign and stop searching
								for (size_t c = 0; c < copy._cols; ++c) { std::swap(copy.At(i, c), copy.At(j, c)); }
								sign = sign * (-1.0);
								break;
							}
						}
						// If after that value at diagonal is still 0, we've det = 0
						if (copy.At(i, i).IsZero()) { return 0; }
					}
					// Zeroing all elements in this column below this row
					for (size_t j = i + 1; j < copy._rows; ++j)
					{
						TYPE multiplier = (copy.At(j, i) / copy.At(i, i)) * (-1.0);
						for (size_t c = 0; c < copy._cols; ++c) { copy.At(j, c) = copy.At(j, c) + copy.At(i, c) * multiplier; }
					}
				}
				// So now we have triangle (stairs-like) matrix
				//std::cout << copy.CastString() << std::endl;
				// Calculating determinant
				TYPE determinant = 1;
				for (size_t i = 0; i < copy._rows; ++i)
				{
					determinant = determinant * copy.At(i, i);
				}
				return determinant * sign;
			}
			TemplateMatrix RemoveRowAndColumn(const Number& row, const Number& col) const
			{
				// Error management
				if (row < TYPE(0) || row >= this->Rows()) throw std::exception(Misc::StdStringFormat("Internal error: trying to remove non-existant row '%s'", row.CastString().c_str()).c_str());
				if (col < TYPE(0) || col >= this->Columns()) throw std::exception(Misc::StdStringFormat("Internal error: trying to remove non-existant column '%s'", col.CastString().c_str()).c_str());
				// Meat goes here
				TemplateMatrix::EnsureInteger({ row, col });
				TemplateMatrix output = TemplateMatrix(this->Rows() - 1, this->Columns() - 1);
				size_t Row = (double)row.Round();
				size_t Col = (double)col.Round();
				for (size_t r = 0; r < output._rows; ++r)
				{
					size_t old_r = r >= Row ? r + 1 : r;
					for (size_t c = 0; c < output._cols; ++c)
					{
						size_t old_c = c >= Col ? c + 1 : c;
						output.At(r, c) = this->At(old_r, old_c);
					}
				}
				return output;
			}
			TemplateMatrix Minor() const
			{
				if (!this->IsSquare()) { throw std::exception("Can't compute minor of non-square matrix"); }
				auto output = TemplateMatrix(this->Rows(), this->Columns());
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								auto tmp = this->RemoveRowAndColumn(row, col);
								output.At(row, col) = tmp.Determinant();
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix Cofactor() const
			{
				if (!this->IsSquare()) { throw std::exception("Can't compute cofactor of non-square matrix"); }
				TemplateMatrix output = this->Minor();
				std::vector<size_t> rows(output._rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(output._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								TYPE cofactor = Math::Power(-1.0, row + col);
								output.At(row, col) = output.At(row, col) * cofactor;
							}
						);
					}
				);
				return output;
			}
			TemplateMatrix Adjoint() const
			{
				if (!this->IsSquare()) throw std::exception("Can't compute adjoint of non-square matrix");
				return this->Cofactor().Transpose();
			}
			TemplateMatrix AlgebraicInverse() const
			{
				if (!this->IsSquare()) throw std::exception("Can't compute inverse of non-square matrix");
				TYPE determinant = this->Determinant();
				if (determinant.IsZero()) throw std::exception("Can't compute inverse of matrix with determinant = 0");
				return this->Adjoint() * (TYPE(1) / determinant);
			}

			void ForEach(const std::function<void(const Number& row, const Number& col)>& func) const
			{
				std::vector<size_t> rows(this->_rows); std::iota(std::begin(rows), std::end(rows), 0);
				std::vector<size_t> cols(this->_cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(rows),
					std::end(rows),
					[&](const size_t& row) {
						std::for_each(
							std::execution::par,
							std::begin(cols),
							std::end(cols),
							[&](const size_t& col) {
								func(Number(row), Number(col));
							}
						);
					}
				);
			}

			TemplateMatrix Apply(const std::function<Number(const Number& val)>& func) const
			{
				auto copy = *this;
				copy._ForEach([&](const size_t& row, const size_t& col) {
					copy.At(row, col) = func(copy.At(row, col));
					});
				return copy;
			}

			TYPE DotProduct(const TemplateMatrix& m) const
			{
				if (!this->IsVector()) { throw std::exception("Left-matrix is not a vector, yet attempted to compute dot product"); }
				if(!m.IsVector()) { throw std::exception("Right-matrix is not a vector, yet attempted to compute dot product"); }
				auto lmatrix = this->RowVector(); 
				auto rmatrix = m.RowVector();
				if (lmatrix._rows != rmatrix._rows) { throw std::exception(Misc::StdStringFormat("Wrong dimensions for vector dot product: %ld vs %ld", lmatrix._rows, rmatrix._rows).c_str()); }
				TYPE output = 0;
				std::vector<size_t> cols(lmatrix._cols); std::iota(std::begin(cols), std::end(cols), 0);
				std::for_each(
					std::execution::par,
					std::begin(cols),
					std::end(cols),
					[&](const size_t& col) {
						output = output + lmatrix.At(0, col) * rmatrix.At(0, col);
					}
				);
				return output;
			}
			TYPE Length() const
			{
				if (!this->IsVector()) { throw std::exception("Length is defined for vectors, but not for matrices"); }
				TYPE sum = 0;
				auto row_vector = this->RowVector();
				for (size_t i = 0; i < row_vector._cols; ++i)
				{
					sum = sum + Math::Power(row_vector.At(0, i), 2);
				}
				return Math::Root(sum, 2);
			}
			TemplateMatrix Normalize() const
			{
				if (!this->IsVector()) { throw std::exception("Normalization is defined for vectors, but not for matrices"); }
				TYPE length = this->Length();
				if (length.IsZero()) return *this;
				return (*this) / length;
			}
			TYPE Sum() const
			{
				TYPE output = 0;
				for (size_t row = 0; row < this->_rows; ++row)
				{
					for (size_t col = 0; col < this->_cols; ++col)
					{
						output = output + this->At(row, col);
					}
				}
				return output;
			}
			TemplateMatrix Inverse() const
			{
				// Implementation by Chat GPT (Gauss-Jordan algorithm
				if (!this->IsSquare()) throw std::exception("Can't compute inverse of non-square matrix");

				size_t n = this->_rows;
				TemplateMatrix augmented(n, 2 * n); // Augmenting the original matrix with the identity matrix

				// Copying the original matrix and forming the augmented matrix with the identity matrix
				for (size_t i = 0; i < n; i++) {
					for (size_t j = 0; j < n; j++) {
						augmented.At(i, j) = this->At(i, j); // Copy the original matrix
						augmented.At(i, j + n) = (i == j) ? 1.0 : 0.0; // Form the identity matrix
					}
				}

				// Applying Gauss-Jordan elimination
				for (size_t col = 0; col < n; col++) {
					if (augmented.At(col, col) == 0) {
						// Find a non-zero element in the same column
						size_t swapRow = col + 1;
						while (swapRow < n && augmented.At(swapRow, col) == 0) {
							swapRow++;
						}

						if (swapRow == n) {
							throw std::runtime_error("Matrix is singular and cannot be inverted.");
						}

						// Swap the rows
						for (size_t k = 0; k < 2 * n; k++) {
							std::swap(augmented.At(col, k), augmented.At(swapRow, k));
						}
					}

					// Normalize the row
					Number divisor = augmented.At(col, col);
					for (size_t j = 0; j < 2 * n; j++) {
						augmented.At(col, j) = augmented.At(col, j) / divisor;
					}

					// Eliminate other rows
					for (size_t row = 0; row < n; row++) {
						if (row != col) {
							Number factor = augmented.At(row, col);
							for (size_t j = 0; j < 2 * n; j++) {
								augmented.At(row, j) = augmented.At(row, j) - factor * augmented.At(col, j);
							}
						}
					}
				}

				// Extracting the inverse matrix from the augmented matrix
				TemplateMatrix inverse(n, n);
				for (size_t i = 0; i < n; i++) {
					for (size_t j = 0; j < n; j++) {
						inverse.At(i, j) = augmented.At(i, j + n);
					}
				}

				return inverse;
			}
			TYPE Max() const
			{
				bool initialised = false;
				TYPE output;
				for (size_t r = 0; r < this->_rows; ++r)
				{
					for (size_t c = 0; c < this->_cols; ++c)
					{
						if (!initialised)
						{
							output = this->At(r, c);
							initialised = true;
						}
						output = Math::Max({ output, this->At(r,c) });
					}
				}
				return output;
			}
			TYPE Min() const
			{
				bool initialised = false;
				TYPE output;
				for (size_t r = 0; r < this->_rows; ++r)
				{
					for (size_t c = 0; c < this->_cols; ++c)
					{
						if (!initialised)
						{
							output = this->At(r, c);
							initialised = true;
						}
						output = Math::Min({ output, this->At(r,c) });
					}
				}
				return output;
			}
			TYPE Product() const
			{
				TYPE output = 1;
				for (size_t r = 0; r < this->_rows; ++r)
				{
					for (size_t c = 0; c < this->_cols; ++c)
					{
						output = output * this->At(r, c);
					}
				}
				return output;
			}
			TemplateMatrix Submatrix(const Number& start_row, const Number& start_col, const Number& num_row, const Number& num_col) const
			{
				TemplateMatrix::EnsureInteger({ start_row, start_col, num_row, num_col });
				TemplateMatrix output = TemplateMatrix(num_row, num_col, 0);
				output._ForEach([&](const size_t& row, const size_t& col) {
					output.At(row, col) = this->At(start_row + Number(row), start_col + Number(col));
					});
				return output;
			}
			TemplateMatrix Paste(const Number& start_row, const Number& start_col, const TemplateMatrix& m) const
			{
				TemplateMatrix::EnsureInteger({ start_row, start_col });
				TemplateMatrix output = *this;
				m._ForEach([&](const size_t& row, const size_t& col) {
					output.At(start_row + Number(row), start_col + Number(col)) = m.At(row, col);
					});
				return output;
			}
			TemplateMatrix Resize(const Number& rows, const Number& cols, const TYPE& padding = 0) const
			{
				TemplateMatrix::EnsureInteger({ rows, cols });
				TemplateMatrix output = TemplateMatrix(rows, cols, padding);
				output._ForEach([&](const size_t& row, const size_t& col) {
					if(this->Exists(row,col)) output.At(row, col) = this->At(row, col);
					});
				return output;
			}
	};

	typedef TemplateMatrix<Number> Matrix;


	namespace Math
	{
		inline Matrix Ln(const Matrix& m, const int& n = 0)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Ln(output.At(row, col), n);
				});
			return output;
		}
		inline Matrix Log(const Matrix& m, const Number& base)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Log(output.At(row, col), base);
				});
			return output;
		}
		inline Matrix Exp(const Matrix& m)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Exp(output.At(row, col));
				});
			return output;
		}
		inline Matrix Power(const Matrix& m, const Number& power, const int& k = 0)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Power(output.At(row, col), power, k);
				});
			return output;
		}
		inline Matrix Root(const Matrix& m, const Number& power, const int& k = 0)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Root(output.At(row, col), power, k);
				});
			return output;
		}
		inline Matrix Sin(const Matrix& m)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Sin(output.At(row, col));
				});
			return output;
		}
		inline Matrix Cos(const Matrix& m)
		{
			Matrix output = m;
			output.ForEach([&](const Number& row, const Number& col) {
				output.At(row, col) = Cos(output.At(row, col));
				});
			return output;
		}
		inline Number Max(const Matrix& m)
		{
			return m.Max();
		}
		inline Number Min(const Matrix& m)
		{
			return m.Min();
		}
		inline Number Sum(const Matrix& m)
		{
			return m.Sum();
		}
	}
}


#endif // !KIRITO_TEMPLATE_MATRIX_HPP
