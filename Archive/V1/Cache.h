#ifndef KIRITO_CACHE_HPP
#define KIRITO_CACHE_HPP

#include <stdexcept>
#include "TypesFirstLayer.h"

namespace Kirito
{
	template<typename TYPE>
	class Cache
	{
	private:
		bool _exist;
		TYPE _value;
	public:
		Bool Exist() const { return this->_exist; }
		TYPE Fetch() const { return this->_value; }
		void Set(const TYPE& val) { this->_value = val; }
		void Clear() { this->_exist = false; this->_value = TYPE(); }
		Cache<TYPE>() { this->_exist = false; this->_value = TYPE(); }
		explicit operator bool() const { return this->_exist; }
		TYPE  operator * () const { if (!this->Exist()) throw std::invalid_argument("Internal error: Accessing value in not-assigned cache."); return this->_value; }
		TYPE& operator * ()       { return this->_value; }
	};
}

#endif