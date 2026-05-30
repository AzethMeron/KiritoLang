
#include "VariableModel.h"
#include "TypesFirstLayer.h"
#include "TypesSecondLayer.h"
#include "Misc.h"

#include "Math.h"
#include "Cache.h"
#include "Statistics.h"
#include "Matrix.h"

#include <iostream>
#include <locale>
#include <unordered_set>
#include <complex>
using namespace Kirito;
using namespace Kirito::Math;

int main()
{
	/*Kirito::Variable A = Kirito::ObjectFactory::New<Integer>();
	try {
		Kirito::Variable i = A;
		Kirito::Variable f = Kirito::ObjectFactory::New<Float>();
		Kirito::Variable b = Kirito::ObjectFactory::New<Bool>();
		Kirito::Integer& a = i.Convert<Integer>();
		Kirito::Integer& c = b.Convert<Integer>();
	}
	catch (std::invalid_argument& e)
	{
		std::cout << e.what() << std::endl;
	}
	std::cout << A.Count().Value();

	/*Float a = 11;
	Bool b = true;
	std::cout << (b + false).Value() << std::endl;
	std::cout << (b * false).Value() << std::endl;

	std::cout << (a == 10.9999999).Value() << std::endl;
	std::cout << Integer::MaxValue().Value() << std::endl;*/

	/*std::string a = "Ara ara eee eee";
	for (const char& c : a)
	{
		std::cout << c << ' ' << islower(c) << std::endl;
	}

	Variable s = ObjectFactory::New<String>("why hello there");
	Variable c = ObjectFactory::New<String>(", general kenobi");
	String& S = s.Convert<String>();
	String& C = c.Convert<String>();

	Variable empty = ObjectFactory::New<String>();

	std::cout << (S+C).Value() << std::endl;
	std::cout << (String("Ale") == String("Ale ")).Value() << std::endl;
	std::cout << Integer::MaxValue().Value();
	*/
	
	/*
	Variable A = ObjectFactory::New < Integer > (10);
	Variable B = ObjectFactory::New < Integer >(30);
	Variable xd = ObjectFactory::New<Array>();
	std::cout << A.Is(B) << std::endl;
	std::cout << A.Is(Variable(A)) << std::endl;
	*/

	/*try
	{
		for(int i = 0; i < 10; ++i) xd.Convert<Array>().Append(A);
		xd.MakeConst();
		std::cout << xd << std::endl;
		Variable B;
		B = A;
		B.Convert<Integer>() = B.Convert<Integer>() + 1;
		const Variable& c = xd.Convert<Array>()[10];
		std::cout << c << std::endl;
	}
	catch(std::exception e)
	{
		std::cout << e.what();
	}*/

	/*try
	{
		Variable k1 = Integer(10);
		Variable k2 = String("10");
		Variable k3 = Float(11.13);
		Variable v1 = String("Int10");
		Variable v2 = String("Str10");
		Variable v3 = Integer(11);
		Dict x = Dict();
		x[k1] = v1;
		x[k2] = v2;
		x[k3] = v3;
		std::cout << x;
	}
	catch (std::exception e)
	{
		std::cout << e.what();
	}*/

	try
	{
		/*ComplexVector a = {{3,1}, {2,5}, {5,2}};
		ComplexVector b = { {1,1}, {9,7}, {2,1} };
		ComplexVector c = { {4,5}, {7,9}, {3,7} };
		ComplexVector d = { {1,2}, {5,2}, {4,3} };

		ComplexMatrix m = { a, b, c };
		std::cout << (m * m.Inverse()).CastString();
		std::cout << m.At(2).CastString() << std::endl;
		std::cout << Statistics::Distribution::Discrete::Binomial(162, 0.0473).Higher(42);*/

		std::initializer_list<Integer> l = {0,1,2,3,4,10, 15, 20, 40};
		for (auto& i : l)
		{
			//std::cout << Math::NewtonBinomial(i, i - 5) << " " << Math::Exp(Math::LnNewtonBinomial(i, i - 5)) << std::endl;
			//std::cout << Math::Factorial(i) << " " << Math::Exp(Math::LnFactorial(i)) << std::endl;
		}
		std::cout << Statistics::Distribution::Discrete::Binomial(262, 0.047281).Higher(42) + Statistics::Distribution::Discrete::Binomial(262, 0.047281).Density(42);
	}
	catch (std::exception e)
	{
		std::cout << e.what();
	}


	return 0;
}