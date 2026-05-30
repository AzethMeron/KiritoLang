#ifndef KIRITO_STLOVERLOADS_H
#define KIRITO_STLOVERLOADS_H

#include <iostream>
#include <functional>

#include "ClassDeclarations.h"

// First layer
namespace std
{
	// Hashing
	template<> struct hash<Kirito::Integer> { size_t operator() (const Kirito::Integer& x) const; };
	template<> struct hash<Kirito::Float> { size_t operator() (const Kirito::Float& x) const; };
	template<> struct hash<Kirito::Bool> { size_t operator() (const Kirito::Bool& x) const; };
	template<> struct hash<Kirito::String> { size_t operator() (const Kirito::String& x) const; };
	// Comparing
	template<> struct equal_to<Kirito::Integer> { bool operator() (const Kirito::Integer& l, const Kirito::Integer& r) const; };
	template<> struct equal_to<Kirito::Float> { bool operator() (const Kirito::Float& l, const Kirito::Float& r) const; };
	template<> struct equal_to<Kirito::Bool> { bool operator() (const Kirito::Bool& l, const Kirito::Bool& r) const; };
	template<> struct equal_to<Kirito::String> { bool operator() (const Kirito::String& l, const Kirito::String& r) const; };
	// Printing
	ostream& operator << (ostream& stream, const Kirito::GenericType& obj);
	ostream& operator << (ostream& stream, const Kirito::String& obj);
}

// Second layer
namespace std
{
	// Hashing
	template<> struct hash<Kirito::Array> { size_t operator() (const Kirito::Array& x) const; };
	// Comparing
	template<> struct equal_to<Kirito::Array> { bool operator() (const Kirito::Array& l, const Kirito::Array& r) const; };
}

namespace std
{
	// Hashing 
	template<> struct hash<Kirito::Variable> { size_t operator() (const Kirito::Variable& x) const; };
	template<> struct hash<Kirito::Address> { size_t operator() (const Kirito::Address& x) const; };
	// Comparing
	template<> struct equal_to<Kirito::Variable> { bool operator() (const Kirito::Variable& l, const Kirito::Variable& r) const; };
	template<> struct equal_to<Kirito::Address> { bool operator() (const Kirito::Address& l, const Kirito::Address& r) const; };
	// Printing
	ostream& operator << (ostream& stream, const Kirito::Variable& obj);
	ostream& operator << (ostream& stream, const Kirito::Address& obj);
}

#endif
