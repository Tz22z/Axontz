#include "axontzz/free_list_allocator.h"
#include <cassert>
#include <iostream>
#include <algorithm>

namespace memplumber {

FreeListAllocator::FreeListAllocator(MemorySource& memory_source, size_t initial_block_size)
    : memory_source_(memory_source)
    , free_list_head_(nullptr)
    , regions_head_(nullptr)
    , stats_{}
    , default_block_size_(initial_block_size) {
    
    std::cout << "FreeListAllocator created with block size: " << initial_block_size << std::endl;
    
    // TODO: 在后续版本中添加初始内存分配
}

FreeListAllocator::~FreeListAllocator() {
    std::cout << "FreeListAllocator destroyed" << std::endl;
    
    // TODO: 在后续版本中释放所有内存区域
}

void* FreeListAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }
    
    std::cout << "Allocating " << size << " bytes (alignment: " << alignment << ")" << std::endl;
    
    // 临时实现：直接从MemorySource分配
    // TODO: 在后续版本中实现真正的自由列表分配
    void* ptr = memory_source_.allocate_block(size);
    
    if (ptr != nullptr) {
        stats_.total_allocated += size;
        stats_.current_usage += size;
        stats_.allocation_count++;
    } else {
        stats_.failed_allocations++;
    }
    
    return ptr;
}

void FreeListAllocator::deallocate(void* ptr, size_t size) {
    if (ptr == nullptr) {
        return;
    }
    
    std::cout << "Deallocating " << size << " bytes at " << ptr << std::endl;
    
    // 临时实现：直接释放回MemorySource
    // TODO: 在后续版本中实现真正的自由列表管理
    memory_source_.deallocate_block(ptr, size);
    
    stats_.total_deallocated += size;
    stats_.current_usage -= size;
    stats_.deallocation_count++;
}

bool FreeListAllocator::owns(void* ptr) const {
    // 临时实现：假设我们拥有所有非空指针
    // TODO: 在后续版本中实现真正的所有权检查
    return ptr != nullptr;
}

FreeListAllocator::AllocatorStats FreeListAllocator::get_stats() const {
    return stats_;
}

void FreeListAllocator::reset_stats() {
    stats_ = AllocatorStats{};
}

// TODO: 在后续版本中实现这些私有方法
void* FreeListAllocator::allocate_from_free_list(size_t size, size_t alignment) {
    // 暂未实现
    return nullptr;
}

void FreeListAllocator::add_to_free_list(FreeBlock* block) {
    // 暂未实现
}

void FreeListAllocator::remove_from_free_list(FreeBlock* block) {
    // 暂未实现
}

FreeListAllocator::FreeBlock* FreeListAllocator::find_suitable_block(size_t size, size_t alignment) {
    // 暂未实现
    return nullptr;
}

void FreeListAllocator::split_block(FreeBlock* block, size_t needed_size) {
    // 暂未实现
}

void FreeListAllocator::coalesce_free_blocks() {
    // 暂未实现
}

bool FreeListAllocator::expand_heap(size_t min_size) {
    // 暂未实现
    return false;
}

size_t FreeListAllocator::align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

bool FreeListAllocator::is_aligned(void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

void* FreeListAllocator::align_pointer(void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

bool FreeListAllocator::validate_free_list() const {
    // 暂时总是返回true
    return true;
}

void FreeListAllocator::dump_free_list() const {
    std::cout << "=== Free List Dump (Basic Version) ===" << std::endl;
    std::cout << "Current stats:" << std::endl;
    std::cout << "  Total allocated: " << stats_.total_allocated << " bytes" << std::endl;
    std::cout << "  Current usage: " << stats_.current_usage << " bytes" << std::endl;
    std::cout << "  Allocations: " << stats_.allocation_count << std::endl;
    std::cout << "  Deallocations: " << stats_.deallocation_count << std::endl;
    std::cout << "===================================" << std::endl;
}

} // namespace memplumber
