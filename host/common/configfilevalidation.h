#ifndef CONFIG_FILE_VALIDATION_H
#define CONFIG_FILE_VALIDATION_H

///
/// \file configfilevalidation.h
/// \brief Common utilities for validating config files before parsing.
///
/// Provides file size validation and safe type conversion to prevent
/// crashes and hangs when reading malformed or fuzzed configuration files.
///

#include <string>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/cstdint.hpp>

/// Maximum allowed size for INI-style config files (1 MB)
static const uintmax_t MAX_INI_CONFIG_FILE_SIZE = 1 * 1024 * 1024;

/// Maximum allowed size for JSON config files (10 MB)
static const uintmax_t MAX_JSON_CONFIG_FILE_SIZE = 10 * 1024 * 1024;

/// \brief Validates that a config file exists and its size is within acceptable limits.
/// \param filePath Path to the config file.
/// \param maxSize Maximum allowed file size in bytes.
/// \param errMsg [out] Populated with error details on failure.
/// \returns true if the file is valid, false otherwise.
inline bool ValidateConfigFileSize(
    const std::string& filePath,
    uintmax_t maxSize,
    std::string& errMsg)
{
    boost::system::error_code ec;
    if (!boost::filesystem::exists(filePath, ec))
    {
        if (ec)
        {
            errMsg = "Error checking existence of config file: " + filePath +
                ". Error: " + ec.message();
        }
        else
        {
            errMsg = "Config file does not exist: " + filePath;
        }
        return false;
    }

    uintmax_t fileSize = boost::filesystem::file_size(filePath, ec);
    if (ec)
    {
        errMsg = "Failed to get file size for: " + filePath +
            ". Error: " + ec.message();
        return false;
    }

    if (fileSize > maxSize)
    {
        errMsg = "Config file exceeds maximum allowed size (" +
            boost::lexical_cast<std::string>(maxSize) + " bytes): " +
            filePath + " (actual size: " +
            boost::lexical_cast<std::string>(fileSize) + " bytes)";
        return false;
    }

    return true;
}

/// \brief Safe wrapper around boost::lexical_cast that returns a default
///        value instead of throwing on conversion failure.
/// \param val The string value to convert.
/// \param defaultVal The default value to return on failure.
/// \returns The converted value, or defaultVal if conversion fails.
template<typename T>
T safe_lexical_cast(const std::string& val, T defaultVal)
{
    try
    {
        return boost::lexical_cast<T>(val);
    }
    catch (const boost::bad_lexical_cast&)
    {
        return defaultVal;
    }
}

/// \brief Safe wrapper around boost::lexical_cast that returns T()
///        (zero/false) instead of throwing on conversion failure.
/// \note  T must be default-constructible (e.g., int, bool, unsigned).
/// \param val The string value to convert.
/// \returns The converted value, or T() if conversion fails.
template<typename T>
T safe_lexical_cast(const std::string& val)
{
    return safe_lexical_cast<T>(val, T());
}

#endif // CONFIG_FILE_VALIDATION_H
