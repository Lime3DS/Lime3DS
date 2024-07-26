// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef ANDROID
#include "common/android_storage.h"

namespace AndroidStorage {
JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                g_jvm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                g_jvm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

AndroidOpenMode ParseOpenmode(const std::string_view openmode) {
    AndroidOpenMode android_open_mode = AndroidOpenMode::NEVER;
    const char* mode = openmode.data();
    int o = 0;
    switch (*mode++) {
    case 'r':
        android_open_mode = AndroidStorage::AndroidOpenMode::READ;
        break;
    case 'w':
        android_open_mode = AndroidStorage::AndroidOpenMode::WRITE;
        o = O_TRUNC;
        break;
    case 'a':
        android_open_mode = AndroidStorage::AndroidOpenMode::WRITE;
        o = O_APPEND;
        break;
    }

    // [rwa]\+ or [rwa]b\+ means read and write
    if (*mode == '+' || (*mode == 'b' && mode[1] == '+')) {
        android_open_mode = AndroidStorage::AndroidOpenMode::READ_WRITE;
    }

    return android_open_mode | o;
}

void InitJNI(JNIEnv* env, jclass clazz) {
    env->GetJavaVM(&g_jvm);
    native_library = clazz;

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(JMethodID, JMethodName, Signature)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature)               \
    F(JMethodID, JMethodName, Signature)
#define F(JMethodID, JMethodName, Signature)                                                       \
    JMethodID = env->GetStaticMethodID(native_library, JMethodName, Signature);
    ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
    ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
}

void CleanupJNI() {
#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature) F(JMethodID)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature) F(JMethodID)
#define F(JMethodID) JMethodID = nullptr;
    ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
    ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
}

bool CreateFile(const std::string& directory, const std::string& filename) {
    if (create_file == nullptr)
        return false;
    auto env = GetEnvForThread();
    jstring j_directory = env->NewStringUTF(directory.c_str());
    jstring j_filename = env->NewStringUTF(filename.c_str());
    return env->CallStaticBooleanMethod(native_library, create_file, j_directory, j_filename);
}

bool CreateDir(const std::string& directory, const std::string& filename) {
    if (create_dir == nullptr)
        return false;
    auto env = GetEnvForThread();
    jstring j_directory = env->NewStringUTF(directory.c_str());
    jstring j_directory_name = env->NewStringUTF(filename.c_str());
    return env->CallStaticBooleanMethod(native_library, create_dir, j_directory, j_directory_name);
}

int OpenContentUri(const std::string& filepath, AndroidOpenMode openmode) {
    if (open_content_uri == nullptr)
        return -1;

    const char* mode = "";
    switch (openmode) {
    case AndroidOpenMode::READ:
        mode = "r";
        break;
    case AndroidOpenMode::WRITE:
        mode = "w";
        break;
    case AndroidOpenMode::READ_WRITE:
        mode = "rw";
        break;
    case AndroidOpenMode::WRITE_TRUNCATE:
        mode = "wt";
        break;
    case AndroidOpenMode::WRITE_APPEND:
        mode = "wa";
        break;
    case AndroidOpenMode::READ_WRITE_APPEND:
        mode = "rwa";
        break;
    case AndroidOpenMode::READ_WRITE_TRUNCATE:
        mode = "rwt";
        break;
    case AndroidOpenMode::NEVER:
        return -1;
    }
    auto env = GetEnvForThread();
    jstring j_filepath = env->NewStringUTF(filepath.c_str());
    jstring j_mode = env->NewStringUTF(mode);
    return env->CallStaticIntMethod(native_library, open_content_uri, j_filepath, j_mode);
}

std::vector<std::string> GetFilesName(const std::string& filepath) {
    auto vector = std::vector<std::string>();
    if (get_files_name == nullptr)
        return vector;
    auto env = GetEnvForThread();
    jstring j_filepath = env->NewStringUTF(filepath.c_str());
    auto j_object =
        (jobjectArray)env->CallStaticObjectMethod(native_library, get_files_name, j_filepath);
    const jsize j_size = env->GetArrayLength(j_object);
    vector.reserve(j_size);
    for (int i = 0; i < j_size; i++) {
        auto string = (jstring)(env->GetObjectArrayElement(j_object, i));
        vector.emplace_back(env->GetStringUTFChars(string, nullptr));
    }
    return vector;
}

bool CopyFile(const std::string& source, const std::string& destination_path,
              const std::string& destination_filename) {
    if (copy_file == nullptr)
        return false;
    auto env = GetEnvForThread();
    jstring j_source_path = env->NewStringUTF(source.c_str());
    jstring j_destination_path = env->NewStringUTF(destination_path.c_str());
    jstring j_destination_filename = env->NewStringUTF(destination_filename.c_str());
    return env->CallStaticBooleanMethod(native_library, copy_file, j_source_path,
                                        j_destination_path, j_destination_filename);
}

bool RenameFile(const std::string& source, const std::string& filename) {
    if (rename_file == nullptr)
        return false;
    auto env = GetEnvForThread();
    jstring j_source_path = env->NewStringUTF(source.c_str());
    jstring j_destination_path = env->NewStringUTF(filename.c_str());
    return env->CallStaticBooleanMethod(native_library, rename_file, j_source_path,
                                        j_destination_path);
}

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(FunctionName, ReturnValue, JMethodID, Caller)
#define F(FunctionName, ReturnValue, JMethodID, Caller)                                            \
    ReturnValue FunctionName(const std::string& filepath) {                                        \
        if (JMethodID == nullptr) {                                                                \
            return 0;                                                                              \
        }                                                                                          \
        auto env = GetEnvForThread();                                                              \
        jstring j_filepath = env->NewStringUTF(filepath.c_str());                                  \
        return env->Caller(native_library, JMethodID, j_filepath);                                 \
    }
ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
#undef F
#undef FR

} // namespace AndroidStorage
#endif
