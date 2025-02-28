#include <cstddef>
#include <mutex>
#include <new>
#include "axontzz/free_list_allocator.h"
#include "axontzz/memory_source.h"

namespace {
    // 全局分配器实例 - 使用静态初始化确保线程安全
    class GlobalAllocatorManager {
    public:
        static GlobalAllocatorManager& instance() {
            static GlobalAllocatorManager instance;
            return instance;
        }
        
        void* allocate(std::size_t size, std::size_t alignment = sizeof(void*)) {
            std::lock_guard<std::mutex> lock(mutex_);
            return allocator_.allocate(size, alignment);
        }
        
        void deallocate(void* ptr, std::size_t size = 0) {
            if (ptr == nullptr) return;
            
            std::lock_guard<std::mutex> lock(mutex_);
            allocator_.deallocate(ptr, size);
        }
        
        bool owns(void* ptr) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return allocator_.owns(ptr);
        }
        
        memplumber::FreeListAllocator::AllocatorStats get_stats() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return allocator_.get_stats();
        }
        
    private:
        GlobalAllocatorManager() : memory_source_(), allocator_(memory_source_, 64 * 1024) {
            // 64KB 初始块大小，适合大多数应用
        }
        
        mutable std::mutex mutex_;
        memplumber::MemorySource memory_source_;
        memplumber::FreeListAllocator allocator_;
        
        // 禁用复制和移动
        GlobalAllocatorManager(const GlobalAllocatorManager&) = delete;
        GlobalAllocatorManager& operator=(const GlobalAllocatorManager&) = delete;
    };
}

// 全局 new 重载
void* operator new(std::size_t size) {
    if (size == 0) {
        size = 1; // C++ 标准要求
    }
    
    void* ptr = GlobalAllocatorManager::instance().allocate(size);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (size == 0) {
        size = 1;
    }
    
    return GlobalAllocatorManager::instance().allocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return operator new(size, std::nothrow);
}

// 全局 delete 重载
void operator delete(void* ptr) noexcept {
    GlobalAllocatorManager::instance().deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept {
    GlobalAllocatorManager::instance().deallocate(ptr, size);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
    operator delete(ptr, size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    operator delete(ptr);
}

// 提供访问全局分配器统计信息的API
namespace memplumber {
    namespace global {
        AllocatorInterface::AllocatorStats get_global_allocator_stats() {
            return GlobalAllocatorManager::instance().get_stats();
        }
        
        bool is_pointer_owned_by_global_allocator(void* ptr) {
            return GlobalAllocatorManager::instance().owns(ptr);
        }
    }
}
