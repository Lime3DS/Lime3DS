// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/seed_db.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"

SERVICE_CONSTRUCT_IMPL(Service::FS::FS_USER)
SERIALIZE_EXPORT_IMPL(Service::FS::FS_USER)
SERIALIZE_EXPORT_IMPL(Service::FS::ClientSlot)

using Kernel::ClientSession;
using Kernel::ServerSession;

namespace Service::FS {

void FS_USER::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 pid = rp.PopPID();

    ClientSlot* slot = GetSessionData(ctx.Session());
    slot->program_id = system.Kernel().GetProcessById(pid)->codeset->program_id;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void FS_USER::OpenFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.Skip(1, false); // Transaction.

    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    const FileSys::Mode mode{rp.Pop<u32>()};
    const auto attributes = rp.Pop<u32>();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(filename.size() == filename_size);
    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "path={}, mode={} attrs={}", file_path.DebugStr(), mode.hex, attributes);

    if (!archives.ArchiveIsSlow(archive_handle)) {
        const auto [file_res, open_timeout_ns] =
            archives.OpenFileFromArchive(archive_handle, file_path, mode, attributes);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(file_res.Code());
        if (file_res.Succeeded()) {
            std::shared_ptr<File> file = *file_res;
            rb.PushMoveObjects(file->Connect());
        } else {
            rb.PushMoveObjects<Kernel::Object>(nullptr);
            LOG_DEBUG(Service_FS, "failed to get a handle for file {}", file_path.DebugStr());
        }

        ctx.SleepClientThread("fs_user::open", open_timeout_ns, nullptr);
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path file_path;
        FileSys::Mode mode;
        u32 attributes;
        std::chrono::steady_clock::time_point pre_timer;

        std::pair<ResultVal<std::shared_ptr<File>>, std::chrono::nanoseconds> file;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->file_path = file_path;
    async_data->mode = mode;
    async_data->attributes = attributes;
    async_data->pre_timer = std::chrono::steady_clock::now();

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->file =
                archives.OpenFileFromArchive(async_data->archive_handle, async_data->file_path,
                                             async_data->mode, async_data->attributes);
            const auto time_took = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - async_data->pre_timer);
            return static_cast<s64>(((async_data->file.second > time_took)
                                         ? (async_data->file.second - time_took)
                                         : std::chrono::nanoseconds())
                                        .count());
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 2);

            rb.Push(async_data->file.first.Code());
            if (async_data->file.first.Succeeded()) {
                std::shared_ptr<File> file = *async_data->file.first;
                rb.PushMoveObjects(file->Connect());
            } else {
                rb.PushMoveObjects<Kernel::Object>(nullptr);
                LOG_DEBUG(Service_FS, "failed to get a handle for file {}",
                          async_data->file_path.DebugStr());
            }
        },
        true);
}

void FS_USER::OpenFileDirectly(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.Skip(1, false); // Transaction

    const auto archive_id = rp.PopEnum<ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    const FileSys::Mode mode{rp.Pop<u32>()};
    const auto attributes = rp.Pop<u32>(); // TODO(Link Mauve): do something with those attributes.
    std::vector<u8> archivename = rp.PopStaticBuffer();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);
    ASSERT(filename.size() == filename_size);
    const FileSys::Path archive_path(archivename_type, std::move(archivename));
    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "archive_id=0x{:08X} archive_path={} file_path={}, mode={} attributes={}",
              archive_id, archive_path.DebugStr(), file_path.DebugStr(), mode.hex, attributes);

    u64 program_id = GetSessionData(ctx.Session())->program_id;

    if (!archives.ArchiveIsSlow(archive_id)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

        ResultVal<ArchiveHandle> archive_handle =
            archives.OpenArchive(archive_id, archive_path, program_id);
        if (archive_handle.Failed()) {
            LOG_ERROR(Service_FS,
                      "Failed to get a handle for archive archive_id=0x{:08X} archive_path={}",
                      archive_id, archive_path.DebugStr());
            rb.Push(archive_handle.Code());
            rb.PushMoveObjects<Kernel::Object>(nullptr);
            return;
        }
        SCOPE_EXIT({ archives.CloseArchive(*archive_handle); });

        const auto [file_res, open_timeout_ns] =
            archives.OpenFileFromArchive(*archive_handle, file_path, mode, attributes);
        rb.Push(file_res.Code());
        if (file_res.Succeeded()) {
            std::shared_ptr<File> file = *file_res;
            rb.PushMoveObjects(file->Connect());
        } else {
            rb.PushMoveObjects<Kernel::Object>(nullptr);
            LOG_ERROR(Service_FS, "failed to get a handle for file {} mode={} attributes={}",
                      file_path.DebugStr(), mode.hex, attributes);
        }

        ctx.SleepClientThread("fs_user::open_directly", open_timeout_ns, nullptr);
        return;
    }

    struct AsyncData {
        ArchiveIdCode archive_id;
        FileSys::Path archive_path;
        FileSys::Path file_path;
        u64 program_id;
        FileSys::Mode mode;
        u32 attributes;
        std::chrono::steady_clock::time_point pre_timer;

        ResultVal<ArchiveHandle> archive_handle;
        std::pair<ResultVal<std::shared_ptr<File>>, std::chrono::nanoseconds> file;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_id = archive_id;
    async_data->archive_path = archive_path;
    async_data->file_path = file_path;
    async_data->program_id = program_id;
    async_data->mode = mode;
    async_data->attributes = attributes;
    async_data->pre_timer = std::chrono::steady_clock::now();

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->archive_handle = archives.OpenArchive(
                async_data->archive_id, async_data->archive_path, async_data->program_id);
            if (async_data->archive_handle.Failed()) {
                LOG_ERROR(Service_FS,
                          "Failed to get a handle for archive archive_id=0x{:08X} archive_path={}",
                          async_data->archive_id, async_data->archive_path.DebugStr());
                return s64();
            }
            async_data->file =
                archives.OpenFileFromArchive(*async_data->archive_handle, async_data->file_path,
                                             async_data->mode, async_data->attributes);
            archives.CloseArchive(*async_data->archive_handle);
            const auto time_took = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - async_data->pre_timer);
            return static_cast<s64>(((async_data->file.second > time_took)
                                         ? (async_data->file.second - time_took)
                                         : std::chrono::nanoseconds())
                                        .count());
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 2);

            if (async_data->archive_handle.Failed()) {
                rb.Push(async_data->archive_handle.Code());
                rb.PushMoveObjects<Kernel::Object>(nullptr);
            }

            rb.Push(async_data->file.first.Code());
            if (async_data->file.first.Succeeded()) {
                std::shared_ptr<File> file = *async_data->file.first;
                rb.PushMoveObjects(file->Connect());
            } else {
                rb.PushMoveObjects<Kernel::Object>(nullptr);
                LOG_DEBUG(Service_FS, "failed to get a handle for file {}",
                          async_data->file_path.DebugStr());
            }
        },
        true);
}

void FS_USER::DeleteFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(filename.size() == filename_size);

    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", filename_type, filename_size,
              file_path.DebugStr());

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.DeleteFileFromArchive(archive_handle, file_path));
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path file_path;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->file_path = file_path;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res =
                archives.DeleteFileFromArchive(async_data->archive_handle, async_data->file_path);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::RenameFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.Skip(1, false); // TransactionId

    const auto src_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto src_filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto src_filename_size = rp.Pop<u32>();
    const auto dest_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dest_filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dest_filename_size = rp.Pop<u32>();
    std::vector<u8> src_filename = rp.PopStaticBuffer();
    std::vector<u8> dest_filename = rp.PopStaticBuffer();
    ASSERT(src_filename.size() == src_filename_size);
    ASSERT(dest_filename.size() == dest_filename_size);

    const FileSys::Path src_file_path(src_filename_type, std::move(src_filename));
    const FileSys::Path dest_file_path(dest_filename_type, std::move(dest_filename));

    LOG_DEBUG(Service_FS,
              "src_type={} src_size={} src_data={} dest_type={} dest_size={} dest_data={}",
              src_filename_type, src_filename_size, src_file_path.DebugStr(), dest_filename_type,
              dest_filename_size, dest_file_path.DebugStr());

    if (!archives.ArchiveIsSlow(src_archive_handle) &&
        !archives.ArchiveIsSlow(dest_archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.RenameFileBetweenArchives(src_archive_handle, src_file_path,
                                                   dest_archive_handle, dest_file_path));
        return;
    }

    struct AsyncData {
        ArchiveHandle src_archive_handle;
        FileSys::Path src_file_path;
        ArchiveHandle dest_archive_handle;
        FileSys::Path dest_file_path;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->src_archive_handle = src_archive_handle;
    async_data->src_file_path = src_file_path;
    async_data->dest_archive_handle = dest_archive_handle;
    async_data->dest_file_path = dest_file_path;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.RenameFileBetweenArchives(
                async_data->src_archive_handle, async_data->src_file_path,
                async_data->dest_archive_handle, async_data->dest_file_path);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::DeleteDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);

    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.DeleteDirectoryFromArchive(archive_handle, dir_path));
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path dir_path;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->dir_path = dir_path;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.DeleteDirectoryFromArchive(async_data->archive_handle,
                                                                  async_data->dir_path);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::DeleteDirectoryRecursively(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);

    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.DeleteDirectoryRecursivelyFromArchive(archive_handle, dir_path));
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path dir_path;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->dir_path = dir_path;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.DeleteDirectoryRecursivelyFromArchive(
                async_data->archive_handle, async_data->dir_path);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::CreateFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    const auto attributes = rp.Pop<u32>();
    const auto file_size = rp.Pop<u64>();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(filename.size() == filename_size);

    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "type={} attributes={} size={:x} data={}", filename_type, attributes,
              file_size, file_path.DebugStr());

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.CreateFileInArchive(archive_handle, file_path, file_size, attributes));
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path file_path;
        u64 file_size;
        u32 attributes;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->file_path = file_path;
    async_data->file_size = file_size;
    async_data->attributes = attributes;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res =
                archives.CreateFileInArchive(async_data->archive_handle, async_data->file_path,
                                             async_data->file_size, async_data->attributes);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::CreateDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    const auto attributes = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);
    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.CreateDirectoryFromArchive(archive_handle, dir_path, attributes));
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path dir_path;
        u32 attributes;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->dir_path = dir_path;
    async_data->attributes = attributes;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.CreateDirectoryFromArchive(
                async_data->archive_handle, async_data->dir_path, async_data->attributes);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::RenameDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.Skip(1, false); // TransactionId
    const auto src_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto src_dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto src_dirname_size = rp.Pop<u32>();
    const auto dest_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dest_dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dest_dirname_size = rp.Pop<u32>();
    std::vector<u8> src_dirname = rp.PopStaticBuffer();
    std::vector<u8> dest_dirname = rp.PopStaticBuffer();
    ASSERT(src_dirname.size() == src_dirname_size);
    ASSERT(dest_dirname.size() == dest_dirname_size);

    const FileSys::Path src_dir_path(src_dirname_type, std::move(src_dirname));
    const FileSys::Path dest_dir_path(dest_dirname_type, std::move(dest_dirname));

    LOG_DEBUG(Service_FS,
              "src_type={} src_size={} src_data={} dest_type={} dest_size={} dest_data={}",
              src_dirname_type, src_dirname_size, src_dir_path.DebugStr(), dest_dirname_type,
              dest_dirname_size, dest_dir_path.DebugStr());

    if (!archives.ArchiveIsSlow(src_archive_handle) &&
        !archives.ArchiveIsSlow(dest_archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.RenameDirectoryBetweenArchives(src_archive_handle, src_dir_path,
                                                        dest_archive_handle, dest_dir_path));
        return;
    }

    struct AsyncData {
        ArchiveHandle src_archive_handle;
        FileSys::Path src_dir_path;
        ArchiveHandle dest_archive_handle;
        FileSys::Path dest_dir_path;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->src_archive_handle = src_archive_handle;
    async_data->src_dir_path = src_dir_path;
    async_data->dest_archive_handle = dest_archive_handle;
    async_data->dest_dir_path = dest_dir_path;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.RenameDirectoryBetweenArchives(
                async_data->src_archive_handle, async_data->src_dir_path,
                async_data->dest_archive_handle, async_data->dest_dir_path);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::OpenDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);

    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        ResultVal<std::shared_ptr<Directory>> dir_res =
            archives.OpenDirectoryFromArchive(archive_handle, dir_path);
        rb.Push(dir_res.Code());
        if (dir_res.Succeeded()) {
            std::shared_ptr<Directory> directory = *dir_res;
            auto [server, client] = system.Kernel().CreateSessionPair(directory->GetName());
            directory->ClientConnected(server);
            rb.PushMoveObjects(client);
        } else {
            LOG_ERROR(Service_FS, "failed to get a handle for directory type={} size={} data={}",
                      dirname_type, dirname_size, dir_path.DebugStr());
            rb.PushMoveObjects<Kernel::Object>(nullptr);
        }
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        FileSys::Path dir_path;

        ResultVal<std::shared_ptr<Directory>> dir_res;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->dir_path = dir_path;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->dir_res =
                archives.OpenDirectoryFromArchive(async_data->archive_handle, async_data->dir_path);
            return 0;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 2);

            rb.Push(async_data->dir_res.Code());
            if (async_data->dir_res.Succeeded()) {
                std::shared_ptr<Directory> directory = *async_data->dir_res;
                auto [server, client] = system.Kernel().CreateSessionPair(directory->GetName());
                directory->ClientConnected(server);
                rb.PushMoveObjects(client);
            } else {
                LOG_DEBUG(Service_FS, "failed to get a handle for directory path={}",
                          async_data->dir_path.DebugStr());
                rb.PushMoveObjects<Kernel::Object>(nullptr);
            }
        },
        true);
}

void FS_USER::OpenArchive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_id = rp.PopEnum<FS::ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    std::vector<u8> archivename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);
    const FileSys::Path archive_path(archivename_type, std::move(archivename));

    LOG_DEBUG(Service_FS, "archive_id=0x{:08X} archive_path={}", archive_id,
              archive_path.DebugStr());
    ClientSlot* slot = GetSessionData(ctx.Session());
    u64 program_id = slot->program_id;

    // Conventional opening
    if (!archives.ArchiveIsSlow(archive_id)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
        const ResultVal<ArchiveHandle> handle =
            archives.OpenArchive(archive_id, archive_path, program_id);
        rb.Push(handle.Code());
        if (handle.Succeeded()) {
            rb.PushRaw(*handle);
        } else {
            rb.Push<u64>(0);
            LOG_ERROR(Service_FS,
                      "failed to get a handle for archive archive_id=0x{:08X} archive_path={}",
                      archive_id, archive_path.DebugStr());
        }
        return;
    }

    struct AsyncData {
        // Input
        ArchiveIdCode archive_id;
        FileSys::Path archive_path;
        u64 program_id;

        // Output
        ResultVal<ArchiveHandle> handle;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_id = archive_id;
    async_data->archive_path = archive_path;
    async_data->program_id = program_id;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->handle = archives.OpenArchive(
                async_data->archive_id, async_data->archive_path, async_data->program_id);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 3, 0);
            rb.Push(async_data->handle.Code());
            if (async_data->handle.Succeeded()) {
                rb.PushRaw(*async_data->handle);
            } else {
                rb.Push<u64>(0);
                LOG_ERROR(Service_FS,
                          "failed to get a handle for archive archive_id=0x{:08X} archive_path={}",
                          async_data->archive_id, async_data->archive_path.DebugStr());
            }
        },
        true);
}

void FS_USER::ControlArchive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto action = rp.Pop<u32>();
    const auto input_size = rp.Pop<u32>();
    const auto output_size = rp.Pop<u32>();

    if (!archives.ArchiveIsSlow(archive_handle)) {
        auto input = rp.PopMappedBuffer();
        auto output = rp.PopMappedBuffer();
        std::vector<u8> in_data(input_size);
        input.Read(in_data.data(), 0, in_data.size());
        std::vector<u8> out_data(output_size);

        const Result res =
            archives.ControlArchive(archive_handle, action, in_data.data(), in_data.size(),
                                    out_data.data(), out_data.size());

        if (res.IsSuccess() && output_size != 0) {
            output.Write(out_data.data(), 0, out_data.size());
        }
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(res);
        return;
    }

    struct AsyncData {
        ArchiveHandle handle;
        u32 action;
        Kernel::MappedBuffer* in_buffer;
        u32 in_size;
        u32 out_size;

        Result res{0};
        std::vector<u8> out_data;
        Kernel::MappedBuffer* out_buffer;
    };

    auto async_data = std::make_shared<AsyncData>();
    async_data->handle = archive_handle;
    async_data->action = action;
    async_data->in_size = input_size;
    async_data->out_size = output_size;
    async_data->in_buffer = &rp.PopMappedBuffer();
    async_data->out_buffer = &rp.PopMappedBuffer();

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            std::vector<u8> in_data(async_data->in_size);
            async_data->in_buffer->Read(in_data.data(), 0, in_data.size());
            async_data->out_data.resize(async_data->out_size);

            async_data->res = archives.ControlArchive(
                async_data->handle, async_data->action, in_data.data(), in_data.size(),
                async_data->out_data.data(), async_data->out_data.size());
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            if (async_data->res.IsSuccess() && async_data->out_size != 0) {
                async_data->out_buffer->Write(async_data->out_data.data(), 0,
                                              async_data->out_data.size());
            }
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::CloseArchive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(archives.CloseArchive(archive_handle));
        return;
    }

    struct AsyncData {
        ArchiveHandle handle;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->handle = archive_handle;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.CloseArchive(async_data->handle);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::IsSdmcDetected(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(Settings::values.use_virtual_sd.GetValue());
}

void FS_USER::IsSdmcWriteable(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    // If the SD isn't enabled, it can't be writeable...else, stubbed true
    rb.Push(Settings::values.use_virtual_sd.GetValue());
    LOG_DEBUG(Service_FS, " (STUBBED)");
}

void FS_USER::FormatSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_id = rp.PopEnum<ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    const auto block_size = rp.Pop<u32>();
    const auto number_directories = rp.Pop<u32>();
    const auto number_files = rp.Pop<u32>();
    const auto directory_buckets = rp.Pop<u32>();
    const auto file_buckets = rp.Pop<u32>();
    const bool duplicate_data = rp.Pop<bool>();
    std::vector<u8> archivename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);
    const FileSys::Path archive_path(archivename_type, std::move(archivename));

    LOG_DEBUG(Service_FS, "archive_path={}", archive_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (archive_id != FS::ArchiveIdCode::SaveData) {
        LOG_ERROR(Service_FS, "tried to format an archive different than SaveData, {}", archive_id);
        rb.Push(FileSys::ResultInvalidPath);
        return;
    }

    if (archive_path.GetType() != FileSys::LowPathType::Empty) {
        // TODO(Subv): Implement formatting the SaveData of other games
        LOG_ERROR(Service_FS, "archive LowPath type other than empty is currently unsupported");
        rb.Push(UnimplementedFunction(ErrorModule::FS));
        return;
    }

    FileSys::ArchiveFormatInfo format_info;
    format_info.duplicate_data = duplicate_data;
    format_info.number_directories = number_directories;
    format_info.number_files = number_files;
    format_info.total_size = block_size * 512;

    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.FormatArchive(ArchiveIdCode::SaveData, format_info, archive_path,
                                   slot->program_id, directory_buckets, file_buckets));
}

void FS_USER::FormatThisUserSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto block_size = rp.Pop<u32>();
    const auto number_directories = rp.Pop<u32>();
    const auto number_files = rp.Pop<u32>();
    const auto directory_buckets = rp.Pop<u32>();
    const auto file_buckets = rp.Pop<u32>();
    const auto duplicate_data = rp.Pop<bool>();

    FileSys::ArchiveFormatInfo format_info;
    format_info.duplicate_data = duplicate_data;
    format_info.number_directories = number_directories;
    format_info.number_files = number_files;
    format_info.total_size = block_size * 512;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.FormatArchive(ArchiveIdCode::SaveData, format_info, FileSys::Path(),
                                   slot->program_id, directory_buckets, file_buckets));

    LOG_TRACE(Service_FS, "called");
}

void FS_USER::GetFreeBytes(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    ResultVal<u64> bytes_res = archives.GetFreeBytesInArchive(archive_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(bytes_res.Code());
    if (bytes_res.Succeeded()) {
        rb.Push<u64>(bytes_res.Unwrap());
    } else {
        rb.Push<u64>(0);
    }
}

void FS_USER::GetSdmcArchiveResource(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service_FS, "(STUBBED) called");

    auto resource = archives.GetArchiveResource(MediaType::SDMC);

    if (resource.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(resource.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(*resource);
}

void FS_USER::GetNandArchiveResource(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service_FS, "(STUBBED) called");

    auto resource = archives.GetArchiveResource(MediaType::NAND);
    if (resource.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(resource.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(*resource);
}

void FS_USER::CreateExtSaveData(Kernel::HLERequestContext& ctx) {
    // TODO(Subv): Figure out the other parameters.
    IPC::RequestParser rp(ctx);
    auto ext_data_info = rp.PopRaw<ExtSaveDataInfo>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    u64 size_limit = rp.Pop<u64>();
    u32 icon_size = rp.Pop<u32>();
    auto icon_buffer = rp.PopMappedBuffer();

    std::vector<u8> icon(icon_size);
    icon_buffer.Read(icon.data(), 0, icon_size);

    FileSys::ArchiveFormatInfo format_info;
    format_info.number_directories = directories;
    format_info.number_files = files;
    format_info.duplicate_data = false;
    format_info.total_size = static_cast<u32>(size_limit);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.CreateExtSaveData(static_cast<MediaType>(ext_data_info.media_type),
                                       ext_data_info.unknown, ext_data_info.save_id_high,
                                       ext_data_info.save_id_low, icon, format_info,
                                       slot->program_id, size_limit));
    rb.PushMappedBuffer(icon_buffer);

    LOG_DEBUG(Service_FS,
              "called, savedata_high={:08X} savedata_low={:08X} unknown={:08X} "
              "files={:08X} directories={:08X} size_limit={:016x} icon_size={:08X}",
              ext_data_info.save_id_high, ext_data_info.save_id_low, ext_data_info.unknown,
              directories, files, size_limit, icon_size);
}

void FS_USER::DeleteExtSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    ExtSaveDataInfo info = rp.PopRaw<ExtSaveDataInfo>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteExtSaveData(static_cast<MediaType>(info.media_type), info.unknown,
                                       info.save_id_high, info.save_id_low));

    LOG_DEBUG(Service_FS,
              "called, save_low={:08X} save_high={:08X} media_type={:08X} unknown={:08X}",
              info.save_id_low, info.save_id_high, info.media_type, info.unknown);
}

void FS_USER::CardSlotIsInserted(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(false);
    LOG_WARNING(Service_FS, "(STUBBED) called");
}

void FS_USER::DeleteSystemSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 savedata_high = rp.Pop<u32>();
    u32 savedata_low = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteSystemSaveData(savedata_high, savedata_low));
}

void FS_USER::CreateSystemSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 savedata_high = rp.Pop<u32>();
    u32 savedata_low = rp.Pop<u32>();
    u32 total_size = rp.Pop<u32>();
    u32 block_size = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    u32 directory_buckets = rp.Pop<u32>();
    u32 file_buckets = rp.Pop<u32>();
    bool duplicate = rp.Pop<bool>();

    LOG_WARNING(
        Service_FS,
        "(STUBBED) savedata_high={:08X} savedata_low={:08X} total_size={:08X}  block_size={:08X} "
        "directories={} files={} directory_buckets={} file_buckets={} duplicate={}",
        savedata_high, savedata_low, total_size, block_size, directories, files, directory_buckets,
        file_buckets, duplicate);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.CreateSystemSaveData(savedata_high, savedata_low, total_size, block_size,
                                          directories, files, directory_buckets, file_buckets,
                                          duplicate ? 1 : 0));
}

void FS_USER::CreateLegacySystemSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 savedata_id = rp.Pop<u32>();
    u32 total_size = rp.Pop<u32>();
    u32 block_size = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    u32 directory_buckets = rp.Pop<u32>();
    u32 file_buckets = rp.Pop<u32>();
    bool duplicate = rp.Pop<bool>();

    LOG_WARNING(Service_FS,
                "(STUBBED) savedata_id={:08X} total_size={:08X} block_size={:08X} directories={} "
                "files={} directory_buckets={} file_buckets={} duplicate={}",
                savedata_id, total_size, block_size, directories, files, directory_buckets,
                file_buckets, duplicate);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // With this command, the SystemSaveData always has save_high = 0 (Always created in the NAND)
    rb.Push(archives.CreateSystemSaveData(0, savedata_id, total_size, block_size, directories,
                                          files, directory_buckets, file_buckets, duplicate));
}

void FS_USER::InitializeWithSdkVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 version = rp.Pop<u32>();
    u32 pid = rp.PopPID();

    ClientSlot* slot = GetSessionData(ctx.Session());
    slot->program_id = system.Kernel().GetProcessById(pid)->codeset->program_id;

    LOG_WARNING(Service_FS, "(STUBBED) called, version: 0x{:08X}", version);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void FS_USER::SetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    priority = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_FS, "called priority=0x{:X}", priority);
}

void FS_USER::GetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (priority == UINT32_MAX) {
        LOG_INFO(Service_FS, "priority was not set, priority=0x{:X}", priority);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(priority);

    LOG_DEBUG(Service_FS, "called priority=0x{:X}", priority);
}

void FS_USER::GetArchiveResource(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto media_type = rp.PopEnum<MediaType>();

    LOG_WARNING(Service_FS, "(STUBBED) called Media type=0x{:08X}", media_type);

    auto resource = archives.GetArchiveResource(media_type);
    if (resource.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(resource.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(*resource);
}

void FS_USER::GetFormatInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_id = rp.PopEnum<FS::ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    std::vector<u8> archivename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);

    const FileSys::Path archive_path(archivename_type, std::move(archivename));

    LOG_DEBUG(Service_FS, "archive_path={}", archive_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    ClientSlot* slot = GetSessionData(ctx.Session());
    auto format_info = archives.GetArchiveFormatInfo(archive_id, archive_path, slot->program_id);
    rb.Push(format_info.Code());
    if (format_info.Failed()) {
        LOG_ERROR(Service_FS, "Failed to retrieve the format info");
        rb.Skip(4, true);
        return;
    }

    rb.Push<u32>(format_info->total_size);
    rb.Push<u32>(format_info->number_directories);
    rb.Push<u32>(format_info->number_files);
    rb.Push<bool>(format_info->duplicate_data != 0);
}

void FS_USER::GetProductInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 process_id = rp.Pop<u32>();

    LOG_DEBUG(Service_FS, "called, process_id={}", process_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(6, 0);

    const auto product_info = GetProductInfo(process_id);
    if (!product_info.has_value()) {
        rb.Push(Result(FileSys::ErrCodes::ArchiveNotMounted, ErrorModule::FS,
                       ErrorSummary::NotFound, ErrorLevel::Status));
        return;
    }

    rb.Push(ResultSuccess);
    rb.PushRaw<ProductInfo>(product_info.value());
}

void FS_USER::GetProgramLaunchInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto process_id = rp.Pop<u32>();

    LOG_DEBUG(Service_FS, "process_id={}", process_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);

    auto program_info_result = GetProgramLaunchInfo(process_id);
    if (program_info_result.Failed()) {
        // Note: In this case, the rest of the parameters are not changed but the command header
        // remains the same.
        rb.Push(program_info_result.Code());
        rb.Skip(4, false);
        return;
    }

    ProgramInfo program_info = program_info_result.Unwrap();

    // Always report the launched program mediatype is SD if the friends module is requesting this
    // information and the media type is game card. Otherwise, friends will append a "romid" field
    // to the NASC request with a cartridge unique identifier. Using a dump of a game card and the
    // game card itself at the same time online is known to have caused issues in the past.
    auto process = ctx.ClientThread()->owner_process.lock();
    if (process && process->codeset->name == "friends" &&
        program_info.media_type == MediaType::GameCard) {
        program_info.media_type = MediaType::SDMC;
    }

    rb.Push(ResultSuccess);
    rb.PushRaw<ProgramInfo>(program_info);
}

void FS_USER::ObsoletedCreateExtSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    MediaType media_type = static_cast<MediaType>(rp.Pop<u8>());
    u32 save_low = rp.Pop<u32>();
    u32 save_high = rp.Pop<u32>();
    u32 icon_size = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    auto icon_buffer = rp.PopMappedBuffer();

    std::vector<u8> icon(icon_size);
    icon_buffer.Read(icon.data(), 0, icon_size);

    FileSys::ArchiveFormatInfo format_info;
    format_info.number_directories = directories;
    format_info.number_files = files;
    format_info.total_size = -1;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.CreateExtSaveData(media_type, 0, save_high, save_low, icon, format_info,
                                       slot->program_id, -1));
    rb.PushMappedBuffer(icon_buffer);

    LOG_DEBUG(Service_FS,
              "called, savedata_high={:08X} savedata_low={:08X} "
              "icon_size={:08X} files={:08X} directories={:08X}",
              save_high, save_low, icon_size, directories, files);
}

void FS_USER::ObsoletedDeleteExtSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    MediaType media_type = static_cast<MediaType>(rp.Pop<u8>());
    u32 save_low = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteExtSaveData(media_type, 0, 0, save_low));

    LOG_DEBUG(Service_FS, "called, save_low={:08X} media_type={:08X}", save_low, media_type);
}

void FS_USER::GetSpecialContentIndex(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const MediaType media_type = static_cast<MediaType>(rp.Pop<u8>());
    const u64 title_id = rp.Pop<u64>();
    const auto type = rp.PopEnum<SpecialContentType>();

    LOG_DEBUG(Service_FS, "called, media_type={:08X} type={:08X}, title_id={:016X}", media_type,
              type, title_id);

    ResultVal<u16> index;
    if (media_type == MediaType::GameCard) {
        index = GetSpecialContentIndexFromGameCard(title_id, type);
    } else {
        index = GetSpecialContentIndexFromTMD(media_type, title_id, type);
    }

    if (index.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.Push(index.Unwrap());
    } else {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(index.Code());
    }
}

void FS_USER::GetNumSeeds(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(FileSys::GetSeedCount());
}

void FS_USER::AddSeed(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 title_id{rp.Pop<u64>()};
    FileSys::Seed::Data seed{rp.PopRaw<FileSys::Seed::Data>()};
    FileSys::AddSeed({title_id, seed, {}});
    IPC::RequestBuilder rb{rp.MakeBuilder(1, 0)};
    rb.Push(ResultSuccess);
}

void FS_USER::ObsoletedSetSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u64 value = rp.Pop<u64>();
    const u32 secure_value_slot = rp.Pop<u32>();
    const u32 unique_id = rp.Pop<u32>();
    const u8 title_variation = rp.Pop<u8>();

    if (!secure_value_backend->BackendIsSlow()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(secure_value_backend->ObsoletedSetSaveDataSecureValue(unique_id, title_variation,
                                                                      secure_value_slot, value));
        return;
    }

    struct AsyncData {
        u64 value;
        u32 secure_value_slot;
        u32 unique_id;
        u8 title_variation;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->value = value;
    async_data->secure_value_slot = secure_value_slot;
    async_data->unique_id = unique_id;
    async_data->title_variation = title_variation;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = secure_value_backend->ObsoletedSetSaveDataSecureValue(
                async_data->unique_id, async_data->title_variation, async_data->secure_value_slot,
                async_data->value);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::ObsoletedGetSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 secure_value_slot = rp.Pop<u32>();
    const u32 unique_id = rp.Pop<u32>();
    const u8 title_variation = rp.Pop<u8>();

    if (!secure_value_backend->BackendIsSlow()) {
        auto res = secure_value_backend->ObsoletedGetSaveDataSecureValue(unique_id, title_variation,
                                                                         secure_value_slot);
        if (res.Failed()) {
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(res.Code());
        } else {
            IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);
            rb.Push(res.Code());
            rb.Push<bool>(std::get<0>(*res)); // indicates if the secure value exists
            rb.Push<u64>(std::get<1>(*res));  // the secure value
        }
        return;
    }

    struct AsyncData {
        u32 secure_value_slot;
        u32 unique_id;
        u8 title_variation;

        ResultVal<std::tuple<bool, u64>> res;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->secure_value_slot = secure_value_slot;
    async_data->unique_id = unique_id;
    async_data->title_variation = title_variation;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = secure_value_backend->ObsoletedGetSaveDataSecureValue(
                async_data->unique_id, async_data->title_variation, async_data->secure_value_slot);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            if (async_data->res.Failed()) {
                IPC::RequestBuilder rb(ctx, 1, 0);
                rb.Push(async_data->res.Code());
            } else {
                IPC::RequestBuilder rb(ctx, 4, 0);
                rb.Push(async_data->res.Code());
                rb.Push<bool>(
                    std::get<0>(*async_data->res));          // indicates if the secure value exists
                rb.Push<u64>(std::get<1>(*async_data->res)); // the secure value
            }
        },
        true);
}

void FS_USER::ControlSecureSave(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto action = rp.Pop<u32>();
    const auto input_size = rp.Pop<u32>();
    const auto output_size = rp.Pop<u32>();
    auto input = rp.PopMappedBuffer();
    auto output = rp.PopMappedBuffer();

    std::vector<u8> in_data(input_size);
    input.Read(in_data.data(), 0, in_data.size());
    std::vector<u8> out_data(output_size);

    Result res = secure_value_backend->ControlSecureSave(action, in_data.data(), in_data.size(),
                                                         out_data.data(), out_data.size());

    if (res.IsSuccess() && output_size != 0) {
        output.Write(out_data.data(), 0, out_data.size());
    }
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(res);
}

void FS_USER::SetThisSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 secure_value_slot = rp.Pop<u32>();
    const u64 value = rp.Pop<u64>();

    if (!secure_value_backend->BackendIsSlow()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(secure_value_backend->SetThisSaveDataSecureValue(secure_value_slot, value));
        return;
    }

    struct AsyncData {
        u64 value;
        u32 secure_value_slot;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->value = value;
    async_data->secure_value_slot = secure_value_slot;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = secure_value_backend->SetThisSaveDataSecureValue(
                async_data->secure_value_slot, async_data->value);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::GetThisSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 secure_value_slot = rp.Pop<u32>();

    if (!secure_value_backend->BackendIsSlow()) {
        auto res = secure_value_backend->GetThisSaveDataSecureValue(secure_value_slot);

        if (res.Failed()) {
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(res.Code());
        } else {
            IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
            rb.Push(res.Code());
            rb.Push<bool>(std::get<0>(*res)); // indicates if the secure value exists
            rb.Push<bool>(std::get<1>(
                *res)); // indicates if the requesting process is a gamecard, overriding the check
            rb.Push<u64>(std::get<2>(*res)); // the secure value
        }
        return;
    }

    struct AsyncData {
        u32 secure_value_slot;

        ResultVal<std::tuple<bool, bool, u64>> res;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->secure_value_slot = secure_value_slot;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res =
                secure_value_backend->GetThisSaveDataSecureValue(async_data->secure_value_slot);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            if (async_data->res.Failed()) {
                IPC::RequestBuilder rb(ctx, 1, 0);
                rb.Push(async_data->res.Code());
            } else {
                IPC::RequestBuilder rb(ctx, 5, 0);
                rb.Push(async_data->res.Code());
                rb.Push<bool>(
                    std::get<0>(*async_data->res)); // indicates if the secure value exists
                rb.Push<bool>(std::get<1>(*async_data->res)); // indicates if the requesting process
                                                              // is a gamecard, overriding the check
                rb.Push<u64>(std::get<2>(*async_data->res));  // the secure value
            }
        },
        true);
}

void FS_USER::SetSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const u32 secure_value_slot = rp.Pop<u32>();
    const u64 value = rp.Pop<u64>();
    const bool flush = rp.Pop<bool>();

    if (!archives.ArchiveIsSlow(archive_handle)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

        rb.Push(archives.SetSaveDataSecureValue(archive_handle, secure_value_slot, value, flush));
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        u64 value;
        u32 secure_value_slot;
        bool flush;

        Result res{0};
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->value = value;
    async_data->secure_value_slot = secure_value_slot;
    async_data->flush = flush;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.SetSaveDataSecureValue(async_data->archive_handle,
                                                              async_data->secure_value_slot,
                                                              async_data->value, async_data->flush);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 1, 0);
            rb.Push(async_data->res);
        },
        true);
}

void FS_USER::GetSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const u32 secure_value_slot = rp.Pop<u32>();

    if (!archives.ArchiveIsSlow(archive_handle)) {
        auto res = archives.GetSaveDataSecureValue(archive_handle, secure_value_slot);
        if (res.Failed()) {
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(res.Code());
        } else {
            IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
            rb.Push(res.Code());
            rb.Push<bool>(std::get<0>(*res)); // indicates if the secure value exists
            rb.Push<bool>(std::get<1>(
                *res)); // indicates if the requesting process is a gamecard, overriding the check
            rb.Push<u64>(std::get<2>(*res)); // the secure value
        }
        return;
    }

    struct AsyncData {
        ArchiveHandle archive_handle;
        u32 secure_value_slot;

        ResultVal<std::tuple<bool, bool, u64>> res;
    };
    auto async_data = std::make_shared<AsyncData>();
    async_data->archive_handle = archive_handle;
    async_data->secure_value_slot = secure_value_slot;

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            async_data->res = archives.GetSaveDataSecureValue(async_data->archive_handle,
                                                              async_data->secure_value_slot);
            return 0;
        },
        [async_data](Kernel::HLERequestContext& ctx) {
            if (async_data->res.Failed()) {
                IPC::RequestBuilder rb(ctx, 1, 0);
                rb.Push(async_data->res.Code());
            } else {
                IPC::RequestBuilder rb(ctx, 5, 0);
                rb.Push(async_data->res.Code());
                rb.Push<bool>(
                    std::get<0>(*async_data->res)); // indicates if the secure value exists
                rb.Push<bool>(std::get<1>(*async_data->res)); // indicates if the requesting process
                                                              // is a gamecard, overriding the check
                rb.Push<u64>(std::get<2>(*async_data->res));  // the secure value
            }
        },
        true);
}

void FS_USER::RegisterProgramInfo(u32 process_id, u64 program_id, const std::string& filepath) {
    const MediaType media_type = GetMediaTypeFromPath(filepath);
    program_info_map.insert_or_assign(process_id, ProgramInfo{program_id, media_type});
    if (media_type == MediaType::GameCard) {
        current_gamecard_path = filepath;
    }
}

std::string FS_USER::GetCurrentGamecardPath() const {
    return current_gamecard_path;
}

void FS_USER::RegisterProductInfo(u32 process_id, const ProductInfo& product_info) {
    product_info_map.insert_or_assign(process_id, product_info);
}

std::optional<FS_USER::ProductInfo> FS_USER::GetProductInfo(u32 process_id) {
    auto it = product_info_map.find(process_id);
    if (it != product_info_map.end()) {
        return it->second;
    } else {
        return {};
    }
}

ResultVal<u16> FS_USER::GetSpecialContentIndexFromGameCard(u64 title_id, SpecialContentType type) {
    // TODO(B3N30) check if on real 3DS NCSD is checked if partition exists

    if (type > SpecialContentType::DLPChild) {
        // Maybe type 4 is New 3DS update/partition 6 but this needs more research
        // TODO(B3N30): Find correct result code
        return ResultUnknown;
    }

    switch (type) {
    case SpecialContentType::Update:
        return static_cast<u16>(NCSDContentIndex::Update);
    case SpecialContentType::Manual:
        return static_cast<u16>(NCSDContentIndex::Manual);
    case SpecialContentType::DLPChild:
        return static_cast<u16>(NCSDContentIndex::DLP);
    default:
        UNREACHABLE();
    }
}

ResultVal<u16> FS_USER::GetSpecialContentIndexFromTMD(MediaType media_type, u64 title_id,
                                                      SpecialContentType type) {
    if (type > SpecialContentType::DLPChild) {
        // TODO(B3N30): Find correct result code
        return ResultUnknown;
    }

    std::string tmd_path = AM::GetTitleMetadataPath(media_type, title_id);

    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) != Loader::ResultStatus::Success || type == SpecialContentType::Update) {
        // TODO(B3N30): Find correct result code
        return ResultUnknown;
    }

    // TODO(B3N30): Does real 3DS check if content exists in TMD?

    switch (type) {
    case SpecialContentType::Manual:
        return static_cast<u16>(FileSys::TMDContentIndex::Manual);
    case SpecialContentType::DLPChild:
        return static_cast<u16>(FileSys::TMDContentIndex::DLP);
    default:
        ASSERT(false);
    }

    return ResultUnknown;
}

FS_USER::FS_USER(Core::System& system)
    : ServiceFramework("fs:USER", 30), system(system), archives(system.ArchiveManager()) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Dummy1"},
        {0x0401, nullptr, "Control"},
        {0x0801, &FS_USER::Initialize, "Initialize"},
        {0x0802, &FS_USER::OpenFile, "OpenFile"},
        {0x0803, &FS_USER::OpenFileDirectly, "OpenFileDirectly"},
        {0x0804, &FS_USER::DeleteFile, "DeleteFile"},
        {0x0805, &FS_USER::RenameFile, "RenameFile"},
        {0x0806, &FS_USER::DeleteDirectory, "DeleteDirectory"},
        {0x0807, &FS_USER::DeleteDirectoryRecursively, "DeleteDirectoryRecursively"},
        {0x0808, &FS_USER::CreateFile, "CreateFile"},
        {0x0809, &FS_USER::CreateDirectory, "CreateDirectory"},
        {0x080A, &FS_USER::RenameDirectory, "RenameDirectory"},
        {0x080B, &FS_USER::OpenDirectory, "OpenDirectory"},
        {0x080C, &FS_USER::OpenArchive, "OpenArchive"},
        {0x080D, &FS_USER::ControlArchive, "ControlArchive"},
        {0x080E, &FS_USER::CloseArchive, "CloseArchive"},
        {0x080F, &FS_USER::FormatThisUserSaveData, "FormatThisUserSaveData"},
        {0x0810, &FS_USER::CreateLegacySystemSaveData, "CreateLegacySystemSaveData"},
        {0x0811, nullptr, "DeleteSystemSaveData"},
        {0x0812, &FS_USER::GetFreeBytes, "GetFreeBytes"},
        {0x0813, nullptr, "GetCardType"},
        {0x0814, &FS_USER::GetSdmcArchiveResource, "GetSdmcArchiveResource"},
        {0x0815, &FS_USER::GetNandArchiveResource, "GetNandArchiveResource"},
        {0x0816, nullptr, "GetSdmcFatfsError"},
        {0x0817, &FS_USER::IsSdmcDetected, "IsSdmcDetected"},
        {0x0818, &FS_USER::IsSdmcWriteable, "IsSdmcWritable"},
        {0x0819, nullptr, "GetSdmcCid"},
        {0x081A, nullptr, "GetNandCid"},
        {0x081B, nullptr, "GetSdmcSpeedInfo"},
        {0x081C, nullptr, "GetNandSpeedInfo"},
        {0x081D, nullptr, "GetSdmcLog"},
        {0x081E, nullptr, "GetNandLog"},
        {0x081F, nullptr, "ClearSdmcLog"},
        {0x0820, nullptr, "ClearNandLog"},
        {0x0821, &FS_USER::CardSlotIsInserted, "CardSlotIsInserted"},
        {0x0822, nullptr, "CardSlotPowerOn"},
        {0x0823, nullptr, "CardSlotPowerOff"},
        {0x0824, nullptr, "CardSlotGetCardIFPowerStatus"},
        {0x0825, nullptr, "CardNorDirectCommand"},
        {0x0826, nullptr, "CardNorDirectCommandWithAddress"},
        {0x0827, nullptr, "CardNorDirectRead"},
        {0x0828, nullptr, "CardNorDirectReadWithAddress"},
        {0x0829, nullptr, "CardNorDirectWrite"},
        {0x082A, nullptr, "CardNorDirectWriteWithAddress"},
        {0x082B, nullptr, "CardNorDirectRead_4xIO"},
        {0x082C, nullptr, "CardNorDirectCpuWriteWithoutVerify"},
        {0x082D, nullptr, "CardNorDirectSectorEraseWithoutVerify"},
        {0x082E, &FS_USER::GetProductInfo, "GetProductInfo"},
        {0x082F, &FS_USER::GetProgramLaunchInfo, "GetProgramLaunchInfo"},
        {0x0830, &FS_USER::ObsoletedCreateExtSaveData, "Obsoleted_3_0_CreateExtSaveData"},
        {0x0831, nullptr, "CreateSharedExtSaveData"},
        {0x0832, nullptr, "ReadExtSaveDataIcon"},
        {0x0833, nullptr, "EnumerateExtSaveData"},
        {0x0834, nullptr, "EnumerateSharedExtSaveData"},
        {0x0835, &FS_USER::ObsoletedDeleteExtSaveData, "Obsoleted_3_0_DeleteExtSaveData"},
        {0x0836, nullptr, "DeleteSharedExtSaveData"},
        {0x0837, nullptr, "SetCardSpiBaudRate"},
        {0x0838, nullptr, "SetCardSpiBusMode"},
        {0x0839, nullptr, "SendInitializeInfoTo9"},
        {0x083A, &FS_USER::GetSpecialContentIndex, "GetSpecialContentIndex"},
        {0x083B, nullptr, "GetLegacyRomHeader"},
        {0x083C, nullptr, "GetLegacyBannerData"},
        {0x083D, nullptr, "CheckAuthorityToAccessExtSaveData"},
        {0x083E, nullptr, "QueryTotalQuotaSize"},
        {0x083F, nullptr, "GetExtDataBlockSize"},
        {0x0840, nullptr, "AbnegateAccessRight"},
        {0x0841, nullptr, "DeleteSdmcRoot"},
        {0x0842, nullptr, "DeleteAllExtSaveDataOnNand"},
        {0x0843, nullptr, "InitializeCtrFileSystem"},
        {0x0844, nullptr, "CreateSeed"},
        {0x0845, &FS_USER::GetFormatInfo, "GetFormatInfo"},
        {0x0846, nullptr, "GetLegacyRomHeader2"},
        {0x0847, nullptr, "FormatCtrCardUserSaveData"},
        {0x0848, nullptr, "GetSdmcCtrRootPath"},
        {0x0849, &FS_USER::GetArchiveResource, "GetArchiveResource"},
        {0x084A, nullptr, "ExportIntegrityVerificationSeed"},
        {0x084B, nullptr, "ImportIntegrityVerificationSeed"},
        {0x084C, &FS_USER::FormatSaveData, "FormatSaveData"},
        {0x084D, nullptr, "GetLegacySubBannerData"},
        {0x084E, nullptr, "UpdateSha256Context"},
        {0x084F, nullptr, "ReadSpecialFile"},
        {0x0850, nullptr, "GetSpecialFileSize"},
        {0x0851, &FS_USER::CreateExtSaveData, "CreateExtSaveData"},
        {0x0852, &FS_USER::DeleteExtSaveData, "DeleteExtSaveData"},
        {0x0853, nullptr, "ReadExtSaveDataIcon"},
        {0x0854, nullptr, "GetExtDataBlockSize"},
        {0x0855, nullptr, "EnumerateExtSaveData"},
        {0x0856, &FS_USER::CreateSystemSaveData, "CreateSystemSaveData"},
        {0x0857, &FS_USER::DeleteSystemSaveData, "DeleteSystemSaveData"},
        {0x0858, nullptr, "StartDeviceMoveAsSource"},
        {0x0859, nullptr, "StartDeviceMoveAsDestination"},
        {0x085A, nullptr, "SetArchivePriority"},
        {0x085B, nullptr, "GetArchivePriority"},
        {0x085C, nullptr, "SetCtrCardLatencyParameter"},
        {0x085D, nullptr, "SetFsCompatibilityInfo"},
        {0x085E, nullptr, "ResetCardCompatibilityParameter"},
        {0x085F, nullptr, "SwitchCleanupInvalidSaveData"},
        {0x0860, nullptr, "EnumerateSystemSaveData"},
        {0x0861, &FS_USER::InitializeWithSdkVersion, "InitializeWithSdkVersion"},
        {0x0862, &FS_USER::SetPriority, "SetPriority"},
        {0x0863, &FS_USER::GetPriority, "GetPriority"},
        {0x0864, nullptr, "GetNandInfo"},
        {0x0865, &FS_USER::ObsoletedSetSaveDataSecureValue, "SetSaveDataSecureValue"},
        {0x0866, &FS_USER::ObsoletedGetSaveDataSecureValue, "GetSaveDataSecureValue"},
        {0x0867, &FS_USER::ControlSecureSave, "ControlSecureSave"},
        {0x0868, nullptr, "GetMediaType"},
        {0x0869, nullptr, "GetNandEraseCount"},
        {0x086A, nullptr, "ReadNandReport"},
        {0x086E, &FS_USER::SetThisSaveDataSecureValue, "SetThisSaveDataSecureValue" },
        {0x086F, &FS_USER::GetThisSaveDataSecureValue, "GetThisSaveDataSecureValue" },
        {0x0875, &FS_USER::SetSaveDataSecureValue, "SetSaveDataSecureValue" },
        {0x0876, &FS_USER::GetSaveDataSecureValue, "GetSaveDataSecureValue" },
        {0x087A, &FS_USER::AddSeed, "AddSeed"},
        {0x087D, &FS_USER::GetNumSeeds, "GetNumSeeds"},
        {0x0886, nullptr, "CheckUpdatedDat"},
        // clang-format on
    };
    RegisterHandlers(functions);
    secure_value_backend = std::make_shared<FileSys::DefaultSecureValueBackend>();
}
template <class Archive>
void Service::FS::FS_USER::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & priority;
    ar & secure_value_backend;
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<FS_USER>(system)->InstallAsService(service_manager);
}
} // namespace Service::FS
