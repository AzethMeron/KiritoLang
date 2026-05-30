#ifndef KIRITO_NUMBER_HPP
#define KIRITO_NUMBER_HPP

#include <exception>
#include <cmath>

namespace Kirito
{
	class Number;
}

#include "Bool.hpp"
#include "String.hpp"
#include "Misc.hpp"

namespace Kirito
{
	// Can represent real, imaginary or complex numbers with double precision (floating-point)
	// Note: as comparison >, < and similar are not defined for complex numbers, i'm comparing Magnitude in such cases
	class Number
	{
		private:
			double _real;
			double _imaginary;
			static constexpr double _relative_error = 0.001;
			static constexpr double _absolute_error = 0.001;
			static bool _doubleCompare(const double& l, const double& r, const double& precision = _relative_error, const double& threshold = _absolute_error);
		public:
			Number() { this->_real = 0; this->_imaginary = 0; }
			Number(const double& real) : Number(real, 0) { }
			Number(const double& real, const double& im) { this->_real = real; this->_imaginary = im; }
			double  Real() const { return this->_real; }
			double& Real() { return this->_real; }
			double  Imaginary() const { return this->_imaginary; }
			double& Imaginary() { return this->_imaginary; }
			explicit operator double() const { // TODO
				if (!Number::_doubleCompare(this->Imaginary(), 0))
					throw std::exception( Misc::StdStringFormat("Can't convert complex/imaginary number %s into floating-point",this->CastString().c_str()).c_str() );
				return this->_real;
			}
			Number Magnitude() const { return Number(std::sqrt((this->Real() * this->Real()) + (this->Imaginary() * this->Imaginary()))); }
			Number Angle() const { return Number(std::atan2(this->Imaginary(), this->Real())); }
			Bool IsImaginary() const { return (Number::_doubleCompare(this->Real(), 0) && !Number::_doubleCompare(this->Imaginary(), 0)); }
			Bool IsReal() const { return (Number::_doubleCompare(this->Imaginary(), 0)); }
			Bool IsZero() const { return (Number::_doubleCompare(this->Real(), 0) && Number::_doubleCompare(this->Imaginary(), 0)); }
			Bool IsComplex() const { return (!Number::_doubleCompare(this->Real(), 0) && !Number::_doubleCompare(this->Imaginary(), 0)); }
			Bool CastBool() const { return !this->IsZero(); }
			Number Conjugate() const { return Number(this->Real(), this->Imaginary() * (-1.0)); }
			Bool operator == (const Number& num) const { return Number::_doubleCompare(this->Real(), num.Real()) && Number::_doubleCompare(this->Imaginary(), num.Imaginary()); }
			Bool operator != (const Number& num) const { return !(*this == num); }
			Bool operator > (const Number& num) const { 
				if (this->IsReal() && num.IsReal()) { return this->Real() > num.Real(); } 
				return this->Magnitude().Real() > num.Magnitude().Real(); 
			}
			Bool operator < (const Number& num) const { 
				if (this->IsReal() && num.IsReal()) { return this->Real() < num.Real(); } 
				return this->Magnitude().Real() < num.Magnitude().Real(); 
			}
			Bool operator >= (const Number& num) const { 
				if (this->IsReal() && num.IsReal()) { return (this->Real() > num.Real()) || (*this == num); }
				return (this->Magnitude() > num.Magnitude()) || (*this == num); 
			}
			Bool operator <= (const Number& num) const { 
				if (this->IsReal() && num.IsReal()) { return (this->Real() < num.Real()) || (*this == num); }
				return (this->Magnitude() < num.Magnitude()) || (*this == num); 
			}
			Number operator + (const Number& num) const;
			Number operator - (const Number& num) const;
			Number operator * (const Number& num) const;
			Number operator / (const Number& num) const;
			static Number FromPolar(const Number& magnitude, const Number& angle);
			String CastString() const;
			Bool IsInteger() const;
			Number Floor() const;
			Number Ceil() const;
			Number Round() const;
	};
}

#endif // !KIRITO_NUMBER_HPP
