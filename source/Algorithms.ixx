export module Algorithms;

import <algorithm>;
import <list>;
import <string>;
import <vector>;


//////// Fielding ////////

/// <summary>
/// Get a 32 bit value from a byte vector at a specified offset.
/// </summary>
/// <param name="values">Vector of byte values.</param>
/// <param name="offset">Offset.</param>
/// <returns>32 bit value.</returns>
export uint32_t getLong(std::vector<uint8_t>& values, const int32_t offset)
{
    return *(uint32_t*)&(values[offset]);
}


/// <summary>
/// Get a 16 bit value from a byte vector at a specified offset.
/// </summary>
/// <param name="values">Vector of byte values.</param>
/// <param name="offset">Offset.</param>
/// <returns>Unsigned 16 bit value.</returns>
export uint16_t getShort(std::vector<uint8_t>& values, const int32_t offset)
{
    return *(uint16_t*)&(values[offset]);
}


/// <summary>
/// Get a byte value from a byte vector at a specified offset.
/// </summary>
/// <param name="values">Vector of byte values.</param>
/// <param name="offset">Offset.</param>
/// <returns>Unsigned 32 bit value.</returns>
export uint8_t getByte(std::vector<uint8_t>& values, const int32_t offset)
{
    return *(uint8_t*)&(values[offset]);
}


/// <summary>
/// Mask out the low word of a 32 bit unsigned integer.
/// </summary>
/// <param name="l">Value.</param>
/// <returns>Low word.</returns>
export uint16_t loWord(uint32_t l)
{
    return uint16_t(l & 0xffff);
}


/// <summary>
/// Mask out the high word of a 32 bit unsigned integer.
/// </summary>
/// <param name="l">Value.</param>
/// <returns>High word.</returns>
export uint16_t hiWord(uint32_t l)
{
    return uint16_t((l >> 16) & 0xffff);
}


//////// Characters ////////

/// <summary>
/// Convert a character to lower case. The conversion considers only the characters A-Z.
/// </summary>
/// <param name="c">Character.</param>
/// <returns>Lower case character.</returns>
export char toLower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    return c;
}


/// <summary>
/// Convert a character to upper case. The conversion considers only the characters a-z.
/// </summary>
/// <param name="c">Character.</param>
/// <returns>Upper case character.</returns>
export char toUpper(char c)
{
    if (c >= 'a' && c <= 'z') {
        c += 'A' - 'a';
    }
    return c;
}


/// <summary>
/// Check if a given character is in the range between space and ~.
/// </summary>
/// <param name="c">Character.</param>
/// <returns>True, if the given character is in the range between space and ~.</returns>
export bool isPrintable(char c)
{
    return c >= ' ' && c <= '~';
}


/// <summary>
/// Check if a given character is alpha-numeric.
/// </summary>
/// <param name="c">Character.</param>
/// <returns>True, if the given character is alpha-numeric.</returns>
export bool isAlphaNumeric(char c)
{
    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9';
}


/// <summary>
/// Check if a given character is numeric.
/// </summary>
/// <param name="c">Character.</param>
/// <returns>True, if the given character is numeric.</returns>
export bool isNumeric(std::string_view s)
{
    for (char c : s) {
        if (!isdigit(c) || c == '-') {
            return false;
        }
    }
    return true;
}


//////// C-Strings ////////

/// <summary>
/// Get the length of a c-string.
/// </summary>
/// <param name="string"></param>
/// <returns></returns>
export uint32_t STRLEN(const char* string)
{
    uint32_t len = 0;

    while (string[len]) {
        ++len;
    }
    return len;
}


/// <summary>
/// Check if a c-string starts with a given c-string.
/// </summary>
/// <param name="control">String.</param>
/// <param name="constant">Containing string.</param>
/// <returns>True, if the given c-string starts with the given c-string.</returns>
export bool CMPSTART(const char* control, const char* constant)
{
    char c;

    while (c = *control) {
        if (*constant != c) {
            return false;
        }
        ++control;
        ++constant;
    }
    return true;
}


/// <summary>
/// CHeck if two c-strings are equal.
/// </summary>
/// <param name="a">String.</param>
/// <param name="b">String.</param>
/// <returns>True, if the two c-strings are equal.</returns>
export bool CMPSTR(const char* a, const char* b)
{
    char c;

    while (toLower(c = *a++) == toLower(*b++)) {
        if (!c) {
            return true;
        }
    }
    return false;
}


//////// Strings ////////

/// <summary>
/// Convert the characters of a string to lower case.
/// </summary>
/// <param name="s">String.</param>
/// <returns>Lower case string.</returns>
export std::string toLower(std::string_view s)
{
    std::string ls = std::string(s.begin(), s.end());

    std::transform(ls.begin(), ls.end(), ls.begin(), [](unsigned char c) { return tolower(c); });
    return ls;
}


/// <summary>
/// Convert the characters of a string to upper case.
/// </summary>
/// <param name="s">String.</param>
/// <returns>Upper case string.</returns>
export std::string toUpper(std::string_view s)
{
    std::string us = std::string(s.begin(), s.end());

    std::transform(us.begin(), us.end(), us.begin(), [](unsigned char c) { return toupper(c); });
    return us;
}


/// <summary>
/// Trim invisible characters from the left hand side of a string.
/// </summary>
/// <param name="s">String.</param>
/// <returns>Trimmed string.</returns>
export std::string trimLeft(std::string_view s)
{
    size_t startpos = s.find_first_not_of(" \n\r\t");

    return (startpos == std::string::npos) ? "" : std::string(s.substr(startpos));
}


/// <summary>
/// Trim invisible characters from the right hand side of a string.
/// </summary>
/// <param name="s">String.</param>
/// <returns>Trimmed string.</returns>
export std::string trimRight(std::string_view s)
{
    size_t endpos = s.find_last_not_of(" \n\r\t");

    return (endpos == std::string::npos) ? "" : std::string(s.substr(0, endpos + 1));
}


/// <summary>
/// Trim invisible characters from both sides of a string.
/// </summary>
/// <param name="s">String.</param>
/// <returns>Trimmed string.</returns>
export std::string trim(std::string_view s)
{
    return trimRight(trimLeft(s));
}


/// <summary>
/// Pad the left hand side of a string to a specified size with spaces.
/// </summary>
/// <param name="s">String.</param>
/// <param name="size">Size.</param>
/// <returns>Padded string.</returns>
export std::string padLeft(std::string_view s, size_t size)
{
    std::string pads{ s };

    if (pads.size() < size) {
        return std::string(size - pads.size(), ' ') + pads;
    }
    return pads;
}


/// <summary>
/// Pad the right hand side of a string to a specified size with spaces.
/// </summary>
/// <param name="s">String.</param>
/// <param name="size">Size.</param>
/// <returns>Padded string.</returns>
export std::string padRight(std::string_view s, size_t size)
{
    std::string pads{ s };

    if (pads.size() < size) {
        return pads + std::string(size - pads.size(), ' ');
    }
    return pads;
}


/// <summary>
/// Split a given string by a given delimiter.
/// </summary>
/// <param name="s">String to be split.</param>
/// <param name="delimiter">Delimiter.</param>
/// <returns>Vector of split substrings.</returns>
export std::vector<std::string> split(std::string_view s, char delimiter)
{
    std::vector<std::string> result;
    std::string sp{ s };

    while (sp.size()) {
        size_t index = sp.find(delimiter);

        if (index != std::string::npos) {
            result.push_back(std::string(sp.substr(0, index)));
            sp = sp.substr(index + 1);
        }
        else {
            break;
        }
    }
    result.push_back(sp);
    return result;
}


/// <summary>
/// Split a given string at the first occurence of a delimiter. The remaining string is returned 
/// in the reference parameter.
/// </summary>
/// <param name="s">String to be split.</param>
/// <param name="delimiter">Delimiter.</param>
/// <returns>Split substring.</returns>
export std::string splitFirst(std::string& s, char delimiter)
{
    std::string ret;
    size_t index = s.find(delimiter);

    if (index != std::string::npos) {
        ret = trim(s.substr(0, index));
        s = trim(s.substr(index + 1));
    }
    else {
        ret = trim(s);
        s.clear();
    }
    return ret;
}


/// <summary>
/// Join a list of strings with a given character.
/// </summary>
/// <param name="sl">String list.</param>
/// <param name="c">Character.</param>
/// <returns>Joined string.</returns>
export std::string join(std::list<std::string>& sl, char c = '\n')
{
    std::string joined;

    for (auto& msg : sl) {
        if (!msg.empty()) {
            joined += msg + c;
        }
    }
    return joined;
}


/// <summary>
/// Replace all occurances of a substring in a string.
/// </summary>
/// <param name="s">Source string.</param>
/// <param name="search">Substring to be replaced.</param>
/// <param name="replace">String used to replace the substring.</param>
export std::string replaceString(std::string_view s, std::string_view search,
    const std::string& replace) 
{
    std::string retStr{ s };
    size_t pos = 0;

    while ((pos = retStr.find(search, pos)) != std::string::npos) {
        retStr.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return retStr;
}


/// <summary>
/// Convert a string to contain only ASCII characters equal or less 127. All other characters 
/// are replaced with a ? character.
/// </summary>
/// <param name="s">String to be converted</param>
/// <returns>Converted string.</returns>
export std::string convertStringToASCII(std::string_view s)
{
    std::string cs;
    unsigned char c;

    for (int i = 0; i < s.length(); i++) {
        c = s.at(i);
        cs += c > char(127) ? '?' : c;
    }
    return cs;
}


//////// Quick swaping ////////

/// <summary>
/// Swap two referenced 32 bit values.
/// </summary>
/// <param name="a">Value.</param>
/// <param name="b">Value.</param>
export void swap(uint32_t& a, uint32_t& b)
{
    a ^= b;
    b ^= a;
    a ^= b;
}


/// <summary>
/// Swap two referenced 16 bit values.
/// </summary>
/// <param name="a">Value.</param>
/// <param name="b">Value.</param>
export void swap(uint16_t& a, uint16_t& b)
{
    a ^= b;
    b ^= a;
    a ^= b;
}

