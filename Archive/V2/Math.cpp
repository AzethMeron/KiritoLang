
#include <vector>
#include <exception>
#include "Number.hpp"
#include "Math.hpp"

namespace Kirito
{
	namespace Math
	{
		Number Factorial(const Number& z)
		{
			if (z.IsReal() && z.IsInteger() && z < 0)
			{
				throw std::exception("Factorial isn't defined for negative integers");
			}
			// Lanczos approximation formula
			// not gonna lie, written by Chat GPT. No idea how or this works
			static const double g = 7;
			static const std::vector<double> p = {
				0.99999999999980993,
				676.5203681218851,
				-1259.1392167224028,
				771.32342877765313,
				-176.61502916214059,
				12.507343278686905,
				-0.13857109526572012,
				9.9843695780195716e-6,
				1.5056327351493116e-7
			};

			Number x = p.at(0);
			for (int i = 1; i < p.size(); i++) {
				x = x + Number(p.at(i)) / (z + Number(i));
			}
			Number t = z + g + Number(0.5);
			return Root(Number(2) * Pi, 2) * Power(t, z + Number(0.5)) * Exp(Number(-1) * t) * x;
		}
	}
}