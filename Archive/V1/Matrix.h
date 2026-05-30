#ifndef KIRITO_MATRIX_H
#define KIRITO_MATRIX_H

#include <vector>
#include <functional>

#include "Misc.h"
#include "TypesFirstLayer.h"
#include "TypesSecondLayer.h"
#include "VariableModel.h"
#include "Math.h"

namespace Kirito
{
	template<typename TYPE>
	class TemplateVector
	{
	private:
		std::vector<TYPE> _vec;
	private:
		TYPE  operator [] (const Integer& ind) const { return this->_vec.at(ind.Value()); }
		TYPE& operator [] (const Integer& ind) { return this->_vec.at(ind.Value()); }
		static std::invalid_argument WrongSize(const String& operation, const Integer& l, const Integer& r)
		{
			return std::invalid_argument(Misc::StdStringFormat("Operation '%s': wrong size %s vs %s",
				operation.Value().c_str(),
				l.GenericType::CastString().Value().c_str(),
				r.GenericType::CastString().Value().c_str()
			));
		}
	public:
		// Basics
		String CastString() const
		{
			std::string output = "[ ";
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output += this->At(i).CastString().Value() + std::string(" ");
			}
			output += std::string("]");
			return output;
		}
		Integer Size() const { return this->_vec.size(); }
		TemplateVector(const Integer& size) : _vec(size.Value()) {}
		TemplateVector(const Integer& size, const TYPE& val) : _vec(size.Value(), val) {}
		TemplateVector(const std::initializer_list<TYPE>& items)
		{
			for (const TYPE& i : items)
			{
				this->_vec.push_back(i);
			}
		}
		TYPE Length() const
		{
			TYPE sum = Integer(0);
			for (Integer i = 0; i < this->Size(); ++i)
			{
				sum = sum + Math::Power((*this)[i], 2);
			}
			return Math::SquareRoot(sum);
		}
		TemplateVector Normalize() const
		{
			TYPE length = this->Length();
			if (length.IsZero()) return *this;
			return (*this) / this->Length();
		}
		// Vector operations
		TYPE operator * (const TemplateVector& v) const
		{
			if (this->Size() != v.Size()) throw TemplateVector::WrongSize("multiplication", this->Size(), v.Size());
			TYPE sum = Integer(0);
			for (Integer i = 0; i < this->Size(); ++i)
			{
				sum = sum + (*this)[i] * v[i];
			}
			return sum;
		}
		TemplateVector operator + (const TemplateVector& v) const
		{
			if (this->Size() != v.Size()) throw TemplateVector::WrongSize("addition", this->Size(), v.Size());
			TemplateVector<TYPE> output(this->Size());
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output[i] = (*this)[i] + v[i];
			}
			return output;
		}
		TemplateVector operator - (const TemplateVector& v) const
		{
			if (this->Size() != v.Size()) throw TemplateVector::WrongSize("substraction", this->Size(), v.Size());
			TemplateVector output(this->Size());
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output[i] = (*this)[i] - v[i];
			}
			return output;
		}
		// Scalar operations
		TemplateVector operator * (const TYPE& o) const
		{
			TemplateVector output(this->Size());
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output[i] = (*this)[i] * o;
			}
			return output;
		}
		TemplateVector operator / (const TYPE& o) const
		{
			TemplateVector output(this->Size());
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output[i] = (*this)[i] / o;
			}
			return output;
		}
		TemplateVector operator + (const TYPE& o) const
		{
			TemplateVector output(this->Size());
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output[i] = (*this)[i] + o;
			}
			return output;
		}
		TemplateVector operator - (const TYPE& o) const
		{
			TemplateVector output(this->Size());
			for (Integer i = 0; i < this->Size(); ++i)
			{
				output[i] = (*this)[i] - o;
			}
			return output;
		}
		TemplateVector Convolution(const TemplateVector<TYPE>& vec) const
		{

		}
		// Access
		TYPE  At(const Integer& i) const { return (*this)[i.Value()]; }
		TYPE& At(const Integer& i) { return (*this)[i.Value()]; }
		void ForEach(std::function<void(TYPE&)> func) { for (TYPE& v : this->_vec) func(v); }
	};

	template<typename TYPE>
	class TemplateMatrix
	{
	private:
		std::vector<TemplateVector<TYPE>> _rows; //[row][col]
		Integer _columns;
	private:
		TemplateVector<TYPE>  _at(const Integer& r) const { return this->_rows.at(r.Value()); }
		TemplateVector<TYPE>& _at(const Integer& r) { return this->_rows.at(r.Value()); }
		static std::invalid_argument WrongDimensions(const String& operation, const String& dimension, const Integer& l, const Integer& r)
		{
			return std::invalid_argument(Misc::StdStringFormat("Operation: %s, Incorrect dimension '%s': %s vs %s",
				operation.Value().c_str(),
				dimension.Value().c_str(),
				l.GenericType::CastString().Value().c_str(),
				r.GenericType::CastString().Value().c_str()
			)
			);
		}
		TemplateMatrix RemoveRowAndColumn(const Integer& row, const Integer& col) const
		{
			// Error management
			if (row < 0 || row >= this->Rows()) throw std::invalid_argument(Misc::StdStringFormat("Internal error: trying to remove non-existing row '%s' from matrix of size %s",
				row.GenericType::CastString().Value().c_str(),
				this->Rows().GenericType::CastString().Value().c_str()
			));
			if (col < 0 || col >= this->Columns()) throw std::invalid_argument(Misc::StdStringFormat("Internal error: trying to remove non-existing column '%s' from matrix of size %s",
				col.GenericType::CastString().Value().c_str(),
				this->Columns().GenericType::CastString().Value().c_str()
			));
			// Meat goes here
			TemplateMatrix output(this->Rows() - 1, this->Columns() - 1);
			for (Integer r = 0; r < output.Rows(); ++r)
			{
				Integer old_r = r >= row ? r + 1 : r;
				for (Integer c = 0; c < output.Columns(); ++c)
				{
					Integer old_c = c >= col ? c + 1 : c;
					output.At(r, c) = this->At(old_r, old_c);
				}
			}
			return output;
		}
	public:
		TemplateMatrix() {}
		TemplateMatrix(const Integer& r, const Integer& c)
		{
			this->_columns = c;
			for (Integer i = 0; i < r; ++i)
			{
				this->_rows.push_back(TemplateVector<TYPE>(c));
			}
		}
		TemplateMatrix(const Integer& r, const Integer& c, const TYPE& val)
		{
			this->_columns = c;
			for (Integer i = 0; i < r; ++i)
			{
				this->_rows.push_back(TemplateVector<TYPE>(c, val));
			}
		}
		TemplateMatrix(const std::initializer_list<TemplateVector<TYPE>>& items)
		{
			Integer columns = -1;
			for (const auto& tmp : items)
			{
				if (columns >= 0) if (tmp.Size() != columns) throw std::invalid_argument(Misc::StdStringFormat("Matrix cration: encountered vector of size %s while another had %s",
					tmp.Size().GenericType::CastString().Value().c_str(),
					columns.GenericType::CastString().Value().c_str()
				));
				columns = tmp.Size();
				this->_rows.push_back(tmp);
			}
			this->_columns = columns;
		}
		TemplateMatrix(const TemplateVector<TYPE>& vec)
		{
			this->AddRow(vec);
		}
		Bool IsEmpty() const { return (this->Rows() == 0) && (this->Columns() == 0); }
		void AddRow(const TemplateVector<TYPE>& row)
		{
			Bool empty = this->IsEmpty();
			if (!empty && this->Columns() != row.Size()) throw std::invalid_argument(Misc::StdStringFormat("Adding row of size %s to matrix with %s columns",
				row.Size().GenericType::CastString().Value().c_str(),
				this->Columns().GenericType::CastString().Value().c_str()));
			this->_rows.push_back(row);
			if (empty) this->_columns = row.Size();
		}
		Bool IsSquare() const { return this->Rows() == this->Columns(); }
		String CastString() const
		{
			std::string output = "";
			for (Integer i = 0; i < this->Rows(); ++i)
			{
				output += this->_rows.at(i.Value()).CastString().Value() + std::string("\n");
			}
			return output;
		}
		Integer Rows() const { return this->_rows.size(); }
		Integer Columns() const { return this->_columns; }
		TYPE& At(const Integer& row, const Integer& col) { return this->_rows.at(row.Value()).At(col); }
		TYPE  At(const Integer& row, const Integer& col) const { return this->_rows.at(row.Value()).At(col); }
		TemplateVector<TYPE>  At(const Integer& r) const { return this->_at(r); }
		// Scalar operations
		TemplateMatrix operator * (const TYPE& multiplier) const
		{
			TemplateMatrix output(this->Rows(), this->Columns());
			for (Integer r = 0; r < this->Rows(); ++r)
			{
				output._at(r) = this->_at(r) * multiplier;
			}
			return output;
		}
		// Matrix operations
		TemplateMatrix operator + (const TemplateMatrix& m) const
		{
			if (this->Rows() != m.Rows()) throw TemplateMatrix::WrongDimensions("matrix addition", "rows", this->Rows(), m.Rows());
			if (this->Columns() != m.Columns()) throw TemplateMatrix::WrongDimensions("matrix addition", "columns", this->Columns(), m.Columns());
			TemplateMatrix output(this->Rows(), this->Columns());
			for (Integer r = 0; r < this->Rows(); ++r)
			{
				for (Integer c = 0; c < this->Columns(); ++c)
				{
					output.At(r, c) = this->At(r, c) + m.At(r, c);
				}
			}
			return output;
		}
		TemplateMatrix operator - (const TemplateMatrix& m) const
		{
			if (this->Rows() != m.Rows()) throw TemplateMatrix::WrongDimensions("matrix subtraction", "rows", this->Rows(), m.Rows());
			if (this->Columns() != m.Columns()) throw TemplateMatrix::WrongDimensions("matrix subtraction", "columns", this->Columns(), m.Columns());
			TemplateMatrix output(this->Rows(), this->Columns());
			for (Integer r = 0; r < this->Rows(); ++r)
			{
				for (Integer c = 0; c < this->Columns(); ++c)
				{
					output.At(r, c) = this->At(r, c) - m.At(r, c);
				}
			}
			return output;
		}
		TemplateMatrix operator * (const TemplateMatrix& m) const
		{
			if (this->Columns() != m.Rows()) throw TemplateMatrix::WrongDimensions("matrix multiplication", "columns vs rows", this->Columns(), m.Rows());
			Integer trail = this->Columns();
			TemplateMatrix output(this->Rows(), m.Columns(), Integer(0));
			for (Integer r = 0; r < output.Rows(); ++r)
			{
				for (Integer c = 0; c < output.Columns(); ++c)
				{
					for (Integer i = 0; i < trail; ++i)
					{
						output.At(r, c) = output.At(r, c) + (this->At(r, i) * m.At(i, c));
					}
				}
			}
			return output;
		}
		TemplateVector<TYPE> operator * (const TemplateVector<TYPE>& v) const
		{
			if (this->Columns() != v.Size()) throw TemplateMatrix::WrongDimensions("matrix by vector multiplication", "columns", this->Rows(), v.Size());
			TemplateVector<TYPE> output(this->Rows());
			for (Integer i = 0; i < this->Rows(); ++i)
			{
				output.At(i) = this->_at(i) * v;
			}
			return output;
		}
		static TemplateMatrix Diagonal(const Integer& size, const TYPE& val = 1)
		{
			TemplateMatrix output(size, size);
			for (Integer i = 0; i < size; ++i)
			{
				output.At(i, i) = val;
			}
			return output;
		}
		TemplateMatrix Transpose() const
		{
			TemplateMatrix output(this->Columns(), this->Rows());
			for (Integer r = 0; r < this->Rows(); ++r)
			{
				for (Integer c = 0; c < this->Columns(); ++c)
				{
					output.At(c, r) = this->At(r, c);
				}
			}
			return output;
		}
		TYPE Determinant() const
		{
			if (!this->IsSquare()) throw std::invalid_argument("Can't calculate determinant of matrix which isn't square");
			// Calculation of determinant using gaussian elimination requires modifications to matrix
			// We don't want to modify real object, so we create a copy
			TemplateMatrix copy = *this;
			// Row swap causes reversal of sign, so we need to store info about it somewhere
			Float sign = 1;
			// Main loop: i goes through trail (diagonal)
			for (Integer i = 0; i < copy.Rows(); ++i)
			{
				// If 0 on diagonal, we need to swap rows
				if (copy.At(i, i).IsZero())
				{
					// Looking for non-zero rows below our currently analysed row
					for (Integer j = i + 1; j < copy.Rows(); ++j)
					{
						// Is it different than 0?
						if (copy.At(j, i).IsZero())
						{
							// Swap, flip sign and stop searching
							std::swap(copy._at(i), copy._at(j));
							sign = sign * Float(-1);
							break;
						}
					}
					// If after that value at diagonal is still 0, we've det = 0
					if (copy.At(i, i).IsZero()) { return Integer(0); }
				}
				// Zeroing all elements in this column below this row
				for (Integer j = i + 1; j < copy.Rows(); ++j)
				{
					TYPE multiplier = (copy.At(j, i) / copy.At(i, i)) * Float(-1);
					copy._rows.at(j.Value()) = copy._at(j) + copy._at(i) * multiplier;
				}
			}
			// So now we have triangle (stairs-like) matrix
			//std::cout << copy.CastString() << std::endl;
			// Calculating determinant
			TYPE determinant = Integer(1);
			for (Integer i = 0; i < copy.Rows(); ++i)
			{
				determinant = determinant * copy.At(i, i);
			}
			return determinant * sign;
		}
		TemplateMatrix Minor() const
		{
			if (!this->IsSquare()) throw std::invalid_argument("Trying to get minor of non-square matrix");
			TemplateMatrix output = TemplateMatrix(this->Rows(), this->Columns());
			for (Integer r = 0; r < output.Rows(); ++r)
			{
				for (Integer c = 0; c < output.Columns(); ++c)
				{
					output.At(r, c) = this->RemoveRowAndColumn(r, c).Determinant();
				}
			}
			return output;
		}
		TemplateMatrix Cofactor() const
		{
			if (!this->IsSquare()) throw std::invalid_argument("Trying to get cofactor of non-square matrix");
			TemplateMatrix output = this->Minor();
			for (Integer r = 0; r < output.Rows(); ++r)
			{
				for (Integer c = 0; c < output.Columns(); ++c)
				{
					Float cofactor = Math::Power(-1, r + c);
					output.At(r, c) = output.At(r, c) * cofactor;
				}
			}
			return output;
		}
		TemplateMatrix Adjoint() const
		{
			if (!this->IsSquare()) throw std::invalid_argument("Trying to get adjoint of non-square matrix");
			return this->Cofactor().Transpose();
		}
		TemplateMatrix Inverse() const
		{
			if (!this->IsSquare()) throw std::invalid_argument("Trying to inverse non-square matrix");
			TYPE determinant = this->Determinant();
			if (determinant.IsZero()) throw std::invalid_argument("Trying to inverse matrix with determinant = 0");
			return this->Adjoint() * (TYPE(Integer(1)) / determinant);
		}
		TemplateMatrix Convolution(const TemplateMatrix<TYPE>& kernel) const
		{

		}
		void ForEach(std::function<void(TYPE&)> func) { for (TemplateVector<TYPE>& v : this->_rows) v.ForEach(func); }
	};

	typedef TemplateVector<Float> Vector;
	typedef TemplateVector<Complex> ComplexVector;
	typedef TemplateMatrix<Float> Matrix;
	typedef TemplateMatrix<Complex> ComplexMatrix;
}

#endif