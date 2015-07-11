//----------------------------------------------------------------------------
/// \file  path.cpp
//----------------------------------------------------------------------------
/// \brief Implementation of general purpose functions for path manipulation.
//----------------------------------------------------------------------------
// Copyright (c) 2010 Serge Aleynikov <saleyn@gmail.com>
// Created: 2010-05-06
//----------------------------------------------------------------------------
/*
***** BEGIN LICENSE BLOCK *****

This file is part of the utxx open-source project.

Copyright (C) 2010 Serge Aleynikov <saleyn@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***** END LICENSE BLOCK *****
*/

#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <utxx/path.hpp>
#include <utxx/error.hpp>
#include <utxx/string.hpp>
#include <utxx/scope_exit.hpp>
#include <dirent.h>

#if defined(_WIN32) || defined (_WIN64) || defined(_MSC_VER)
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <fstream>
#else
#include <sys/stat.h>
#endif

namespace utxx {
namespace path {

const char* basename(const char* begin, const char* end) {
    BOOST_ASSERT(begin <= end);
    for(const char ch = path::slash(); end != begin; --end)
        if (*end == ch)
            return ++end;
    return begin;
}

std::string home() {
    const char* env = getenv("HOME");
    if (env)
        return env;
    #if defined(_MSC_VER) || defined(_WIN32) || defined(__CYGWIN32__)
    env = getenv("USERPROFILE");
    if (env)
        return env;
    char const* c = getenv("HOMEDRIVE"), *home = getenv("HOMEPATH");
    return (!c || !home) ? "" : std::string(c) + home;
    #else
    return "";
    #endif
}

bool file_exists(const char* a_path) {
    #if defined(_MSC_VER) || defined(_WIN32) || defined(__CYGWIN32__)
    std::ifstream l_stream;
    l_stream.open(a_path, std::ios_base::in);
    if(l_stream.is_open()) {
        l_stream.close();
        return true;
    }
    #else
    struct stat buf;
    if (::stat(a_path, &buf) != -1)
        return true;
    #endif
    return false;
}

std::pair<std::string, std::string>
filename_with_backup(const char* a_filename,
    const char* a_backup_dir, const char* a_backup_suffix, const struct tm* a_now)
{
    //boost::algorithm::replace_all(l_filename, std::string(l_slash), "-");
    std::string l_filename = replace_env_vars(a_filename, a_now);
    std::string l_backup(l_filename);
    size_t slash_pos = l_backup.find_last_of(slash());
    size_t dot_pos   = l_backup.find_last_of('.');
    std::string l_ext;

    if (dot_pos != std::string::npos &&
        (slash_pos == std::string::npos || dot_pos > slash_pos)) {
        l_ext = l_backup.substr(dot_pos);
        l_backup.erase(dot_pos);
    }

    using namespace boost::posix_time;
    struct tm  tm;
    const struct tm* ptm = &tm;
    if (a_now != NULL) {
        ptm = a_now;
    } else {
        ptime now = second_clock::local_time();
        tm = boost::posix_time::to_tm(now);
    }

    int n;
    char buf[384];
    if (a_backup_dir == NULL)
        n = snprintf(buf, sizeof(buf), "%s", l_backup.c_str());
    else
        n = snprintf(buf, sizeof(buf), "%s%c%s", 
            a_backup_dir, slash(), l_backup.c_str());
    if (a_backup_suffix != NULL)
        snprintf(buf+n, sizeof(buf)-n, "%s.%s", l_ext.c_str(), a_backup_suffix);
    else
        snprintf(buf+n, sizeof(buf)-n, "@%04d-%02d-%02d.%02d%02d%02d%s",
            ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
            ptm->tm_hour, ptm->tm_min, ptm->tm_sec, l_ext.c_str());
    return std::make_pair(l_filename, buf);
}

std::pair<bool, std::list<std::string>>
list_files(std::string const& a_dir, std::string const& a_filter,
           FileMatchT a_match_type)
{
    DIR*           dir;
    struct dirent  ent;
    struct dirent* res;
    std::regex     filter;

    if (a_match_type == FileMatchT::REGEX)
        filter = a_filter.c_str();

    std::list<std::string> out;

    if ((dir = ::opendir(a_dir.c_str())) == nullptr)
        return std::make_pair(false, out);

    char buf[512];

    getcwd(buf, sizeof(buf));
    chdir(a_dir.c_str());

    scope_exit se([dir, &buf]() { closedir(dir); chdir(buf); });

    while (::readdir_r(dir, &ent, &res) == 0 && res != nullptr) {
        struct stat s;
        std::string file(ent.d_name);

        if (stat(ent.d_name, &s) < 0 || !S_ISREG(s.st_mode))
            continue;

        if (!a_filter.empty()) {
            bool matched;
            // Skip if no match
            switch (a_match_type) {
                case FileMatchT::REGEX:
                    matched = std::regex_match(file, filter);
                    break;
                case FileMatchT::PREFIX:
                    matched = strncmp(file.c_str(), a_filter.c_str(), a_filter.size()) == 0;
                    break;
                case FileMatchT::WILDCARD:
                    matched = wildcard_match(file.c_str(), a_filter.c_str());
                    break;
                default:
                    matched = false;
            }
            if (!matched)
                continue;
        }
        out.push_back(file);
    }

    return std::make_pair(true, out);
}

program::program()
{
#ifdef __linux__
    m_exe      = __progname;
    m_rel_path = __progname_full;
    size_t n   = m_rel_path.find_last_of('/');
    if (n != std::string::npos)
        m_rel_path.erase(n);
    char exe[256];
    n = readlink("/proc/self/exe", exe, sizeof(exe));
    for (char* p=exe+n; p > exe; --p)
        if (*p == '/') {
            *p = '\0';
            m_abs_path = exe;
            return;
        }
#elif defined(_MSC_VER) || defined(_WIN32) || defined(__CYGWIN32__)
    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
    TCHAR str_dll_path[MAX_PATH];
    //::GetModuleFileName((HINSTANCE)&__ImageBase, str_dll_path, sizeof(str_dll_path));

    // HWND h = ???
    DWORD dwPid=0;
    ::GetWindowThreadProcessId((HWND)&__ImageBase, &dwPid); 
    HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS ,0,dwPid);
    if ( hProcess == 0 )
        throw std::runtime_error("Cannot open process in order to get executable path!");

    typedef DWORD (WINAPI *tGetModuleFileNameExA)(  HANDLE, HMODULE,LPTSTR,DWORD);
    tGetModuleFileNameExA pGetModuleFileNameExA =0;

    boost::scoped_ptr<HINSTANCE>  hIns(::LoadLibrary("Psapi.dll"), &::FreeLibrary);

    if (hIns) 
        pGetModuleFileNameExA =
            (tGetModuleFileNameExA)::GetProcAddress(hIns.get(),"GetModuleFileNameExA"); 

    if(pGetModuleFileNameExA)
        pGetModuleFileNameExA(hProcess, 0, str_dll_path, sizeof(str_dll_path));

    m_abs_path = str_dll_path;
    m_exe = m_abs_path;
    size_t n = m_abs_path.find_last_of('\\');
    if (n != std::string::npos) {
        m_abs_path.erase(n);
        m_exe.erase(0, n+1);
    } else {
        m_abs_path = "";
    }
    m_rel_path = m_abs_path;
#endif
}

} // namespace path
} // namespace utxx

