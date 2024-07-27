// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Common {

class DynamicLibrary {
public:
    explicit DynamicLibrary();
    explicit DynamicLibrary(void* handle);
    explicit DynamicLibrary(std::string_view name, int major = -1, int minor = -1);
    ~DynamicLibrary();

    /// Returns true if the library is loaded, otherwise false.
    [[nodiscard]] bool IsLoaded() const noexcept {
        return handle != nullptr;
    }

    /// Loads (or replaces) the handle with the specified library file name.
    /// Returns true if the library was loaded and can be used.
    [[nodiscard]] bool Load(std::string_view filename);

    /// Returns a string containing the last generated load error, if it occured.
    [[nodiscard]] std::string_view GetLoadError() const {
        return load_error;
    }

    /// Obtains the address of the specified symbol, automatically casting to the correct type.
    template <typename T>
    [[nodiscard]] T GetSymbol(std::string_view name) const {
        return reinterpret_cast<T>(GetRawSymbol(name));
    }

    /// Returns the specified library name in platform-specific format.
    /// Major/minor versions will not be included if set to -1.
    /// If libname already contains the "lib" prefix, it will not be added again.
    [[nodiscard]] static std::string GetLibraryName(std::string_view name, int major = -1,
                                                    int minor = -1);

private:
    void* GetRawSymbol(std::string_view name) const;

    void* handle;
    std::string load_error;
};

} // namespace Common
