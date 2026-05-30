
#include <exception>
#include <sstream>
#include "Number.hpp"
#include "String.hpp"

namespace Kirito
{
	bool Number::_doubleCompare(const double& l, const double& r, const double& precision, const double& threshold)
	{
		double diff = std::abs(l - r);
		// Absolute difference check
		if (diff < threshold) {
			return true;
		}
		// Relative difference check
		double avg = (std::abs(l) + std::abs(r)) / 2.0;
		double relativeDiff = diff / avg;
		if (relativeDiff < precision) {
			return true;
		}
		// None of checks passed?
		return false;
	}

	Number Number::operator + (const Number& num) const
	{
		double real = this->Real() + num.Real();
		double im = this->Imaginary() + num.Imaginary();
		return Number(real, im);
	}

	Number Number::operator - (const Number& num) const
	{
		double real = this->Real() - num.Real();
		double im = this->Imaginary() - num.Imaginary();
		return Number(real, im);
	}

	Number Number::operator * (const Number& num) const
	{
		double real = this->Real() * num.Real() - this->Imaginary() * num.Imaginary();
		double im = this->Real() * num.Imaginary() + this->Imaginary() * num.Real();
		return Number(real, im);
	}

	Number Number::operator / (const Number& num) const
	{
		double denominator = (double)(num.Real() * num.Real() + num.Imaginary() * num.Imaginary());
		double real = (this->Real() * num.Real() + this->Imaginary() * num.Imaginary()) / denominator;
		double imag = (this->Imaginary() * num.Real() - this->Real() * num.Imaginary()) / denominator;
		return Number(real, imag);
	}

	Number Number::FromPolar(const Number& magnitude, const Number& angle)
	{
		double real = (double)magnitude * std::cos((double)angle);
		double imag = (double)magnitude * std::sin((double)angle);
		return Number(real, imag);
	}

	String Number::CastString() const
	{
		std::stringstream stream;
		if (this->IsReal())
			stream << this->Real();
		else if (this->IsImaginary())
		{
			stream << this->Imaginary() << 'i';
		}
		else
		{
			stream << '(' << this->Real() << std::showpos << this->Imaginary() << 'i' << ')';
		}
		return stream.str();
	}

	Bool Number::IsInteger() const
	{
		if (*this == this->Round())
		{
			return true;
		}
		return false;
	}

	Number Number::Floor() const
	{
		return Number(std::floor(this->Real()), std::floor(this->Imaginary()));
	}

	Number Number::Ceil() const
	{
		return Number(std::ceil(this->Real()), std::ceil(this->Imaginary()));
	}

	Number Number::Round() const
	{
		return Number(std::round(this->Real()), std::round(this->Imaginary()));
	}

}