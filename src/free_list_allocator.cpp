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
    
    // 确保默认块大小足够大
    default_block_size_ = std::max(default_block_size_, 
                                   sizeof(MemoryRegion) + sizeof(FreeBlock) + 256);
    
    // 创建初始内存区域
    if (!expand_heap(default_block_size_)) {
        throw std::bad_alloc();
    }
    
    std::cout << "FreeListAllocator initialization complete" << std::endl;
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
    if (block == nullptr) {
        std::cout << "Warning: Attempted to add null block to free list" << std::endl;
        return;
    }
    
    std::cout << "Adding block " << block << " (size: " << block->size << ") to free list" << std::endl;
    
    // 简单的头部插入策略
    block->next = free_list_head_;
    block->prev = nullptr;
    
    if (free_list_head_ != nullptr) {
        free_list_head_->prev = block;
    }
    
    free_list_head_ = block;
    
    std::cout << "Free list head now: " << free_list_head_ << std::endl;
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
    std::cout << "Expanding heap with min_size: " << min_size << std::endl;
    
    // 确保请求的大小至少能容纳区域描述符和一个自由块
    size_t region_size = std::max(min_size, default_block_size_);
    region_size = std::max(region_size, sizeof(MemoryRegion) + sizeof(FreeBlock));
    
    // 从OS获取内存
    void* new_region = memory_source_.allocate_block(region_size);
    if (new_region == nullptr) {
        std::cout << "Failed to allocate " << region_size << " bytes from OS" << std::endl;
        return false;
    }
    
    std::cout << "Got " << region_size << " bytes from OS at " << new_region << std::endl;
    
    // 在区域开始处放置区域描述符
    MemoryRegion* region_desc = static_cast<MemoryRegion*>(new_region);
    region_desc->start = new_region;
    region_desc->size = region_size;
    region_desc->next = regions_head_;
    
    // 将新区域链接到区域列表
    regions_head_ = region_desc;
    
    // 在区域描述符后创建自由块
    char* region_start = static_cast<char*>(new_region);
    char* free_block_start = region_start + sizeof(MemoryRegion);
    FreeBlock* free_block = reinterpret_cast<FreeBlock*>(free_block_start);
    
    // 设置自由块
    free_block->size = region_size - sizeof(MemoryRegion);
    free_block->next = nullptr;
    free_block->prev = nullptr;
    
    std::cout << "Created free block at " << free_block << " with size " << free_block->size << std::endl;
    
    // 将自由块添加到自由列表
    add_to_free_list(free_block);
    
    return true;
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
