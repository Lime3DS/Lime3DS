// Copyright 2017-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "common/string_util.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "jni/android_common/android_common.h"
#include "jni/id_cache.h"

namespace {

static constexpr u64 UPDATE_TID_HIGH = 0x0004000e00000000;

struct GameInfoData {
    Loader::SMDH smdh;
    u64 title_id = 0;
    bool loaded = false;
    bool is_encrypted = false;
    std::string file_type = "";
    bool is_insertable = false;
};

GameInfoData* GetNewGameInfoData(const std::string& path) {
    std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(path);
    u64 program_id = 0;
    bool is_encrypted = false;
    Loader::ResultStatus result{};
    if (loader) {
        result = loader->ReadProgramId(program_id);
        if (result == Loader::ResultStatus::ErrorNotImplemented) {
            // This can happen for 3DSX and ELF files.
            program_id = 0;
            result = Loader::ResultStatus::Success;
        }
    }

    if (!loader || result != Loader::ResultStatus::Success) {
        GameInfoData* gid = new GameInfoData();
        memset(&gid->smdh, 0, sizeof(Loader::SMDH));
        return gid;
    }

    std::vector<u8> smdh = [program_id, &loader, &is_encrypted]() -> std::vector<u8> {
        std::vector<u8> original_smdh;
        auto result = loader->ReadIcon(original_smdh);
        if (result != Loader::ResultStatus::Success) {
            is_encrypted = result == Loader::ResultStatus::ErrorEncrypted;
            return {};
        }

        if (program_id < 0x00040000'00000000 || program_id > 0x00040000'FFFFFFFF)
            return original_smdh;

        u64 update_tid = (program_id & 0xFFFFFFFFULL) | UPDATE_TID_HIGH;
        std::string update_path =
            Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC, update_tid);

        if (!FileUtil::Exists(update_path))
            return original_smdh;

        std::unique_ptr<Loader::AppLoader> update_loader = Loader::GetLoader(update_path);

        if (!update_loader)
            return original_smdh;

        std::vector<u8> update_smdh;
        result = update_loader->ReadIcon(update_smdh);
        if (result != Loader::ResultStatus::Success) {
            is_encrypted = result == Loader::ResultStatus::ErrorEncrypted;
            return original_smdh;
        }
        return update_smdh;
    }();

    GameInfoData* gid = new GameInfoData();
    if (smdh.empty()) {
        std::memset(&gid->smdh, 0, sizeof(Loader::SMDH));
    } else {
        std::memcpy(&gid->smdh, smdh.data(), smdh.size());
    }
    gid->loaded = true;
    gid->is_encrypted = is_encrypted;
    gid->title_id = program_id;
    gid->file_type = Loader::GetFileTypeString(loader->GetFileType(), loader->IsFileCompressed());
    gid->is_insertable = loader->GetFileType() == Loader::FileType::CCI;

    return gid;
}

} // namespace

extern "C" {

static GameInfoData* GetPointer(JNIEnv* env, jobject obj) {
    return reinterpret_cast<GameInfoData*>(env->GetLongField(obj, IDCache::GetGameInfoPointer()));
}

JNIEXPORT jlong JNICALL Java_org_citra_citra_1emu_model_GameInfo_initialize(JNIEnv* env, jclass,
                                                                            jstring j_path) {
    GameInfoData* game_info_data = GetNewGameInfoData(GetJString(env, j_path));
    return reinterpret_cast<jlong>(game_info_data);
}

JNIEXPORT jboolean JNICALL Java_org_citra_citra_1emu_model_GameInfo_isValid(JNIEnv* env,
                                                                            jobject obj) {
    return GetPointer(env, obj)->loaded;
}

JNIEXPORT jboolean JNICALL Java_org_citra_citra_1emu_model_GameInfo_isEncrypted(JNIEnv* env,
                                                                                jobject obj) {
    return GetPointer(env, obj)->is_encrypted;
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_model_GameInfo_finalize(JNIEnv* env, jobject obj) {
    delete GetPointer(env, obj);
}

jstring Java_org_citra_citra_1emu_model_GameInfo_getTitle(JNIEnv* env, jobject obj) {
    Loader::SMDH* smdh = &GetPointer(env, obj)->smdh;
    if (!smdh->IsValid()) {
        return ToJString(env, "");
    }

    Loader::SMDH::TitleLanguage language = Loader::SMDH::TitleLanguage::English;

    // Get the title from SMDH in UTF-16 format
    std::u16string title{reinterpret_cast<char16_t*>(
        smdh->titles[static_cast<std::size_t>(language)].long_title.data())};

    return ToJString(env, Common::UTF16ToUTF8(title).data());
}

jstring Java_org_citra_citra_1emu_model_GameInfo_getCompany(JNIEnv* env, jobject obj) {
    Loader::SMDH* smdh = &GetPointer(env, obj)->smdh;
    if (!smdh->IsValid()) {
        return ToJString(env, "");
    }

    Loader::SMDH::TitleLanguage language = Loader::SMDH::TitleLanguage::English;

    // Get the Publisher's name from SMDH in UTF-16 format
    char16_t* publisher;
    publisher = reinterpret_cast<char16_t*>(
        smdh->titles[static_cast<std::size_t>(language)].publisher.data());

    return ToJString(env, Common::UTF16ToUTF8(publisher).data());
}

jlong Java_org_citra_citra_1emu_model_GameInfo_getTitleID(JNIEnv* env, jobject obj) {
    return static_cast<jlong>(GetPointer(env, obj)->title_id);
}

jstring Java_org_citra_citra_1emu_model_GameInfo_getRegions(JNIEnv* env, jobject obj) {
    Loader::SMDH* smdh = &GetPointer(env, obj)->smdh;
    if (!smdh->IsValid()) {
        return ToJString(env, "");
    }

    using GameRegion = Loader::SMDH::GameRegion;
    static const std::map<GameRegion, const char*> regions_map = {
        {GameRegion::Japan, "Japan"},   {GameRegion::NorthAmerica, "North America"},
        {GameRegion::Europe, "Europe"}, {GameRegion::Australia, "Australia"},
        {GameRegion::China, "China"},   {GameRegion::Korea, "Korea"},
        {GameRegion::Taiwan, "Taiwan"}};
    std::vector<GameRegion> regions = smdh->GetRegions();

    if (regions.empty()) {
        return ToJString(env, "Invalid region");
    }

    const bool region_free =
        std::all_of(regions_map.begin(), regions_map.end(), [&regions](const auto& it) {
            return std::find(regions.begin(), regions.end(), it.first) != regions.end();
        });

    if (region_free) {
        return ToJString(env, "Region free");
    }

    const std::string separator = "|";
    std::string result = regions_map.at(regions.front());
    for (auto region = ++regions.begin(); region != regions.end(); ++region) {
        result += separator + regions_map.at(*region);
    }

    return ToJString(env, result);
}

jintArray Java_org_citra_citra_1emu_model_GameInfo_getIcon(JNIEnv* env, jobject obj) {
    Loader::SMDH* smdh = &GetPointer(env, obj)->smdh;
    if (!smdh->IsValid()) {
        return nullptr;
    }

    // Always get a 48x48(large) icon
    std::vector<u16> icon_data = smdh->GetIcon(true);
    if (icon_data.empty()) {
        return nullptr;
    }

    jintArray icon = env->NewIntArray(static_cast<jsize>(icon_data.size() / 2));
    env->SetIntArrayRegion(icon, 0, env->GetArrayLength(icon),
                           reinterpret_cast<jint*>(icon_data.data()));

    return icon;
}

jboolean Java_org_citra_citra_1emu_model_GameInfo_isSystemTitle(JNIEnv* env, jobject obj) {
    return ((GetPointer(env, obj)->title_id >> 32) & 0xFFFFFFFF) == 0x00040010;
}

jboolean Java_org_citra_citra_1emu_model_GameInfo_getIsVisibleSystemTitle(JNIEnv* env,
                                                                          jobject obj) {
    Loader::SMDH* smdh = &GetPointer(env, obj)->smdh;
    if (!smdh->IsValid()) {
        return false;
    }

    return smdh->flags.visible;
}

jstring Java_org_citra_citra_1emu_model_GameInfo_getFileType(JNIEnv* env, jobject obj) {
    std::string& file_type = GetPointer(env, obj)->file_type;

    return ToJString(env, file_type);
}
jboolean Java_org_citra_citra_1emu_model_GameInfo_getIsInsertable(JNIEnv* env, jobject obj) {
    return GetPointer(env, obj)->is_insertable;
}
}
