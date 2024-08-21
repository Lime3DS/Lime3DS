// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/ldr_ro/cro_helper.h"

namespace Service::LDR {

static const Result ERROR_BUFFER_TOO_SMALL = // 0xE0E12C1F
    Result(static_cast<ErrorDescription>(31), ErrorModule::RO, ErrorSummary::InvalidArgument,
           ErrorLevel::Usage);

static constexpr Result CROFormatError(u32 description) {
    return Result(static_cast<ErrorDescription>(description), ErrorModule::RO,
                  ErrorSummary::WrongArgument, ErrorLevel::Permanent);
}

const std::array<int, 17> CROHelper::ENTRY_SIZE{{
    1, // code
    1, // data
    1, // module name
    sizeof(SegmentEntry),
    sizeof(ExportNamedSymbolEntry),
    sizeof(ExportIndexedSymbolEntry),
    1, // export strings
    sizeof(ExportTreeEntry),
    sizeof(ImportModuleEntry),
    sizeof(ExternalRelocationEntry),
    sizeof(ImportNamedSymbolEntry),
    sizeof(ImportIndexedSymbolEntry),
    sizeof(ImportAnonymousSymbolEntry),
    1, // import strings
    sizeof(StaticAnonymousSymbolEntry),
    sizeof(InternalRelocationEntry),
    sizeof(StaticRelocationEntry),
}};

const std::array<CROHelper::HeaderField, 4> CROHelper::FIX_BARRIERS{{
    Fix0Barrier,
    Fix1Barrier,
    Fix2Barrier,
    Fix3Barrier,
}};

VAddr CROHelper::SegmentTagToAddress(SegmentTag segment_tag) const {
    u32 segment_num = GetField(SegmentNum);

    if (segment_tag.segment_index >= segment_num)
        return 0;

    SegmentEntry entry;
    GetEntry(system.Memory(), segment_tag.segment_index, entry);

    if (segment_tag.offset_into_segment >= entry.size)
        return 0;

    return entry.offset + segment_tag.offset_into_segment;
}

Result CROHelper::ApplyRelocation(VAddr target_address, RelocationType relocation_type, u32 addend,
                                  u32 symbol_address, u32 target_future_address) {

    switch (relocation_type) {
    case RelocationType::Nothing:
        break;
    case RelocationType::AbsoluteAddress:
    case RelocationType::AbsoluteAddress2:
        system.Memory().Write32(target_address, symbol_address + addend);
        system.InvalidateCacheRange(target_address, sizeof(u32));
        break;
    case RelocationType::RelativeAddress:
        system.Memory().Write32(target_address, symbol_address + addend - target_future_address);
        system.InvalidateCacheRange(target_address, sizeof(u32));
        break;
    case RelocationType::ThumbBranch:
    case RelocationType::ArmBranch:
    case RelocationType::ModifyArmBranch:
    case RelocationType::AlignedRelativeAddress:
        // TODO(wwylele): implement other types
        UNIMPLEMENTED();
        break;
    default:
        return CROFormatError(0x22);
    }
    return ResultSuccess;
}

Result CROHelper::ClearRelocation(VAddr target_address, RelocationType relocation_type) {
    switch (relocation_type) {
    case RelocationType::Nothing:
        break;
    case RelocationType::AbsoluteAddress:
    case RelocationType::AbsoluteAddress2:
    case RelocationType::RelativeAddress:
        system.Memory().Write32(target_address, 0);
        system.InvalidateCacheRange(target_address, sizeof(u32));
        break;
    case RelocationType::ThumbBranch:
    case RelocationType::ArmBranch:
    case RelocationType::ModifyArmBranch:
    case RelocationType::AlignedRelativeAddress:
        // TODO(wwylele): implement other types
        UNIMPLEMENTED();
        break;
    default:
        return CROFormatError(0x22);
    }
    return ResultSuccess;
}

Result CROHelper::ApplyRelocationBatch(VAddr batch, u32 symbol_address, bool reset) {
    if (symbol_address == 0 && !reset)
        return CROFormatError(0x10);

    VAddr relocation_address = batch;
    while (true) {
        RelocationEntry relocation;
        system.Memory().ReadBlock(process, relocation_address, &relocation,
                                  sizeof(RelocationEntry));

        VAddr relocation_target = SegmentTagToAddress(relocation.target_position);
        if (relocation_target == 0) {
            return CROFormatError(0x12);
        }

        Result result = ApplyRelocation(relocation_target, relocation.type, relocation.addend,
                                        symbol_address, relocation_target);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying relocation {:08X}", result.raw);
            return result;
        }

        if (relocation.is_batch_end)
            break;

        relocation_address += sizeof(RelocationEntry);
    }

    RelocationEntry relocation;
    system.Memory().ReadBlock(process, batch, &relocation, sizeof(RelocationEntry));
    relocation.is_batch_resolved = reset ? 0 : 1;
    system.Memory().WriteBlock(process, batch, &relocation, sizeof(RelocationEntry));
    return ResultSuccess;
}

VAddr CROHelper::FindExportNamedSymbol(const std::string& name) const {
    if (!GetField(ExportTreeNum))
        return 0;

    std::size_t len = name.size();
    ExportTreeEntry entry;
    GetEntry(system.Memory(), 0, entry);
    ExportTreeEntry::Child next;
    next.raw = entry.left.raw;
    u32 found_id;

    while (true) {
        GetEntry(system.Memory(), next.next_index, entry);

        if (next.is_end) {
            found_id = entry.export_table_index;
            break;
        }

        u16 test_byte = entry.test_bit >> 3;
        u16 test_bit_in_byte = entry.test_bit & 7;

        if (test_byte >= len) {
            next.raw = entry.left.raw;
        } else if ((name[test_byte] >> test_bit_in_byte) & 1) {
            next.raw = entry.right.raw;
        } else {
            next.raw = entry.left.raw;
        }
    }

    u32 export_named_symbol_num = GetField(ExportNamedSymbolNum);

    if (found_id >= export_named_symbol_num)
        return 0;

    u32 export_strings_size = GetField(ExportStringsSize);
    ExportNamedSymbolEntry symbol_entry;
    GetEntry(system.Memory(), found_id, symbol_entry);

    if (system.Memory().ReadCString(symbol_entry.name_offset, export_strings_size) != name)
        return 0;

    return SegmentTagToAddress(symbol_entry.symbol_position);
}

Result CROHelper::RebaseHeader(u32 cro_size) {
    Result error = CROFormatError(0x11);

    // verifies magic
    if (GetField(Magic) != MAGIC_CRO0)
        return error;

    // verifies not registered
    if (GetField(NextCRO) != 0 || GetField(PreviousCRO) != 0)
        return error;

    // This seems to be a hard limit set by the RO module
    if (GetField(FileSize) > 0x10000000 || GetField(BssSize) > 0x10000000)
        return error;

    // verifies not fixed
    if (GetField(FixedSize) != 0)
        return error;

    if (GetField(CodeOffset) < CRO_HEADER_SIZE)
        return error;

    // verifies that all offsets are in the correct order
    constexpr std::array<HeaderField, 18> OFFSET_ORDER = {{
        CodeOffset,
        ModuleNameOffset,
        SegmentTableOffset,
        ExportNamedSymbolTableOffset,
        ExportTreeTableOffset,
        ExportIndexedSymbolTableOffset,
        ExportStringsOffset,
        ImportModuleTableOffset,
        ExternalRelocationTableOffset,
        ImportNamedSymbolTableOffset,
        ImportIndexedSymbolTableOffset,
        ImportAnonymousSymbolTableOffset,
        ImportStringsOffset,
        StaticAnonymousSymbolTableOffset,
        InternalRelocationTableOffset,
        StaticRelocationTableOffset,
        DataOffset,
        FileSize,
    }};

    u32 prev_offset = GetField(OFFSET_ORDER[0]);
    u32 cur_offset;
    for (std::size_t i = 1; i < OFFSET_ORDER.size(); ++i) {
        cur_offset = GetField(OFFSET_ORDER[i]);
        if (cur_offset < prev_offset)
            return error;
        prev_offset = cur_offset;
    }

    // rebases offsets
    u32 offset = GetField(NameOffset);
    if (offset != 0)
        SetField(NameOffset, offset + module_address);

    for (int field = CodeOffset; field < Fix0Barrier; field += 2) {
        HeaderField header_field = static_cast<HeaderField>(field);
        offset = GetField(header_field);
        if (offset != 0)
            SetField(header_field, offset + module_address);
    }

    // verifies everything is not beyond the buffer
    u32 file_end = module_address + cro_size;
    for (int field = CodeOffset, i = 0; field < Fix0Barrier; field += 2, ++i) {
        HeaderField offset_field = static_cast<HeaderField>(field);
        HeaderField size_field = static_cast<HeaderField>(field + 1);
        if (GetField(offset_field) + GetField(size_field) * ENTRY_SIZE[i] > file_end)
            return error;
    }

    return ResultSuccess;
}

ResultVal<VAddr> CROHelper::RebaseSegmentTable(u32 cro_size, VAddr data_segment_address,
                                               u32 data_segment_size, VAddr bss_segment_address,
                                               u32 bss_segment_size) {

    u32 prev_data_segment = 0;
    u32 segment_num = GetField(SegmentNum);
    for (u32 i = 0; i < segment_num; ++i) {
        SegmentEntry segment;
        GetEntry(system.Memory(), i, segment);
        if (segment.type == SegmentType::Data) {
            if (segment.size != 0) {
                if (segment.size > data_segment_size)
                    return ERROR_BUFFER_TOO_SMALL;
                prev_data_segment = segment.offset;
                segment.offset = data_segment_address;
            }
        } else if (segment.type == SegmentType::BSS) {
            if (segment.size != 0) {
                if (segment.size > bss_segment_size)
                    return ERROR_BUFFER_TOO_SMALL;
                segment.offset = bss_segment_address;
            }
        } else if (segment.offset != 0) {
            segment.offset += module_address;
            if (segment.offset > module_address + cro_size)
                return CROFormatError(0x19);
        }
        SetEntry(system.Memory(), i, segment);
    }
    return prev_data_segment + module_address;
}

Result CROHelper::RebaseExportNamedSymbolTable() {
    VAddr export_strings_offset = GetField(ExportStringsOffset);
    VAddr export_strings_end = export_strings_offset + GetField(ExportStringsSize);

    u32 export_named_symbol_num = GetField(ExportNamedSymbolNum);
    for (u32 i = 0; i < export_named_symbol_num; ++i) {
        ExportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.name_offset != 0) {
            entry.name_offset += module_address;
            if (entry.name_offset < export_strings_offset ||
                entry.name_offset >= export_strings_end) {
                return CROFormatError(0x11);
            }
        }

        SetEntry(system.Memory(), i, entry);
    }
    return ResultSuccess;
}

Result CROHelper::VerifyExportTreeTable() const {
    u32 tree_num = GetField(ExportTreeNum);
    for (u32 i = 0; i < tree_num; ++i) {
        ExportTreeEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.left.next_index >= tree_num || entry.right.next_index >= tree_num) {
            return CROFormatError(0x11);
        }
    }
    return ResultSuccess;
}

Result CROHelper::RebaseImportModuleTable() {
    VAddr import_strings_offset = GetField(ImportStringsOffset);
    VAddr import_strings_end = import_strings_offset + GetField(ImportStringsSize);
    VAddr import_indexed_symbol_table_offset = GetField(ImportIndexedSymbolTableOffset);
    VAddr index_import_table_end =
        import_indexed_symbol_table_offset +
        GetField(ImportIndexedSymbolNum) * sizeof(ImportIndexedSymbolEntry);
    VAddr import_anonymous_symbol_table_offset = GetField(ImportAnonymousSymbolTableOffset);
    VAddr offset_import_table_end =
        import_anonymous_symbol_table_offset +
        GetField(ImportAnonymousSymbolNum) * sizeof(ImportAnonymousSymbolEntry);

    u32 module_num = GetField(ImportModuleNum);
    for (u32 i = 0; i < module_num; ++i) {
        ImportModuleEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.name_offset != 0) {
            entry.name_offset += module_address;
            if (entry.name_offset < import_strings_offset ||
                entry.name_offset >= import_strings_end) {
                return CROFormatError(0x18);
            }
        }

        if (entry.import_indexed_symbol_table_offset != 0) {
            entry.import_indexed_symbol_table_offset += module_address;
            if (entry.import_indexed_symbol_table_offset < import_indexed_symbol_table_offset ||
                entry.import_indexed_symbol_table_offset > index_import_table_end) {
                return CROFormatError(0x18);
            }
        }

        if (entry.import_anonymous_symbol_table_offset != 0) {
            entry.import_anonymous_symbol_table_offset += module_address;
            if (entry.import_anonymous_symbol_table_offset < import_anonymous_symbol_table_offset ||
                entry.import_anonymous_symbol_table_offset > offset_import_table_end) {
                return CROFormatError(0x18);
            }
        }

        SetEntry(system.Memory(), i, entry);
    }
    return ResultSuccess;
}

Result CROHelper::RebaseImportNamedSymbolTable() {
    VAddr import_strings_offset = GetField(ImportStringsOffset);
    VAddr import_strings_end = import_strings_offset + GetField(ImportStringsSize);
    VAddr external_relocation_table_offset = GetField(ExternalRelocationTableOffset);
    VAddr external_relocation_table_end =
        external_relocation_table_offset +
        GetField(ExternalRelocationNum) * sizeof(ExternalRelocationEntry);

    u32 num = GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < num; ++i) {
        ImportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.name_offset != 0) {
            entry.name_offset += module_address;
            if (entry.name_offset < import_strings_offset ||
                entry.name_offset >= import_strings_end) {
                return CROFormatError(0x1B);
            }
        }

        if (entry.relocation_batch_offset != 0) {
            entry.relocation_batch_offset += module_address;
            if (entry.relocation_batch_offset < external_relocation_table_offset ||
                entry.relocation_batch_offset > external_relocation_table_end) {
                return CROFormatError(0x1B);
            }
        }

        SetEntry(system.Memory(), i, entry);
    }
    return ResultSuccess;
}

Result CROHelper::RebaseImportIndexedSymbolTable() {
    VAddr external_relocation_table_offset = GetField(ExternalRelocationTableOffset);
    VAddr external_relocation_table_end =
        external_relocation_table_offset +
        GetField(ExternalRelocationNum) * sizeof(ExternalRelocationEntry);

    u32 num = GetField(ImportIndexedSymbolNum);
    for (u32 i = 0; i < num; ++i) {
        ImportIndexedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.relocation_batch_offset != 0) {
            entry.relocation_batch_offset += module_address;
            if (entry.relocation_batch_offset < external_relocation_table_offset ||
                entry.relocation_batch_offset > external_relocation_table_end) {
                return CROFormatError(0x14);
            }
        }

        SetEntry(system.Memory(), i, entry);
    }
    return ResultSuccess;
}

Result CROHelper::RebaseImportAnonymousSymbolTable() {
    VAddr external_relocation_table_offset = GetField(ExternalRelocationTableOffset);
    VAddr external_relocation_table_end =
        external_relocation_table_offset +
        GetField(ExternalRelocationNum) * sizeof(ExternalRelocationEntry);

    u32 num = GetField(ImportAnonymousSymbolNum);
    for (u32 i = 0; i < num; ++i) {
        ImportAnonymousSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.relocation_batch_offset != 0) {
            entry.relocation_batch_offset += module_address;
            if (entry.relocation_batch_offset < external_relocation_table_offset ||
                entry.relocation_batch_offset > external_relocation_table_end) {
                return CROFormatError(0x17);
            }
        }

        SetEntry(system.Memory(), i, entry);
    }
    return ResultSuccess;
}

VAddr CROHelper::GetOnUnresolvedAddress() {
    return SegmentTagToAddress(SegmentTag(GetField(OnUnresolvedSegmentTag)));
}

Result CROHelper::ResetExternalRelocations() {
    u32 unresolved_symbol = GetOnUnresolvedAddress();
    u32 external_relocation_num = GetField(ExternalRelocationNum);
    ExternalRelocationEntry relocation;

    // Verifies that the last relocation is the end of a batch
    GetEntry(system.Memory(), external_relocation_num - 1, relocation);
    if (!relocation.is_batch_end) {
        return CROFormatError(0x12);
    }

    bool batch_begin = true;
    for (u32 i = 0; i < external_relocation_num; ++i) {
        GetEntry(system.Memory(), i, relocation);
        VAddr relocation_target = SegmentTagToAddress(relocation.target_position);

        if (relocation_target == 0) {
            return CROFormatError(0x12);
        }

        Result result = ApplyRelocation(relocation_target, relocation.type, relocation.addend,
                                        unresolved_symbol, relocation_target);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying relocation {:08X}", result.raw);
            return result;
        }

        if (batch_begin) {
            // resets to unresolved state
            relocation.is_batch_resolved = 0;
            SetEntry(system.Memory(), i, relocation);
        }

        // if current is an end, then the next is a beginning
        batch_begin = relocation.is_batch_end != 0;
    }

    return ResultSuccess;
}

Result CROHelper::ClearExternalRelocations() {
    u32 external_relocation_num = GetField(ExternalRelocationNum);
    ExternalRelocationEntry relocation;

    bool batch_begin = true;
    for (u32 i = 0; i < external_relocation_num; ++i) {
        GetEntry(system.Memory(), i, relocation);
        VAddr relocation_target = SegmentTagToAddress(relocation.target_position);

        if (relocation_target == 0) {
            return CROFormatError(0x12);
        }

        Result result = ClearRelocation(relocation_target, relocation.type);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error clearing relocation {:08X}", result.raw);
            return result;
        }

        if (batch_begin) {
            // resets to unresolved state
            relocation.is_batch_resolved = 0;
            SetEntry(system.Memory(), i, relocation);
        }

        // if current is an end, then the next is a beginning
        batch_begin = relocation.is_batch_end != 0;
    }

    return ResultSuccess;
}

Result CROHelper::ApplyStaticAnonymousSymbolToCRS(VAddr crs_address) {
    VAddr static_relocation_table_offset = GetField(StaticRelocationTableOffset);
    VAddr static_relocation_table_end =
        static_relocation_table_offset +
        GetField(StaticRelocationNum) * sizeof(StaticRelocationEntry);

    CROHelper crs(crs_address, process, system);
    u32 offset_export_num = GetField(StaticAnonymousSymbolNum);
    LOG_INFO(Service_LDR, "CRO \"{}\" exports {} static anonymous symbols", ModuleName(),
             offset_export_num);
    for (u32 i = 0; i < offset_export_num; ++i) {
        StaticAnonymousSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);
        u32 batch_address = entry.relocation_batch_offset + module_address;

        if (batch_address < static_relocation_table_offset ||
            batch_address > static_relocation_table_end) {
            return CROFormatError(0x16);
        }

        u32 symbol_address = SegmentTagToAddress(entry.symbol_position);
        LOG_TRACE(Service_LDR, "CRO \"{}\" exports 0x{:08X} to the static module", ModuleName(),
                  symbol_address);
        Result result = crs.ApplyRelocationBatch(batch_address, symbol_address);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
            return result;
        }
    }
    return ResultSuccess;
}

Result CROHelper::ApplyInternalRelocations(u32 old_data_segment_address) {
    u32 segment_num = GetField(SegmentNum);
    u32 internal_relocation_num = GetField(InternalRelocationNum);
    for (u32 i = 0; i < internal_relocation_num; ++i) {
        InternalRelocationEntry relocation;
        GetEntry(system.Memory(), i, relocation);
        VAddr target_addressB = SegmentTagToAddress(relocation.target_position);
        if (target_addressB == 0) {
            return CROFormatError(0x15);
        }

        VAddr target_address;
        SegmentEntry target_segment;
        GetEntry(system.Memory(), relocation.target_position.segment_index, target_segment);

        if (target_segment.type == SegmentType::Data) {
            // If the relocation is to the .data segment, we need to relocate it in the old buffer
            target_address =
                old_data_segment_address + relocation.target_position.offset_into_segment;
        } else {
            target_address = target_addressB;
        }

        if (relocation.symbol_segment >= segment_num) {
            return CROFormatError(0x15);
        }

        SegmentEntry symbol_segment;
        GetEntry(system.Memory(), relocation.symbol_segment, symbol_segment);
        LOG_TRACE(Service_LDR, "Internally relocates 0x{:08X} with 0x{:08X}", target_address,
                  symbol_segment.offset);
        Result result = ApplyRelocation(target_address, relocation.type, relocation.addend,
                                        symbol_segment.offset, target_addressB);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying relocation {:08X}", result.raw);
            return result;
        }
    }
    return ResultSuccess;
}

Result CROHelper::ClearInternalRelocations() {
    u32 internal_relocation_num = GetField(InternalRelocationNum);
    for (u32 i = 0; i < internal_relocation_num; ++i) {
        InternalRelocationEntry relocation;
        GetEntry(system.Memory(), i, relocation);
        VAddr target_address = SegmentTagToAddress(relocation.target_position);

        if (target_address == 0) {
            return CROFormatError(0x15);
        }

        Result result = ClearRelocation(target_address, relocation.type);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error clearing relocation {:08X}", result.raw);
            return result;
        }
    }
    return ResultSuccess;
}

void CROHelper::UnrebaseImportAnonymousSymbolTable() {
    u32 num = GetField(ImportAnonymousSymbolNum);
    for (u32 i = 0; i < num; ++i) {
        ImportAnonymousSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.relocation_batch_offset != 0) {
            entry.relocation_batch_offset -= module_address;
        }

        SetEntry(system.Memory(), i, entry);
    }
}

void CROHelper::UnrebaseImportIndexedSymbolTable() {
    u32 num = GetField(ImportIndexedSymbolNum);
    for (u32 i = 0; i < num; ++i) {
        ImportIndexedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.relocation_batch_offset != 0) {
            entry.relocation_batch_offset -= module_address;
        }

        SetEntry(system.Memory(), i, entry);
    }
}

void CROHelper::UnrebaseImportNamedSymbolTable() {
    u32 num = GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < num; ++i) {
        ImportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.name_offset != 0) {
            entry.name_offset -= module_address;
        }

        if (entry.relocation_batch_offset) {
            entry.relocation_batch_offset -= module_address;
        }

        SetEntry(system.Memory(), i, entry);
    }
}

void CROHelper::UnrebaseImportModuleTable() {
    u32 module_num = GetField(ImportModuleNum);
    for (u32 i = 0; i < module_num; ++i) {
        ImportModuleEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.name_offset != 0) {
            entry.name_offset -= module_address;
        }

        if (entry.import_indexed_symbol_table_offset) {
            entry.import_indexed_symbol_table_offset -= module_address;
        }

        if (entry.import_anonymous_symbol_table_offset) {
            entry.import_anonymous_symbol_table_offset -= module_address;
        }

        SetEntry(system.Memory(), i, entry);
    }
}

void CROHelper::UnrebaseExportNamedSymbolTable() {
    u32 export_named_symbol_num = GetField(ExportNamedSymbolNum);
    for (u32 i = 0; i < export_named_symbol_num; ++i) {
        ExportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);

        if (entry.name_offset != 0) {
            entry.name_offset -= module_address;
        }

        SetEntry(system.Memory(), i, entry);
    }
}

void CROHelper::UnrebaseSegmentTable() {
    u32 segment_num = GetField(SegmentNum);
    for (u32 i = 0; i < segment_num; ++i) {
        SegmentEntry segment;
        GetEntry(system.Memory(), i, segment);

        if (segment.type == SegmentType::BSS) {
            segment.offset = 0;
        } else if (segment.offset != 0) {
            segment.offset -= module_address;
        }

        SetEntry(system.Memory(), i, segment);
    }
}

void CROHelper::UnrebaseHeader() {
    u32 offset = GetField(NameOffset);
    if (offset != 0)
        SetField(NameOffset, offset - module_address);

    for (int field = CodeOffset; field < Fix0Barrier; field += 2) {
        HeaderField header_field = static_cast<HeaderField>(field);
        offset = GetField(header_field);
        if (offset != 0)
            SetField(header_field, offset - module_address);
    }
}

Result CROHelper::ApplyImportNamedSymbol(VAddr crs_address) {
    u32 import_strings_size = GetField(ImportStringsSize);
    u32 symbol_import_num = GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < symbol_import_num; ++i) {
        ImportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        if (!relocation_entry.is_batch_resolved) {
            Result result = ForEachAutoLinkCRO(
                process, system, crs_address, [&](CROHelper source) -> ResultVal<bool> {
                    std::string symbol_name =
                        system.Memory().ReadCString(entry.name_offset, import_strings_size);
                    u32 symbol_address = source.FindExportNamedSymbol(symbol_name);

                    if (symbol_address != 0) {
                        LOG_TRACE(Service_LDR, "CRO \"{}\" imports \"{}\" from \"{}\"",
                                  ModuleName(), symbol_name, source.ModuleName());

                        Result result = ApplyRelocationBatch(relocation_addr, symbol_address);
                        if (result.IsError()) {
                            LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}",
                                      result.raw);
                            return result;
                        }

                        return false;
                    }

                    return true;
                });
            if (result.IsError()) {
                return result;
            }
        }
    }
    return ResultSuccess;
}

Result CROHelper::ResetImportNamedSymbol() {
    u32 unresolved_symbol = GetOnUnresolvedAddress();

    u32 symbol_import_num = GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < symbol_import_num; ++i) {
        ImportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        Result result = ApplyRelocationBatch(relocation_addr, unresolved_symbol, true);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reseting relocation batch {:08X}", result.raw);
            return result;
        }
    }
    return ResultSuccess;
}

Result CROHelper::ResetImportIndexedSymbol() {
    u32 unresolved_symbol = GetOnUnresolvedAddress();

    u32 import_num = GetField(ImportIndexedSymbolNum);
    for (u32 i = 0; i < import_num; ++i) {
        ImportIndexedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        Result result = ApplyRelocationBatch(relocation_addr, unresolved_symbol, true);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reseting relocation batch {:08X}", result.raw);
            return result;
        }
    }
    return ResultSuccess;
}

Result CROHelper::ResetImportAnonymousSymbol() {
    u32 unresolved_symbol = GetOnUnresolvedAddress();

    u32 import_num = GetField(ImportAnonymousSymbolNum);
    for (u32 i = 0; i < import_num; ++i) {
        ImportAnonymousSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        Result result = ApplyRelocationBatch(relocation_addr, unresolved_symbol, true);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reseting relocation batch {:08X}", result.raw);
            return result;
        }
    }
    return ResultSuccess;
}

Result CROHelper::ApplyModuleImport(VAddr crs_address) {
    u32 import_strings_size = GetField(ImportStringsSize);

    u32 import_module_num = GetField(ImportModuleNum);
    for (u32 i = 0; i < import_module_num; ++i) {
        ImportModuleEntry entry;
        GetEntry(system.Memory(), i, entry);
        std::string want_cro_name =
            system.Memory().ReadCString(entry.name_offset, import_strings_size);

        Result result = ForEachAutoLinkCRO(
            process, system, crs_address, [&](CROHelper source) -> ResultVal<bool> {
                if (want_cro_name == source.ModuleName()) {
                    LOG_INFO(Service_LDR, "CRO \"{}\" imports {} indexed symbols from \"{}\"",
                             ModuleName(), entry.import_indexed_symbol_num, source.ModuleName());
                    for (u32 j = 0; j < entry.import_indexed_symbol_num; ++j) {
                        ImportIndexedSymbolEntry im;
                        entry.GetImportIndexedSymbolEntry(process, system.Memory(), j, im);
                        ExportIndexedSymbolEntry ex;
                        source.GetEntry(system.Memory(), im.index, ex);
                        u32 symbol_address = source.SegmentTagToAddress(ex.symbol_position);
                        LOG_TRACE(Service_LDR, "    Imports 0x{:08X}", symbol_address);
                        Result result =
                            ApplyRelocationBatch(im.relocation_batch_offset, symbol_address);
                        if (result.IsError()) {
                            LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}",
                                      result.raw);
                            return result;
                        }
                    }
                    LOG_INFO(Service_LDR, "CRO \"{}\" imports {} anonymous symbols from \"{}\"",
                             ModuleName(), entry.import_anonymous_symbol_num, source.ModuleName());
                    for (u32 j = 0; j < entry.import_anonymous_symbol_num; ++j) {
                        ImportAnonymousSymbolEntry im;
                        entry.GetImportAnonymousSymbolEntry(process, system.Memory(), j, im);
                        u32 symbol_address = source.SegmentTagToAddress(im.symbol_position);
                        LOG_TRACE(Service_LDR, "    Imports 0x{:08X}", symbol_address);
                        Result result =
                            ApplyRelocationBatch(im.relocation_batch_offset, symbol_address);
                        if (result.IsError()) {
                            LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}",
                                      result.raw);
                            return result;
                        }
                    }
                    return false;
                }
                return true;
            });
        if (result.IsError()) {
            return result;
        }
    }
    return ResultSuccess;
}

Result CROHelper::ApplyExportNamedSymbol(CROHelper target) {
    LOG_DEBUG(Service_LDR, "CRO \"{}\" exports named symbols to \"{}\"", ModuleName(),
              target.ModuleName());
    u32 target_import_strings_size = target.GetField(ImportStringsSize);
    u32 target_symbol_import_num = target.GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < target_symbol_import_num; ++i) {
        ImportNamedSymbolEntry entry;
        target.GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        if (!relocation_entry.is_batch_resolved) {
            std::string symbol_name =
                system.Memory().ReadCString(entry.name_offset, target_import_strings_size);
            u32 symbol_address = FindExportNamedSymbol(symbol_name);
            if (symbol_address != 0) {
                LOG_TRACE(Service_LDR, "    exports symbol \"{}\"", symbol_name);
                Result result = target.ApplyRelocationBatch(relocation_addr, symbol_address);
                if (result.IsError()) {
                    LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
                    return result;
                }
            }
        }
    }
    return ResultSuccess;
}

Result CROHelper::ResetExportNamedSymbol(CROHelper target) {
    LOG_DEBUG(Service_LDR, "CRO \"{}\" unexports named symbols to \"{}\"", ModuleName(),
              target.ModuleName());
    u32 unresolved_symbol = target.GetOnUnresolvedAddress();
    u32 target_import_strings_size = target.GetField(ImportStringsSize);
    u32 target_symbol_import_num = target.GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < target_symbol_import_num; ++i) {
        ImportNamedSymbolEntry entry;
        target.GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        if (relocation_entry.is_batch_resolved) {
            std::string symbol_name =
                system.Memory().ReadCString(entry.name_offset, target_import_strings_size);
            u32 symbol_address = FindExportNamedSymbol(symbol_name);
            if (symbol_address != 0) {
                LOG_TRACE(Service_LDR, "    unexports symbol \"{}\"", symbol_name);
                Result result =
                    target.ApplyRelocationBatch(relocation_addr, unresolved_symbol, true);
                if (result.IsError()) {
                    LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
                    return result;
                }
            }
        }
    }
    return ResultSuccess;
}

Result CROHelper::ApplyModuleExport(CROHelper target) {
    std::string module_name = ModuleName();
    u32 target_import_string_size = target.GetField(ImportStringsSize);
    u32 target_import_module_num = target.GetField(ImportModuleNum);
    for (u32 i = 0; i < target_import_module_num; ++i) {
        ImportModuleEntry entry;
        target.GetEntry(system.Memory(), i, entry);

        if (system.Memory().ReadCString(entry.name_offset, target_import_string_size) !=
            module_name)
            continue;

        LOG_INFO(Service_LDR, "CRO \"{}\" exports {} indexed symbols to \"{}\"", module_name,
                 entry.import_indexed_symbol_num, target.ModuleName());
        for (u32 j = 0; j < entry.import_indexed_symbol_num; ++j) {
            ImportIndexedSymbolEntry im;
            entry.GetImportIndexedSymbolEntry(process, system.Memory(), j, im);
            ExportIndexedSymbolEntry ex;
            GetEntry(system.Memory(), im.index, ex);
            u32 symbol_address = SegmentTagToAddress(ex.symbol_position);
            LOG_TRACE(Service_LDR, "    exports symbol 0x{:08X}", symbol_address);
            Result result = target.ApplyRelocationBatch(im.relocation_batch_offset, symbol_address);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
                return result;
            }
        }

        LOG_INFO(Service_LDR, "CRO \"{}\" exports {} anonymous symbols to \"{}\"", module_name,
                 entry.import_anonymous_symbol_num, target.ModuleName());
        for (u32 j = 0; j < entry.import_anonymous_symbol_num; ++j) {
            ImportAnonymousSymbolEntry im;
            entry.GetImportAnonymousSymbolEntry(process, system.Memory(), j, im);
            u32 symbol_address = SegmentTagToAddress(im.symbol_position);
            LOG_TRACE(Service_LDR, "    exports symbol 0x{:08X}", symbol_address);
            Result result = target.ApplyRelocationBatch(im.relocation_batch_offset, symbol_address);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
                return result;
            }
        }
    }

    return ResultSuccess;
}

Result CROHelper::ResetModuleExport(CROHelper target) {
    u32 unresolved_symbol = target.GetOnUnresolvedAddress();

    std::string module_name = ModuleName();
    u32 target_import_string_size = target.GetField(ImportStringsSize);
    u32 target_import_module_num = target.GetField(ImportModuleNum);
    for (u32 i = 0; i < target_import_module_num; ++i) {
        ImportModuleEntry entry;
        target.GetEntry(system.Memory(), i, entry);

        if (system.Memory().ReadCString(entry.name_offset, target_import_string_size) !=
            module_name)
            continue;

        LOG_DEBUG(Service_LDR, "CRO \"{}\" unexports indexed symbols to \"{}\"", module_name,
                  target.ModuleName());
        for (u32 j = 0; j < entry.import_indexed_symbol_num; ++j) {
            ImportIndexedSymbolEntry im;
            entry.GetImportIndexedSymbolEntry(process, system.Memory(), j, im);
            Result result =
                target.ApplyRelocationBatch(im.relocation_batch_offset, unresolved_symbol, true);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
                return result;
            }
        }

        LOG_DEBUG(Service_LDR, "CRO \"{}\" unexports anonymous symbols to \"{}\"", module_name,
                  target.ModuleName());
        for (u32 j = 0; j < entry.import_anonymous_symbol_num; ++j) {
            ImportAnonymousSymbolEntry im;
            entry.GetImportAnonymousSymbolEntry(process, system.Memory(), j, im);
            Result result =
                target.ApplyRelocationBatch(im.relocation_batch_offset, unresolved_symbol, true);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}", result.raw);
                return result;
            }
        }
    }

    return ResultSuccess;
}

Result CROHelper::ApplyExitRelocations(VAddr crs_address) {
    u32 import_strings_size = GetField(ImportStringsSize);
    u32 symbol_import_num = GetField(ImportNamedSymbolNum);
    for (u32 i = 0; i < symbol_import_num; ++i) {
        ImportNamedSymbolEntry entry;
        GetEntry(system.Memory(), i, entry);
        VAddr relocation_addr = entry.relocation_batch_offset;
        ExternalRelocationEntry relocation_entry;
        system.Memory().ReadBlock(process, relocation_addr, &relocation_entry,
                                  sizeof(ExternalRelocationEntry));

        if (system.Memory().ReadCString(entry.name_offset, import_strings_size) ==
            "__aeabi_atexit") {
            Result result = ForEachAutoLinkCRO(
                process, system, crs_address, [&](CROHelper source) -> ResultVal<bool> {
                    u32 symbol_address = source.FindExportNamedSymbol("nnroAeabiAtexit_");

                    if (symbol_address != 0) {
                        LOG_DEBUG(Service_LDR, "CRO \"{}\" import exit function from \"{}\"",
                                  ModuleName(), source.ModuleName());

                        Result result = ApplyRelocationBatch(relocation_addr, symbol_address);
                        if (result.IsError()) {
                            LOG_ERROR(Service_LDR, "Error applying relocation batch {:08X}",
                                      result.raw);
                            return result;
                        }

                        return false;
                    }

                    return true;
                });
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying exit relocation {:08X}", result.raw);
                return result;
            }
        }
    }
    return ResultSuccess;
}

/**
 * Verifies a string or a string table matching a predicted size (i.e. terminated by 0)
 * if it is not empty. There can be many other nulls in the string table because
 * they are composed by many sub strings. This function is to check whether the
 * whole string (table) is terminated properly, despite that it is not actually one string.
 * @param address the virtual address of the string (table)
 * @param size the size of the string (table), including the terminating 0
 * @returns Result ResultSuccess if the size matches, otherwise error code.
 */
static Result VerifyStringTableLength(Memory::MemorySystem& memory, VAddr address, u32 size) {
    if (size != 0) {
        if (memory.Read8(address + size - 1) != 0)
            return CROFormatError(0x0B);
    }
    return ResultSuccess;
}

Result CROHelper::Rebase(VAddr crs_address, u32 cro_size, VAddr data_segment_addresss,
                         u32 data_segment_size, VAddr bss_segment_address, u32 bss_segment_size,
                         bool is_crs) {

    Result result = RebaseHeader(cro_size);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing header {:08X}", result.raw);
        return result;
    }

    result = VerifyStringTableLength(system.Memory(), GetField(ModuleNameOffset),
                                     GetField(ModuleNameSize));
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying module name {:08X}", result.raw);
        return result;
    }

    u32 prev_data_segment_address = 0;
    if (!is_crs) {
        auto result_val = RebaseSegmentTable(cro_size, data_segment_addresss, data_segment_size,
                                             bss_segment_address, bss_segment_size);
        if (result_val.Failed()) {
            LOG_ERROR(Service_LDR, "Error rebasing segment table {:08X}", result_val.Code().raw);
            return result_val.Code();
        }
        prev_data_segment_address = *result_val;
    }

    result = RebaseExportNamedSymbolTable();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing symbol export table {:08X}", result.raw);
        return result;
    }

    result = VerifyExportTreeTable();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying export tree {:08X}", result.raw);
        return result;
    }

    result = VerifyStringTableLength(system.Memory(), GetField(ExportStringsOffset),
                                     GetField(ExportStringsSize));
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying export strings {:08X}", result.raw);
        return result;
    }

    result = RebaseImportModuleTable();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing object table {:08X}", result.raw);
        return result;
    }

    result = ResetExternalRelocations();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error resetting all external relocations {:08X}", result.raw);
        return result;
    }

    result = RebaseImportNamedSymbolTable();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing symbol import table {:08X}", result.raw);
        return result;
    }

    result = RebaseImportIndexedSymbolTable();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing index import table {:08X}", result.raw);
        return result;
    }

    result = RebaseImportAnonymousSymbolTable();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing offset import table {:08X}", result.raw);
        return result;
    }

    result = VerifyStringTableLength(system.Memory(), GetField(ImportStringsOffset),
                                     GetField(ImportStringsSize));
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying import strings {:08X}", result.raw);
        return result;
    }

    if (!is_crs) {
        result = ApplyStaticAnonymousSymbolToCRS(crs_address);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying offset export to CRS {:08X}", result.raw);
            return result;
        }
    }

    result = ApplyInternalRelocations(prev_data_segment_address);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error applying internal relocations {:08X}", result.raw);
        return result;
    }

    if (!is_crs) {
        result = ApplyExitRelocations(crs_address);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying exit relocations {:08X}", result.raw);
            return result;
        }
    }

    return ResultSuccess;
}

void CROHelper::Unrebase(bool is_crs) {
    UnrebaseImportAnonymousSymbolTable();
    UnrebaseImportIndexedSymbolTable();
    UnrebaseImportNamedSymbolTable();
    UnrebaseImportModuleTable();
    UnrebaseExportNamedSymbolTable();

    if (!is_crs)
        UnrebaseSegmentTable();

    SetNextModule(0);
    SetPreviousModule(0);

    SetField(FixedSize, 0);

    UnrebaseHeader();
}

Result CROHelper::VerifyHash(u32 cro_size, VAddr crr) const {
    // TODO(wwylele): actually verify the hash
    return ResultSuccess;
}

Result CROHelper::Link(VAddr crs_address, bool link_on_load_bug_fix) {
    Result result = ResultSuccess;

    {
        VAddr data_segment_address = 0;
        if (link_on_load_bug_fix) {
            // this is a bug fix introduced by 7.2.0-17's LoadCRO_New
            // The bug itself is:
            // If a relocation target is in .data segment, it will relocate to the
            // user-specified buffer. But if this is linking during loading,
            // the .data segment hasn't been transfer from CRO to the buffer,
            // thus the relocation will be overwritten by data transfer.
            // To fix this bug, we need temporarily restore the old .data segment
            // offset and apply imported symbols.

            // RO service seems assuming segment_index == segment_type,
            // so we do the same
            if (GetField(SegmentNum) >= 2) { // means we have .data segment
                SegmentEntry entry;
                GetEntry(system.Memory(), 2, entry);
                ASSERT(entry.type == SegmentType::Data);
                data_segment_address = entry.offset;
                entry.offset = GetField(DataOffset);
                SetEntry(system.Memory(), 2, entry);
            }
        }
        SCOPE_EXIT({
            // Restore the new .data segment address after importing
            if (link_on_load_bug_fix) {
                if (GetField(SegmentNum) >= 2) {
                    SegmentEntry entry;
                    GetEntry(system.Memory(), 2, entry);
                    entry.offset = data_segment_address;
                    SetEntry(system.Memory(), 2, entry);
                }
            }
        });

        // Imports named symbols from other modules
        result = ApplyImportNamedSymbol(crs_address);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying symbol import {:08X}", result.raw);
            return result;
        }

        // Imports indexed and anonymous symbols from other modules
        result = ApplyModuleImport(crs_address);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying module import {:08X}", result.raw);
            return result;
        }
    }

    // Exports symbols to other modules
    result = ForEachAutoLinkCRO(process, system, crs_address,
                                [this](CROHelper target) -> ResultVal<bool> {
                                    Result result = ApplyExportNamedSymbol(target);
                                    if (result.IsError())
                                        return result;

                                    result = ApplyModuleExport(target);
                                    if (result.IsError())
                                        return result;

                                    return true;
                                });
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error applying export {:08X}", result.raw);
        return result;
    }

    return ResultSuccess;
}

Result CROHelper::Unlink(VAddr crs_address) {

    // Resets all imported named symbols
    Result result = ResetImportNamedSymbol();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error resetting symbol import {:08X}", result.raw);
        return result;
    }

    // Resets all imported indexed symbols
    result = ResetImportIndexedSymbol();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error resetting indexed import {:08X}", result.raw);
        return result;
    }

    // Resets all imported anonymous symbols
    result = ResetImportAnonymousSymbol();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error resetting anonymous import {:08X}", result.raw);
        return result;
    }

    // Resets all symbols in other modules imported from this module
    // Note: the RO service seems only searching in auto-link modules
    result = ForEachAutoLinkCRO(process, system, crs_address,
                                [this](CROHelper target) -> ResultVal<bool> {
                                    Result result = ResetExportNamedSymbol(target);
                                    if (result.IsError())
                                        return result;

                                    result = ResetModuleExport(target);
                                    if (result.IsError())
                                        return result;

                                    return true;
                                });
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error resetting export {:08X}", result.raw);
        return result;
    }

    return ResultSuccess;
}

Result CROHelper::ClearRelocations() {
    Result result = ClearExternalRelocations();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error clearing external relocations {:08X}", result.raw);
        return result;
    }

    result = ClearInternalRelocations();
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error clearing internal relocations {:08X}", result.raw);
        return result;
    }
    return ResultSuccess;
}

void CROHelper::InitCRS() {
    SetNextModule(0);
    SetPreviousModule(0);
}

void CROHelper::Register(VAddr crs_address, bool auto_link) {
    CROHelper crs(crs_address, process, system);
    CROHelper head(auto_link ? crs.NextModule() : crs.PreviousModule(), process, system);

    if (head.module_address) {
        // there are already CROs registered
        // register as the new tail
        CROHelper tail(head.PreviousModule(), process, system);

        // link with the old tail
        ASSERT(tail.NextModule() == 0);
        SetPreviousModule(tail.module_address);
        tail.SetNextModule(module_address);

        // set previous of the head pointing to the new tail
        head.SetPreviousModule(module_address);
    } else {
        // register as the first CRO
        // set previous to self as tail
        SetPreviousModule(module_address);

        // set self as head
        if (auto_link)
            crs.SetNextModule(module_address);
        else
            crs.SetPreviousModule(module_address);
    }

    // the new one is the tail
    SetNextModule(0);
}

void CROHelper::Unregister(VAddr crs_address) {
    CROHelper crs(crs_address, process, system);
    CROHelper next_head(crs.NextModule(), process, system);
    CROHelper previous_head(crs.PreviousModule(), process, system);
    CROHelper next(NextModule(), process, system);
    CROHelper previous(PreviousModule(), process, system);

    if (module_address == next_head.module_address ||
        module_address == previous_head.module_address) {
        // removing head
        if (next.module_address) {
            // the next is new head
            // let its previous point to the tail
            next.SetPreviousModule(previous.module_address);
        }

        // set new head
        if (module_address == previous_head.module_address) {
            crs.SetPreviousModule(next.module_address);
        } else {
            crs.SetNextModule(next.module_address);
        }
    } else if (next.module_address) {
        // link previous and next
        previous.SetNextModule(next.module_address);
        next.SetPreviousModule(previous.module_address);
    } else {
        // removing tail
        // set previous as new tail
        previous.SetNextModule(0);

        // let head's previous point to the new tail
        if (next_head.module_address && next_head.PreviousModule() == module_address) {
            next_head.SetPreviousModule(previous.module_address);
        } else if (previous_head.module_address &&
                   previous_head.PreviousModule() == module_address) {
            previous_head.SetPreviousModule(previous.module_address);
        } else {
            UNREACHABLE();
        }
    }

    // unlink self
    SetNextModule(0);
    SetPreviousModule(0);
}

u32 CROHelper::GetFixEnd(u32 fix_level) const {
    u32 end = CRO_HEADER_SIZE;
    end = std::max<u32>(end, GetField(CodeOffset) + GetField(CodeSize));

    u32 entry_size_i = 2;
    int field = ModuleNameOffset;
    while (true) {
        end = std::max<u32>(end, GetField(static_cast<HeaderField>(field)) +
                                     GetField(static_cast<HeaderField>(field + 1)) *
                                         ENTRY_SIZE[entry_size_i]);

        ++entry_size_i;
        field += 2;

        if (field == FIX_BARRIERS[fix_level])
            return end;
    }
}

u32 CROHelper::Fix(u32 fix_level) {
    u32 fix_end = GetFixEnd(fix_level);

    if (fix_level != 0) {
        SetField(Magic, MAGIC_FIXD);

        for (int field = FIX_BARRIERS[fix_level]; field < Fix0Barrier; field += 2) {
            SetField(static_cast<HeaderField>(field), fix_end);
            SetField(static_cast<HeaderField>(field + 1), 0);
        }
    }

    fix_end = Common::AlignUp(fix_end, Memory::LIME3DS_PAGE_SIZE);

    u32 fixed_size = fix_end - module_address;
    SetField(FixedSize, fixed_size);
    return fixed_size;
}

bool CROHelper::IsLoaded() const {
    u32 magic = GetField(Magic);
    if (magic != MAGIC_CRO0 && magic != MAGIC_FIXD)
        return false;

    // TODO(wwylele): verify memory state here after memory aliasing is implemented

    return true;
}

std::tuple<VAddr, u32> CROHelper::GetExecutablePages() const {
    u32 segment_num = GetField(SegmentNum);
    for (u32 i = 0; i < segment_num; ++i) {
        SegmentEntry entry;
        GetEntry(system.Memory(), i, entry);
        if (entry.type == SegmentType::Code && entry.size != 0) {
            VAddr begin = Common::AlignDown(entry.offset, Memory::LIME3DS_PAGE_SIZE);
            VAddr end = Common::AlignUp(entry.offset + entry.size, Memory::LIME3DS_PAGE_SIZE);
            return std::make_tuple(begin, end - begin);
        }
    }
    return std::make_tuple(0, 0);
}

} // namespace Service::LDR
