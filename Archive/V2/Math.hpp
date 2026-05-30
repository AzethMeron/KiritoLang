#ifndef KIRITO_MATH_HPP
#define KIRITO_MATH_HPP

#include <numbers>
#include <cmath>
#include <exception>
#include <vector>
#include "Number.hpp"

namespace Kirito
{
	namespace Math
	{
		const Number Pi = std::numbers::pi;
		const Number Phi = std::numbers::phi;
		const Number E = std::numbers::e;

		inline Number Ln(const Number& number, const int& n = 0)
		{
			double real = log((double)number.Magnitude());
			double imaginary = (double)number.Angle() + (2.0 * std::numbers::pi * (double)n);
			return Number(real, imaginary);
		}

		inline Number Log(const Number& number, const Number& base)
		{
			return Ln(number) / Ln(base);
		}

		inline Number Exp(const Number& power)
		{
			double real = std::exp(power.Real()) * std::cos(power.Imaginary());
			double imaginary = std::exp(power.Real()) * std::sin(power.Imaginary());
			return Number(real, imaginary);
		}

		inline Number Power(const Number& base, const Number& power, const int& k = 0)
		{
			return Exp(power * Ln(base, k));
		}

		inline Number Root(const Number& base, const Number& power, const int& k = 0)
		{
			return Power(base, Number(1) / power, k);
		}

		inline Number Sin(const Number& angle)
		{
			double real = std::sin(angle.Real()) * std::cosh(angle.Imaginary());
			double imaginary = std::cos(angle.Real()) * std::sinh(angle.Imaginary());
			return Number(real, imaginary);
		}
		inline Number Cos(const Number& angle)
		{
			double real = std::cos(angle.Real()) * std::cosh(angle.Imaginary());
			double imaginary = std::sin(angle.Real()) * std::sinh(angle.Imaginary()) * (-1.0);
			return Number(real, imaginary);
		}
		inline Number Tan(const Number& angle)
		{
			return Sin(angle) / Cos(angle);
		}
		inline Number SinH(const Number& angle)
		{
			double real = std::sinh(angle.Real()) * std::cos(angle.Imaginary());
			double imaginary = std::cosh(angle.Real()) * std::sin(angle.Imaginary());
			return Number(real, imaginary);
		}
		inline Number CosH(const Number& angle)
		{
			double real = std::cosh(angle.Real()) * std::cos(angle.Imaginary());
			double imaginary = std::sinh(angle.Real()) * std::sin(angle.Imaginary());
			return Number(real, imaginary);
		}
		inline Number TanH(const Number& angle)
		{
			return SinH(angle) / CosH(angle);
		}
		Number Factorial(const Number& z);
		inline Number Min(std::initializer_list<Number> list) { return std::min(list); }
		inline Number Max(std::initializer_list<Number> list) { return std::max(list); }
		// TODO
		inline Number ArcSin(const Number& angle) // Still incomplete
		{
			static const auto i = Number(0, 1);
			return Number(-1) * i * Log( i*angle + Root(Number(1) - Power(angle,2), 2), E);
		}
		inline Number ArcCos(const Number& angle);
		inline Number ArcTan(const Number& angle);
		inline Number ArcSinH(const Number& angle);
		inline Number ArcCosH(const Number& angle);
		inline Number ArcTanH(const Number& angle);
		inline Number FourQuadrantArcTan(const Number& y, const Number& x);
		inline Number DegToRad(const Number& degrees)
		{
			return degrees * (Pi / Number(180.0));
		}
		inline Number RadToDeg(const Number& radians)
		{
			return radians * (Number(180.0) / Pi);
		}
		inline Number Erf(const Number& x);
	}
}

#endif // !KIRITO_MATH_HPP
