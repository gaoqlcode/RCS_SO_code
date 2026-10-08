#ifndef RCS_UTIL_HPP
#define RCS_UTIL_HPP

// 小工具：数值夹取、字符串、读文件、目录（C++11，Linux/Windows）
// Windows：路径按 UTF-8 处理（与 Qt toStdString 一致），内部转宽字符 API

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>
#include <errno.h>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <direct.h>
#  include <io.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#else
#  include <dirent.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

template <typename T>
inline T imax(T a, T b) { return a > b ? a : b; }

template <typename T>
inline T imin(T a, T b) { return a < b ? a : b; }

template <typename T>
inline T ibound(T lo, T v, T hi) { return imax(lo, imin(v, hi)); }

template <typename T>
inline T clampv(T v, T lo, T hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline std::string trimStr(const std::string &s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::string toUpperStr(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    return s;
}

inline std::string pathJoin(const std::string &a, const std::string &b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    const char last = a[a.size() - 1];
    if (last == '/' || last == '\\')
        return a + b;
#if defined(_WIN32)
    return a + "\\" + b;
#else
    return a + "/" + b;
#endif
}

#if defined(_WIN32)
inline std::wstring utf8ToWide(const std::string &utf8)
{
    if (utf8.empty())
        return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                      static_cast<int>(utf8.size()), 0, 0);
    if (n <= 0)
        return std::wstring();
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &w[0], n);
    return w;
}

inline std::string wideToUtf8(const wchar_t *w)
{
    if (!w || !w[0])
        return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
    if (n <= 1)
        return std::string();
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, 0, 0);
    return s;
}

/** 统一成反斜杠，避免 Qt 传入 E:/... 与 Win32 混用出问题 */
inline std::string winNormPath(std::string p)
{
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] == '/')
            p[i] = '\\';
    }
    while (p.size() > 3 && (p[p.size() - 1] == '\\' || p[p.size() - 1] == '/'))
        p.resize(p.size() - 1);
    return p;
}

inline DWORD winAttrs(const std::string &p)
{
    const std::wstring w = utf8ToWide(winNormPath(p));
    if (w.empty())
        return INVALID_FILE_ATTRIBUTES;
    return GetFileAttributesW(w.c_str());
}

inline FILE *winOpenFile(const std::string &path, bool write)
{
    const std::wstring w = utf8ToWide(winNormPath(path));
    if (w.empty())
        return 0;
    HANDLE h = CreateFileW(
        w.c_str(),
        write ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
        write ? 0 : FILE_SHARE_READ,
        0,
        write ? CREATE_ALWAYS : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), write ? (_O_RDWR | _O_BINARY) : (_O_RDONLY | _O_BINARY));
    if (fd < 0) {
        CloseHandle(h);
        return 0;
    }
    FILE *fp = _fdopen(fd, write ? "w+b" : "rb");
    if (!fp) {
        _close(fd);
        return 0;
    }
    return fp;
}
#endif

inline bool pathExists(const std::string &p)
{
#if defined(_WIN32)
    return winAttrs(p) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
#endif
}

inline bool isDir(const std::string &p)
{
#if defined(_WIN32)
    const DWORD a = winAttrs(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (::stat(p.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
#endif
}

inline bool makeDirs(const std::string &path)
{
    if (path.empty()) return false;
#if defined(_WIN32)
    const std::string norm = winNormPath(path);
    if (isDir(norm)) return true;
    std::string cur;
    for (size_t i = 0; i < norm.size(); ++i) {
        cur.push_back(norm[i]);
        const bool sep = (norm[i] == '\\');
        if (sep || i + 1 == norm.size()) {
            if (cur.empty() || cur == "." || cur == "\\") continue;
            if (cur.size() == 2 && cur[1] == ':') continue;
            std::string d = cur;
            while (!d.empty() && d[d.size() - 1] == '\\')
                d.resize(d.size() - 1);
            if (d.empty() || isDir(d)) continue;
            const std::wstring wd = utf8ToWide(d);
            if (!CreateDirectoryW(wd.c_str(), 0)) {
                const DWORD e = GetLastError();
                if (e != ERROR_ALREADY_EXISTS && !isDir(d))
                    return false;
            }
        }
    }
    return isDir(norm);
#else
    if (isDir(path)) return true;
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        const bool sep = (path[i] == '/' || path[i] == '\\');
        if (sep || i + 1 == path.size()) {
            if (cur.empty() || cur == "/" || cur == "." || cur == "\\") continue;
            if (cur.size() == 2 && cur[1] == ':') continue;
            std::string d = cur;
            while (!d.empty() && (d[d.size() - 1] == '/' || d[d.size() - 1] == '\\'))
                d.resize(d.size() - 1);
            if (d.empty() || isDir(d)) continue;
            if (::mkdir(d.c_str(), 0755) != 0 && errno != EEXIST) {
                if (!isDir(d)) return false;
            }
        }
    }
    return isDir(path);
#endif
}

inline bool removeFile(const std::string &p)
{
#if defined(_WIN32)
    const std::wstring w = utf8ToWide(winNormPath(p));
    return DeleteFileW(w.c_str()) != 0;
#else
    return ::remove(p.c_str()) == 0;
#endif
}

inline int64_t fileSizeBytes(const std::string &p)
{
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA fad;
    const std::wstring w = utf8ToWide(winNormPath(p));
    if (!GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &fad))
        return -1;
    LARGE_INTEGER li;
    li.HighPart = fad.nFileSizeHigh;
    li.LowPart = fad.nFileSizeLow;
    return static_cast<int64_t>(li.QuadPart);
#else
    struct stat st;
    if (::stat(p.c_str(), &st) != 0)
        return -1;
    return static_cast<int64_t>(st.st_size);
#endif
}

inline bool readFileAll(const std::string &path, std::vector<uint8_t> &out)
{
#if defined(_WIN32)
    FILE *fp = winOpenFile(path, false);
    if (!fp) return false;
    if (_fseeki64(fp, 0, SEEK_END) != 0) { fclose(fp); return false; }
    const __int64 n = _ftelli64(fp);
    if (n < 0) { fclose(fp); return false; }
    if (_fseeki64(fp, 0, SEEK_SET) != 0) { fclose(fp); return false; }
    out.resize(static_cast<size_t>(n));
    if (n > 0) {
        const size_t got = fread(out.empty() ? 0 : &out[0], 1, static_cast<size_t>(n), fp);
        fclose(fp);
        return got == static_cast<size_t>(n);
    }
    fclose(fp);
    return true;
#else
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    if (n > 0)
        f.read(reinterpret_cast<char *>(&out[0]), n);
    return f.good() || n == 0;
#endif
}

inline bool writeFileAll(const std::string &path, const void *data, size_t n)
{
#if defined(_WIN32)
    FILE *fp = winOpenFile(path, true);
    if (!fp) return false;
    if (n && data) {
        const size_t got = fwrite(data, 1, n, fp);
        fclose(fp);
        return got == n;
    }
    fclose(fp);
    return true;
#else
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (n)
        f.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(n));
    return f.good();
#endif
}

inline bool writeFileAll(const std::string &path, const std::vector<uint8_t> &data)
{
    return writeFileAll(path, data.empty() ? 0 : &data[0], data.size());
}

inline std::vector<std::string> listFileNames(const std::string &folder)
{
    std::vector<std::string> names;
#if defined(_WIN32)
    const std::wstring pattern = utf8ToWide(pathJoin(winNormPath(folder), "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return names;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (fd.cFileName[0] == L'.') continue;
        names.push_back(wideToUtf8(fd.cFileName));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    DIR *d = ::opendir(folder.c_str());
    if (!d) return names;
    while (dirent *ent = ::readdir(d)) {
        const char *n = ent->d_name;
        if (!n || n[0] == '.') continue;
        const std::string full = pathJoin(folder, n);
        if (isDir(full)) continue;
        names.push_back(n);
    }
    ::closedir(d);
#endif
    return names;
}

inline std::string fileNameOf(const std::string &path)
{
    const size_t p1 = path.find_last_of('/');
    const size_t p2 = path.find_last_of('\\');
    size_t p = std::string::npos;
    if (p1 == std::string::npos) p = p2;
    else if (p2 == std::string::npos) p = p1;
    else p = imax(p1, p2);
    if (p == std::string::npos) return path;
    return path.substr(p + 1);
}

inline std::string fileStem(const std::string &path)
{
    std::string name = fileNameOf(path);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return name;
    return name.substr(0, dot);
}

/** UTF-8 路径打开文件（Windows 用 CreateFileW，避免中文路径失败） */
inline FILE *fopenUtf8(const std::string &path, const char *mode)
{
#if defined(_WIN32)
    const bool write = mode && (std::strchr(mode, 'w') || std::strchr(mode, 'a') || std::strchr(mode, '+'));
    return winOpenFile(path, write);
#else
    return std::fopen(path.c_str(), mode);
#endif
}

inline bool copyFileBytes(const std::string &src, const std::string &dst)
{
    std::vector<uint8_t> buf;
    if (!readFileAll(src, buf))
        return false;
    return writeFileAll(dst, buf);
}

#endif
