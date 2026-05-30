
#include <exception>
#include <algorithm>
#include <vector>

#include "Number.hpp"
#include "String.hpp"

namespace Kirito
{
	String::operator Number() const // TODO
	{
		throw std::exception("Not implemented yet");
		return Number();
	}

	std::ostream& operator << (std::ostream& stream, const String& string)
	{
		stream << string.Value();
		return stream;
	}

	Number Levenshtein(const String& string1, const String& string2)
	{
		const std::string& s1 = string1.Value();
		const std::string& s2 = string2.Value();
		
        int len1 = s1.length();
        int len2 = s2.length();

        // Create a 2D vector to store the distances
        std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

        // Initialize the first column and row
        for (int i = 0; i <= len1; i++) d[i][0] = i;
        for (int j = 0; j <= len2; j++) d[0][j] = j;

        // Compute the distances
        for (int i = 1; i <= len1; i++) {
            for (int j = 1; j <= len2; j++) {
                // Cost of substitution
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;

                // Calculate minimum of insert, delete, and substitute
                d[i][j] = std::min({ d[i - 1][j] + 1,  // Deletion
                                    d[i][j - 1] + 1,  // Insertion
                                    d[i - 1][j - 1] + cost }); // Substitution
            }
        }

        // The bottom-right corner of the matrix contains the Levenshtein distance
        return Number(d[len1][len2]);
	}
}