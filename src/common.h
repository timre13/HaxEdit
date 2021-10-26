#pragma once

#include "types.h"

class InvalidUnicodeError : public std::runtime_error
{
public:
    InvalidUnicodeError()
        : std::runtime_error("Invalid Unicode content")
    {
    }
};

/*
 * Loads a text file while taking care of non-ASCII characters
 * and returns the content as UTF-32 encoded `String`.
 *
 * Throws `std::runtime_error` when `std::fstream.open()` fails.
 * Throws `InvalidUnicodeError` when the file contains invalid Unicode values.
 */
String loadUnicodeFile(const std::string& filePath);
