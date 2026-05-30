
#include <random>
#include "Statistics.h"

namespace Kirito
{
	namespace Statistics
	{
		/*void DataSeries::Append(const Float& val) {
			if (this->Size() == 0)
			{
				this->_max = Float::MinValue();
				this->_min = Float::MaxValue();
			}
			if (val > this->_max) this->_max = val;
			if (val < this->_min) this->_min = val;
			this->_data.push_back(val);
			this->_sum = this->_sum + val;
		}

		Float DataSeries::Variance() const
		{ 
			Float avg = this->Average(); 
			Float sum = 0; 
			for (const Float& v : this->_data) { 
				sum = sum + Math::Power(v - avg, 2); 
			} 
			return sum / Float(this->Size()); 
		}

		DataSeries DataSeries::operator + (const DataSeries& r) const
		{
			DataSeries output = *this;
			for (const Float& v : r._data)
			{
				output.Append(v);
			}
			return output;
		}
		Float DataSeries::Median() const
		{
			DataSeries sorted = this->Sorted();
			if (this->Size() == 0) return 0;
			if (this->Size().IsOdd())
			{
				Integer index = (Float(sorted.Size()) / 2).Floor();
				return sorted[index];
			}
			else
			{
				Integer index = (Float(sorted.Size()) / 2).Ceil();
				return (sorted[index - 1] + sorted[index]) / 2;
			}
		}*/

		///////////////////////////////////////////////////////////////////////////////////////////////////////////

		namespace Distribution
		{
			namespace Continuous
			{
				Gaussian::Gaussian(const Float& mean, const Float& standard_deviation) : _mean(mean), _standard_deviation(standard_deviation) {
					if (standard_deviation < 0) throw std::invalid_argument(Misc::StdStringFormat("Standard deviation for gaussian distribution must be positive: %s", standard_deviation.GenericType::CastString()));
				}

				Float Gaussian::Density(const Float& x) const // chance of acquiring X in random
				{
					Float multiplier = Float(1) / Math::SquareRoot(Math::Pi * this->Variance() * 2);
					Float exponent = Float(-0.5) * Math::Power(x - this->Mean(), 2) / this->Variance();
					return multiplier * Math::Power(Math::E, exponent);
				}

				Float Gaussian::Random() const
				{
					static std::random_device rd;
					static std::mt19937 generator(rd());
					std::normal_distribution<double> distribution(this->Mean().Value(), this->StandardDeviation().Value());
					return distribution(generator);
				}

				Float Gaussian::Cumulative(const Float& x) const
				{
					return Float(0.5) * (Float(1) + Math::Erf((x - this->Mean()) / (this->StandardDeviation() * Math::SquareRoot(2))));
				}

				///////////////////////////////////////////////////////////////////////////////////////////////////////////

				Uniform::Uniform(const Float& min, const Float& max) : _min(min), _max(max) {
					if (min > max) throw std::invalid_argument(Misc::StdStringFormat("Min (%s) cannot be higher than max (%s)",
						min.CastString().Value().c_str(),
						max.CastString().Value().c_str())
					);
				}
				Float Uniform::Density(const Float& x) const
				{
					if (x < this->Min() || x > this->Max()) return 0;
					return Float(1) / (this->Max() - this->Min());
				}
				Float Uniform::Random() const
				{
					static std::random_device rd;
					static std::mt19937 generator(rd());
					std::uniform_real_distribution<double> distribution(this->Min().Value(), this->Max().Value());
					return distribution(generator);
				}
				Float Uniform::Cumulative(const Float& x) const
				{
					if (x < this->Min()) return 0;
					if (x > this->Max()) return 1;
					return (x - this->Min()) / (this->Max() - x);
				}

				///////////////////////////////////////////////////////////////////////////////////////////////////////////

				Float Exponential::Density(const Float& x) const
				{
					return this->Rate() * Math::Power(Math::E, this->Rate() * x * (-1));
				}
				Float Exponential::Random() const
				{
					static std::random_device rd;
					static std::mt19937 generator(rd());
					std::exponential_distribution<double> distribution(this->Rate().Value());
					return distribution(generator);
				}

				Float Exponential::Cumulative(const Float& x) const
				{
					return Float(1) - Math::Power(Math::E, this->Rate() * x * Float(-1));
				}
			}
			namespace Discrete
			{
				Integer Binomial::Random() const
				{
					static std::random_device rd;
					static std::mt19937 generator(rd());
					std::binomial_distribution<int64_t> distribution(this->NumberOfTrials().Value(), this->ProbabilityOfSuccess().Value());
					return distribution(generator);
				}
				Float Binomial::Density(const Integer& k) const
				{
					Integer n = this->NumberOfTrials();
					Float p = this->ProbabilityOfSuccess();
					Float q = Float(1) - p;
					//return Math::NewtonBinomial(n, k) * Math::Power(p, k) * Math::Power(q, n - k);
					Float LnResult = Math::LnNewtonBinomial(n, k) + Float(k) * Math::Ln(p) + Float(n - k) * Math::Ln(q);
					return Math::Exp(LnResult);
				}
				Float Binomial::Mean() const { return this->ProbabilityOfSuccess() * this->NumberOfTrials(); }
				Float Binomial::Variance() const { Float q = Float(1) - this->ProbabilityOfSuccess(); return q * this->ProbabilityOfSuccess() * this->NumberOfTrials(); }
				Float Binomial::Cumulative(const Integer& k) const { Float sum = 0; for (Integer ki = 0; ki <= k; ++ki) { sum = sum + this->Density(ki); } return sum; }
			}
		}
	}
}