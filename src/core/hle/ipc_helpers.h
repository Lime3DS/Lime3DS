// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "core/hle/ipc.h"
#include "core/hle/kernel/hle_ipc.h"

namespace IPC {

class RequestHelperBase {
protected:
    Kernel::HLERequestContext* context;
    u32* cmdbuf;
    std::size_t index = 1;
    Header header;

public:
    RequestHelperBase(Kernel::HLERequestContext& context, Header desired_header)
        : context(&context), cmdbuf(context.CommandBuffer()), header(desired_header) {}

    /// Returns the total size of the request in words
    std::size_t TotalSize() const {
        return 1 /* command header */ + header.normal_params_size + header.translate_params_size;
    }

    void ValidateHeader() {
        DEBUG_ASSERT_MSG(index == TotalSize(), "Operations do not match the header (cmd {:#x})",
                         header.raw);
    }

    void Skip(unsigned size_in_words, bool set_to_null) {
        if (set_to_null)
            std::memset(cmdbuf + index, 0, size_in_words * sizeof(u32));
        index += size_in_words;
    }
};

class RequestBuilder : public RequestHelperBase {
public:
    RequestBuilder(Kernel::HLERequestContext& context, Header command_header)
        : RequestHelperBase(context, command_header) {
        // From this point we will start overwriting the existing command buffer, so it's safe to
        // release all previous incoming Object pointers since they won't be usable anymore.
        context.ClearIncomingObjects();
        cmdbuf[0] = header.raw;
    }

    RequestBuilder(Kernel::HLERequestContext& context, u16 command_id, unsigned normal_params_size,
                   unsigned translate_params_size)
        : RequestBuilder(
              context, Header{MakeHeader(command_id, normal_params_size, translate_params_size)}) {}

    RequestBuilder(Kernel::HLERequestContext& context, unsigned normal_params_size,
                   unsigned translate_params_size)
        : RequestBuilder(context, Header{MakeHeader(context.CommandID(), normal_params_size,
                                                    translate_params_size)}) {}

    // Validate on destruction, as there shouldn't be any case where we don't want it
    ~RequestBuilder() {
        ValidateHeader();
    }

    template <typename T>
    void Push(T value);

    template <typename First, typename... Other>
    void Push(const First& first_value, const Other&... other_values);

    template <typename T>
    void PushEnum(T value) {
        static_assert(std::is_enum<T>(), "T must be an enum type within a PushEnum call.");
        static_assert(!std::is_convertible<T, int>(),
                      "enum type in PushEnum must be a strongly typed enum.");
        static_assert(sizeof(value) < sizeof(u64), "64-bit enums may not be pushed.");
        Push(static_cast<std::underlying_type_t<T>>(value));
    }

    /**
     * @brief Copies the content of the given trivially copyable class to the buffer as a normal
     * param
     * @note: The input class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    void PushRaw(const T& value);

    // TODO : ensure that translate params are added after all regular params
    template <typename... O>
    void PushCopyObjects(std::shared_ptr<O>... pointers);

    template <typename... O>
    void PushMoveObjects(std::shared_ptr<O>... pointers);

    void PushStaticBuffer(std::vector<u8> buffer, u8 buffer_id);

    /// Pushes an HLE MappedBuffer interface back to unmapped the buffer.
    void PushMappedBuffer(const Kernel::MappedBuffer& mapped_buffer);

private:
    template <typename... H>
    void PushCopyHLEHandles(H... handles);

    template <typename... H>
    void PushMoveHLEHandles(H... handles);
};

/// Push ///

template <>
inline void RequestBuilder::Push(u32 value) {
    cmdbuf[index++] = value;
}

template <>
inline void RequestBuilder::Push(s32 value) {
    cmdbuf[index++] = static_cast<u32>(value);
}

template <typename T>
void RequestBuilder::PushRaw(const T& value) {
    static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
    std::memcpy(cmdbuf + index, &value, sizeof(T));
    index += (sizeof(T) + 3) / 4; // round up to word length
}

template <>
inline void RequestBuilder::Push(u8 value) {
    PushRaw(value);
}

template <>
inline void RequestBuilder::Push(u16 value) {
    PushRaw(value);
}

template <>
inline void RequestBuilder::Push(u64 value) {
    Push(static_cast<u32>(value));
    Push(static_cast<u32>(value >> 32));
}

template <>
inline void RequestBuilder::Push(f32 value) {
    u32 integral;
    std::memcpy(&integral, &value, sizeof(u32));
    Push(integral);
}

template <>
inline void RequestBuilder::Push(f64 value) {
    u64 integral;
    std::memcpy(&integral, &value, sizeof(u64));
    Push(integral);
}

template <>
inline void RequestBuilder::Push(bool value) {
    Push(static_cast<u8>(value));
}

template <>
inline void RequestBuilder::Push(Result value) {
    Push(value.raw);
}

template <typename First, typename... Other>
void RequestBuilder::Push(const First& first_value, const Other&... other_values) {
    Push(first_value);
    Push(other_values...);
}

template <typename... H>
inline void RequestBuilder::PushCopyHLEHandles(H... handles) {
    Push(CopyHandleDesc(sizeof...(H)));
    Push(static_cast<u32>(handles)...);
}

template <typename... H>
inline void RequestBuilder::PushMoveHLEHandles(H... handles) {
    Push(MoveHandleDesc(sizeof...(H)));
    Push(static_cast<u32>(handles)...);
}

template <typename... O>
inline void RequestBuilder::PushCopyObjects(std::shared_ptr<O>... pointers) {
    PushCopyHLEHandles(context->AddOutgoingHandle(std::move(pointers))...);
}

template <typename... O>
inline void RequestBuilder::PushMoveObjects(std::shared_ptr<O>... pointers) {
    PushMoveHLEHandles(context->AddOutgoingHandle(std::move(pointers))...);
}

inline void RequestBuilder::PushStaticBuffer(std::vector<u8> buffer, u8 buffer_id) {
    ASSERT_MSG(buffer_id < MAX_STATIC_BUFFERS, "Invalid static buffer id");

    Push(StaticBufferDesc(buffer.size(), buffer_id));
    // This address will be replaced by the correct static buffer address during IPC translation.
    Push<VAddr>(0xDEADC0DE);

    context->AddStaticBuffer(buffer_id, std::move(buffer));
}

inline void RequestBuilder::PushMappedBuffer(const Kernel::MappedBuffer& mapped_buffer) {
    Push(mapped_buffer.GenerateDescriptor());
    Push(mapped_buffer.GetId());
}

class RequestParser : public RequestHelperBase {
public:
    RequestParser(Kernel::HLERequestContext& context)
        : RequestHelperBase(context, context.CommandHeader()) {}

    RequestBuilder MakeBuilder(u32 normal_params_size, u32 translate_params_size,
                               bool validateHeader = true) {
        if (validateHeader)
            ValidateHeader();
        Header builderHeader{MakeHeader(static_cast<u16>(header.command_id), normal_params_size,
                                        translate_params_size)};
        return {*context, builderHeader};
    }

    template <typename T>
    T Pop();

    template <typename T>
    void Pop(T& value);

    template <typename First, typename... Other>
    void Pop(First& first_value, Other&... other_values);

    template <typename T>
    T PopEnum() {
        static_assert(std::is_enum<T>(), "T must be an enum type within a PopEnum call.");
        static_assert(!std::is_convertible<T, int>(),
                      "enum type in PopEnum must be a strongly typed enum.");
        static_assert(sizeof(T) < sizeof(u64), "64-bit enums cannot be popped.");
        return static_cast<T>(Pop<std::underlying_type_t<T>>());
    }

    /// Equivalent to calling `PopGenericObjects<1>()[0]`.
    std::shared_ptr<Kernel::Object> PopGenericObject();

    /// Equivalent to calling `std::get<0>(PopObjects<T>())`.
    template <typename T>
    std::shared_ptr<T> PopObject();

    /**
     * Pop a descriptor containing `N` handles and resolves them to Kernel::Object pointers. If a
     * handle is invalid, null is returned for that object instead. The descriptor must contain
     * exactly `N` handles, it is not permitted to, for example, call PopGenericObjects<1>() twice
     * to read a multi-handle descriptor with 2 handles, or to make a single PopGenericObjects<2>()
     * call to read 2 single-handle descriptors.
     */
    template <unsigned int N>
    std::array<std::shared_ptr<Kernel::Object>, N> PopGenericObjects();

    /**
     * Resolves handles to Kernel::Objects as in PopGenericsObjects(), but then also casts them to
     * the passed `T` types, while verifying that the cast is valid. If the type of an object does
     * not match, null is returned instead.
     */
    template <typename... T>
    std::tuple<std::shared_ptr<T>...> PopObjects();

    /// Convenience wrapper around PopObjects() which assigns the handles to the passed references.
    template <typename... T>
    void PopObjects(std::shared_ptr<T>&... pointers) {
        std::tie(pointers...) = PopObjects<T...>();
    }

    u32 PopPID();

    /**
     * @brief Pops a static buffer from the IPC request buffer.
     * @return The buffer that was copied from the IPC request originator.
     *
     * In real services, static buffers must be set up before any IPC request using those is sent.
     * It is the duty of the process (usually services) to allocate and set up the receiving static
     * buffer information. Our HLE services do not need to set up the buffers beforehand.
     */
    const std::vector<u8>& PopStaticBuffer();

    /// Pops a mapped buffer descriptor with its vaddr and resolves it to an HLE interface
    Kernel::MappedBuffer& PopMappedBuffer();

    /**
     * @brief Reads the next normal parameters as a struct, by copying it
     * @note: The output class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    void PopRaw(T& value);

    /**
     * @brief Reads the next normal parameters as a struct, by copying it into a new value
     * @note: The output class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    T PopRaw();

private:
    template <unsigned int N>
    std::array<u32, N> PopHLEHandles();
};

/// Pop ///

template <>
inline u32 RequestParser::Pop() {
    return cmdbuf[index++];
}

template <typename T>
void RequestParser::PopRaw(T& value) {
    static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
    std::memcpy(&value, cmdbuf + index, sizeof(T));
    index += (sizeof(T) + 3) / 4; // round up to word length
}

template <typename T>
T RequestParser::PopRaw() {
    T value;
    PopRaw(value);
    return value;
}

template <>
inline u8 RequestParser::Pop() {
    return PopRaw<u8>();
}

template <>
inline u16 RequestParser::Pop() {
    return PopRaw<u16>();
}

template <>
inline u64 RequestParser::Pop() {
    const u64 lsw = Pop<u32>();
    const u64 msw = Pop<u32>();
    return msw << 32 | lsw;
}

template <>
inline s32 RequestParser::Pop() {
    s32_le data = PopRaw<s32_le>();
    return data;
}

template <>
inline f32 RequestParser::Pop() {
    const u32 value = Pop<u32>();
    float real;
    std::memcpy(&real, &value, sizeof(real));
    return real;
}

template <>
inline f64 RequestParser::Pop() {
    const u64 value = Pop<u64>();
    f64 real;
    std::memcpy(&real, &value, sizeof(real));
    return real;
}

template <>
inline bool RequestParser::Pop() {
    return Pop<u8>() != 0;
}

template <>
inline Result RequestParser::Pop() {
    return Result{Pop<u32>()};
}

template <typename T>
void RequestParser::Pop(T& value) {
    value = Pop<T>();
}

template <typename First, typename... Other>
void RequestParser::Pop(First& first_value, Other&... other_values) {
    first_value = Pop<First>();
    Pop(other_values...);
}

template <unsigned int N>
std::array<u32, N> RequestParser::PopHLEHandles() {
    u32 handle_descriptor = Pop<u32>();
    ASSERT_MSG(IsHandleDescriptor(handle_descriptor),
               "Tried to pop handle(s) but the descriptor is not a handle descriptor");
    ASSERT_MSG(N == HandleNumberFromDesc(handle_descriptor),
               "Number of handles doesn't match the descriptor");

    std::array<u32, N> handles{};
    for (u32& handle : handles) {
        handle = Pop<u32>();
    }
    return handles;
}

inline std::shared_ptr<Kernel::Object> RequestParser::PopGenericObject() {
    auto [handle] = PopHLEHandles<1>();
    return context->GetIncomingHandle(handle);
}

template <typename T>
std::shared_ptr<T> RequestParser::PopObject() {
    return Kernel::DynamicObjectCast<T>(PopGenericObject());
}

template <unsigned int N>
inline std::array<std::shared_ptr<Kernel::Object>, N> RequestParser::PopGenericObjects() {
    std::array<u32, N> handles = PopHLEHandles<N>();
    std::array<std::shared_ptr<Kernel::Object>, N> pointers;
    for (int i = 0; i < N; ++i) {
        pointers[i] = context->GetIncomingHandle(handles[i]);
    }
    return pointers;
}

namespace detail {
template <typename... T, std::size_t... I>
std::tuple<std::shared_ptr<T>...> PopObjectsHelper(
    std::array<std::shared_ptr<Kernel::Object>, sizeof...(T)>&& pointers,
    std::index_sequence<I...>) {
    return std::make_tuple(Kernel::DynamicObjectCast<T>(std::move(pointers[I]))...);
}
} // namespace detail

template <typename... T>
inline std::tuple<std::shared_ptr<T>...> RequestParser::PopObjects() {
    return detail::PopObjectsHelper<T...>(PopGenericObjects<sizeof...(T)>(),
                                          std::index_sequence_for<T...>{});
}

inline u32 RequestParser::PopPID() {
    ASSERT(Pop<u32>() == static_cast<u32>(DescriptorType::CallingPid));
    return Pop<u32>();
}

inline const std::vector<u8>& RequestParser::PopStaticBuffer() {
    const u32 sbuffer_descriptor = Pop<u32>();
    // Pop the address from the incoming request buffer
    Pop<VAddr>();

    StaticBufferDescInfo buffer_info{sbuffer_descriptor};
    return context->GetStaticBuffer(static_cast<u8>(buffer_info.buffer_id));
}

inline Kernel::MappedBuffer& RequestParser::PopMappedBuffer() {
    u32 mapped_buffer_descriptor = Pop<u32>();
    ASSERT_MSG(GetDescriptorType(mapped_buffer_descriptor) == MappedBuffer,
               "Tried to pop mapped buffer but the descriptor is not a mapped buffer descriptor");
    return context->GetMappedBuffer(Pop<u32>());
}

} // namespace IPC
