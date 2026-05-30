#ifndef KIRITO_TYPESSECONDLAYER_H
#define KIRITO_TYPESSECONDLAYER_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ClassDeclarations.h"
#include "TypesFirstLayer.h"
#include "VariableModel.h"
#include "StlOverloads.h"

namespace Kirito
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Array : public GenericType
	{
	private:
		std::vector<Variable> _array;
	public:
		constexpr static Types::TypeName T = Types::Array;
		Bool CastBool() const override;
		String CastString(AddressSet& set) const override;
		String CastStringMultireference() const override;
		Types::TypeName Type() const override;
		size_t Hash() const override;
		Array();
		Array(const Array& obj);
		Array(const Array&& obj) noexcept;
		Array(const Integer& dimensions);
		Array(const Integer& rows, const Integer& columns);
		Array& operator = (const Array& obj);
		Integer Size() const;
		void Append(const Variable& obj);
		void Clear();
		std::vector<Variable>::iterator begin();
		std::vector<Variable>::iterator end();
		std::vector<Variable>::const_iterator begin() const;
		std::vector<Variable>::const_iterator end() const;
		const Variable operator [] (const Integer& ind) const;
		Variable& operator [] (const Integer& ind);
		Bool operator == (const Array& obj) const;
		Bool operator != (const Array& obj) const;
		Bool Contains(const Variable& obj) const; // O(n), use only if necessary
		//const Variable First() const;
		//Variable& First();
		//const Variable Last() const;
		//Variable& Last();

		static std::out_of_range OutOfBoundError(const Integer& ind, const Integer& size);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Dict : public GenericType
	{
	private:
		std::unordered_map<Variable, Variable> _dict;
	public:
		constexpr static Types::TypeName T = Types::Dict;
		Bool CastBool() const override;
		String CastString(AddressSet& set) const override;
		String CastStringMultireference() const override;
		Types::TypeName Type() const override;
		size_t Hash() const override;

		Integer Size() const;
		Bool Contains(const Variable& key) const;
		const Variable operator [] (const Variable& key) const;
		Variable& operator [] (const Variable& key);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Set is different than other types: when you put smt into Set, it is COPIED.
	// This is because Set requires values kept inside to be constant, otherwise it may break
	// so all variables you put in are copied and new objects are created for them. 
	// Because only this set has variables connected to those objecte, and there're no methods to access those variables, they can't be changed
	// and set stays safe
	// Also - then only existing Variable connected to object in set is instantly converted into constant variable, so even if you have access, it won't let Kirito user to change it
	class Set : public GenericType
	{
	private:
		std::unordered_set<Variable> _set;
	public:
		Set() {};
		Set(const Array& obj_arr);
		constexpr static Types::TypeName T = Types::Set;
		Bool CastBool() const override;
		String CastString(AddressSet& set) const override;
		Types::TypeName Type() const override;
		size_t Hash() const override;
		Integer Size() const;
		Bool Contains(const Variable& obj) const;
		void Update(const Set& obj);
		void Remove(const Variable& obj);
		void Insert(const Variable& obj); 
		void Insert(const Array& obj_arr);
		Bool operator == (const Set& obj) const;
		Bool operator != (const Set& obj) const;
		// Union, Intersection
		static Set Union(const Set& l, const Set& r);
		static Set Intersection(const Set& l, const Set& r);
		static Set Difference(const Set& l, const Set& r);
		static Set InverseIntersection(const Set& l, const Set& r);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif // !KIRITO_TYPESSECONDLAYER_H
