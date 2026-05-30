#ifndef KIRITO_VARIABLE_MODEL_H
#define KIRITO_VARIABLE_MODEL_H

#include <cstdint>
#include <unordered_set>
#include <stdexcept>

#include "ClassDeclarations.h"
#include "TypesFirstLayer.h"
#include "StlOverloads.h"

namespace Kirito
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Address {
	private:
		const void* _addr;
	public:
		Address(const void* addr) : _addr(addr) {}
		size_t Hash() const;
		String CastString() const;
		Bool CastBool() const;
		Bool operator == (const Address& obj) const;
		Bool operator != (const Address& obj) const;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class AddressSet {
	private:
		std::unordered_set<Address> _set;
	public:
		void Insert(const Address& obj);
		Bool Contains(const Address& obj);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Object // all objects MUST be on heap. Constructor will be parametrised and private, so only friend (ObjectFactory) may create objects
	// User should NEVER have raw access to Object class, only via Variable
	{
	private:
		int32_t _count;
		GenericType* _ptr;
		Object(GenericType* obj); friend class ObjectFactory;
	public:
		Object() = delete;
		Object(const Object&) = delete;
		Object(const Object&&) = delete;
		Object& operator = (const Object&) = delete;
		// Increase/decrease counter. If it drops to 0, delete _ptr and "delete this"
		void Reference();
		void Dereference();
		int32_t Count() const;
		Types::TypeName Type() const;
		const GenericType& Access() const;

		template<typename TYPE>
		TYPE& Convert()
		{
			if (this->Type() != TYPE::T)
			{
				throw Types::TypeError(TYPE::T, this->Type());
			}
			return *((TYPE*)this->_ptr);
		}

		template<typename TYPE>
		const TYPE& Convert() const
		{
			if (this->Type() != TYPE::T)
			{
				throw Types::TypeError(TYPE::T, this->Type());
			}
			return *((TYPE*)this->_ptr);
		}

		static std::runtime_error CounterOverflowError();
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class Variable
	{
	private:
		bool _const; // Doesn't actually preventyou from changing values of object, but please, don't
		Object* _ptr; // nullptr by default
		Variable(Object* obj); friend class ObjectFactory;
	public:
		Variable();
		~Variable();
		// copyable
		Variable(const Variable&);
		Variable(const Variable&&) noexcept;
		Variable& operator = (const Variable& obj);
		// Get basic info
		Bool IsConst() const;
		Integer Count() const;
		Bool Exist() const; // Check if variable has assigned any object
		Address Identity() const; // Address of underlying Object, used to determine if two variables are referenced to the same
		Bool Is(const Variable& obj) const;
		// Change to constant/reference
		void MakeConst();

		template<typename TYPE>
		TYPE& Convert()
		{
			return this->_ptr->Convert<TYPE>();
		}
		template<typename TYPE>
		const TYPE& Convert() const
		{
			return this->_ptr->Convert<TYPE>();
		}

		template<typename TYPE>
		Variable(const TYPE& obj) : Variable(ObjectFactory::New<TYPE>(obj)) {}

		explicit operator bool() const
		{
			return this->_ptr->Access().CastBool().Value();
		}

		Bool CastBool() const;
		String CastString() const;
		String CastString(AddressSet& set) const;
		size_t Hash() const;
		Types::TypeName Type() const;
		Bool operator == (const Variable& obj) const;
		Bool operator != (const Variable& obj) const;
		Bool IsInstanceOf(const Variable& obj) const;
		Variable Copy() const;

		static std::invalid_argument TriedWriteIntoConst();
		static std::invalid_argument InvalidTypes(const Types::TypeName& l, const Types::TypeName& r);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class ObjectFactory // static class, creates Object on heap and returns Variable with it
	{
	public:
		ObjectFactory() = delete;

		template<typename TYPE>
		static Variable New()
		{
			GenericType* ptr = new TYPE();
			Object* obj_ptr = new Object(ptr);
			return Variable(obj_ptr);
		}
		template<typename TYPE>
		static Variable New(const TYPE& obj)
		{
			GenericType* ptr = new TYPE(obj);
			Object* obj_ptr = new Object(ptr);
			return Variable(obj_ptr);
		}

		template<>
		static Variable New<None>();

		static Variable Copy(const Variable& obj);
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif // !KIRITO_VARIABLE_MODEL_H
