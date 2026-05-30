
#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <limits>
#include <functional>
#include <sstream>
#include <iomanip>

#include "TypesFirstLayer.h"
#include "VariableModel.h"
#include "TypesSecondLayer.h"
#include "Misc.h"

namespace Kirito
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Bool Array::CastBool() const
	{ return Bool(this->Size() != 0); }
	String Array::CastString(AddressSet& set) const
	{
		std::string container = "[ ";
		for (Integer i = 0; i < this->Size(); ++i)
		{
			const Variable& v = (*this)[i];
			container +=  Misc::StdStringFormat("%s", v.CastString(set).Value().c_str());
			if (i < this->Size() - 1) container += ", ";
		}
		container = container + " ]";
		return String(container);
	}
	String Array::CastStringMultireference() const
	{
		return "[...]";
	}
	Types::TypeName Array::Type() const
	{ return Array::T; }
	size_t Array::Hash() const
	{ throw Types::Unhashable(this->Type()); }
	Array::Array() {}
	Array::Array(const Array& obj) // temporary
	{ *this = obj; }
	Array::Array(const Array&& obj) noexcept // temporary
	{ *this = obj; }
	Array& Array::operator = (const Array& obj) // temporary
	{
		this->Clear();
		this->_array.reserve(obj.Size().Value());
		for (const Variable& v : obj)
		{
			this->Append(v.Copy());
		}
		return *this;
	}
	void Array::Clear() { this->_array.clear();  }
	Integer Array::Size() const
	{ return this->_array.size(); }
	void Array::Append(const Variable& obj)
	{ this->_array.push_back(obj); }
	std::vector<Variable>::iterator Array::begin() { return this->_array.begin();  }
	std::vector<Variable>::iterator Array::end() { return this->_array.end(); }
	std::vector<Variable>::const_iterator Array::begin() const { return this->_array.begin(); }
	std::vector<Variable>::const_iterator Array::end() const { return this->_array.end(); }
	const Variable Array::operator [] (const Integer& ind) const
	{
		if (ind.Abs() >= this->Size()) throw Array::OutOfBoundError(ind, this->Size());
		if (ind < 0)
		{
			return this->_array.at((this->Size() + ind).Value());
		}
		else
		{
			return this->_array.at(ind.Value());
		}
	}
	Variable& Array::operator [] (const Integer& ind)
	{
		if (ind.Abs() >= this->Size()) throw Array::OutOfBoundError(ind, this->Size());
		if (ind < 0)
		{
			return this->_array.at((this->Size() + ind).Value());
		}
		else
		{
			return this->_array.at(ind.Value());
		}
	}
	std::out_of_range Array::OutOfBoundError(const Integer& ind, const Integer& size)
	{
		return std::out_of_range(Misc::StdStringFormat("Out of bound error: index '%ld' out of range for size '%ld'.", ind.Value(), size.Value()));
	}
	Array::Array(const Integer& dimensions) : _array(dimensions.Value()) {}
	Array::Array(const Integer& rows, const Integer& columns) : _array(rows.Value())
	{
		for (Variable& v : this->_array)
		{
			v = Variable(Array(columns));
		}
	}
	Bool Array::operator == (const Array& obj) const
	{
		if (this->Size() != obj.Size()) return false;
		for (Integer i = 0; i < this->Size(); ++i)
		{
			if ((*this)[i] != obj[i]) return false; 
		}
		return true;
	}
	Bool Array::operator != (const Array& obj) const
	{
		return !(*this == obj);
	}
	Bool Array::Contains(const Variable& obj) const
	{
		for (const Variable& v : this->_array)
		{
			if(v == obj) return true;
		}
		return false;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Bool Dict::CastBool() const
	{ return this->Size() != Integer(0); }
	String Dict::CastString(AddressSet& set) const
	{
		std::string container = "{\n";
		for (auto p : this->_dict)
		{
			container += Misc::StdStringFormat("%s : %s\n",p.first.CastString(set).Value().c_str(), p.second.CastString(set).Value().c_str());
		}
		container += " }";
		return String(container);
	}
	String Dict::CastStringMultireference() const
	{ return String("{ ... }"); }
	Types::TypeName Dict::Type() const
	{ return Dict::T;	}
	size_t Dict::Hash() const
	{ throw Types::Unhashable(this->Type()); }

	Integer Dict::Size() const
	{ return this->_dict.size(); }
	Bool Dict::Contains(const Variable& key) const
	{ return this->_dict.contains(key); }
	
	const Variable Dict::operator [] (const Variable& key) const
	{
		if (!this->Contains(key)) { return Variable(); }
		return this->_dict.at(key);
	}
	Variable& Dict::operator [] (const Variable& key)
	{
		if (!this->Contains(key)) { this->_dict.insert_or_assign(key, Variable()); }
		return this->_dict.at(key);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Bool Set::CastBool() const { return this->Size() != 0; }
	String Set::CastString(AddressSet& set) const {
		std::string output = "( "; 
		for (const Variable& v : this->_set) {
			output += Misc::StdStringFormat("%s ", v.CastString(set).Value().c_str());
		}
		output += ")"; 
		return String(output); 
	}
	Types::TypeName Set::Type() const { return Set::T; }
	size_t Set::Hash() const { throw Types::Unhashable(this->Type()); }
	Integer Set::Size() const { return this->_set.size(); }
	Bool Set::Contains(const Variable& obj) const { return this->_set.contains(obj); }
	void Set::Update(const Set& obj) { for (const Variable& v : obj._set) { this->_set.insert(v); } }
	void Set::Remove(const Variable& obj) { this->_set.erase(obj); }
	void Set::Insert(const Variable& obj) { Variable dummy = obj.Copy(); dummy.MakeConst(); this->_set.insert(dummy); }
	void Set::Insert(const Array& obj_arr) { // Note: calls this->Insert, so it creates copies
		this->_set.reserve( (this->Size() + obj_arr.Size()).Value() ); 
		for (const Variable& v : obj_arr) 
			this->Insert(v); 
	}
	Set::Set(const Array& obj_arr)
	{ this->Insert(obj_arr); }
	Bool Set::operator == (const Set & obj) const
	{
		if (this->Size() != obj.Size()) return false;
		for (const Variable& v : this->_set)
		{
			if (!obj.Contains(v)) return false;
		}
		return true;
	}
	Bool Set::operator != (const Set& obj) const
	{ return !(*this == obj); }
	Set Set::Union(const Set& l, const Set& r)
	{
		Set output = l;
		for (const Variable& v : r._set)
		{
			output.Insert(v);
		}
		return output;
	}
	Set Set::Difference(const Set& l, const Set& r)
	{
		Set output = l;
		for (const Variable& v : r._set)
		{
			output.Remove(v);
		}
		return output;
	}
	Set Set::Intersection(const Set& l, const Set& r)
	{
		Set output;
		for (const Variable& v : l._set)
		{
			if (r.Contains(v)) output.Insert(v);
		}
		return output;
	}
	Set Set::InverseIntersection(const Set& l, const Set& r)
	{
		Set output;
		for (const Variable& v : l._set)
		{
			if (! r.Contains(v)) output.Insert(v);
		}
		for (const Variable& v : r._set)
		{
			if (! l.Contains(v)) output.Insert(v);
		}
		return output;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
}
