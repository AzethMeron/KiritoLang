#ifndef KIRITO_STRING_HPP
#define KIRITO_STRING_HPP

namespace Kirito
{
	class String;
}

#include <string>
#include <iostream>
#include "Number.hpp"

namespace Kirito
{
	class String
	{
		private:
			std::string _value;
			size_t _hash;
			void UpdateHash() // Any non-constant method must call it
			{
				static auto hasher = std::hash<std::string>();
				this->_hash = hasher(this->_value);
			}
		public:
			String() { this->_value = ""; this->UpdateHash(); }
			String(const std::string& i) { this->_value = i; this->UpdateHash(); }
			String(const std::string&& i) { this->_value = i; this->UpdateHash(); }
			std::string Value() const { return this->_value; }
			const char* c_str() const { return this->_value.c_str(); }
			explicit operator Number() const;
	};

	std::ostream& operator << (std::ostream& stream, const String& string);

	Number Levenshtein(const String& string1, const String& string2);
}

#endif // !KIRITO_STRING_HPP
