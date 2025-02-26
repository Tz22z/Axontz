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
    
    // 验证这个指针确实属于我们
    if (!owns(ptr)) {
        std::cout << "Warning: Attempt to deallocate pointer not owned by this allocator" << std::endl;
        return;
    }
    // 读取分配头部来恢复真实的分配跨度
    const size_t header_size = sizeof(AllocationHeader);
    char* user_ptr = static_cast<char*>(ptr);
    AllocationHeader* header = reinterpret_cast<AllocationHeader*>(user_ptr - header_size);

    // 先保存头部数据，避免在构造自由块时覆盖
    size_t payload = header->requested;
    size_t free_size = header->span;
    size_t saved_prefix = header->prefix_size;
    char* free_start = reinterpret_cast<char*>(header) - saved_prefix;

    // 构造自由块并回收到自由列表
    FreeBlock* block = reinterpret_cast<FreeBlock*>(free_start);
    block->size = free_size;
    block->next = nullptr;
    block->prev = nullptr;

    std::cout << "Converting allocated span back to free block at " << (void*)free_start
              << " with size " << free_size << std::endl;

    add_to_free_list(block);

    // 尝试合并相邻的自由块
    coalesce_free_blocks();

    // 使用真实请求大小更新统计信息
    std::cout << "Stats before dealloc: current_usage=" << stats_.current_usage
              << ", payload=" << payload << std::endl;
    stats_.total_deallocated += payload;
    stats_.current_usage -= payload;
    stats_.deallocation_count++;
    std::cout << "Stats after dealloc: current_usage=" << stats_.current_usage << std::endl;

    std::cout << "Successfully returned block to free list" << std::endl;
}

bool FreeListAllocator::owns(void* ptr) const {
    if (ptr == nullptr) {
        return false;
    }
    
    // 检查指针是否在我们管理的任何内存区域内
    MemoryRegion* current = regions_head_;
    while (current != nullptr) {
        char* region_start = static_cast<char*>(current->start);
        char* region_end = region_start + current->size;
        char* check_ptr = static_cast<char*>(ptr);
        
        if (check_ptr >= region_start && check_ptr < region_end) {
            std::cout << "Pointer " << ptr << " is owned (in region " 
                      << current->start << "-" << (void*)region_end << ")" << std::endl;
            return true;
        }
        current = current->next;
    }
    
    std::cout << "Pointer " << ptr << " is NOT owned by this allocator" << std::endl;
    return false;
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

    // 查找考虑头部与对齐后的合适块
    FreeBlock* block = find_suitable_block(size, alignment);
    if (block == nullptr) {
        std::cout << "No suitable block found in free list" << std::endl;
        return nullptr;
    }

    // 从自由列表移除选中的块
    remove_from_free_list(block);

    const size_t header_size = sizeof(AllocationHeader);
    char* block_start = reinterpret_cast<char*>(block);
    char* block_end = block_start + block->size;

    // 计算用户指针与头部位置
    char* user_ptr = reinterpret_cast<char*>(align_pointer(block_start + header_size, alignment));
    char* header_addr = user_ptr - header_size;

    size_t prefix_size = static_cast<size_t>(header_addr - block_start);
    char* used_end = user_ptr + size;
    size_t suffix_size = static_cast<size_t>(block_end - used_end);

    // 如果前缀足够大，作为自由块回收
    if (prefix_size >= MIN_BLOCK_SIZE) {
        FreeBlock* prefix = reinterpret_cast<FreeBlock*>(block_start);
        prefix->size = prefix_size;
        prefix->next = prefix->prev = nullptr;
        add_to_free_list(prefix);
        block_start = header_addr; // 分配从头部开始
        prefix_size = 0;
    }

    // 默认 span 覆盖 [block_start, used_end)
    size_t span = static_cast<size_t>(used_end - block_start);

    // 如果尾部足够大，分裂成自由块；否则并入此次分配
    if (suffix_size >= MIN_BLOCK_SIZE) {
        FreeBlock* suffix = reinterpret_cast<FreeBlock*>(used_end);
        suffix->size = suffix_size;
        suffix->next = suffix->prev = nullptr;
        add_to_free_list(suffix);
    } else {
        span = static_cast<size_t>(block_end - block_start);
    }

    // 写入分配头部
    AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_addr);
    header->span = span;
    header->requested = size;
    header->prefix_size = prefix_size;

    std::cout << "Write header at " << (void*)header_addr
              << " {span=" << header->span
              << ", requested=" << header->requested
              << ", prefix=" << header->prefix_size << "}" << std::endl;

    std::cout << "Allocated span=" << span << " at " << (void*)user_ptr
              << " (requested " << size << ")" << std::endl;
    return static_cast<void*>(user_ptr);
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

    const size_t header_size = sizeof(AllocationHeader);
    FreeBlock* current = free_list_head_;
    while (current != nullptr) {
        std::cout << "  Checking block at " << current << " with size " << current->size << std::endl;

        char* block_start = reinterpret_cast<char*>(current);
        char* block_end = block_start + current->size;

        char* user_ptr = reinterpret_cast<char*>(align_pointer(block_start + header_size, alignment));
        if (user_ptr < block_end && user_ptr + size <= block_end) {
            std::cout << "  Found suitable block at " << current << std::endl;
            return current;
        }

        current = current->next;
    }

    std::cout << "  No suitable block found" << std::endl;
    return nullptr;
}

void FreeListAllocator::split_block(FreeBlock* block, size_t needed_size) {
    if (block == nullptr || needed_size == 0) {
        std::cout << "Warning: split_block called with invalid parameters" << std::endl;
        return;
    }
    
    std::cout << "Splitting block " << block << " (size " << block->size 
              << ") to needed size " << needed_size << std::endl;
    
    // 检查是否值得分裂
    if (block->size <= needed_size + MIN_BLOCK_SIZE) {
        std::cout << "Block too small to split, keeping as is" << std::endl;
        return;
    }
    
    // 计算剩余部分
    size_t remaining_size = block->size - needed_size;
    
    // 创建新的自由块从剩余部分
    char* block_start = reinterpret_cast<char*>(block);
    FreeBlock* new_block = reinterpret_cast<FreeBlock*>(block_start + needed_size);
    
    new_block->size = remaining_size;
    new_block->next = nullptr;
    new_block->prev = nullptr;
    
    // 调整原块大小
    block->size = needed_size;
    
    // 将新块添加到自由列表
    add_to_free_list(new_block);
    
    std::cout << "Split complete: original block " << block << " (size " << block->size 
              << "), new block " << new_block << " (size " << new_block->size << ")" << std::endl;
}

void FreeListAllocator::coalesce_free_blocks() {
    std::cout << "Starting coalesce_free_blocks" << std::endl;
    
    if (free_list_head_ == nullptr) {
        return;
    }
    
    bool merged = true;
    int iterations = 0;
    const int max_iterations = 100; // 防止无限循环
    
    while (merged && iterations < max_iterations) {
        merged = false;
        iterations++;
        
        // 遍历自由列表，寻找相邻的块进行合并
        FreeBlock* current = free_list_head_;
        while (current != nullptr) {
            FreeBlock* next_in_list = current->next;
            
            // 检查当前块是否与列表中其他块相邻
            FreeBlock* check = free_list_head_;
            while (check != nullptr) {
                if (check != current) {
                    char* current_start = reinterpret_cast<char*>(current);
                    char* current_end = current_start + current->size;
                    char* check_start = reinterpret_cast<char*>(check);
                    char* check_end = check_start + check->size;
                    
                    // 检查 current 是否紧邻 check 之后
                    if (current_end == check_start) {
                        std::cout << "Merging block " << current << " (size " << current->size 
                                  << ") with block " << check << " (size " << check->size << ")" << std::endl;
                        
                        // 扩展 current 块包含 check 块
                        current->size += check->size;
                        
                        // 从自由列表中移除 check
                        remove_from_free_list(check);
                        
                        merged = true;
                        std::cout << "Merged result: block " << current << " now has size " << current->size << std::endl;
                        break;
                    }
                    
                    // 检查 check 是否紧邻 current 之后  
                    if (check_end == current_start) {
                        std::cout << "Merging block " << check << " (size " << check->size 
                                  << ") with block " << current << " (size " << current->size << ")" << std::endl;
                        
                        // 扩展 check 块包含 current 块
                        check->size += current->size;
                        
                        // 从自由列表中移除 current
                        remove_from_free_list(current);
                        
                        merged = true;
                        std::cout << "Merged result: block " << check << " now has size " << check->size << std::endl;
                        break;
                    }
                }
                check = check->next;
            }
            
            if (merged) {
                break; // 重新开始扫描
            }
            
            current = next_in_list;
        }
    }
    
    if (iterations >= max_iterations) {
        std::cout << "Warning: coalesce_free_blocks hit iteration limit" << std::endl;
    }
    
    std::cout << "Coalesce completed after " << iterations << " iterations" << std::endl;
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
