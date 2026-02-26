// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include "core/arm/fastinterp/fastinterp_types.h"

namespace Core::FastInterp {

// ============================================================================
// TLB-Style Block Cache
// ============================================================================
// Fixed-size, 4-way set-associative block cache (chosen over unordered_map for
// O(1) lookups with no per-entry allocation), with LRU replacement.

/// Number of sets in the cache (power of 2)
/// 4096 sets * 4 ways * ~660 bytes per block = ~10MB cache
constexpr size_t CACHE_SETS = 4096;

/// Number of ways per set (associativity)
constexpr size_t CACHE_WAYS = 4;

/// Total number of cache entries
constexpr size_t CACHE_SIZE = CACHE_SETS * CACHE_WAYS;

/// Invalid tag marker (a guest PC of 0xFFFFFFFF can never be a real block start)
constexpr u32 INVALID_TAG = 0xFFFFFFFF;

/// Set-associative block cache with LRU replacement
class BlockCache {
public:
    BlockCache() {
        Clear();
    }

    /// Clear all cache entries
    void Clear() {
        tags_.fill(INVALID_TAG);
        for (auto& block : blocks_) {
            block.inst_count = 0;
            block.chain_target = nullptr; // Prevent dangling pointers
        }
        std::memset(lru_state_.data(), 0, lru_state_.size());
    }

    /// Invalidate cache entries whose decoded range intersects [start_addr, start_addr+length)
    void InvalidateRange(u32 start_addr, u32 length) {
        // Set indices are hashed, so an address range does not map to a set
        // range: walk every entry and match on the block's own address range.
        // inst_count is zeroed so surviving pointers to the entry (chain
        // targets) fizzle instead of executing the stale decode.
        const u64 end_addr = static_cast<u64>(start_addr) + length;
        for (size_t i = 0; i < CACHE_SIZE; ++i) {
            if (tags_[i] == INVALID_TAG) {
                continue;
            }
            if (blocks_[i].start_pc < end_addr && blocks_[i].end_pc > start_addr) {
                tags_[i] = INVALID_TAG;
                blocks_[i].inst_count = 0;
                blocks_[i].chain_target = nullptr;
            }
        }
    }

    /// Look up a block by PC (returns nullptr if not found)
    BasicBlock* Lookup(u32 pc) {
        u32 set_idx = GetSetIndex(pc);
        u32 base = set_idx * CACHE_WAYS;

        for (u32 way = 0; way < CACHE_WAYS; ++way) {
            if (tags_[base + way] == pc) {
                UpdateLRU(set_idx, way);
                return &blocks_[base + way];
            }
        }
        return nullptr;
    }

    /// Look up a block, allocating an LRU victim on miss; sets was_hit
    BasicBlock* LookupOrAllocate(u32 pc, bool& was_hit) {
        u32 set_idx = GetSetIndex(pc);
        u32 base = set_idx * CACHE_WAYS;

        for (u32 way = 0; way < CACHE_WAYS; ++way) {
            if (tags_[base + way] == pc) {
                UpdateLRU(set_idx, way);
                was_hit = true;
                return &blocks_[base + way];
            }
        }

        was_hit = false;
        u32 victim_way = GetLRUVictim(set_idx);
        BasicBlock& block = blocks_[base + victim_way];

        tags_[base + victim_way] = pc;
        block.start_pc = pc;
        block.end_pc = pc;
        block.inst_count = 0;
        block.exec_count = 0;
        block.flags = 0;
        block.chain_target = nullptr;

        UpdateLRU(set_idx, victim_way);
        return &block;
    }

private:
    /// Hash a PC to a set index. Multiply-xor-shift spreads the aligned,
    /// clustered PCs of real code across sets better than bit extraction.
    static u32 GetSetIndex(u32 pc) {
        constexpr u32 HASH_MULT = 0x9E3779B9; // 2^32 / golden ratio
        u32 hash = pc * HASH_MULT;
        hash ^= (hash >> 16);
        return (hash >> 4) & (CACHE_SETS - 1);
    }

    /// Update LRU state when way is accessed
    void UpdateLRU(u32 set_idx, u32 accessed_way) {
        // Simple pseudo-LRU using a 4-bit field per set
        // Bit i is set if way i was recently used
        u8& lru = lru_state_[set_idx];
        lru |= (1 << accessed_way);
        if (lru == 0x0F) {
            // All ways recently used, clear except accessed
            lru = (1 << accessed_way);
        }
    }

    /// Get LRU victim for eviction
    u32 GetLRUVictim(u32 set_idx) {
        u8 lru = lru_state_[set_idx];
        // Find first way that wasn't recently used
        for (u32 way = 0; way < CACHE_WAYS; ++way) {
            if (!(lru & (1 << way))) {
                return way;
            }
        }
        // All recently used, pick way 0
        return 0;
    }

    // Tags hold the full PC, so a match needs no start_pc re-check. Split from
    // the blocks (structure-of-arrays): a 4-way set probe touches one cache
    // line of tags instead of 4 far-apart block headers.
    std::array<u32, CACHE_SIZE> tags_;
    std::array<BasicBlock, CACHE_SIZE> blocks_;
    std::array<u8, CACHE_SETS> lru_state_;
};

} // namespace Core::FastInterp
