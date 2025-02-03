#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>

namespace memplumber {

/**
 * MemorySource: Direct OS memory management through mmap
 * 
 * This class encapsulates low-level memory acquisition from the operating system,
 * bypassing the standard library's malloc/free. It directly uses mmap() to obtain
 * large contiguous blocks of virtual memory.
 * 
 * Key Design Principles:
 * - Page-aligned allocations for optimal OS interaction
 * - Minimal metadata overhead
 * - Direct system call interface
 * - Exception-safe RAII management
 */
class MemorySource {
public:
    // Default page size for most x86_64 systems
    static constexpr size_t DEFAULT_PAGE_SIZE = 4096;
    
    MemorySource();
    ~MemorySource() = default;
    
    /**
     * Allocate a large block of memory from the OS
     * @param size: Requested size in bytes (will be rounded up to page boundaries)
     * @return: Pointer to allocated memory, or nullptr on failure
     */
    void* allocate_block(size_t size);
    
    /**
     * Return memory block to the OS
     * @param ptr: Pointer to memory block (must be from allocate_block)
     * @param size: Size of the block (must match original allocation)
     */
    void deallocate_block(void* ptr, size_t size);
    
    /**
     * Get system page size
     * @return: Page size in bytes
     */
    size_t get_page_size() const { return page_size_; }
    
    /**
     * Round size up to next page boundary
     * @param size: Requested size
     * @return: Page-aligned size
     */
    size_t align_to_page(size_t size) const;
    
    // Statistics for monitoring and debugging
    struct Stats {
        size_t total_allocated = 0;    // Total bytes allocated from OS
        size_t total_deallocated = 0;  // Total bytes returned to OS  
        size_t current_usage = 0;      // Current memory usage
        size_t allocation_count = 0;   // Number of mmap calls
        size_t deallocation_count = 0; // Number of munmap calls
    };
    
    const Stats& get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats{}; }
    
private:
    size_t page_size_;
    Stats stats_;
    
    // Disable copying - this manages OS resources
    MemorySource(const MemorySource&) = delete;
    MemorySource& operator=(const MemorySource&) = delete;
};

} // namespace memplumber
