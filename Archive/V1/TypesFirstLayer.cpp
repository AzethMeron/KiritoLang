
#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <limits>
#include <functional>
#include <sstream>
#include <iomanip>

#include "ClassDeclarations.h"
#include "TypesFirstLayer.h"
#include "VariableModel.h"
#include "Misc.h"
#include "Math.h"

namespace Kirito
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	String GenericType::CastStringMultireference() const
	{
		return Misc::StdStringFormat("Multireference%s", Types::CastString(this->Type()).c_str());
	}

	String GenericType::CastString(AddressSet& set) const {
		Address adr(this);
		if (set.Contains(adr))
		{
			return Misc::StdStringFormat("Looping%sAt%s", Types::CastString(this->Type()).c_str(), adr.CastString().Value().c_str());
		}
		return Misc::StdStringFormat("%sAt%s", Types::CastString(this->Type()).c_str(), adr.CastString().Value().c_str());
	}

	String GenericType::CastString() const {
		AddressSet set;
		return this->CastString(set);
	}

	Bool Types::IsFirstLayer(const TypeName& type)
	{
		switch (type)
		{
		case Types::Bool: {return true;} break;
		case Types::Integer: {return true;} break;
		case Types::Float: {return true;} break;
		case Types::String: {return true;} break;
		case Types::None: {return true;} break;
		case Types::Dump: {return true;} break;
		case Types::Complex: {return true;} break;
		}
		return false;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::string Types::CastString(const TypeName& obj)
	{
		switch (obj)
		{
		case Types::None: { return "None"; } break;
		case Types::Integer: { return "Integer"; } break;
		case Types::Float: { return "Float"; } break;
		case Types::Bool: { return "Bool"; } break;
		case Types::String: { return "String"; } break;
		case Types::Function: { return "Function"; } break;
		case Types::List: { return "List"; } break;
		case Types::Array: { return "Array"; } break;
		case Types::Set: { return "Set"; } break;
		case Types::Dict: { return "Dict"; } break;
		case Types::Class: { return "Class"; } break;
		case Types::Dump: { return "Dump"; } break;
		case Types::Complex: { return "Complex"; } break;
		default: { throw Types::UnkownError(obj); } break;
		}
	}
	std::invalid_argument Types::TypeError(const TypeName& expected, const TypeName& received)
	{
		return std::invalid_argument(Misc::StdStringFormat("Type error: expected '%s', got '%s'.",
			Types::CastString(expected).c_str(),
			Types::CastString(received).c_str()
		));
	}
	std::invalid_argument Types::Unhashable(const TypeName& expected)
	{
		return std::invalid_argument(Misc::StdStringFormat("Hashing error: attempting to hash unhashable '%s' type.",
			Types::CastString(expected).c_str()
		));
	}
	std::invalid_argument Types::UnkownError(const TypeName& unknown_type)
	{
		return std::invalid_argument(Misc::StdStringFormat("Internal error: unknown type '%s'.",
			Types::CastString(unknown_type).c_str()
		));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Bool None::CastBool() const { return Bool(false); }
	String None::CastString(AddressSet& set) const
	{
		static const String none = "None";
		return none;
	}
	Types::TypeName None::Type() const { return None::T; }

	size_t None::Hash() const
	{
		throw Types::Unhashable(this->T);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Integer::Integer() : _integer(0) {}
	Integer::Integer(const int64_t& i) : _integer(i) {}
	int64_t Integer::Value() const { return this->_integer; }
	Types::TypeName Integer::Type() const { return this->T; }
	Integer Integer::operator + (const Integer& obj) const { return Integer(this->Value() + obj.Value()); }
	Integer Integer::operator - (const Integer& obj) const { return Integer(this->Value() - obj.Value()); }
	Integer Integer::operator / (const Integer& obj) const { return Integer(this->Value() / obj.Value()); }
	Integer Integer::operator * (const Integer& obj) const { return Integer(this->Value() * obj.Value()); }
	Integer Integer::operator ++ () { return Integer(++(this->_integer)); }
	Integer Integer::operator -- () { return Integer(--(this->_integer)); }
	Integer Integer::Abs() const { return Integer(abs(this->Value())); }
	Bool Integer::operator == (const Integer& obj) const { return Bool(this->Value() == obj.Value()); }
	Bool Integer::operator != (const Integer& obj) const { return Bool(this->Value() != obj.Value()); }
	Bool Integer::operator < (const Integer& obj) const { return Bool(this->Value() < obj.Value()); }
	Bool Integer::operator > (const Integer& obj) const { return Bool(this->Value() > obj.Value()); }
	Bool Integer::operator <= (const Integer& obj) const { return Bool(this->Value() <= obj.Value()); }
	Bool Integer::operator >= (const Integer& obj) const { return Bool(this->Value() >= obj.Value()); }
	Bool Integer::IsOdd() const { return (this->_integer % 2) == 1; }
	Bool Integer::IsEven() const { return (this->_integer % 2) == 0; }
	Integer Integer::MinValue() { return std::numeric_limits<int64_t>::min(); }
	Integer Integer::MaxValue() { return std::numeric_limits<int64_t>::max(); }
	Bool Integer::CastBool() const { return (*this != 0).Value(); }
	String Integer::CastString(AddressSet& set) const { return Misc::StdStringFormat("%ld", this->Value()); }
	String Integer::CastString() const { return this->GenericType::CastString(); }
	size_t Integer::Hash() const { return std::hash<int64_t>()(this->_integer); }
	Bool Integer::IsZero() const { return this->Value() == 0; }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	double Float::Value() const { return this->_float; }
	Types::TypeName Float::Type() const { return this->T; }
	Float::Float() : _float(0) { }
	Float::Float(const double& i) : _float(i) {}
	Float::Float(const Integer& i) : _float(i.Value()) {}
	Float Float::operator + (const Float& obj) const { return Float(this->Value() + obj.Value()); }
	Float Float::operator - (const Float& obj) const { return Float(this->Value() - obj.Value()); }
	Float Float::operator / (const Float& obj) const { return Float(this->Value() / obj.Value()); }
	Float Float::operator * (const Float& obj) const { return Float(this->Value() * obj.Value()); }
	Float Float::Abs() const { return Float(abs(this->Value())); }
	Bool Float::operator != (const Float& obj) const { return (*this == obj).Not(); }
	Bool Float::operator < (const Float& obj) const { return Bool(this->Value() < obj.Value()); }
	Bool Float::operator > (const Float& obj) const { return Bool(this->Value() > obj.Value()); }
	Bool Float::operator <= (const Float& obj) const { return Bool(this->Value() <= obj.Value()); }
	Bool Float::operator >= (const Float& obj) const { return Bool(this->Value() >= obj.Value()); }
	Bool Float::CastBool() const { return (*this != 0).Value(); }
	String Float::CastString(AddressSet& set) const { 
		if (this->IsInteger()) return this->Round().CastString();
		return this->DisplaySignificantFigures(Float::dis_sig_fig); 
	}
	String Float::CastString() const { return this->GenericType::CastString(); }
	size_t Float::Hash() const { return std::hash<double>()(this->Value()); }
	String Float::DisplaySignificantFigures(const Integer& significant_figures) const
	{
		std::stringstream stream;
		stream << std::setprecision(significant_figures.Value()) << this->Value();
		return String(stream.str());
	}
	Float Float::GetSignificantFigures(const Integer& significant_figures) const
	{
		return (Float)this->DisplaySignificantFigures(significant_figures);
	}
	Float Float::MinValue() { return std::numeric_limits<double>::min(); }
	Float Float::MaxValue() { return std::numeric_limits<double>::max(); }
	Bool Float::Compare(const Float& obj, const Integer& significant_figures) const
	{
		Float m = std::min(this->Abs(), obj.Abs()) * Math::Power(10.0, -significant_figures.Value());
		if ((*this - obj).Abs() < m) return true;
		return false;
	}
	Bool Float::operator == (const Float& obj) const
	{ return this->Compare(obj, Float::hash_sig_fig); }
	Bool Float::IsZero(const Integer& significant_figures) const
	{ return this->Abs() < Math::Power(10, -significant_figures.Value()); }
	Bool Float::IsZero() const
	{ return this->IsZero(Float::hash_sig_fig); }
	Bool Float::IsInteger(const Integer& significant_figures) const
	{ return this->Compare(this->Round(), significant_figures); }
	Bool Float::IsInteger() const
	{ return this->IsInteger(Float::hash_sig_fig); }
	Integer Float::Round() const
	{ return round(this->Value()); }
	Integer Float::Floor() const
	{ return floor(this->Value()); }
	Integer Float::Ceil() const
	{ return ceil(this->Value()); }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool Bool::Value() const { return this->_bool; }
	Types::TypeName Bool::Type() const { return this->T; }
	Bool::Bool() : _bool(false) {}
	Bool::Bool(const bool& i) : _bool(i) {}
	Bool Bool::operator + (const Bool& obj) const { return Bool(this->Value() || obj.Value()); }
	Bool Bool::operator * (const Bool& obj) const { return Bool(this->Value() && obj.Value()); }
	Bool Bool::operator == (const Bool& obj) const { return Bool(this->Value() == obj.Value()); }
	Bool Bool::operator != (const Bool& obj) const { return Bool(this->Value() != obj.Value()); }
	Bool Bool::Not() const { return Bool(!this->Value()); }
	Bool Bool::CastBool() const { return *this; }
	String Bool::CastString(AddressSet& set) const { return this->Value() ? "true" : "false"; }
	size_t Bool::Hash() const { return std::hash<bool>()(this->_bool); }

	Bool Bool::Toggle()
	{
		this->_bool = this->Not().Value();
		return *this;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::string String::Value() const { return this->_string; }
	Types::TypeName String::Type() const { return this->T; }
	void String::SetString(const std::string& i)
	{
		this->_string = i;
		this->_hash = String::CalculateHash(i);
		this->_islower = String::CalculateIsLower(i);
		this->_isupper = String::CalculateIsUpper(i);
		this->_isalphanumeric = String::CalculateIsAlphanumeric(i);
	}
	String::String() { this->SetString(std::string()); }
	String::String(const char* i) { this->SetString(std::string(i)); }
	String::String(const std::string& i) { this->SetString(std::string(i)); }
	String::String(const std::string& i, const Bool& islower, const Bool& isupper, const Bool& isalphanumeric) : _string(i)
	{
		this->_hash = String::CalculateHash(this->_string);
		this->_islower = islower.Value();
		this->_isupper = isupper.Value();
		this->_isalphanumeric = isalphanumeric.Value();
	}
	String String::operator + (const String& obj) const
	{
		std::string concated = this->Value() + obj.Value();
		Bool islower = this->IsLower() * obj.IsLower();
		Bool isupper = this->IsUpper() * obj.IsUpper();
		Bool isalphanumeric = this->IsAlphanumeric() * obj.IsAlphanumeric();
		return String(concated, islower, isupper, isalphanumeric);
	}
	Bool String::operator == (const String& obj) const { return this->_hash == obj._hash; }
	Bool String::operator != (const String& obj) const { return !(*this == obj); }
	Integer String::Compare(const String& obj) const { return Integer(this->_string.compare(obj.Value())); }
	Integer String::Size() const { return Integer(this->_string.size()); }
	size_t String::CalculateHash(const std::string& i) { return std::hash<std::string>()(i); }
	Bool String::IsLower() const { return this->_islower; }
	Bool String::IsUpper() const { return this->_isupper; }
	Bool String::IsAlphanumeric() const { return this->_isalphanumeric; }
	Bool String::CastBool() const
	{
		static const String nullstr = "";
		return (*this != nullstr);
	}
	String String::CastString(AddressSet& set) const { return *this; }
	Bool String::Contains(const String& obj) const
	{
		if (this->_string.find(obj.Value()) != std::string::npos)
		{
			return Bool(true);
		}
		return Bool(false);
	}
	size_t String::Hash() const { return this->_hash; }

	String String::operator [] (const Integer& ind) const
	{
		if (ind.Abs() >= this->Size()) throw String::OutOfBoundError(ind, this->Size());
		if (ind < 0)
		{
			return Misc::StdStringFormat("%c", this->_string.at((this->Size() + ind).Value()));
		}
		else
		{
			return Misc::StdStringFormat("%c", this->_string.at(ind.Value()));
		}
	}
	String String::Substring(const Integer& min_ind, const Integer& max_ind) const
	{
		Integer starting_point = min_ind;
		Integer length = max_ind - min_ind;
		if (max_ind <= 0 && min_ind > 0) length = (this->Size() + max_ind) - min_ind;
		if (length < 0) length = 0;
		return String(this->_string.substr(starting_point.Value(), length.Value()));
	}
	std::out_of_range String::OutOfBoundError(const Integer& ind, const Integer& size)
	{
		return std::out_of_range(Misc::StdStringFormat("Out of bound error: index '%ld' out of range for size '%ld'.", ind.Value(), size.Value()));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Bool Dump::CastBool() const
	{
		return (this->StoredType() != Types::None);
	}
	String Dump::CastString(AddressSet& set) const
	{
		return String(Misc::StdStringFormat("Dump of %s", Types::CastString(this->StoredType()).c_str()));
	}
	Types::TypeName Dump::Type() const
	{
		return Dump::T;
	}
	size_t Dump::Hash() const
	{
		throw Types::Unhashable(this->Type());
	}
	Integer Dump::Size() const
	{
		return this->_dump.size();
	}
	Types::TypeName Dump::StoredType() const
	{
		return this->_stored_type;
	}
	std::stringstream Dump::StringStream() const
	{
		return std::stringstream(this->_dump);
	}
	Dump::Dump()
	{
		// here put None into dump
	}
	Dump::Dump(const Types::TypeName& type, const std::string& string) : _dump(string)
	{
		this->_stored_type = type;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	size_t Complex::Hash() const 
	{
		throw Types::Unhashable(this->T);
	}

	Bool Complex::CastBool() const 
	{
		return !this->IsZero();
	}

	Complex Complex::operator + (const Complex& num) const
	{
		Float real = this->Real() + num.Real();
		Float imag = this->Imaginary() + num.Imaginary();
		return Complex(real, imag);
	}
	Complex Complex::operator - (const Complex& num) const
	{
		Float real = this->Real() - num.Real();
		Float imag = this->Imaginary() - num.Imaginary();
		return Complex(real, imag);
	}
	Complex Complex::operator * (const Complex& num) const
	{
		Float real = this->Real() * num.Real() - this->Imaginary() * num.Imaginary();
		Float imag = this->Real() * num.Imaginary() + this->Imaginary() * num.Real();
		return Complex(real, imag);
	}
	Complex Complex::operator / (const Complex& num) const
	{
		Float denominator = (Math::Power(num.Real(), 2) + Math::Power(num.Imaginary(), 2));
		Float real = (this->Real() * num.Real() + this->Imaginary() * num.Imaginary()) / denominator;
		Float imag = (this->Imaginary() * num.Real() - this->Real() * num.Imaginary()) / denominator;
		return Complex(real, imag);
	}
	Bool Complex::IsReal() const
	{
		return this->Imaginary().IsZero();
	}
	Bool Complex::IsImaginary() const
	{
		return this->Real().IsZero() && !this->Imaginary().IsZero();
	}
	Bool Complex::IsZero() const
	{
		return this->Imaginary().IsZero() && this->Real().IsZero();
	}
	Float Complex::Abs() const // Magnitude, length
	{
		return Math::SquareRoot(Math::Power(this->Real(), 2) + Math::Power(this->Imaginary(), 2));
	}
	Float Complex::Angle() const
	{
		return Math::FourQuadrantArcTan(this->Imaginary(), this->Real());
	}
	Complex Complex::Conjugate() const
	{
		return Complex(this->Real(), Float(-1) * this->Imaginary());
	}
	Bool Complex::operator == (const Complex& num)
	{
		return (this->Real() == num.Real()) && (this->Imaginary() == num.Imaginary());
	}
	Bool Complex::operator != (const Complex& num)
	{
		return !(*this == num);
	}
	String Complex::CastString() const
	{
		return this->GenericType::CastString();
	}
	String Complex::CastString(AddressSet&) const
	{
		std::stringstream stream;
		stream << this->Real();
		if (this->Imaginary() >= 0) stream << '+';
		stream << this->Imaginary() << 'i';
		return stream.str();
	}
	Complex Complex::FromPolar(const Float& magnitude, const Float& angle)
	{
		Float real = magnitude * Math::Cos(angle);
		Float imag = magnitude * Math::Sin(angle);
		return Complex(real, imag);
	}
}