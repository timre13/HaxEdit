#pragma once

#include "../types.h"
#include <stdexcept>
#include <string>

class InvalidUnicodeError : public std::runtime_error
{
public:
    InvalidUnicodeError();
};

/*
 * Loads a text file while taking care of non-ASCII characters
 * and returns the content as UTF-32 encoded `String`.
 *
 * Throws `std::runtime_error` when `std::fstream.open()` fails.
 * Throws `InvalidUnicodeError` when the file contains invalid Unicode values.
 */
String loadUnicodeFile(const std::string& filePath);


/*
 * Loads an ASCII encoded text file and returns the content as `std::string`.
 *
 * Throws `std::runtime_error` when `std::fstream.open()` fails.
 */
std::string loadAsciiFile(const std::string& filePath);
