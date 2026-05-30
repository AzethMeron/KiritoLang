#ifndef KIRITO_MATH_H
#define KIRITO_MATH_H

#include <cmath>
#include <algorithm>
#include <initializer_list>
#include <numbers>

#include "Misc.h"
#include "TypesFirstLayer.h"
#include "TypesSecondLayer.h"
#include "VariableModel.h"

namespace Kirito
{
	namespace Math
	{
		const Float Pi = std::numbers::pi;
		const Float Phi = std::numbers::phi;
		const Float E = std::numbers::e;

		inline Float SquareRoot(const Float& number) { return Float(sqrt(number.Value())); }
		inline Float Power(const Float& base, const Float& power) { return Float(pow(base.Value(), power.Value())); }
		inline Float Exp(const Float& power) { return exp(power.Value()); }
		inline Float Root(const Float& number, const Integer& root_tier) { return Power(number, Float(1) / root_tier); }
		inline Float Log(const Float& number, const Float& base) { return log2(number.Value()) / log2(base.Value()); }
		inline Float Ln(const Float& number) { return log(number.Value()); }
		// Trigonometry. Requires more work (checking boundaries, % 360 degree and so on)
		inline Float Sin(const Float& angle) { return sin(angle.Value()); }
		inline Float Cos(const Float& angle) { return cos(angle.Value()); }
		inline Float Tan(const Float& angle) { return tan(angle.Value()); }
		inline Float SinH(const Float& angle) { return sinh(angle.Value()); }
		inline Float CosH(const Float& angle) { return cosh(angle.Value()); }
		inline Float TanH(const Float& angle) { return tanh(angle.Value()); }
		inline Float ArcSin(const Float& sine) { return asin(sine.Value()); }
		inline Float ArcCos(const Float& cosine) { return acos(cosine.Value()); }
		inline Float ArcTan(const Float& tangent) { return atan(tangent.Value()); }
		inline Float ArcSinH(const Float& angle) { return asinh(angle.Value()); }
		inline Float ArcCosH(const Float& angle) { return acosh(angle.Value()); }
		inline Float ArcTanH(const Float& angle) { return atanh(angle.Value()); }
		inline Float FourQuadrantArcTan(const Float& y, const Float& x) { return atan2(y.Value(), x.Value()); }
		inline Float DegToRad(const Float& degrees) { return (degrees * (Pi / Float(180))); }
		inline Float RadToDeg(const Float& radians) { return (radians * (Float(180) / Pi)); }
		inline Float Erf(const Float& x) { return erf(x.Value()); }
		inline Float Factorial(const Integer& from, const Integer& to)
		{
			Float output = 1;
			for (Integer i = from; i <= to; ++i)
			{
				output = output * Float(i);
			}
			return output;
		}
		inline Float Factorial(const Integer& x)
		{
			return Factorial(1, x);
		}
		inline Float NewtonBinomial(const Integer& n, const Integer& k)
		{
			Float u = Factorial(k+1,n);
			Float d = Factorial(n - k);
			return u / d;
		}
		inline Float LnFactorial(const Integer& from, const Integer& to)
		{
			Float output = 0;
			for (Integer i = from; i <= to; ++i)
			{
				output = output + Ln(i);
			}
			return output;
		}
		inline Float LnFactorial(const Integer& x)
		{
			return LnFactorial(1, x);
		}
		inline Float LnNewtonBinomial(const Integer& n, const Integer& k)
		{
			return LnFactorial(n) - (LnFactorial(k) + LnFactorial(n-k));
		}

		inline Float Min(std::initializer_list<Float> list)
		{ return std::min(list); }
		inline Float Max(std::initializer_list<Float> list)
		{ return std::max(list); }


		// Uses demoivre theorem
		// It should work only for integer powers, but I've compared results for floating-point ones with Matlab and it checks out
		// so leaving it here
		// Tested for powers: 2, 2.5, 0.5, -2
		// of numbers; (3+4i),(3-4i),(-3+4i),(-3-4i)
		inline Complex Power(const Complex& base, const Float& power)
		{
			Float magnitude = Power(base.Abs(), power);
			Float angle = power * base.Angle();
			return Complex::FromPolar(magnitude, angle);
		}
		inline Complex Root(const Complex& base, const Integer& root_tier, const Integer& k = 0)
		{
			if (!(k < root_tier)) throw std::invalid_argument(Misc::StdStringFormat("k (%s) must be lower than root_tier (%s)",
				k.GenericType::CastString().Value().c_str(), 
				root_tier.GenericType::CastString().Value().c_str()
			));
			Float magnitude = Root(base.Abs(), root_tier);
			Float angle = (base.Angle() + Pi * k * 2) / root_tier;
			return Complex::FromPolar(magnitude, angle);
		}
		inline Complex SquareRoot(const Complex& num)
		{
			return Root(num, 2);
		}
		inline Complex Sin(const Complex& num)
		{
			Float real = Sin(num.Real()) * CosH(num.Imaginary());
			Float imag = Cos(num.Real()) * SinH(num.Imaginary());
			return Complex(real, imag);
		}
		inline Complex Cos(const Complex& num)
		{
			Float real = Cos(num.Real()) * CosH(num.Imaginary());
			Float imag = Sin(num.Real()) * SinH(num.Imaginary()) * (-1);
			return Complex(real, imag);
		}
		inline Complex Tan(const Complex& num)
		{
			return Sin(num) / Cos(num);
		}
	}
}

#endif // !KIRITO_NUMERIC_H
