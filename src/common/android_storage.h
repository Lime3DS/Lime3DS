// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#ifdef ANDROID
#include <string>
#include <vector>
#include <fcntl.h>
#include <jni.h>

#define ANDROID_STORAGE_FUNCTIONS(V)                                                               \
    V(CreateFile, bool, (const std::string& directory, const std::string& filename), create_file,  \
      "createFile", "(Ljava/lang/String;Ljava/lang/String;)Z")                                     \
    V(CreateDir, bool, (const std::string& directory, const std::string& filename), create_dir,    \
      "createDir", "(Ljava/lang/String;Ljava/lang/String;)Z")                                      \
    V(OpenContentUri, int, (const std::string& filepath, AndroidOpenMode openmode),                \
      open_content_uri, "openContentUri", "(Ljava/lang/String;Ljava/lang/String;)I")               \
    V(GetFilesName, std::vector<std::string>, (const std::string& filepath), get_files_name,       \
      "getFilesName", "(Ljava/lang/String;)[Ljava/lang/String;")                                   \
    V(CopyFile, bool,                                                                              \
      (const std::string& source, const std::string& destination_path,                             \
       const std::string& destination_filename),                                                   \
      copy_file, "copyFile", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z")          \
    V(RenameFile, bool, (const std::string& source, const std::string& filename), rename_file,     \
      "renameFile", "(Ljava/lang/String;Ljava/lang/String;)Z")
#define ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(V)                                                 \
    V(IsDirectory, bool, is_directory, CallStaticBooleanMethod, "isDirectory",                     \
      "(Ljava/lang/String;)Z")                                                                     \
    V(FileExists, bool, file_exists, CallStaticBooleanMethod, "fileExists",                        \
      "(Ljava/lang/String;)Z")                                                                     \
    V(GetSize, std::uint64_t, get_size, CallStaticLongMethod, "getSize", "(Ljava/lang/String;)J")  \
    V(DeleteDocument, bool, delete_document, CallStaticBooleanMethod, "deleteDocument",            \
      "(Ljava/lang/String;)Z")
namespace AndroidStorage {
static JavaVM* g_jvm = nullptr;
static jclass native_library = nullptr;
#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature) F(JMethodID)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature) F(JMethodID)
#define F(JMethodID) static jmethodID JMethodID = nullptr;
ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
// Reference:
// https://developer.android.com/reference/android/os/ParcelFileDescriptor#parseMode(java.lang.String)
enum class AndroidOpenMode {
    READ = O_RDONLY,                        // "r"
    WRITE = O_WRONLY,                       // "w"
    READ_WRITE = O_RDWR,                    // "rw"
    WRITE_APPEND = O_WRONLY | O_APPEND,     // "wa"
    WRITE_TRUNCATE = O_WRONLY | O_TRUNC,    // "wt
    READ_WRITE_APPEND = O_RDWR | O_APPEND,  // "rwa"
    READ_WRITE_TRUNCATE = O_RDWR | O_TRUNC, // "rwt"
    NEVER = EINVAL,
};

inline AndroidOpenMode operator|(AndroidOpenMode a, int b) {
    return static_cast<AndroidOpenMode>(static_cast<int>(a) | b);
}

AndroidOpenMode ParseOpenmode(const std::string_view openmode);

void InitJNI(JNIEnv* env, jclass clazz);

void CleanupJNI();

#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature)               \
    F(FunctionName, Parameters, ReturnValue)
#define F(FunctionName, Parameters, ReturnValue) ReturnValue FunctionName Parameters;
ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(FunctionName, ReturnValue)
#define F(FunctionName, ReturnValue) ReturnValue FunctionName(const std::string& filepath);
ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
#undef F
#undef FR
} // namespace AndroidStorage
#endif
