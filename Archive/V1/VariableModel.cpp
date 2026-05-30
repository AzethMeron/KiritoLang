
#include <cstdint>
#include <limits>

#include "VariableModel.h"
#include "Misc.h"
#include "TypesSecondLayer.h"

namespace Kirito
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	size_t Address::Hash() const
	{
		return (uintptr_t)this->_addr;
	}
	String Address::CastString() const
	{
		return Misc::StdStringFormat("%#08x", this->_addr);
	}
	Bool Address::CastBool() const
	{
		return this->_addr != nullptr;
	}
	Bool Address::operator == (const Address& obj) const
	{
		return this->Hash() == obj.Hash();
	}
	Bool Address::operator != (const Address& obj) const
	{
		return !(*this == obj);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	void AddressSet::Insert(const Address& obj) { this->_set.insert(obj); }
	Bool AddressSet::Contains(const Address& obj) { return Bool(this->_set.contains(obj)); }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Object::Object(GenericType* obj)
	{
		this->_count = 0;
		this->_ptr = obj;
	}
	void Object::Reference()
	{
		static constexpr int32_t limit = std::numeric_limits<int32_t>::max();
		if (this->_count == limit) throw Object::CounterOverflowError();
		this->_count++;
	}
	void Object::Dereference()
	{
		this->_count--;
		if (this->_count <= 0)
		{
			delete this->_ptr;
			delete this;
		}
	}
	int32_t Object::Count() const
	{
		return this->_count;
	}
	Types::TypeName Object::Type() const
	{
		return this->_ptr->Type();
	}

	const GenericType& Object::Access() const
	{
		return *(this->_ptr);
	}

	std::runtime_error Object::CounterOverflowError()
	{
		return std::runtime_error("Internal error: too many references created to the same object, counter overflown.");
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Variable::Variable()
	{
		this->_ptr = nullptr;
		this->_const = false;
		*this = ObjectFactory::New<None>();
	}
	Variable::~Variable()
	{
		this->_ptr->Dereference();
	}
	Bool Variable::Exist() const
	{
		return this->Type() != Types::None;
	}
	Variable::Variable(const Variable& obj)
	{
		this->_const = (bool)obj.IsConst();
		this->_ptr = obj._ptr;
		this->_ptr->Reference();
	}

	Variable::Variable(const Variable&& obj) noexcept
	{
		this->_const = (bool)obj.IsConst();
		this->_ptr = obj._ptr;
		this->_ptr->Reference();
	}

	Variable::Variable(Object* obj)
	{
		this->_const = false;
		this->_ptr = obj;
		this->_ptr->Reference();
	}

	std::invalid_argument Variable::TriedWriteIntoConst()
	{
		return std::invalid_argument("Constant error: attempt to modify value stored in constant var/ref.");
	}

	std::invalid_argument Variable::InvalidTypes(const Types::TypeName& l, const Types::TypeName& r)
	{
		return std::invalid_argument(Misc::StdStringFormat("Type assignment error: '%s' is not instance of '%s'.",
			Types::CastString(l).c_str(),
			Types::CastString(r).c_str()
		));
	}

	Bool Variable::IsInstanceOf(const Variable& obj) const
	{
		return this->Type() == obj.Type(); // temporary
	}

	Variable Variable::Copy() const
	{
		return ObjectFactory::Copy(*this);
	}

	Variable& Variable::operator = (const Variable& obj)
	{
		if (this->IsConst()) { throw Variable::TriedWriteIntoConst(); }
		if (this->_ptr != nullptr) this->_ptr->Dereference();
		this->_const = (bool)obj.IsConst();
		this->_ptr = obj._ptr;
		this->_ptr->Reference();
		return *this;
	}

	Types::TypeName Variable::Type() const
	{
		return this->_ptr->Type();
	}

	Bool Variable::IsConst() const
	{
		return this->_const;
	}

	void Variable::MakeConst()
	{
		this->_const = true;
	}

	Integer Variable::Count() const
	{
		if (this->Type() == Types::None) return 0;
		return this->_ptr->Count();
	}

	size_t Variable::Hash() const
	{
		return this->_ptr->Access().Hash();
	}
	Bool Variable::CastBool() const
	{
		return this->_ptr->Access().CastBool();
	}
	String Variable::CastString() const
	{
		AddressSet set;
		return this->CastString(set);
	}
	String Variable::CastString(AddressSet& set) const
	{
		if (set.Contains(this->Identity()))
		{
			if (!Types::IsFirstLayer(this->Type()))
				return this->_ptr->Access().CastStringMultireference();
		}
		set.Insert(this->Identity());
		return this->_ptr->Access().CastString(set);
	}

	Bool Variable::operator == (const Variable& obj) const
	{
		if (this->IsInstanceOf(obj))
		{
			switch (this->Type())
			{
			case Types::Bool: { return this->Convert<Bool>() == obj.Convert<Bool>(); } break;
			case Types::Integer: { return this->Convert<Integer>() == obj.Convert<Integer>(); } break;
			case Types::Float: { return this->Convert<Float>() == obj.Convert<Float>(); } break;
			case Types::String: { return this->Convert<String>() == obj.Convert<String>(); } break;
			case Types::Array: { return this->Convert<Array>() == obj.Convert<Array>(); } break;
			case Types::Set: { return this->Convert<Set>() == obj.Convert<Set>(); } break;
			//case Types::Dict: { return this->Convert<Dict>() == obj.Convert<Dict>(); } break;
			case Types::Dump: { return false;} break; // temporary
			default: { throw Types::UnkownError(this->Type()); } break;
			}
		}
		return false;
	}
	Bool Variable::operator != (const Variable& obj) const
	{
		return !(*this == obj);
	}

	Address Variable::Identity() const { return Address(this->_ptr); }
	Bool Variable::Is(const Variable& obj) const { return this->Identity() == obj.Identity(); }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	Variable ObjectFactory::Copy(const Variable& obj)
	{
		switch (obj.Type())
		{
		case Types::Integer: { return ObjectFactory::New(obj.Convert<Integer>()); } break;
		case Types::Float: { return ObjectFactory::New(obj.Convert<Float>()); } break;
		case Types::Bool: { return ObjectFactory::New(obj.Convert<Bool>()); } break;
		case Types::String: { return ObjectFactory::New(obj.Convert<String>()); } break;
			//case Types::Function: { return ObjectFactory::New(obj.Convert<Function>()); } break;
			//case Types::List: { return ObjectFactory::New(obj.Convert<List>()); } break;
		case Types::Array: { return ObjectFactory::New(obj.Convert<Array>()); } break;
			//case Types::Set: { return ObjectFactory::New(obj.Convert<Set>()); } break;
			//case Types::Dict: { return ObjectFactory::New(obj.Convert<Dict>()); } break;
			//case Types::Class: { return ObjectFactory::New(obj.Convert<Class>()); } break;
			//case Types::Dump: { return ObjectFactory::New(obj.Convert<Dump>()); } break;
		default: { return Variable(); } break;
		}
	}

	template<>
	static Variable ObjectFactory::New<None>()
	{
		// If, at some point, you decide to hack-into multithreading, i suggest making those static variables thread_local
		static None* none = new None();
		static Object* ptr = new Object(none);
		static Variable holder(ptr);
		return holder;
	}
}