// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright Dolphin Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <ios>
#include <limits>
#include <memory>
#ifdef HAVE_LIBRETRO
#include <mutex>
#endif
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/wrapper.hpp>
#include "common/common_types.h"
#ifdef _MSC_VER
#include "common/string_util.h"
#endif
#if defined(ANDROID) && !defined(HAVE_LIBRETRO_VFS)
#include "android_utils.h"
#endif

#ifdef HAVE_LIBRETRO_VFS
#define SKIP_STDIO_REDEFINES
#include <streams/file_stream_transforms.h>
#define CORE_FILE RFILE
#else
#define CORE_FILE std::FILE
#endif

namespace FileUtil {

// User paths for GetUserPath
enum class UserPath {
    CacheDir,
    CheatsDir,
    ConfigDir,
    DLLDir,
    DumpDir,
    IconsDir,
    LegacyCitraCacheDir,  // LegacyXXXCacheDir and LegacyXXXConfigDir are only defined if migrating
    LegacyCitraConfigDir, // these directories is necessary (aka not a child of LegacyXXXUserDir)
    LegacyCitraUserDir,
    LegacyLime3DSCacheDir,
    LegacyLime3DSConfigDir,
    LegacyLime3DSUserDir,
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
[[nodiscard]] u64 GetSize(CORE_FILE* f);

// Returns true if successful, or path already exists.
bool CreateDir(const std::string& filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string& fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string& filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string& filename);

// Renames file srcFullPath to destFullPath, returns true on success
bool Rename(const std::string& srcFullPath, const std::string& destFullPath);

// Copies file srcFilename to destFilename, returns true on success
bool Copy(const std::string& srcFilename, const std::string& destFilename);

// Creates an empty file filename, returns true on success
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
 * @param stop_flag Optional stop flag, the scan will stop if it becomes true
 * @return the total number of files/directories found
 */
u64 ScanDirectoryTree(const std::string& directory, FSTEntry& parent_entry,
                      unsigned int recursion = 0, std::atomic<bool>* stop_flag = nullptr);

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
/**
 * Recursively copies a directory. Existing files in the destination are left alone.
 * @param should_continue Consulted before each entry; returning false aborts the copy. Lets a
 *                        caller on a deadline bound a copy whose size it cannot know up front.
 * @returns True if every entry was copied, false if the source is unreadable, any copy failed,
 *          or the copy was aborted.
 */
bool CopyDir(const std::string& source_path, const std::string& dest_path,
             const std::function<bool()>& should_continue = {});

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
#else
[[nodiscard]] const std::string GetHomeDirectory();
[[nodiscard]] const std::string GetUserDirectory(const std::string& envvar);
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

class IOFileBase;

class IOType {
public:
    enum class Type {
        IOFile,
        NullIOFile,
        SubIOFile,
        CryptoFile,
        Z3DSWriteIOFile,
        Z3DSReadIOFile,

        MAX,
    };

    Type GetBaseType() const {
        return types.front();
    }

    Type GetLastType() const {
        return types.back();
    }

    bool HasType(Type t) const {
        return std::find_if(types.begin(), types.end(), [t](Type curr) { return t == curr; }) !=
               types.end();
    }

    bool HasCompressedType() {
        return HasType(Type::Z3DSReadIOFile) || HasType(Type::Z3DSWriteIOFile);
    }

    std::string to_string() const {
        constexpr std::array<const char*, static_cast<u32>(Type::MAX)> names = {{
            "IOFile",
            "NullIOFile",
            "SubIOFile",
            "CryptoFile",
            "Z3DSWriteIOFile",
            "Z3DSReadIOFile",
        }};

        std::string ret;
        bool first = true;
        for (auto it = types.rbegin(); it != types.rend(); it++) {
            if (!first) {
                ret += " -> ";
            }
            first = false;
            ret += names[static_cast<u32>(*it)];
        }
        return ret;
    }

private:
    friend class IOFileBase;
    std::vector<Type> types;
};

/**
 * Base IOFile class that can be derived to implement files with abstracted properties
 * to the programmer, such as compression or encryption. IOFileBase is modeled so that it
 * holds a unique pointer to a child file, that way it's possible to have a chain of
 * files with different properties (for example, a compressed file inside an encrypted file).
 * The final chained file must always be a derived class that overrides all methods to
 * prevent recursion or null dereferences.
 */
class IOFileBase : public NonCopyable {
public:
    virtual ~IOFileBase() = 0;

    [[nodiscard]] explicit operator bool() const {
        return IsGood();
    }

    virtual bool Open() {
        return Forward(&IOFileBase::Open);
    }

    virtual std::unique_ptr<IOFileBase> OpenCopy() const = 0;

    virtual bool Close() {
        return Forward(&IOFileBase::Close);
    }

    virtual bool IsOpen() const {
        return Forward(&IOFileBase::IsOpen);
    }
    virtual bool IsGood() const {
        return Forward(&IOFileBase::IsGood);
    }

    virtual int GetFd() const {
        return Forward(&IOFileBase::GetFd);
    }

    virtual u64 GetSize() const {
        return Forward(&IOFileBase::GetSize);
    }
    virtual bool Resize(u64 size) {
        return Forward(&IOFileBase::Resize, size);
    }

    virtual bool Flush() {
        return Forward(&IOFileBase::Flush);
    }

    virtual void Clear() {
        Forward(&IOFileBase::Clear);
    }

    virtual const std::string& Filename() const {
        return Forward(&IOFileBase::Filename);
    }

    virtual bool Seek(s64 off, int origin) {
        return Forward(&IOFileBase::Seek, off, origin);
    }
    virtual u64 Tell() const {
        return Forward(&IOFileBase::Tell);
    }

    /// Returns the amount of T items read
    template <typename T>
    std::size_t ReadArray(T* data, std::size_t length) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Given array does not consist of trivially copyable objects");

        std::size_t items_read = ReadImpl(data, length, sizeof(T));

        return items_read;
    }

    /// Returns the amount of bytes read
    template <typename T>
    std::size_t ReadAtArray(T* data, std::size_t length, std::size_t offset) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Given array does not consist of trivially copyable objects");

        const size_t bytes = length * sizeof(T);
        std::size_t size_read = ReadAtImpl(data, bytes, offset);

        return size_read;
    }

    template <typename T>
    std::size_t WriteArray(const T* data, std::size_t length) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Given array does not consist of trivially copyable objects");

        std::size_t items_written = WriteImpl(data, length, sizeof(T));

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

    /**
     * Reads a span of T data from a file sequentially.
     * This function reads from the current position of the file pointer and
     * advances it by the (count of T * sizeof(T)) bytes successfully read.
     *
     * Failures occur when:
     * - The file is not open
     * - The opened file lacks read permissions
     * - Attempting to read beyond the end-of-file
     *
     * @tparam T Data type
     *
     * @param data Span of T data
     *
     * @returns Count of T data successfully read.
     */
    template <typename T>
    [[nodiscard]] size_t ReadSpan(std::span<T> data) {
        static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");

        return ReadImpl(data.data(), data.size(), sizeof(T));
    }

    /**
     * Writes a span of T data to a file sequentially.
     * This function writes from the current position of the file pointer and
     * advances it by the (count of T * sizeof(T)) bytes successfully written.
     *
     * Failures occur when:
     * - The file is not open
     * - The opened file lacks write permissions
     *
     * @tparam T Data type
     *
     * @param data Span of T data
     *
     * @returns Count of T data successfully written.
     */
    template <typename T>
    [[nodiscard]] size_t WriteSpan(std::span<const T> data) {
        static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");

        return WriteImpl(data.data(), data.size(), sizeof(T));
    }

    /**
     * Reads the file line by line, returning true if data
     * was read and false when reaching the end of file.
     *
     * @param line The output string to write the read data to
     *
     * @returns Whether the line was read or not
     */
    bool ReadLine(std::string& line);

    /**
     * Writes the specified line to the file
     * automatically appending a newline
     * character to it.
     *
     * @param line The input string to write
     *
     * @returns Count of bytes written, including the newline.
     */
    size_t WriteLine(const std::string_view line);

    // Returns the type of the file.
    IOType GetType() const {
        IOType ret;
        const IOFileBase* curr = this;
        while (curr) {
            ret.types.push_back(curr->MyType());
            curr = curr->Child().get();
        }
        return ret;
    }

protected:
    std::unique_ptr<IOFileBase>& Child() {
        return child_file;
    }

    const std::unique_ptr<IOFileBase>& Child() const {
        return child_file;
    }

    virtual std::size_t ReadImpl(void* data, std::size_t length, std::size_t elem_size) {
        return Forward(&IOFileBase::ReadImpl, data, length, elem_size);
    }
    virtual std::size_t ReadAtImpl(void* data, std::size_t byte_count, std::size_t offset) {
        return Forward(&IOFileBase::ReadAtImpl, data, byte_count, offset);
    }
    virtual std::size_t WriteImpl(const void* data, std::size_t length, std::size_t elem_size) {
        return Forward(&IOFileBase::WriteImpl, data, length, elem_size);
    }

    virtual IOType::Type MyType() const = 0;

private:
    template <typename Ret, typename... Args>
    Ret Forward(Ret (IOFileBase::*method)(Args...), Args... args) {
        return (child_file.get()->*method)(args...);
    }

    template <typename Ret, typename... Args>
    Ret Forward(Ret (IOFileBase::*method)(Args...) const, Args... args) const {
        return (child_file.get()->*method)(args...);
    }

    std::unique_ptr<IOFileBase> child_file{};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & child_file;
    }
    friend class boost::serialization::access;
};
inline IOFileBase::~IOFileBase() {}

// File class that acts as a wrapper to cstdlib
// file function. This function must override all base
// methods.
class IOFile : public IOFileBase {
public:
    IOFile();

    // flags is used for windows specific file open mode flags, which
    // allows citra to open the logs in shared write mode, so that the file
    // isn't considered "locked" while citra is open and people can open the log file and view it
    IOFile(const std::string& filename, const char openmode[], int flags = 0);

    ~IOFile() override;

    IOFile(IOFile&& other) noexcept;
    IOFile& operator=(IOFile&& other) noexcept;

    bool Close() override;

    [[nodiscard]] bool IsOpen() const override;

    // m_good is set to false when a read, write or other function fails
    [[nodiscard]] bool IsGood() const override;
    [[nodiscard]] int GetFd() const override;

    bool Seek(s64 off, int origin) override;
    u64 Tell() const override;
    u64 GetSize() const override;
    bool Resize(u64 size) override;
    bool Flush() override;

    // clear error state
    void Clear() override;

    const std::string& Filename() const override;

    std::unique_ptr<IOFileBase> OpenCopy() const override;

protected:
    void Swap(IOFile& other) noexcept;

    bool Open() override;

    std::size_t ReadImpl(void* data, std::size_t length, std::size_t elem_size) override;
    std::size_t ReadAtImpl(void* data, std::size_t byte_count, std::size_t offset) override;
    std::size_t WriteImpl(const void* data, std::size_t length, std::size_t elem_size) override;

    IOType::Type MyType() const override {
        return IOType::Type::IOFile;
    }

private:
    CORE_FILE* m_file = nullptr;
    int m_fd = -1;
    bool m_good = true;
#ifdef HAVE_LIBRETRO_VFS
    // pread() doesn't touch the file position, so it's safe alongside
    // concurrent fread/fwrite. Libretro VFS has no pread equivalent, so
    // ReadAtImpl emulates it with seek+read+seek, which would corrupt the
    // file position for concurrent Read/Write operations.
    mutable std::mutex m_file_pos_mutex;
#endif

    std::string filename;
    std::string openmode;
    u32 flags = 0;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<IOFileBase>(*this);
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

constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return a | b << 8 | c << 16 | d << 24;
}

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

BOOST_CLASS_EXPORT_KEY(FileUtil::IOFile)
