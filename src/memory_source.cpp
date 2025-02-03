#include "memory_source.hpp"
#include <cassert>
#include <iostream>

namespace memplumber {

MemorySource::MemorySource() 
    : page_size_(static_cast<size_t>(getpagesize()))
    , stats_{} {
    // Verify we got a reasonable page size
    assert(page_size_ > 0 && page_size_ <= 65536);
    assert((page_size_ & (page_size_ - 1)) == 0); // Must be power of 2
}

void* MemorySource::allocate_block(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    
    // Round up to page boundary
    size_t aligned_size = align_to_page(size);
    
    // Use mmap to get memory directly from OS
    // MAP_PRIVATE | MAP_ANONYMOUS gives us a private, zero-filled mapping
    void* ptr = mmap(nullptr,                    // Let kernel choose address
                     aligned_size,               // Size (page-aligned)
                     PROT_READ | PROT_WRITE,     // Read/write permissions
                     MAP_PRIVATE | MAP_ANONYMOUS, // Private, not backed by file
                     -1,                         // No file descriptor
                     0);                         // No offset
    
    if (ptr == MAP_FAILED) {
        // mmap failed - could be out of virtual address space or memory
        return nullptr;
    }
    
    // Update statistics
    stats_.total_allocated += aligned_size;
    stats_.current_usage += aligned_size;
    stats_.allocation_count++;
    
    return ptr;
}

void MemorySource::deallocate_block(void* ptr, size_t size) {
    if (ptr == nullptr || size == 0) {
        return;
    }
    
    size_t aligned_size = align_to_page(size);
    
    // Return memory to OS
    int result = munmap(ptr, aligned_size);
    
    if (result == 0) {
        // Success
        stats_.total_deallocated += aligned_size;
        stats_.current_usage -= aligned_size;
        stats_.deallocation_count++;
    } else {
        // munmap failed - this is a serious error
        std::cerr << "Warning: munmap failed for ptr=" << ptr 
                  << " size=" << aligned_size << std::endl;
    }
}

size_t MemorySource::align_to_page(size_t size) const {
    // Round up to next page boundary
    // Formula: (size + page_size - 1) & ~(page_size - 1)
    // This works because page_size is always a power of 2
    return (size + page_size_ - 1) & ~(page_size_ - 1);
}

} // namespace memplumber
