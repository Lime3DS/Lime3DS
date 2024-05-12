// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/unique_ptr.hpp>
#include "common/archives.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/service/fs/file.h"

SERIALIZE_EXPORT_IMPL(Service::FS::File)
SERIALIZE_EXPORT_IMPL(Service::FS::FileSessionSlot)

namespace Service::FS {

template <class Archive>
void File::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & path;
    ar & backend;
}

File::File() : File(Core::Global<Kernel::KernelSystem>()) {}

File::File(Kernel::KernelSystem& kernel, std::unique_ptr<FileSys::FileBackend>&& backend,
           const FileSys::Path& path)
    : File(kernel) {
    this->backend = std::move(backend);
    this->path = path;
}

File::File(Kernel::KernelSystem& kernel)
    : ServiceFramework("", 1), path(""), backend(nullptr), kernel(kernel) {
    static const FunctionInfo functions[] = {
        {0x0801, &File::OpenSubFile, "OpenSubFile"},
        {0x0802, &File::Read, "Read"},
        {0x0803, &File::Write, "Write"},
        {0x0804, &File::GetSize, "GetSize"},
        {0x0805, &File::SetSize, "SetSize"},
        {0x0808, &File::Close, "Close"},
        {0x0809, &File::Flush, "Flush"},
        {0x080A, &File::SetPriority, "SetPriority"},
        {0x080B, &File::GetPriority, "GetPriority"},
        {0x080C, &File::OpenLinkFile, "OpenLinkFile"},
    };
    RegisterHandlers(functions);
}

void File::Read(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 offset = rp.Pop<u64>();
    u32 length = rp.Pop<u32>();
    LOG_TRACE(Service_FS, "Read {}: offset=0x{:x} length=0x{:08X}", GetName(), offset, length);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    if (file->subfile && length > file->size) {
        LOG_WARNING(Service_FS, "Trying to read beyond the subfile size, truncating");
        length = static_cast<u32>(file->size);
    }

    // This file session might have a specific offset from where to start reading, apply it.
    offset += file->offset;

    if (offset + length > backend->GetSize()) {
        LOG_ERROR(Service_FS,
                  "Reading from out of bounds offset=0x{:x} length=0x{:08X} file_size=0x{:x}",
                  offset, length, backend->GetSize());
    }

    // Conventional reading if the backend does not support cache.
    if (!backend->AllowsCachedReads()) {
        auto& buffer = rp.PopMappedBuffer();
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
        std::unique_ptr<u8*> data = std::make_unique<u8*>(static_cast<u8*>(operator new(length)));
        const auto read = backend->Read(offset, length, *data);
        if (read.Failed()) {
            rb.Push(read.Code());
            rb.Push<u32>(0);
        } else {
            buffer.Write(*data, 0, *read);
            rb.Push(ResultSuccess);
            rb.Push<u32>(static_cast<u32>(*read));
        }
        rb.PushMappedBuffer(buffer);

        std::chrono::nanoseconds read_timeout_ns{backend->GetReadDelayNs(length)};
        ctx.SleepClientThread("file::read", read_timeout_ns, nullptr);
        return;
    }

    struct AsyncData {
        // Input
        u32 length;
        u64 offset;
        std::chrono::steady_clock::time_point pre_timer;
        bool cache_ready;

        // Output
        Result ret{0};
        Kernel::MappedBuffer* buffer;
        std::unique_ptr<u8*> data;
        std::size_t read_size;
    };

    auto async_data = std::make_shared<AsyncData>();
    async_data->buffer = &rp.PopMappedBuffer();
    async_data->length = length;
    async_data->offset = offset;
    async_data->cache_ready = backend->CacheReady(offset, length);
    if (!async_data->cache_ready) {
        async_data->pre_timer = std::chrono::steady_clock::now();
    }

    // LOG_DEBUG(Service_FS, "cache={}, offset={}, length={}", cache_ready, offset, length);
    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->data =
                std::make_unique<u8*>(static_cast<u8*>(operator new(async_data->length)));
            const auto read =
                backend->Read(async_data->offset, async_data->length, *async_data->data);
            if (read.Failed()) {
                async_data->ret = read.Code();
                async_data->read_size = 0;
            } else {
                async_data->ret = ResultSuccess;
                async_data->read_size = *read;
            }

            const auto read_delay = static_cast<s64>(backend->GetReadDelayNs(async_data->length));
            if (!async_data->cache_ready) {
                const auto time_took = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                           std::chrono::steady_clock::now() - async_data->pre_timer)
                                           .count();
                /*
                if (time_took > read_delay) {
                    LOG_DEBUG(Service_FS, "Took longer! length={}, time_took={}, read_delay={}",
                              async_data->length, time_took, read_delay);
                }
                */
                return static_cast<s64>((read_delay > time_took) ? (read_delay - time_took) : 0);
            } else {
                return static_cast<s64>(read_delay);
            }
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 0x0802, 2, 2);
            if (async_data->ret.IsError()) {
                rb.Push(async_data->ret);
                rb.Push<u32>(0);
            } else {
                async_data->buffer->Write(*async_data->data, 0, async_data->read_size);
                rb.Push(ResultSuccess);
                rb.Push<u32>(static_cast<u32>(async_data->read_size));
            }
            rb.PushMappedBuffer(*async_data->buffer);
        },
        !async_data->cache_ready);
}

void File::Write(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 offset = rp.Pop<u64>();
    u32 length = rp.Pop<u32>();
    u32 flags = rp.Pop<u32>();
    LOG_TRACE(Service_FS, "Write {}: offset=0x{:x} length={}, flags=0x{:x}", GetName(), offset,
              length, flags);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    FileSessionSlot* file = GetSessionData(ctx.Session());

    // Subfiles can not be written to
    if (file->subfile) {
        rb.Push(FileSys::ResultUnsupportedOpenFlags);
        rb.Push<u32>(0);
        rb.PushMappedBuffer(rp.PopMappedBuffer());
        return;
    }
    bool flush = (flags & 0xFF) != 0, update_timestamp = (flags & 0xFF00) != 0;

    if (!backend->AllowsCachedReads()) {
        std::vector<u8> data(length);
        auto& buffer = rp.PopMappedBuffer();
        buffer.Read(data.data(), 0, data.size());
        ResultVal<std::size_t> written =
            backend->Write(offset, data.size(), flush, update_timestamp, data.data());

        // Update file size
        file->size = backend->GetSize();

        if (written.Failed()) {
            rb.Push(written.Code());
            rb.Push<u32>(0);
        } else {
            rb.Push(ResultSuccess);
            rb.Push<u32>(static_cast<u32>(*written));
        }
        rb.PushMappedBuffer(buffer);
        return;
    }

    struct AsyncData {
        // Input
        u32 length;
        u64 offset;
        bool flush;
        bool update_timestamp;
        Kernel::MappedBuffer* buffer;
        FileSessionSlot* file;

        // Output
        ResultVal<std::size_t> written;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->length = length;
    async_data->offset = offset;
    async_data->flush = flush;
    async_data->update_timestamp = update_timestamp;
    async_data->buffer = &rp.PopMappedBuffer();
    async_data->file = file;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            std::vector<u8> data(async_data->length);
            async_data->buffer->Read(data.data(), 0, data.size());
            async_data->written = backend->Write(async_data->offset, data.size(), async_data->flush,
                                                 async_data->update_timestamp, data.data());

            // Update file size
            async_data->file->size = backend->GetSize();
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 2, 2);
            if (async_data->written.Failed()) {
                rb.Push(async_data->written.Code());
                rb.Push<u32>(0);
            } else {
                rb.Push(ResultSuccess);
                rb.Push<u32>(static_cast<u32>(*async_data->written));
            }
            rb.PushMappedBuffer(*async_data->buffer);
        },
        true);
}

void File::GetSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push<u64>(file->size);
}

void File::SetSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 size = rp.Pop<u64>();

    FileSessionSlot* file = GetSessionData(ctx.Session());

    // SetSize can not be called on subfiles.
    if (file->subfile) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(FileSys::ResultUnsupportedOpenFlags);
        return;
    }

    if (!backend->AllowsCachedReads()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        file->size = size;
        backend->SetSize(size);
        rb.Push(ResultSuccess);
        return;
    }

    ctx.RunAsync(
        [file, size, this](Kernel::HLERequestContext& ctx) {
            file->size = size;
            backend->SetSize(size);
            return 0;
        },
        [](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(ResultSuccess);
        },
        true);
}

void File::Close(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    // TODO(Subv): Only close the backend if this client is the only one left.
    if (connected_sessions.size() > 1)
        LOG_WARNING(Service_FS, "Closing File backend but {} clients still connected",
                    connected_sessions.size());

    if (!backend->AllowsCachedReads()) {
        backend->Close();
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        return;
    }

    ctx.RunAsync(
        [this](Kernel::HLERequestContext& ctx) {
            backend->Close();
            return 0;
        },
        [](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(ResultSuccess);
        },
        true);
}

void File::Flush(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    // Subfiles can not be flushed.
    if (file->subfile) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(FileSys::ResultUnsupportedOpenFlags);
        return;
    }

    if (!backend->AllowsCachedReads()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        backend->Flush();
        rb.Push(ResultSuccess);
    }

    ctx.RunAsync(
        [this](Kernel::HLERequestContext& ctx) {
            backend->Flush();
            return 0;
        },
        [](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(ResultSuccess);
        },
        true);
}

void File::SetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    FileSessionSlot* file = GetSessionData(ctx.Session());
    file->priority = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void File::GetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const FileSessionSlot* file = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(file->priority);
}

void File::OpenLinkFile(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) File command OpenLinkFile {}", GetName());
    using Kernel::ClientSession;
    using Kernel::ServerSession;
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    auto [server, client] = kernel.CreateSessionPair(GetName());
    ClientConnected(server);

    FileSessionSlot* slot = GetSessionData(std::move(server));
    const FileSessionSlot* original_file = GetSessionData(ctx.Session());

    slot->priority = original_file->priority;
    slot->offset = 0;
    slot->size = backend->GetSize();
    slot->subfile = false;

    rb.Push(ResultSuccess);
    rb.PushMoveObjects(client);
}

void File::OpenSubFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    s64 offset = rp.PopRaw<s64>();
    s64 size = rp.PopRaw<s64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    const FileSessionSlot* original_file = GetSessionData(ctx.Session());

    if (original_file->subfile) {
        // OpenSubFile can not be called on a file which is already as subfile
        rb.Push(FileSys::ResultUnsupportedOpenFlags);
        return;
    }

    if (offset < 0 || size < 0) {
        rb.Push(FileSys::ResultWriteBeyondEnd);
        return;
    }

    std::size_t end = offset + size;

    // TODO(Subv): Check for overflow and return ResultWriteBeyondEnd

    if (end > original_file->size) {
        rb.Push(FileSys::ResultWriteBeyondEnd);
        return;
    }

    using Kernel::ClientSession;
    using Kernel::ServerSession;
    auto [server, client] = kernel.CreateSessionPair(GetName());
    ClientConnected(server);

    FileSessionSlot* slot = GetSessionData(std::move(server));
    slot->priority = original_file->priority;
    slot->offset = offset;
    slot->size = size;
    slot->subfile = true;

    rb.Push(ResultSuccess);
    rb.PushMoveObjects(client);
}

std::shared_ptr<Kernel::ClientSession> File::Connect() {
    auto [server, client] = kernel.CreateSessionPair(GetName());
    ClientConnected(server);

    FileSessionSlot* slot = GetSessionData(std::move(server));
    slot->priority = 0;
    slot->offset = 0;
    slot->size = backend->GetSize();
    slot->subfile = false;

    return client;
}

std::size_t File::GetSessionFileOffset(std::shared_ptr<Kernel::ServerSession> session) {
    const FileSessionSlot* slot = GetSessionData(std::move(session));
    ASSERT(slot);
    return slot->offset;
}

std::size_t File::GetSessionFileSize(std::shared_ptr<Kernel::ServerSession> session) {
    const FileSessionSlot* slot = GetSessionData(std::move(session));
    ASSERT(slot);
    return slot->size;
}

} // namespace Service::FS
