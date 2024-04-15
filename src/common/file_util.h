// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdio>
#include <functional>
#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/wrapper.hpp>
#include "common/common_types.h"
#ifdef _MSC_VER
#include "common/string_util.h"
#endif

namespace FileUtil {

// User paths for GetUserPath
enum class UserPath {
    CacheDir,
    CheatsDir,
    ConfigDir,
    DLLDir,
    DumpDir,
    LoadDir,
    LogDir,
    NANDDir,
    RootDir,
    SDMCDir,
    ShaderDir,
    StatesDir,
    SysDataDir,
    UserDir,
};

// Replaces install-specific paths with standard placeholders, and back again
std::string SerializePath(const std::string& input, bool is_saving);

// A serializable path string
struct Path : public boost::serialization::wrapper_traits<const Path> {
    std::string& str;

    explicit Path(std::string& _str) : str(_str) {}

    static const Path make(std::string& str) {
        return Path(str);
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int) const {
        auto s_path = SerializePath(str, true);
        ar << s_path;
    }
    template <class Archive>
    void load(Archive& ar, const unsigned int) const {
        ar >> str;
        str = SerializePath(str, false);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();
    friend class boost::serialization::access;
};

// FileSystem tree node/
struct FSTEntry {
    bool isDirectory;
    u64 size;                 // file length or number of entries from children
    std::string physicalName; // name on disk
    std::string virtualName;  // name in FST names table
    std::vector<FSTEntry> children;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & isDirectory;
        ar & size;
        ar& Path::make(physicalName);
        ar& Path::make(virtualName);
        ar & children;
    }
    friend class boost::serialization::access;
};

// Returns true if file filename exists
[[nodiscard]] bool Exists(const std::string& filename);

// Returns true if filename is a directory
[[nodiscard]] bool IsDirectory(const std::string& filename);

// Returns the size of filename (64bit)
[[nodiscard]] u64 GetSize(const std::string& filename);

// Overloaded GetSize, accepts file descriptor
[[nodiscard]] u64 GetSize(int fd);

// Overloaded GetSize, accepts FILE*
[[nodiscard]] u64 GetSize(FILE* f);

// Returns true if successful, or path already exists.
bool CreateDir(const std::string& filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string& fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string& filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string& filename);

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string& srcFilename, const std::string& destFilename);

// copies file srcFilename to destFilename, returns true on success
bool Copy(const std::string& srcFilename, const std::string& destFilename);

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string& filename);

/**
 * @param num_entries_out to be assigned by the callable with the number of iterated directory
 * entries, never null
 * @param directory the path to the enclosing directory
 * @param virtual_name the entry name, without any preceding directory info
 * @return whether handling the entry succeeded
 */
using DirectoryEntryCallable = std::function<bool(
    u64* num_entries_out, const std::string& directory, const std::string& virtual_name)>;

/**
 * Scans a directory, calling the callback for each file/directory contained within.
 * If the callback returns failure, scanning halts and this function returns failure as well
 * @param num_entries_out assigned by the function with the number of iterated directory entries,
 * can be null
 * @param directory the directory to scan
 * @param callback The callback which will be called for each entry
 * @return whether scanning the directory succeeded
 */
bool ForeachDirectoryEntry(u64* num_entries_out, const std::string& directory,
                           DirectoryEntryCallable callback);

/**
 * Scans the directory tree, storing the results.
 * @param directory the parent directory to start scanning from
 * @param parent_entry FSTEntry where the filesystem tree results will be stored.
 * @param recursion Number of children directories to read before giving up.
 * @return the total number of files/directories found
 */
u64 ScanDirectoryTree(const std::string& directory, FSTEntry& parent_entry,
                      unsigned int recursion = 0);

/**
 * Recursively searches through a FSTEntry for files, and stores them.
 * @param directory The FSTEntry to start scanning from
 * @param parent_entry FSTEntry vector where the results will be stored.
 */
void GetAllFilesFromNestedEntries(FSTEntry& directory, std::vector<FSTEntry>& output);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string& directory, unsigned int recursion = 256);

// Returns the current directory
[[nodiscard]] std::optional<std::string> GetCurrentDir();

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string& source_path, const std::string& dest_path);

// Set the current directory to given directory
bool SetCurrentDir(const std::string& directory);

void SetUserPath(const std::string& path = "");

void SetCurrentRomPath(const std::string& path);

// Returns a pointer to a string with a Citra data dir in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
[[nodiscard]] const std::string& GetUserPath(UserPath path);

// Returns a pointer to a string with the default Citra data dir in the user's home
// directory.
[[nodiscard]] const std::string& GetDefaultUserPath(UserPath path);

// Update the Global Path with the new value
void UpdateUserPath(UserPath path, const std::string& filename);

#ifdef __APPLE__
[[nodiscard]] std::optional<std::string> GetBundleDirectory();
#endif

#ifdef _WIN32
[[nodiscard]] const std::string& GetExeDirectory();
[[nodiscard]] std::string AppDataRoamingDirectory();
#endif

std::size_t WriteStringToFile(bool text_file, const std::string& filename, std::string_view str);

std::size_t ReadFileToString(bool text_file, const std::string& filename, std::string& str);

/**
 * Splits the filename into 8.3 format
 * Loosely implemented following https://en.wikipedia.org/wiki/8.3_filename
 * @param filename The normal filename to use
 * @param short_name A 9-char array in which the short name will be written
 * @param extension A 4-char array in which the extension will be written
 */
void SplitFilename83(const std::string& filename, std::array<char, 9>& short_name,
                     std::array<char, 4>& extension);

// Splits the path on '/' or '\' and put the components into a vector
// i.e. "C:\Users\Yuzu\Documents\save.bin" becomes {"C:", "Users", "Yuzu", "Documents", "save.bin" }
[[nodiscard]] std::vector<std::string> SplitPathComponents(std::string_view filename);

// Gets all of the text up to the last '/' or '\' in the path.
[[nodiscard]] std::string_view GetParentPath(std::string_view path);

// Gets all of the text after the first '/' or '\' in the path.
[[nodiscard]] std::string_view GetPathWithoutTop(std::string_view path);

// Gets the filename of the path
[[nodiscard]] std::string_view GetFilename(std::string_view path);

// Gets the extension of the filename
[[nodiscard]] std::string_view GetExtensionFromFilename(std::string_view name);

// Removes the final '/' or '\' if one exists
[[nodiscard]] std::string_view RemoveTrailingSlash(std::string_view path);

// Creates a new vector containing indices [first, last) from the original.
template <typename T>
[[nodiscard]] std::vector<T> SliceVector(const std::vector<T>& vector, std::size_t first,
                                         std::size_t last) {
    if (first >= last) {
        return {};
    }
    last = std::min<std::size_t>(last, vector.size());
    return std::vector<T>(vector.begin() + first, vector.begin() + first + last);
}

enum class DirectorySeparator {
    ForwardSlash,
    BackwardSlash,
    PlatformDefault,
};

// Removes trailing slash, makes all '\\' into '/', and removes duplicate '/'. Makes '/' into '\\'
// depending if directory_separator is BackwardSlash or PlatformDefault and running on windows
[[nodiscard]] std::string SanitizePath(
    std::string_view path,
    DirectorySeparator directory_separator = DirectorySeparator::ForwardSlash);

// simple wrapper for cstdlib file functions to
// hopefully will make error checking easier
// and make forgetting an fclose() harder
class IOFile : public NonCopyable {
public:
    IOFile();

    // flags is used for windows specific file open mode flags, which
    // allows citra to open the logs in shared write mode, so that the file
    // isn't considered "locked" while citra is open and people can open the log file and view it
    IOFile(const std::string& filename, const char openmode[], int flags = 0);

    ~IOFile();

    IOFile(IOFile&& other) noexcept;
    IOFile& operator=(IOFile&& other) noexcept;

    void Swap(IOFile& other) noexcept;

    bool Close();

    template <typename T>
    std::size_t ReadArray(T* data, std::size_t length) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Given array does not consist of trivially copyable objects");

        std::size_t items_read = ReadImpl(data, length, sizeof(T));
        if (items_read != length)
            m_good = false;

        return items_read;
    }

    template <typename T>
    std::size_t ReadAtArray(T* data, std::size_t length, std::size_t offset) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Given array does not consist of trivially copyable objects");

        std::size_t items_read = ReadAtImpl(data, length, sizeof(T), offset);
        if (items_read != length)
            m_good = false;

        return items_read;
    }

    template <typename T>
    std::size_t WriteArray(const T* data, std::size_t length) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Given array does not consist of trivially copyable objects");

        std::size_t items_written = WriteImpl(data, length, sizeof(T));
        if (items_written != length)
            m_good = false;

        return items_written;
    }

    template <typename T>
    std::size_t ReadBytes(T* data, std::size_t length) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return ReadArray(reinterpret_cast<char*>(data), length);
    }

    template <typename T>
    std::size_t ReadAtBytes(T* data, std::size_t length, std::size_t offset) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return ReadAtArray(reinterpret_cast<char*>(data), length, offset);
    }

    template <typename T>
    std::size_t WriteBytes(const T* data, std::size_t length) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return WriteArray(reinterpret_cast<const char*>(data), length);
    }

    template <typename T>
    std::size_t WriteObject(const T& object) {
        static_assert(!std::is_pointer_v<T>, "WriteObject arguments must not be a pointer");
        return WriteArray(&object, 1);
    }

    std::size_t WriteString(std::string_view str) {
        return WriteArray(str.data(), str.length());
    }

    [[nodiscard]] bool IsOpen() const {
        return nullptr != m_file;
    }

    // m_good is set to false when a read, write or other function fails
    [[nodiscard]] bool IsGood() const {
        return m_good;
    }
    [[nodiscard]] int GetFd() const {
#ifdef ANDROID
        return m_fd;
#else
        if (m_file == nullptr)
            return -1;
        return fileno(m_file);
#endif
    }
    [[nodiscard]] explicit operator bool() const {
        return IsGood();
    }

    bool Seek(s64 off, int origin);
    [[nodiscard]] u64 Tell() const;
    [[nodiscard]] u64 GetSize() const;
    bool Resize(u64 size);
    bool Flush();

    // clear error state
    void Clear() {
        m_good = true;
        std::clearerr(m_file);
    }

private:
    std::size_t ReadImpl(void* data, std::size_t length, std::size_t data_size);
    std::size_t ReadAtImpl(void* data, std::size_t length, std::size_t data_size,
                           std::size_t offset);
    std::size_t WriteImpl(const void* data, std::size_t length, std::size_t data_size);

    bool Open();

    std::FILE* m_file = nullptr;
    int m_fd = -1;
    bool m_good = true;

    std::string filename;
    std::string openmode;
    u32 flags;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& Path::make(filename);
        ar & openmode;
        ar & flags;
        u64 pos;
        if (Archive::is_saving::value) {
            pos = Tell();
        }
        ar & pos;
        if (Archive::is_loading::value) {
            Open();
            Seek(pos, SEEK_SET);
        }
    }
    friend class boost::serialization::access;
};

template <std::ios_base::openmode o, typename T>
void OpenFStream(T& fstream, const std::string& filename);
} // namespace FileUtil

// To deal with Windows being dumb at unicode:
template <typename T>
void OpenFStream(T& fstream, const std::string& filename, std::ios_base::openmode openmode) {
#ifdef _MSC_VER
    fstream.open(Common::UTF8ToUTF16W(filename), openmode);
#else
    fstream.open(filename, openmode);
#endif
}
