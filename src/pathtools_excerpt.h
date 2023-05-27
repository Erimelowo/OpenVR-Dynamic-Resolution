#pragma once
#ifndef PATHTOOLS_H

#include <string>

/** Makes an absolute path from a relative path and a base path */
std::string Path_MakeAbsolute(const std::string& sRelativePath, const std::string& sBasePath);

/** Returns the path (including filename) to the current executable */
std::string Path_GetExecutablePath();

/** Returns the specified path without its filename */
std::string Path_StripFilename(const std::string& sPath, char slash = 0);

bool Path_IsAbsolute(const std::string& sPath);
std::string Path_Compact(const std::string& sRawPath, char slash = 0);
std::string Path_FixSlashes(const std::string& sPath, char slash = 0);
std::string Path_Join(const std::string& first, const std::string& second, char slash = 0);
char Path_GetSlash();

#ifndef MAX_UNICODE_PATH
#define MAX_UNICODE_PATH 32767
#endif

#ifndef MAX_UNICODE_PATH_IN_UTF8
#define MAX_UNICODE_PATH_IN_UTF8 (MAX_UNICODE_PATH * 4)
#endif

#endif