#ifndef KIRITO_STATISTICS_H
#define KIRITO_STATISTICS_H

#include <vector>

#include "Cache.h"
#include "TypesFirstLayer.h"
#include "TypesSecondLayer.h"
#include "VariableModel.h"

#include "Misc.h"
#include "Math.h"

namespace Kirito
{
	namespace Statistics
	{
		/*class DataSeries
		{
		private:
			std::vector<Float> _data;
			Float _sum;
			Float _min;
			Float _max;
		public:
			Integer Size() const { return this->_data.size(); }
			void Append(const Float& val);
			Float Sum() const { return this->_sum; }
			Float Average() const { return this->Sum() / Float(this->Size()); }
			Float Variance() const;
			Float StandardDeviation() const { return Math::SquareRoot(this->Variance()); }
			Float Max() const { return this->_max; }
			Float Min() const { return this->_min; }
			Float RelativeDeviation() const { return this->StandardDeviation() / this->Average(); }
			DataSeries Sorted() const { DataSeries sorted = *this; std::sort(sorted._data.begin(), sorted._data.end()); return sorted; }
			DataSeries Reversed() const { DataSeries reversed = *this; std::reverse(reversed._data.begin(), reversed._data.end()); return reversed; }
			Float operator [] (const Integer& ind) const { return this->_data.at(ind.Value()); }
			DataSeries operator + (const DataSeries& r) const;
			Float Median() const;
		};*/
		namespace Distribution
		{
			namespace Continuous
			{
				class Gaussian
				{
				private:
					Float _mean;
					Float _standard_deviation;
				public:
					Gaussian(const Float& mean, const Float& standard_deviation);
					Float Mean() const { return this->_mean; }
					Float Variance() const { return Math::Power(this->StandardDeviation(), 2); }
					Float StandardDeviation() const { return this->_standard_deviation; }
					Float Density(const Float& x) const; // chance of acquiring X in random
					Float Random() const;
					Float Cumulative(const Float& x) const;
					Float Lower(const Float& x) const { return this->Cumulative(x); }
					Float Higher(const Float& x) const { return Float(1) - this->Cumulative(x); }
				};
				class Uniform
				{
				private:
					Float _min;
					Float _max;
				public:
					Uniform(const Float& min, const Float& max);
					Float Max() const { return this->_max; }
					Float Min() const { return this->_min; }
					Float Density(const Float& x) const;
					Float Random() const;
					Float Mean() const { return (this->Min() + this->Max()) * 0.5; }
					Float Variance() const { return Math::Power(this->Max() - this->Min(), 2) * Float(1) / Float(12); }
					Float Cumulative(const Float& x) const;
					Float Lower(const Float& x) const { return this->Cumulative(x); }
					Float Higher(const Float& x) const { return Float(1) - this->Cumulative(x); }
				};
				class Exponential
				{
				private:
					Float _rate;
				public:
					Exponential(const Float& rate) : _rate(rate) {}
					Float Rate() const { return this->_rate; }
					Float Density(const Float& x) const;
					Float Random() const;
					Float Mean() const { return Float(1) / this->Rate(); }
					Float Variance() const { return Float(1) / Math::Power(this->Rate(), 2); }
					Float Cumulative(const Float& x) const;
					Float Lower(const Float& x) const { return this->Cumulative(x); }
					Float Higher(const Float& x) const { return Float(1) - this->Cumulative(x); }
				};
			}
			namespace Discrete
			{
				class Gaussian;
				class Uniform;
				class Binomial
				{
				private:
					Integer _n;
					Float _p;
				public:
					Binomial(const Integer& n, const Float& p) : _n(n), _p(p)  {}
					Float Density(const Integer& k) const;
					Integer Random() const;
					Float Mean() const;
					Float Variance() const;
					Float Cumulative(const Integer& k) const;
					Float Lower(const Integer& x) const { return this->Cumulative(x); }
					Float Higher(const Integer& x) const { return Float(1) - this->Cumulative(x); }
					Integer NumberOfTrials() const { return this->_n; }
					Float ProbabilityOfSuccess() const { return this->_p; }
				};
			}
		}
	}
}

#endif