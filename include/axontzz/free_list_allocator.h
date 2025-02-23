#pragma once

#include "allocator_interface.h"
#include "memory_source.h"
#include <cstddef>
#include <cstdint>

namespace memplumber {

/**
 * Free-List Allocator Implementation
 * 
 * This is a classic allocator design that maintains linked lists of free memory blocks.
 * It's the foundation allocator that demonstrates basic dynamic memory management
 * principles before moving to more sophisticated strategies.
 * 
 * Key Features:
 * - Segregated free lists for different size classes
 * - Basic coalescing of adjacent free blocks
 * - First-fit allocation strategy
 * - Metadata stored in-band (within free blocks)
 * 
 * Performance Characteristics:
 * - Allocation: O(n) in worst case (linear search)
 * - Deallocation: O(1) insertion + O(n) coalescing
 * - Space overhead: ~8-16 bytes per free block for metadata
 */
class FreeListAllocator : public AllocatorInterface {
public:
    /**
     * Constructor
     * @param memory_source: Source for obtaining large memory blocks from OS
     * @param initial_block_size: Size of initial memory block to request
     */
    explicit FreeListAllocator(MemorySource& memory_source, 
                              size_t initial_block_size = 1024 * 1024); // 1MB default
    
    ~FreeListAllocator() override;
    
    // AllocatorInterface implementation
    void* allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    bool owns(void* ptr) const override;
    AllocatorStats get_stats() const override;
    void reset_stats() override;
    const char* get_name() const override { return "FreeListAllocator"; }
    
private:
    // Per-allocation header placed immediately before the user pointer
    struct AllocationHeader {
        size_t span;         // Total bytes consumed from the original free block
        size_t requested;    // Payload size requested by caller
        size_t prefix_size;  // Bytes before header absorbed from the original block
    };

    // Free block header - stored at the beginning of each free block
    struct FreeBlock {
        size_t size;           // Size of this free block (including header)
        FreeBlock* next;       // Next block in free list
        FreeBlock* prev;       // Previous block in free list (for fast removal)
    };
    
    // Minimum allocation size must accommodate the free block header
    static constexpr size_t MIN_BLOCK_SIZE = sizeof(FreeBlock);
    
    // Memory region descriptor - tracks OS allocations
    struct MemoryRegion {
        void* start;           // Start of memory region
        size_t size;           // Size of region
        MemoryRegion* next;    // Next region in list
    };
    
    MemorySource& memory_source_;
    FreeBlock* free_list_head_;    // Head of free block list
    MemoryRegion* regions_head_;   // Head of memory regions list
    AllocatorStats stats_;
    size_t default_block_size_;
    
    // Internal helper methods
    void* allocate_from_free_list(size_t size, size_t alignment);
    void add_to_free_list(FreeBlock* block);
    void remove_from_free_list(FreeBlock* block);
    FreeBlock* find_suitable_block(size_t size, size_t alignment);
    void split_block(FreeBlock* block, size_t needed_size);
    void coalesce_free_blocks();
    bool expand_heap(size_t min_size);
    
    // Alignment and size utilities
    static size_t align_size(size_t size, size_t alignment);
    static bool is_aligned(void* ptr, size_t alignment);
    static void* align_pointer(void* ptr, size_t alignment);
    
public:
    // Validation and debugging (public for testing)
    bool validate_free_list() const;
    void dump_free_list() const;

private:
    
    // Disable copying
    FreeListAllocator(const FreeListAllocator&) = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;
};

} // namespace memplumber
