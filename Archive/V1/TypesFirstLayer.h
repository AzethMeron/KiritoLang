#ifndef KIRITO_BUILTINTYPES_H
#define KIRITO_BUILTINTYPES_H

#include <string>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <sstream>

#include "ClassDeclarations.h"

namespace Kirito
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct Types
	{
	public:
		enum TypeName {
			None = 0, // Not hashable
			Integer = 1, // Hashable
			Float = 2, // Hashable
			Bool = 3, // Hashable
			String = 4, // Hashable
			Function = 5, // Not hashable
			Array = 6, // Not hashable
			List = 7,  // Not hashable
			Set = 8, // Not hashable
			Dict = 9, // Not hashable
			Class = 10, // Not hashable
			Dump = 11, // Not hashable
			Complex = 12 // Not hashable (yet? I just don't know how to hash it)
		};
		static Kirito::Bool IsFirstLayer(const TypeName& type);
		static std::invalid_argument TypeError(const TypeName& expected, const TypeName& received);
		static std::invalid_argument Unhashable(const TypeName& expected);
		static std::invalid_argument UnkownError(const TypeName& expected);
		static std::string CastString(const TypeName& obj);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class GenericType
	{
	public:
		virtual Types::TypeName Type() const = 0;
		virtual Bool CastBool() const = 0;
		virtual String CastString(AddressSet& set) const; // Unused by Integer, Float, Bool, String, but vital for Array, Set
		virtual String CastStringMultireference() const;
		String CastString() const;
		virtual size_t Hash() const = 0; // unhashable types throw exception, but still have this method
		//virtual Dump Pickle() const = 0;
		//virtual void Unpickle(const Dump&) = 0;
		
;	// Unspoken requirements:
		//constexpr static Types::TypeName T - Types::...
		//GenericType();
		//GenericType(const GenericType&);
		//GenericType(const GenericType&&);
		//GenericType& operator = (const GenericType&);
		//~GenericType();
	// Essentially, class inheriting GenericType must be safe to be initialised with default, copy or move constructor and safe to assign
	// Don't use raw pointers, or manage them accordingly
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class None : public GenericType
	{
	public:
		constexpr static Types::TypeName T = Types::None;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		Types::TypeName Type() const override;
		size_t Hash() const override;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Integer : public GenericType
	{
	private:
		int64_t _integer;
	public:
		constexpr static Types::TypeName T = Types::Integer;
		int64_t Value() const;
		Types::TypeName Type() const override;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		String CastString() const;
		size_t Hash() const override;
		Integer operator + (const Integer& obj) const;
		Integer operator - (const Integer& obj) const;
		Integer operator / (const Integer& obj) const;
		Integer operator * (const Integer& obj) const;
		Integer operator ++ ();
		Integer operator -- ();
		Integer Abs() const;
		Bool operator == (const Integer& obj) const;
		Bool operator != (const Integer& obj) const;
		Bool operator < (const Integer& obj) const;
		Bool operator > (const Integer& obj) const;
		Bool operator <= (const Integer& obj) const;
		Bool operator >= (const Integer& obj) const;
		Bool IsOdd() const;
		Bool IsEven() const;
		Bool IsZero() const;
		Integer();
		Integer(const int64_t& i);
		explicit operator int64_t() const { return this->_integer; }

		static Integer MinValue();
		static Integer MaxValue();
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Float : public GenericType
	{
	private:
		double _float; // I don't care it's double, not float, it's floating point, just more precise
		constexpr static int hash_sig_fig = 8;
		constexpr static int dis_sig_fig = 4;
	public:
		constexpr static Types::TypeName T = Types::Float;
		double Value() const;
		Types::TypeName Type() const override;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		String CastString() const;
		size_t Hash() const override;
		Float();
		Float(const double& i);
		Float(const Integer& i);
		explicit operator double() const { return this->_float; }
		Float operator + (const Float& obj) const;
		Float operator - (const Float& obj) const;
		Float operator / (const Float& obj) const;
		Float operator * (const Float& obj) const;
		Bool operator == (const Float& obj) const; 
		Bool operator != (const Float& obj) const;
		Bool operator < (const Float& obj) const;
		Bool operator > (const Float& obj) const;
		Bool operator <= (const Float& obj) const;
		Bool operator >= (const Float& obj) const;
		Bool Compare(const Float& obj, const Integer& relative_tolerance) const;
		Bool IsZero(const Integer& significant_figures) const;
		Bool IsZero() const;
		Bool IsInteger(const Integer& significant_figures) const;
		Bool IsInteger() const;
		Integer Round() const;
		Integer Floor() const;
		Integer Ceil() const;
		Float Abs() const;
		String DisplaySignificantFigures(const Integer& significant_figures) const;
		Float GetSignificantFigures(const Integer& significant_figures) const;

		static Float MinValue();
		static Float MaxValue();
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Bool : public GenericType
	{
	private:
		bool _bool;
	public:
		constexpr static Types::TypeName T = Types::Bool;
		bool Value() const;
		Types::TypeName Type() const override;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		size_t Hash() const override;
		Bool(); // default: false
		Bool(const bool& i); 
		explicit operator bool() const { return this->_bool; }

		Bool operator + (const Bool& obj) const; // or 
		Bool operator * (const Bool& obj) const; // and
		Bool operator == (const Bool& obj) const; // xnor
		Bool operator != (const Bool& obj) const; // xor
		Bool Not() const; 
		Bool Toggle();
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class String : public GenericType
	{
	private:
		std::string _string;
		size_t _hash;
		bool _islower;
		bool _isalphanumeric;
		bool _isupper;
	private:
		void SetString(const std::string& i); // used to set given string and evaluate all flags for it
		static size_t CalculateHash(const std::string& i);
		static bool CalculateIsUpper(const std::string& i) { return false; } // temporary
		static bool CalculateIsLower(const std::string& i) { return false; } // temporary
		static bool CalculateIsAlphanumeric(const std::string& i) { return false; } // temporary 
		String(const std::string& i, const Bool& islower, const Bool& isupper, const Bool& isalphanumeric);
	public:
		constexpr static Types::TypeName T = Types::String;
		std::string Value() const;
		Types::TypeName Type() const override;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		size_t Hash() const override;
		String();
		String(const char* i);
		String(const std::string& i);
		explicit operator std::string() const { return this->_string; }
		explicit operator Integer() const { return Integer(std::stoll(this->Value())); }
		explicit operator Float() const { return Float(std::stod(this->Value())); }
		explicit operator Bool() const { 
			bool alpha = false, noalpha = false; 
			std::istringstream(this->Value()) >> std::boolalpha >> alpha; 
			std::istringstream(this->Value()) >> noalpha;
			return Bool(alpha) + Bool(noalpha); }
		String operator + (const String& obj) const; // concat
		Bool operator == (const String& obj) const;
		Bool operator != (const String& obj) const;
		Bool IsLower() const;
		Bool IsUpper() const;
		Bool IsAlphanumeric() const;
		Integer Compare(const String& obj) const;
		Integer Size() const; 
		Bool Contains(const String& obj) const;
		String operator [] (const Integer& ind) const;
		String Substring(const Integer& min_ind, const Integer& max_ind) const;
		static std::out_of_range OutOfBoundError(const Integer& ind, const Integer& size);
		//String Lower() const;
		//String Upper() const;
		//Array Split(const String& splitters) const; 
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Dump : public GenericType
	{
	private:
		std::string _dump;
		Types::TypeName _stored_type;
	public:
		constexpr static Types::TypeName T = Types::Dump;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		Types::TypeName Type() const override;
		size_t Hash() const override;
		Integer Size() const;
		Types::TypeName StoredType() const;
		std::stringstream StringStream() const;
		Dump();
		Dump(const Types::TypeName& type, const std::string& string);
		Dump(const String& relative_path);
		void Save(const String& relative_path) const;
		void Load(const String& relative_path);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Complex : public GenericType
	{
	private:
		Float _real;
		Float _imaginary;
	public:
		constexpr static Types::TypeName T = Types::Complex;
		Bool CastBool() const override;
		String CastString(AddressSet&) const override;
		Types::TypeName Type() const override { return this->T; }
		size_t Hash() const override;
		Float  Real() const { return this->_real; }
		Float& Real() { return this->_real; }
		Float  Imaginary() const { return this->_imaginary; }
		Float& Imaginary() { return this->_imaginary; }
		Complex operator + (const Complex& num) const;
		Complex operator - (const Complex& num) const;
		Complex operator * (const Complex& num) const;
		Complex operator / (const Complex& num) const;
		Bool IsReal() const;
		Bool IsImaginary() const;
		Bool IsZero() const;
		Float Abs() const; // Magnitude, length
		Float Angle() const;
		Complex() : Complex(0, 0) { }
		Complex(const Float& num) : Complex(num, 0) { }
		Complex(const Float& real, const Float& imaginary)
		{ this->_real = real; this->_imaginary = imaginary; }
		Complex(const Integer& num) : Complex(num, 0) {}
		Complex Conjugate() const;
		Bool operator == (const Complex& num);
		Bool operator != (const Complex& num);
		String CastString() const;
		static Complex FromPolar(const Float& magnitude, const Float& angle);
	};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif KIRITO_BUILTINTYPES_H