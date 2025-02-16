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
    
    // 确保对齐是2的幂
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        alignment = sizeof(void*);
    }
    
    std::cout << "Allocating " << size << " bytes (alignment: " << alignment << ")" << std::endl;
    
    // 首先尝试从自由列表分配
    void* ptr = allocate_from_free_list(size, alignment);
    
    if (ptr == nullptr) {
        // 自由列表中没有合适的块，需要扩展堆
        std::cout << "No suitable block in free list, expanding heap" << std::endl;
        
        size_t expand_size = std::max(size + alignment, default_block_size_);
        if (!expand_heap(expand_size)) {
            std::cout << "Failed to expand heap" << std::endl;
            stats_.failed_allocations++;
            return nullptr;
        }
        
        // 扩展后再次尝试分配
        ptr = allocate_from_free_list(size, alignment);
    }
    
    if (ptr != nullptr) {
        stats_.total_allocated += size;
        stats_.current_usage += size;
        stats_.allocation_count++;
        std::cout << "Successfully allocated " << size << " bytes at " << ptr << std::endl;
    } else {
        stats_.failed_allocations++;
        std::cout << "Allocation failed for " << size << " bytes" << std::endl;
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
    std::cout << "Trying to allocate " << size << " bytes from free list" << std::endl;
    
    // 查找合适的块
    FreeBlock* suitable_block = find_suitable_block(size, alignment);
    if (suitable_block == nullptr) {
        std::cout << "No suitable block found in free list" << std::endl;
        return nullptr;
    }
    
    // 从自由列表中移除这个块
    remove_from_free_list(suitable_block);
    
    // 计算对齐的指针
    void* aligned_ptr = align_pointer(suitable_block, alignment);
    
    // 简单实现：暂时不分割块，直接返回整个块
    // TODO: 后续实现split_block来提高内存利用率
    
    std::cout << "Allocated " << suitable_block->size << " bytes at " << aligned_ptr 
              << " (requested " << size << " bytes)" << std::endl;
    
    return aligned_ptr;
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
    if (block == nullptr) {
        std::cout << "Warning: Attempted to remove null block from free list" << std::endl;
        return;
    }
    
    std::cout << "Removing block " << block << " from free list" << std::endl;
    
    // 更新前驱节点的next指针
    if (block->prev != nullptr) {
        block->prev->next = block->next;
    } else {
        // 这是头节点
        free_list_head_ = block->next;
    }
    
    // 更新后继节点的prev指针
    if (block->next != nullptr) {
        block->next->prev = block->prev;
    }
    
    // 清理被移除块的指针
    block->next = nullptr;
    block->prev = nullptr;
    
    std::cout << "Block removed, new head: " << free_list_head_ << std::endl;
}

FreeListAllocator::FreeBlock* FreeListAllocator::find_suitable_block(size_t size, size_t alignment) {
    std::cout << "Looking for block of size " << size << " with alignment " << alignment << std::endl;
    
    FreeBlock* current = free_list_head_;
    while (current != nullptr) {
        std::cout << "  Checking block at " << current << " with size " << current->size << std::endl;
        
        // 检查这个块是否足够大
        if (current->size >= size) {
            // 检查对齐要求
            char* block_start = reinterpret_cast<char*>(current);
            char* aligned_start = reinterpret_cast<char*>(align_pointer(block_start, alignment));
            char* block_end = block_start + current->size;
            
            // 确保对齐后仍有足够空间
            if (aligned_start + size <= block_end) {
                std::cout << "  Found suitable block at " << current << std::endl;
                return current;
            }
        }
        
        current = current->next;
    }
    
    std::cout << "  No suitable block found" << std::endl;
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
