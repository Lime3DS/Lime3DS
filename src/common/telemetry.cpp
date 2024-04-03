// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include "common/arch.h"
#include "common/assert.h"
#include "common/scm_rev.h"
#include "common/telemetry.h"

#if CITRA_ARCH(x86_64)
#include "common/x64/cpu_detect.h"
#endif

namespace Common::Telemetry {

void FieldCollection::Accept(VisitorInterface& visitor) const {
    for (const auto& field : fields) {
        field.second->Accept(visitor);
    }
}

void FieldCollection::AddField(std::unique_ptr<FieldInterface> field) {
    fields[field->GetName()] = std::move(field);
}

template <class T>
void Field<T>::Accept(VisitorInterface& visitor) const {
    visitor.Visit(*this);
}

template class Field<bool>;
template class Field<double>;
template class Field<float>;
template class Field<u8>;
template class Field<u16>;
template class Field<u32>;
template class Field<u64>;
template class Field<s8>;
template class Field<s16>;
template class Field<s32>;
template class Field<s64>;
template class Field<std::string>;
template class Field<const char*>;
template class Field<std::chrono::microseconds>;

void AppendBuildInfo(FieldCollection& fc) {
    const bool is_git_dirty{std::strstr(Common::g_scm_desc, "dirty") != nullptr};
    fc.AddField(FieldType::App, "Git_IsDirty", is_git_dirty);
    fc.AddField(FieldType::App, "Git_Branch", Common::g_scm_branch);
    fc.AddField(FieldType::App, "Git_Revision", Common::g_scm_rev);
    fc.AddField(FieldType::App, "BuildDate", Common::g_build_date);
    fc.AddField(FieldType::App, "BuildName", Common::g_build_name);
}

void AppendCPUInfo(FieldCollection& fc) {
#if CITRA_ARCH(x86_64)
    fc.AddField(FieldType::UserSystem, "CPU_Model", Common::GetCPUCaps().cpu_string);
    fc.AddField(FieldType::UserSystem, "CPU_BrandString", Common::GetCPUCaps().brand_string);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_AES", Common::GetCPUCaps().aes);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_AVX", Common::GetCPUCaps().avx);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_AVX2", Common::GetCPUCaps().avx2);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_AVX512", Common::GetCPUCaps().avx512);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_BMI1", Common::GetCPUCaps().bmi1);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_BMI2", Common::GetCPUCaps().bmi2);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_FMA", Common::GetCPUCaps().fma);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_FMA4", Common::GetCPUCaps().fma4);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_SSE", Common::GetCPUCaps().sse);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_SSE2", Common::GetCPUCaps().sse2);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_SSE3", Common::GetCPUCaps().sse3);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_SSSE3", Common::GetCPUCaps().ssse3);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_SSE41", Common::GetCPUCaps().sse4_1);
    fc.AddField(FieldType::UserSystem, "CPU_Extension_x64_SSE42", Common::GetCPUCaps().sse4_2);
#else
    fc.AddField(FieldType::UserSystem, "CPU_Model", "Other");
#endif
}

void AppendOSInfo(FieldCollection& fc) {
#ifdef __APPLE__
    fc.AddField(FieldType::UserSystem, "OsPlatform", "Apple");
#elif defined(_WIN32)
    fc.AddField(FieldType::UserSystem, "OsPlatform", "Windows");
#elif defined(__linux__) || defined(linux) || defined(__linux)
    fc.AddField(FieldType::UserSystem, "OsPlatform", "Linux");
#else
    fc.AddField(FieldType::UserSystem, "OsPlatform", "Unknown");
#endif
}

} // namespace Common::Telemetry
