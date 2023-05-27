/*
pathtools_excerpt.h - Excerpt of the full shared pathtools.c from valve:

Copyright(c) 2015, Valve Corporation
All rights reserved.

Redistributionand use in sourceand binary forms, with or without modification,
are permitted provided that the following conditions are met :

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditionsand the following disclaimer in the documentationand /or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#if defined(WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#elif defined(__unix__)

#include <unistd.h>

#endif
#include <stdlib.h>
#include "pathtools_excerpt.h"

bool Path_IsAbsolute(const std::string& sPath)
{
    if (sPath.empty())
        return false;

#if defined( WIN32 )
    if (sPath.size() < 3) // must be c:\x or \\x at least
        return false;

    if (sPath[1] == ':') // drive letter plus slash, but must test both slash cases
    {
        if (sPath[2] == '\\' || sPath[2] == '/')
            return true;
    }
    else if (sPath[0] == '\\' && sPath[1] == '\\') // UNC path
        return true;
#else
    if (sPath[0] == '\\' || sPath[0] == '/') // any leading slash
        return true;
#endif

    return false;
}


/** Removes redundant <dir>/.. elements in the path. Returns an empty path if the
* specified path has a broken number of directories for its number of ..s */
std::string Path_Compact(const std::string& sRawPath, char slash)
{
    if (slash == 0)
        slash = Path_GetSlash();

    std::string sPath = Path_FixSlashes(sRawPath, slash);
    std::string sSlashString(1, slash);

    // strip out all /./
    for (std::string::size_type i = 0; (i + 3) < sPath.length(); )
    {
        if (sPath[i] == slash && sPath[i + 1] == '.' && sPath[i + 2] == slash)
        {
            sPath.replace(i, 3, sSlashString);
        }
        else
        {
            ++i;
        }
    }


    // get rid of trailing /. but leave the path separator
    if (sPath.length() > 2)
    {
        std::string::size_type len = sPath.length();
        if (sPath[len - 1] == '.' && sPath[len - 2] == slash)
        {
            // sPath.pop_back();
            sPath[len - 1] = 0;  // for now, at least
        }
    }

    // get rid of leading ./ 
    if (sPath.length() > 2)
    {
        if (sPath[0] == '.' && sPath[1] == slash)
        {
            sPath.replace(0, 2, "");
        }
    }

    // each time we encounter .. back up until we've found the previous directory name
    // then get rid of both
    std::string::size_type i = 0;
    while (i < sPath.length())
    {
        if (i > 0 && sPath.length() - i >= 2
            && sPath[i] == '.'
            && sPath[i + 1] == '.'
            && (i + 2 == sPath.length() || sPath[i + 2] == slash)
            && sPath[i - 1] == slash)
        {
            // check if we've hit the start of the string and have a bogus path
            if (i == 1)
                return "";

            // find the separator before i-1
            std::string::size_type iDirStart = i - 2;
            while (iDirStart > 0 && sPath[iDirStart - 1] != slash)
                --iDirStart;

            // remove everything from iDirStart to i+2
            sPath.replace(iDirStart, (i - iDirStart) + 3, "");

            // start over
            i = 0;
        }
        else
        {
            ++i;
        }
    }

    return sPath;
}

/** Fixes the directory separators for the current platform */
std::string Path_FixSlashes(const std::string& sPath, char slash)
{
    if (slash == 0)
        slash = Path_GetSlash();

    std::string sFixed = sPath;
    for (std::string::iterator i = sFixed.begin(); i != sFixed.end(); i++)
    {
        if (*i == '/' || *i == '\\')
            *i = slash;
    }

    return sFixed;
}

/** Jams two paths together with the right kind of slash */
std::string Path_Join(const std::string& first, const std::string& second, char slash)
{
    if (slash == 0)
        slash = Path_GetSlash();

    // only insert a slash if we don't already have one
    std::string::size_type nLen = first.length();
    if (!nLen)
        return second;
#if defined(_WIN32)
    if (first.back() == '\\' || first.back() == '/')
        nLen--;
#else
    char last_char = first[first.length() - 1];
    if (last_char == '\\' || last_char == '/')
        nLen--;
#endif

    return first.substr(0, nLen) + std::string(1, slash) + second;
}


/** Makes an absolute path from a relative path and a base path */
std::string Path_MakeAbsolute(const std::string& sRelativePath, const std::string& sBasePath)
{
    if (Path_IsAbsolute(sRelativePath))
        return sRelativePath;
    else
    {
        if (!Path_IsAbsolute(sBasePath))
            return "";

        std::string sCompacted = Path_Compact(Path_Join(sBasePath, sRelativePath));
        if (Path_IsAbsolute(sCompacted))
            return sCompacted;
        else
            return "";
    }
}

/** Returns the path (including filename) to the current executable */
std::string Path_GetExecutablePath()
{
#if defined( _WIN32 )
	wchar_t *pwchPath = new wchar_t[MAX_UNICODE_PATH];
	char *pchPath = new char[MAX_UNICODE_PATH_IN_UTF8];
	::GetModuleFileNameW( NULL, pwchPath, MAX_UNICODE_PATH );
	WideCharToMultiByte( CP_UTF8, 0, pwchPath, -1, pchPath, MAX_UNICODE_PATH_IN_UTF8, NULL, NULL );
	delete[] pwchPath;

	std::string sPath = pchPath;
	delete[] pchPath;
	return sPath;
#elif defined( __unix__ )
	char rchPath[1024];
	size_t nBuff = sizeof( rchPath );
	ssize_t nRead = readlink("/proc/self/exe", rchPath, nBuff-1 );
	if ( nRead != -1 )
	{
		rchPath[ nRead ] = 0;
		return rchPath;
	}
	else
	{
		return "";
	}
#else
    #error Implement Plat_GetExecutablePath
	// AssertMsg( false, "Implement Plat_GetExecutablePath" );
	// return "";
#endif
}

/** Returns the specified path without its filename */
std::string Path_StripFilename(const std::string& sPath, char slash)
{
    if (slash == 0)
        slash = Path_GetSlash();

    std::string::size_type n = sPath.find_last_of(slash);
    if (n == std::string::npos)
        return sPath;
    else
        return std::string(sPath.begin(), sPath.begin() + n);
}

char Path_GetSlash()
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}