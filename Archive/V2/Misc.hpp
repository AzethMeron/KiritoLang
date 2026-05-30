#ifndef KIRITO_MISC_HPP
#define KIRITO_MISC_HPP

#include <string>
#include <memory>
#include <string>
#include <stdexcept>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

namespace Kirito
{
	class Misc
	{
	private:
		//https://rosettacode.org/wiki/CRC-32#C.2B.2B
		// Generates a lookup table for the checksums of all 8-bit values.
		static std::array<std::uint_fast32_t, 256> _generate_crc_lookup_table() noexcept
		{
			auto const reversed_polynomial = std::uint_fast32_t{ 0xEDB88320uL };

			// This is a function object that calculates the checksum for a value,
			// then increments the value, starting from zero.
			struct byte_checksum
			{
				std::uint_fast32_t operator()() noexcept
				{
					auto checksum = static_cast<std::uint_fast32_t>(n++);

					for (auto i = 0; i < 8; ++i)
						checksum = (checksum >> 1) ^ ((checksum & 0x1u) ? reversed_polynomial : 0);

					return checksum;
				}

				unsigned n = 0;
			};

			auto table = std::array<std::uint_fast32_t, 256>{};
			std::generate(table.begin(), table.end(), byte_checksum{});

			return table;
		}
	public:
		// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
		template<typename ... Args>
		static std::string StdStringFormat(const std::string& format, Args ... args)
		{
			int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
			if (size_s <= 0) { throw std::runtime_error("Internal Error: Error during string formatting."); }
			auto size = static_cast<size_t>(size_s);
			std::unique_ptr<char[]> buf(new char[size]);
			std::snprintf(buf.get(), size, format.c_str(), args ...);
			return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
		}

		// Calculates the CRC for any sequence of values. (You could use type traits and a
		// static assert to ensure the values can be converted to 8 bits.)
		template <typename InputIterator>
		static std::uint_fast32_t CRC(InputIterator first, InputIterator last)
		{
			// Generate lookup table only on first use then cache it - this is thread-safe.
			static auto const table = _generate_crc_lookup_table();

			// Calculate the checksum - make sure to clip to 32 bits, for systems that don't
			// have a true (fast) 32-bit type.
			return std::uint_fast32_t{ 0xFFFFFFFFuL } &
				~std::accumulate(first, last,
					~std::uint_fast32_t{ 0 } &std::uint_fast32_t{ 0xFFFFFFFFuL },
					[](std::uint_fast32_t checksum, std::uint_fast8_t value)
					{ return table[(checksum ^ value) & 0xFFu] ^ (checksum >> 8); });
		}
	};
}

#endif KIRITO_MISC_HPP