#ifndef KIRITO_BOOL_HPP
#define KIRITO_BOOL_HPP

namespace Kirito
{
	class Bool;
}

namespace Kirito
{
	class Bool
	{
		private:
			bool _value;
		public:
			bool Value() const { return this->_value; }
			explicit operator bool() const { return this->_value; }
			Bool(const bool& b) { this->_value = b; }
			Bool() { this->_value = false; }

			Bool operator == (const Bool& b) const { return this->_value == b._value; } // xnor
			Bool operator != (const Bool& b) const { return this->_value != b._value; } // xor
			Bool operator + (const Bool& b) const { return this->_value || b._value; } // or
			Bool operator * (const Bool& b) const { return this->_value && b._value; } // and
			Bool Not() const { return !this->_value; }
	};
}

#endif // !KIRITO_BOOL_HPP
