// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <tuple>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {
class Process;
}

namespace Service::LDR {

#define ASSERT_CRO_STRUCT(name, size)                                                              \
    static_assert(std::is_standard_layout<name>::value,                                            \
                  "CRO structure " #name " doesn't use standard layout");                          \
    static_assert(std::is_trivially_copyable<name>::value,                                         \
                  "CRO structure " #name " isn't trivially copyable");                             \
    static_assert(sizeof(name) == (size), "Unexpected struct size for CRO structure " #name)

static constexpr u32 CRO_HEADER_SIZE = 0x138;
static constexpr u32 CRO_HASH_SIZE = 0x80;

/// Represents a loaded module (CRO) with interfaces manipulating it.
class CROHelper final {
public:
    // TODO (wwylele): pass in the process handle for memory access
    explicit CROHelper(VAddr cro_address, Kernel::Process& process, Core::System& system)
        : module_address(cro_address), process(process), system(system) {}

    std::string ModuleName() const {
        return system.Memory().ReadCString(GetField(ModuleNameOffset), GetField(ModuleNameSize));
    }

    u32 GetFileSize() const {
        return GetField(FileSize);
    }

    /**
     * Rebases the module according to its address.
     * @param crs_address the virtual address of the static module
     * @param cro_size the size of the CRO file
     * @param data_segment_address buffer address for .data segment
     * @param data_segment_size the buffer size for .data segment
     * @param bss_segment_address the buffer address for .bss segment
     * @param bss_segment_size the buffer size for .bss segment
     * @param is_crs true if the module itself is the static module
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result Rebase(VAddr crs_address, u32 cro_size, VAddr data_segment_address,
                  u32 data_segment_size, VAddr bss_segment_address, u32 bss_segment_size,
                  bool is_crs);

    /**
     * Unrebases the module.
     * @param is_crs true if the module itself is the static module
     */
    void Unrebase(bool is_crs);

    /**
     * Verifies module hash by CRR.
     * @param cro_size the size of the CRO
     * @param crr the virtual address of the CRR
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result VerifyHash(u32 cro_size, VAddr crr) const;

    /**
     * Links this module with all registered auto-link module.
     * @param crs_address the virtual address of the static module
     * @param link_on_load_bug_fix true if links when loading and fixes the bug
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result Link(VAddr crs_address, bool link_on_load_bug_fix);

    /**
     * Unlinks this module with other modules.
     * @param crs_address the virtual address of the static module
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result Unlink(VAddr crs_address);

    /**
     * Clears all relocations to zero.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ClearRelocations();

    /// Initialize this module as the static module (CRS)
    void InitCRS();

    /**
     * Registers this module and adds it to the module list.
     * @param crs_address the virtual address of the static module
     * @param auto_link   whether to register as an auto link module
     */
    void Register(VAddr crs_address, bool auto_link);

    /**
     * Unregisters this module and removes from the module list.
     * @param crs_address the virtual address of the static module
     */
    void Unregister(VAddr crs_address);

    /**
     * Gets the end of reserved data according to the fix level.
     * @param fix_level fix level from 0 to 3
     * @returns the end of reserved data.
     */
    u32 GetFixEnd(u32 fix_level) const;

    /**
     * Zeros offsets to cropped data according to the fix level and marks as fixed.
     * @param fix_level fix level from 0 to 3
     * @returns page-aligned size of the module after fixing.
     */
    u32 Fix(u32 fix_level);

    bool IsFixed() const {
        return GetField(Magic) == MAGIC_FIXD;
    }

    u32 GetFixedSize() const {
        return GetField(FixedSize);
    }

    bool IsLoaded() const;

    /**
     * Gets the page address and size of the code segment.
     * @returns a tuple of (address, size); (0, 0) if the code segment doesn't exist.
     */
    std::tuple<VAddr, u32> GetExecutablePages() const;

private:
    const VAddr module_address; ///< the virtual address of this module
    Kernel::Process& process;   ///< the owner process of this module
    Core::System& system;

    /**
     * Each item in this enum represents a u32 field in the header begin from address+0x80,
     * successively. We don't directly use a struct here, to avoid GetPointer, reinterpret_cast, or
     * Read/WriteBlock repeatedly.
     */
    enum HeaderField {
        Magic = 0,
        NameOffset,
        NextCRO,
        PreviousCRO,
        FileSize,
        BssSize,
        FixedSize,
        UnknownZero,
        UnkSegmentTag,
        OnLoadSegmentTag,
        OnExitSegmentTag,
        OnUnresolvedSegmentTag,

        CodeOffset,
        CodeSize,
        DataOffset,
        DataSize,
        ModuleNameOffset,
        ModuleNameSize,
        SegmentTableOffset,
        SegmentNum,

        ExportNamedSymbolTableOffset,
        ExportNamedSymbolNum,
        ExportIndexedSymbolTableOffset,
        ExportIndexedSymbolNum,
        ExportStringsOffset,
        ExportStringsSize,
        ExportTreeTableOffset,
        ExportTreeNum,

        ImportModuleTableOffset,
        ImportModuleNum,
        ExternalRelocationTableOffset,
        ExternalRelocationNum,
        ImportNamedSymbolTableOffset,
        ImportNamedSymbolNum,
        ImportIndexedSymbolTableOffset,
        ImportIndexedSymbolNum,
        ImportAnonymousSymbolTableOffset,
        ImportAnonymousSymbolNum,
        ImportStringsOffset,
        ImportStringsSize,

        StaticAnonymousSymbolTableOffset,
        StaticAnonymousSymbolNum,
        InternalRelocationTableOffset,
        InternalRelocationNum,
        StaticRelocationTableOffset,
        StaticRelocationNum,
        Fix0Barrier,

        Fix3Barrier = ExportNamedSymbolTableOffset,
        Fix2Barrier = ImportModuleTableOffset,
        Fix1Barrier = StaticAnonymousSymbolTableOffset,
    };
    static_assert(Fix0Barrier == (CRO_HEADER_SIZE - CRO_HASH_SIZE) / 4,
                  "CRO Header fields are wrong!");

    enum class SegmentType : u32 {
        Code = 0,
        ROData = 1,
        Data = 2,
        BSS = 3,
    };

    /**
     * Identifies a program location inside of a segment.
     * Required to refer to program locations because individual segments may be relocated
     * independently of each other.
     */
    union SegmentTag {
        u32_le raw;
        BitField<0, 4, u32> segment_index;
        BitField<4, 28, u32> offset_into_segment;

        SegmentTag() = default;
        explicit SegmentTag(u32 raw_) : raw(raw_) {}
    };

    /// Information of a segment in this module.
    struct SegmentEntry {
        u32_le offset;
        u32_le size;
        enum_le<SegmentType> type;

        static constexpr HeaderField TABLE_OFFSET_FIELD = SegmentTableOffset;
    };
    ASSERT_CRO_STRUCT(SegmentEntry, 12);

    /// Identifies a named symbol exported from this module.
    struct ExportNamedSymbolEntry {
        u32_le name_offset;         // pointing to a substring in ExportStrings
        SegmentTag symbol_position; // to self's segment

        static constexpr HeaderField TABLE_OFFSET_FIELD = ExportNamedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ExportNamedSymbolEntry, 8);

    /// Identifies an indexed symbol exported from this module.
    struct ExportIndexedSymbolEntry {
        SegmentTag symbol_position; // to self's segment

        static constexpr HeaderField TABLE_OFFSET_FIELD = ExportIndexedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ExportIndexedSymbolEntry, 4);

    /// A tree node in the symbol lookup tree.
    struct ExportTreeEntry {
        u16_le test_bit; // bit address into the name to test
        union Child {
            u16_le raw;
            BitField<0, 15, u16> next_index;
            BitField<15, 1, u16> is_end;
        } left, right;
        u16_le export_table_index; // index of an ExportNamedSymbolEntry

        static constexpr HeaderField TABLE_OFFSET_FIELD = ExportTreeTableOffset;
    };
    ASSERT_CRO_STRUCT(ExportTreeEntry, 8);

    /// Identifies a named symbol imported from another module.
    struct ImportNamedSymbolEntry {
        u32_le name_offset;             // pointing to a substring in ImportStrings
        u32_le relocation_batch_offset; // pointing to a relocation batch in ExternalRelocationTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportNamedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ImportNamedSymbolEntry, 8);

    /// Identifies an indexed symbol imported from another module.
    struct ImportIndexedSymbolEntry {
        u32_le index; // index of an ExportIndexedSymbolEntry in the exporting module
        u32_le relocation_batch_offset; // pointing to a relocation batch in ExternalRelocationTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportIndexedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ImportIndexedSymbolEntry, 8);

    /// Identifies an anonymous symbol imported from another module.
    struct ImportAnonymousSymbolEntry {
        SegmentTag symbol_position;     // in the exporting segment
        u32_le relocation_batch_offset; // pointing to a relocation batch in ExternalRelocationTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportAnonymousSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ImportAnonymousSymbolEntry, 8);

    /// Information of a imported module and symbols imported from it.
    struct ImportModuleEntry {
        u32_le name_offset;                        // pointing to a substring in ImportStrings
        u32_le import_indexed_symbol_table_offset; // pointing to a subtable in
                                                   // ImportIndexedSymbolTable
        u32_le import_indexed_symbol_num;
        u32_le import_anonymous_symbol_table_offset; // pointing to a subtable in
                                                     // ImportAnonymousSymbolTable
        u32_le import_anonymous_symbol_num;

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportModuleTableOffset;

        void GetImportIndexedSymbolEntry(Kernel::Process& process, Memory::MemorySystem& memory,
                                         u32 index, ImportIndexedSymbolEntry& entry) {
            memory.ReadBlock(process,
                             import_indexed_symbol_table_offset +
                                 index * sizeof(ImportIndexedSymbolEntry),
                             &entry, sizeof(ImportIndexedSymbolEntry));
        }

        void GetImportAnonymousSymbolEntry(Kernel::Process& process, Memory::MemorySystem& memory,
                                           u32 index, ImportAnonymousSymbolEntry& entry) {
            memory.ReadBlock(process,
                             import_anonymous_symbol_table_offset +
                                 index * sizeof(ImportAnonymousSymbolEntry),
                             &entry, sizeof(ImportAnonymousSymbolEntry));
        }
    };
    ASSERT_CRO_STRUCT(ImportModuleEntry, 20);

    enum class RelocationType : u8 {
        Nothing = 0,
        AbsoluteAddress = 2,
        RelativeAddress = 3,
        ThumbBranch = 10,
        ArmBranch = 28,
        ModifyArmBranch = 29,
        AbsoluteAddress2 = 38,
        AlignedRelativeAddress = 42,
    };

    struct RelocationEntry {
        SegmentTag target_position; // to self's segment as an ExternalRelocationEntry; to static
                                    // module segment as a StaticRelocationEntry
        RelocationType type;
        u8 is_batch_end;
        u8 is_batch_resolved; // set at a batch beginning if the batch is resolved
        INSERT_PADDING_BYTES(1);
        u32_le addend;
    };

    /// Identifies a normal cross-module relocation.
    struct ExternalRelocationEntry : RelocationEntry {
        static constexpr HeaderField TABLE_OFFSET_FIELD = ExternalRelocationTableOffset;
    };
    ASSERT_CRO_STRUCT(ExternalRelocationEntry, 12);

    /// Identifies a special static relocation (no game is known using this).
    struct StaticRelocationEntry : RelocationEntry {
        static constexpr HeaderField TABLE_OFFSET_FIELD = StaticRelocationTableOffset;
    };
    ASSERT_CRO_STRUCT(StaticRelocationEntry, 12);

    /// Identifies a in-module relocation.
    struct InternalRelocationEntry {
        SegmentTag target_position; // to self's segment
        RelocationType type;
        u8 symbol_segment;
        INSERT_PADDING_BYTES(2);
        u32_le addend;

        static constexpr HeaderField TABLE_OFFSET_FIELD = InternalRelocationTableOffset;
    };
    ASSERT_CRO_STRUCT(InternalRelocationEntry, 12);

    /// Identifies a special static anonymous symbol (no game is known using this).
    struct StaticAnonymousSymbolEntry {
        SegmentTag symbol_position;     // to self's segment
        u32_le relocation_batch_offset; // pointing to a relocation batch in StaticRelocationTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = StaticAnonymousSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(StaticAnonymousSymbolEntry, 8);

    /**
     * Entry size of each table, from Code to StaticRelocationTable.
     * Byte string contents (such as Code) are treated with entries of size 1.
     * This is used for verifying the size of each table and calculating the fix end.
     */
    static const std::array<int, 17> ENTRY_SIZE;

    /// The offset field of the table where to crop for each fix level
    static const std::array<HeaderField, 4> FIX_BARRIERS;

    static constexpr u32 MAGIC_CRO0 = 0x304F5243;
    static constexpr u32 MAGIC_FIXD = 0x44584946;

    VAddr Field(HeaderField field) const {
        return module_address + CRO_HASH_SIZE + field * 4;
    }

    u32 GetField(HeaderField field) const {
        return system.Memory().Read32(Field(field));
    }

    void SetField(HeaderField field, u32 value) {
        system.Memory().Write32(Field(field), value);
    }

    /**
     * Reads an entry in one of module tables.
     * @param index index of the entry
     * @param data where to put the read entry
     * @note the entry type must have the static member TABLE_OFFSET_FIELD
     *       indicating which table the entry is in.
     */
    template <typename T>
    void GetEntry(Memory::MemorySystem& memory, std::size_t index, T& data) const {
        memory.ReadBlock(process,
                         GetField(T::TABLE_OFFSET_FIELD) + static_cast<u32>(index * sizeof(T)),
                         &data, sizeof(T));
    }

    /**
     * Writes an entry to one of module tables.
     * @param index index of the entry
     * @param data the entry data to write
     * @note the entry type must have the static member TABLE_OFFSET_FIELD
     *       indicating which table the entry is in.
     */
    template <typename T>
    void SetEntry(Memory::MemorySystem& memory, std::size_t index, const T& data) {
        memory.WriteBlock(process,
                          GetField(T::TABLE_OFFSET_FIELD) + static_cast<u32>(index * sizeof(T)),
                          &data, sizeof(T));
    }

    /**
     * Converts a segment tag to virtual address in this module.
     * @param segment_tag the segment tag to convert
     * @returns VAddr the virtual address the segment tag points to; 0 if invalid.
     */
    VAddr SegmentTagToAddress(SegmentTag segment_tag) const;

    VAddr NextModule() const {
        return GetField(NextCRO);
    }

    VAddr PreviousModule() const {
        return GetField(PreviousCRO);
    }

    void SetNextModule(VAddr next) {
        SetField(NextCRO, next);
    }

    void SetPreviousModule(VAddr previous) {
        SetField(PreviousCRO, previous);
    }

    /**
     * A helper function iterating over all registered auto-link modules, including the static
     * module.
     * @param crs_address the virtual address of the static module
     * @param func a function object to operate on a module. It accepts one parameter
     *        CROHelper and returns ResultVal<bool>. It should return true to continue the
     * iteration,
     *        false to stop the iteration, or an error code (which will also stop the iteration).
     * @returns Result indicating the result of the operation, ResultSuccess if all iteration
     * success,
     *         otherwise error code of the last iteration.
     */
    template <typename FunctionObject>
    static Result ForEachAutoLinkCRO(Kernel::Process& process, Core::System& system,
                                     VAddr crs_address, FunctionObject func) {
        VAddr current = crs_address;
        while (current != 0) {
            CROHelper cro(current, process, system);
            CASCADE_RESULT(bool next, func(cro));
            if (!next)
                break;
            current = cro.NextModule();
        }
        return ResultSuccess;
    }

    /**
     * Applies a relocation
     * @param target_address where to apply the relocation
     * @param relocation_type the type of the relocation
     * @param addend address addend applied to the relocated symbol
     * @param symbol_address the symbol address to be relocated with
     * @param target_future_address the future address of the target.
     *        Usually equals to target_address, but will be different for a target in .data segment
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyRelocation(VAddr target_address, RelocationType relocation_type, u32 addend,
                           u32 symbol_address, u32 target_future_address);

    /**
     * Clears a relocation to zero
     * @param target_address where to apply the relocation
     * @param relocation_type the type of the relocation
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ClearRelocation(VAddr target_address, RelocationType relocation_type);

    /**
     * Applies or resets a batch of relocations
     * @param batch the virtual address of the first relocation in the batch
     * @param symbol_address the symbol address to be relocated with
     * @param reset false to set the batch to resolved state, true to reset the batch to unresolved
     * state
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyRelocationBatch(VAddr batch, u32 symbol_address, bool reset = false);

    /**
     * Finds an exported named symbol in this module.
     * @param name the name of the symbol to find
     * @return VAddr the virtual address of the symbol; 0 if not found.
     */
    VAddr FindExportNamedSymbol(const std::string& name) const;

    /**
     * Rebases offsets in module header according to module address.
     * @param cro_size the size of the CRO file
     * @returns Result ResultSuccess if all offsets are verified as valid, otherwise error
     * code.
     */
    Result RebaseHeader(u32 cro_size);

    /**
     * Rebases offsets in segment table according to module address.
     * @param cro_size the size of the CRO file
     * @param data_segment_address the buffer address for .data segment
     * @param data_segment_size the buffer size for .data segment
     * @param bss_segment_address the buffer address for .bss segment
     * @param bss_segment_size the buffer size for .bss segment
     * @returns ResultVal<VAddr> with the virtual address of .data segment in CRO.
     */
    ResultVal<VAddr> RebaseSegmentTable(u32 cro_size, VAddr data_segment_address,
                                        u32 data_segment_size, VAddr bss_segment_address,
                                        u32 bss_segment_size);

    /**
     * Rebases offsets in exported named symbol table according to module address.
     * @returns Result ResultSuccess if all offsets are verified as valid, otherwise error
     * code.
     */
    Result RebaseExportNamedSymbolTable();

    /**
     * Verifies indices in export tree table.
     * @returns Result ResultSuccess if all indices are verified as valid, otherwise error
     * code.
     */
    Result VerifyExportTreeTable() const;

    /**
     * Rebases offsets in exported module table according to module address.
     * @returns Result ResultSuccess if all offsets are verified as valid, otherwise error
     * code.
     */
    Result RebaseImportModuleTable();

    /**
     * Rebases offsets in imported named symbol table according to module address.
     * @returns Result ResultSuccess if all offsets are verified as valid, otherwise error
     * code.
     */
    Result RebaseImportNamedSymbolTable();

    /**
     * Rebases offsets in imported indexed symbol table according to module address.
     * @returns Result ResultSuccess if all offsets are verified as valid, otherwise error
     * code.
     */
    Result RebaseImportIndexedSymbolTable();

    /**
     * Rebases offsets in imported anonymous symbol table according to module address.
     * @returns Result ResultSuccess if all offsets are verified as valid, otherwise error
     * code.
     */
    Result RebaseImportAnonymousSymbolTable();

    /**
     * Gets the address of OnUnresolved function in this module.
     * Used as the applied symbol for reset relocation.
     * @returns the virtual address of OnUnresolved. 0 if not provided.
     */
    VAddr GetOnUnresolvedAddress();

    /**
     * Resets all external relocations to unresolved state.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ResetExternalRelocations();

    /**
     * Clears all external relocations to zero.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ClearExternalRelocations();

    /**
     * Applies all static anonymous symbol to the static module.
     * @param crs_address the virtual address of the static module
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyStaticAnonymousSymbolToCRS(VAddr crs_address);

    /**
     * Applies all internal relocations to the module itself.
     * @param old_data_segment_address the virtual address of data segment in CRO buffer
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyInternalRelocations(u32 old_data_segment_address);

    /**
     * Clears all internal relocations to zero.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ClearInternalRelocations();

    /// Unrebases offsets in imported anonymous symbol table
    void UnrebaseImportAnonymousSymbolTable();

    /// Unrebases offsets in imported indexed symbol table
    void UnrebaseImportIndexedSymbolTable();

    /// Unrebases offsets in imported named symbol table
    void UnrebaseImportNamedSymbolTable();

    /// Unrebases offsets in imported module table
    void UnrebaseImportModuleTable();

    /// Unrebases offsets in exported named symbol table
    void UnrebaseExportNamedSymbolTable();

    /// Unrebases offsets in segment table
    void UnrebaseSegmentTable();

    /// Unrebases offsets in module header
    void UnrebaseHeader();

    /**
     * Looks up all imported named symbols of this module in all registered auto-link modules, and
     * resolves them if found.
     * @param crs_address the virtual address of the static module
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyImportNamedSymbol(VAddr crs_address);

    /**
     * Resets all imported named symbols of this module to unresolved state.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ResetImportNamedSymbol();

    /**
     * Resets all imported indexed symbols of this module to unresolved state.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ResetImportIndexedSymbol();

    /**
     * Resets all imported anonymous symbols of this module to unresolved state.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ResetImportAnonymousSymbol();

    /**
     * Finds registered auto-link modules that this module imports, and resolves indexed and
     * anonymous symbols exported by them.
     * @param crs_address the virtual address of the static module
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyModuleImport(VAddr crs_address);

    /**
     * Resolves target module's imported named symbols that exported by this module.
     * @param target the module to resolve.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyExportNamedSymbol(CROHelper target);

    /**
     * Resets target's named symbols imported from this module to unresolved state.
     * @param target the module to reset.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ResetExportNamedSymbol(CROHelper target);

    /**
     * Resolves imported indexed and anonymous symbols in the target module which imports this
     * module.
     * @param target the module to resolve.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyModuleExport(CROHelper target);

    /**
     * Resets target's indexed and anonymous symbol imported from this module to unresolved state.
     * @param target the module to reset.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ResetModuleExport(CROHelper target);

    /**
     * Resolves the exit function in this module
     * @param crs_address the virtual address of the static module.
     * @returns Result ResultSuccess on success, otherwise error code.
     */
    Result ApplyExitRelocations(VAddr crs_address);
};

} // namespace Service::LDR
