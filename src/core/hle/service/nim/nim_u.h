// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NIM {

class NIM_U final : public ServiceFramework<NIM_U> {
public:
    explicit NIM_U(Core::System& system);
    ~NIM_U();

private:
    /**
     * NIM::StartNetworkUpdate service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void StartNetworkUpdate(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetProgress service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-11 : SystemUpdateProgress structure for the foreground system update
     *      12: ?
     *      13: ?
     */
    void GetProgress(Kernel::HLERequestContext& ctx);

    /**
     * NIM::Cancel service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Cancel(Kernel::HLERequestContext& ctx);

    /**
     * NIM::CommitSystemTitles service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CommitSystemTitles(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetBackgroundEventForMenu service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy handle IPC header
     *      3 : System update ready event handle for home menu
     */
    void GetBackgroundEventForMenu(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetBackgroundEventForNews service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy handle IPC header
     *      3 : System update ready event handle for news module
     */
    void GetBackgroundEventForNews(Kernel::HLERequestContext& ctx);

    /**
     * NIM::FormatSaveData service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FormatSaveData(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetCustomerSupportCode service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Customer support code for the last system update error
     */
    void GetCustomerSupportCode(Kernel::HLERequestContext& ctx);

    /**
     * NIM::IsCommittableAllSystemTitles service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Boolean indicating whether system titles are ready to commit
     */
    void IsCommittableAllSystemTitles(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetBackgroundProgress service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-11 : SystemUpdateProgress structure for the background system update
     *      12: ?
     *      13: ?
     */
    void GetBackgroundProgress(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetSavedHash service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-10 : NUL-terminated saved system update hash
     */
    void GetSavedHash(Kernel::HLERequestContext& ctx);

    /**
     * NIM::UnregisterTask service function
     *  Inputs:
     *      1-2 : Title ID
     *      3 : Process ID IPC Header
     *      4 : Process ID (Auto-filled by kernel)
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void UnregisterTask(Kernel::HLERequestContext& ctx);

    /**
     * NIM::IsRegistered service function
     *  Inputs:
     *      1-2 : Title ID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Boolean indicating whether a download task is registered for the title ID
     */
    void IsRegistered(Kernel::HLERequestContext& ctx);

    /**
     * NIM::FindTaskInfo service function
     *  Inputs:
     *      1-2 : Title ID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Static Buffer IPC Header (ID = 0, Size = 0x120)
     *      3 : BackgroundTitleDownloadTaskInfo structure pointer
     */
    void FindTaskInfo(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetTaskInfos service function
     *  Inputs:
     *      1 : Maximum Number of BackgroundTitleDownloadTaskInfos
     *      2 : Mapped Output Buffer IPC Header
     *      3 : BackgroundTitleDownloadTaskInfos Output Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Number of BackgroundTitleDownloadTaskInfos Read
     *      3 : Mapped Output Buffer IPC Header
     *      4 : BackgroundTitleDownloadTaskInfos Output Buffer Pointer
     */
    void GetTaskInfos(Kernel::HLERequestContext& ctx);

    /**
     * NIM::DeleteUnmanagedContexts service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteUnmanagedContexts(Kernel::HLERequestContext& ctx);

    /**
     * NIM::UpdateAutoTitleDownloadTasksAsync service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy Handle IPC Header
     *      3 : Event handle signaled when the operation completes
     */
    void UpdateAutoTitleDownloadTasksAsync(Kernel::HLERequestContext& ctx);

    /**
     * NIM::StartPendingAutoTitleDownloadTasksAsync service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy Handle IPC Header
     *      3 : Event handle signaled when the operation completes
     */
    void StartPendingAutoTitleDownloadTasksAsync(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetAsyncResult service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Async operation result code
     *      3 : Async operation customer support code
     */
    void GetAsyncResult(Kernel::HLERequestContext& ctx);

    /**
     * NIM::CancelAsyncCall service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CancelAsyncCall(Kernel::HLERequestContext& ctx);

    /**
     * NIM::IsPendingAutoTitleDownloadTasks service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Boolean indicating whether there are auto title downloads ready to start
     */
    void IsPendingAutoTitleDownloadTasks(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetNumAutoTitleDownloadTasks service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Number of auto title download tasks
     */
    void GetNumAutoTitleDownloadTasks(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetAutoTitleDownloadTasks service function
     *  Inputs:
     *      1 : Maximum number of AutoTitleDownloadTaskInfos
     *      2 : Mapped Output Buffer IPC Header
     *      3 : AutoTitleDownloadTaskInfos Output Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Number of AutoTitleDownloadTaskInfos Read
     *      3 : Mapped Output Buffer IPC Header
     *      4 : AutoTitleDownloadTaskInfos Output Buffer Pointer
     */
    void GetAutoTitleDownloadTaskInfos(Kernel::HLERequestContext& ctx);

    /**
     * NIM::CancelAutoTitleDownloadTask service function
     *  Inputs:
     *      1-2 : Auto Title Download Task ID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CancelAutoTitleDownloadTask(Kernel::HLERequestContext& ctx);

    /**
     * NIM::SetAutoDbgDat service function
     *  Inputs:
     *      1 : Mapped Input Buffer IPC Header
     *      2 : AutoDbgDat Input Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Mapped Input Buffer IPC Header
     *      3 : AutoDbgDat Input Buffer Pointer
     */
    void SetAutoDbgDat(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetAutoDbgDat service function
     *  Inputs:
     *      1 : Mapped Output Buffer IPC Header
     *      2 : AutoDbgDat Output Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Mapped Output Buffer IPC Header
     *      3 : AutoDbgDat Output Buffer Pointer
     */
    void GetAutoDbgDat(Kernel::HLERequestContext& ctx);

    /**
     * NIM::SetDbgTasks service function
     *  Inputs:
     *      1 : Number of AutoTitleDownloadTaskInfos
     *      2 : Mapped Input Buffer IPC Header
     *      3 : AutoTitleDownloadTaskInfos Input Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      3 : Mapped Input Buffer IPC Header
     *      4 : AutoTitleDownloadTaskInfos Input Buffer Pointer
     */
    void SetDbgTasks(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetDbgTasks service function
     *  Inputs:
     *      1 : Maximum number of AutoTitleDownloadTaskInfos
     *      2 : Mapped Output Buffer IPC Header
     *      3 : AutoTitleDownloadTaskInfos Output Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Number of AutoTitleDownloadTaskInfos Read
     *      3 : Mapped Output Buffer IPC Header
     *      4 : AutoTitleDownloadTaskInfos Output Buffer Pointer
     */
    void GetDbgTasks(Kernel::HLERequestContext& ctx);

    /**
     * NIM::DeleteDbgData service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteDbgData(Kernel::HLERequestContext& ctx);

    /**
     * NIM::SetTslXml service function
     *  Inputs:
     *      1 : Buffer Size
     *      2 : Mapped Input Buffer IPC Header
     *      3 : XML Input Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Mapped Input Buffer IPC Header
     *      3 : XML Input Buffer Pointer
     */
    void SetTslXml(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetTslXmlSize service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3 : XML Size
     */
    void GetTslXmlSize(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetTslXml service function
     *  Inputs:
     *      1 : Buffer Capacity
     *      2 : Mapped Output Buffer IPC Header
     *      3 : XML Output Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Mapped Output Buffer IPC Header
     *      3 : XML Output Buffer Pointer
     */
    void GetTslXml(Kernel::HLERequestContext& ctx);

    /**
     * NIM::DeleteTslXml service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteTslXml(Kernel::HLERequestContext& ctx);

    /**
     * NIM::SetDtlXml service function
     *  Inputs:
     *      1 : Buffer Size
     *      2 : Mapped Input Buffer IPC Header
     *      3 : XML Input Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Mapped Input Buffer IPC Header
     *      3 : XML Input Buffer Pointer
     */
    void SetDtlXml(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetDtlXmlSize service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3 : XML Size
     */
    void GetDtlXmlSize(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetDtlXml service function
     *  Inputs:
     *      1 : Buffer Capacity
     *      2 : Mapped Output Buffer IPC Header
     *      3 : XML Output Buffer Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Mapped Output Buffer IPC Header
     *      3 : XML Output Buffer Pointer
     */
    void GetDtlXml(Kernel::HLERequestContext& ctx);

    /**
     * NIM::UpdateAccountStatus service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Result of the actual operation
     *      3 : Customer support code of the actual operation
     */
    void UpdateAccountStatus(Kernel::HLERequestContext& ctx);

    /**
     * NIM::StartTitleDownload service function
     *  Inputs:
     *      1-6 : TitleDownloadConfig structure
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void StartTitleDownload(Kernel::HLERequestContext& ctx);

    /**
     * NIM::StopDownload service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void StopTitleDownload(Kernel::HLERequestContext& ctx);

    /**
     * NIM::GetTitleDownloadProgress service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-7 : TitleDownloadProgress structure
     *      8: ?
     *      9: ?
     */
    void GetTitleDownloadProgress(Kernel::HLERequestContext& ctx);

    /**
     * NIM::RegisterTask service function
     *  Inputs:
     *      1-6 : TitleDownloadConfig structure
     *      7: ?
     *      8: ?
     *      9: ?
     *      10: Process ID IPC Header
     *      11: Process ID (Auto-filled by Kernel)
     *      12: Static Buffer IPC Header (ID = 0, Size = 0x90)
     *      13: Title Name UTF-16 String Pointer
     *      14: Static Buffer IPC Header (ID = 1, Size = 0x48)
     *      15: Developer Name UTF-16 String Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void RegisterTask(Kernel::HLERequestContext& ctx);

    /**
     * NIM::IsSystemUpdateAvailable service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Result code from the actual operation
     *      3 : Customer support code from the actual operation
     *      4 : Boolean indicating whether a system update is available
     */
    void IsSystemUpdateAvailable(Kernel::HLERequestContext& ctx);

    /**
     * NIM::Unknown2B service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Unknown2B(Kernel::HLERequestContext& ctx);

    /**
     * NIM::UpdateTickets service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Result code from the actual operation
     *      3 : Customer support code from the actual operation
     */
    void UpdateTickets(Kernel::HLERequestContext& ctx);

    /**
     * NIM::DownloadTitleSeedAsync service function
     *  Inputs:
     *      1-2 : Title ID
     *      3: u16 Country Code
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy Handle IPC Header
     *      3 : Event handle signaled when the operation completes
     */
    void DownloadTitleSeedAsync(Kernel::HLERequestContext& ctx);

    /**
     * NIM::DownloadMissingTitleSeedsAsync service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy Handle IPC Header
     *      3 : Event handle signaled when the operation completes
     */
    void DownloadMissingTitleSeedsAsync(Kernel::HLERequestContext& ctx);

    std::shared_ptr<Kernel::Event> nim_system_update_event_for_menu;
    std::shared_ptr<Kernel::Event> nim_system_update_event_for_news;
    std::shared_ptr<Kernel::Event> nim_async_completion_event;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar & nim_system_update_event_for_menu;
        ar & nim_system_update_event_for_news;
        ar & nim_async_completion_event;
    }
    friend class boost::serialization::access;
};

} // namespace Service::NIM

SERVICE_CONSTRUCT(Service::NIM::NIM_U)
BOOST_CLASS_EXPORT_KEY(Service::NIM::NIM_U)
