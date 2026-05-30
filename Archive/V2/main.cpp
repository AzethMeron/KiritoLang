#include <iostream>

#include <cmath>
#include <complex>
#include <chrono>

#include "Misc.hpp"
#include "Number.hpp"
#include "TemplateMatrix.hpp"
using namespace Kirito;

#include <random>

Matrix RandomMatrix(const int size = 5, const Number min = -50, const Number max = 100)
{
	static thread_local std::random_device rd;
	static thread_local std::mt19937 generator(rd());
	std::uniform_int_distribution<int> distribution(min.Real(), max.Real());
	auto m = Matrix(size, size);
	for (size_t r = 0; r < m.Rows().Real(); ++r)
	{
		for (size_t c = 0; c < m.Columns().Real(); ++c)
		{
			m.At(Number(r), Number(c)) = Number(distribution(generator));
		}
	}
	return m;
}

int main() {
	/*
	std::vector<Number> c = { Number(0.5), Number(1), Number(2), Number(10), Number(-1,1), Number(3,2), Number(0.4,0.3), Number(-2.5), Number(0, -2), Number(0.001, 3) };
	for (const auto& i : c)
	{
		std::cout << i.CastString() << " : " << Math::Factorial(i).CastString() << std::endl;
	}

	try
	{
		auto v = Matrix({ {1,2,3,4} });
		auto c = Matrix({ {1},{2},{3},{4} });
		std::cout << v.Normalize().CastString() << v.Normalize().Length().CastString() << std::endl;
		std::cout << v.ColumnVector().CastString() << std::endl;
		std::cout << v.DotProduct(c).CastString() << std::endl;
		std::cout << (v.ColumnVector() * v.RowVector()).Sum().CastString();

		auto z = Matrix({ {1,2,3}, {4,5,6}, {7,8,9} });
		std::cout << z.CastString();
	}
	catch (std::exception e)
	{
		std::cout << e.what();
	}
	std::cout << Matrix::Diagonal(10).Inverse().CastString();
	Number a = Number(3, 4);
	Number b = Number::FromPolar(a.Magnitude(), a.Angle());
	std::cout << a.CastString() << " " << b.CastString();
	std::cout << (Number(-10) > Number(5)).Value() << std::endl;
	*/

	

	try
	{
		/*auto p = Matrix({{1,2,3,4},{5,6,7,8}, {9,10,11,12},{13,14,15,16}});
		std::cout << p.CastString() << std::endl;
		std::cout << p.Paste(1, 1, Matrix({ {4,3,1},{2,1,1} })).Resize(10,3,1).CastString() << std::endl;


		for (int i = -360; i <= 360; i = i + 30)
		{
			std::cout << i << " : " << Math::ArcSin(Math::Sin(Number(Math::DegToRad(i)))).CastString() << ' ' << std::asin(std::sin(std::complex<double>(
			Math::DegToRad(i).Real(), 0
			))) << std::endl;
		}*/

		//auto p = RandomMatrix();
		//std::cout << p.AlgebraicInverse().CastString() << std::endl;;
		//std::cout << p.Inverse().CastString();

		//Matrix X = RandomMatrix(1000);
		//X = X.Apply(Math::Sin);
		// 
		//std::cout << X.CastString();
		/*std::cout << Levenshtein(String("Why hello there"), String("hello Why there")).CastString() << std::endl;
		auto m = RandomMatrix(4);
		std::cout << m.CastString() << std::endl;
		std::cout << Math::Exp(m).CastString() << std::endl;
		std::cout << Math::Ln(Math::Exp(m)).CastString() << std::endl;*/

		/*for (int i = 0; i < 100; i++)
		{
			Matrix m = RandomMatrix(30);
			m = m.AlgebraicInverse() - m.Inverse();
			std::cout << "Max: " << Math::Max(m).CastString() << ", Min: " << Math::Min(m).CastString() << std::endl;
		}*/

		Matrix m = RandomMatrix(200); 

		//auto algebraic_start = std::chrono::high_resolution_clock::now();
		//Matrix a = m.AlgebraicInverse();
		//auto algebraic_stop = std::chrono::high_resolution_clock::now();
		//auto algebraic_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(algebraic_stop - algebraic_start);

		auto fast_start = std::chrono::high_resolution_clock::now();
		Matrix b = m.Inverse();
		auto fast_stop = std::chrono::high_resolution_clock::now();
		auto fast_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(fast_stop - fast_start);

		//std::cout << "Algebraic: " << algebraic_delta_ms << std::endl << "Fast: " << fast_delta_ms << std::endl;
		auto diff = (m*b) - Matrix::Diagonal(m.Rows());
		std::cout << "Max diff: " << Math::Max(diff).CastString() << ", min: " << Math::Min(diff).CastString();
	}
	catch (std::exception e)
	{
		std::cout << e.what();
	}

	return 0;
}
