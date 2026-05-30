
#include "StlOverloads.h"
#include "TypesFirstLayer.h"
#include "TypesSecondLayer.h"

namespace std
{
	// Hashing
	size_t hash<Kirito::Integer>::operator() (const Kirito::Integer& x) const
	{ return x.Hash(); }
	size_t hash<Kirito::Float>::operator() (const Kirito::Float& x) const
	{ return x.Hash(); }
	size_t hash<Kirito::Bool>::operator() (const Kirito::Bool& x) const
	{ return x.Hash(); }
	size_t hash<Kirito::String>::operator() (const Kirito::String& x) const
	{ return x.Hash(); }
	size_t hash<Kirito::Variable>::operator() (const Kirito::Variable& x) const
	{ return x.Hash(); }
	size_t hash<Kirito::Array>::operator() (const Kirito::Array& x) const
	{ return x.Hash(); }
	size_t hash<Kirito::Address>::operator() (const Kirito::Address& x) const
	{ return x.Hash(); }
	// Comparing
	bool equal_to<Kirito::Integer>::operator() (const Kirito::Integer& l, const Kirito::Integer& r) const
	{ return (l == r).Value(); }
	bool equal_to<Kirito::Float>::operator() (const Kirito::Float& l, const Kirito::Float& r) const
	{ return (l == r).Value(); }
	bool equal_to<Kirito::Bool>::operator() (const Kirito::Bool& l, const Kirito::Bool& r) const
	{ return (l == r).Value(); }
	bool equal_to<Kirito::String>::operator() (const Kirito::String& l, const Kirito::String& r) const
	{ return (l == r).Value(); }
	bool equal_to<Kirito::Variable>::operator() (const Kirito::Variable& l, const Kirito::Variable& r) const
	{ return (l == r).Value(); }
	bool equal_to<Kirito::Array>::operator() (const Kirito::Array& l, const Kirito::Array& r) const
	{ return false; } // temporary
	bool equal_to<Kirito::Address>::operator() (const Kirito::Address& l, const Kirito::Address& r) const
	{ return (l == r).Value(); }
	// Printing
	ostream& operator << (ostream& stream, const Kirito::Variable& obj)
	{ stream << obj.CastString(); return stream; }
	ostream& operator << (ostream& stream, const Kirito::GenericType& obj)
	{ stream << obj.CastString(); return stream; }
	ostream& operator << (ostream& stream, const Kirito::String& obj)
	{ stream << obj.Value(); return stream; }
	ostream& operator << (ostream& stream, const Kirito::Address& obj)
	{ stream << obj.CastString(); return stream; }
}