#pragma once

#include <cstddef>
#include <mutex>

namespace memplumber {

/**
 * Abstract base class for all allocators
 * 
 * This interface defines the common API that all allocator implementations
 * must provide. It ensures consistency and allows for polymorphic usage
 * of different allocation strategies.
 */
class AllocatorInterface {
public:
    virtual ~AllocatorInterface() = default;
    
    /**
     * Allocate memory block
     * @param size: Number of bytes to allocate
     * @param alignment: Required alignment (default: natural alignment)
     * @return: Pointer to allocated memory, or nullptr on failure
     */
    virtual void* allocate(size_t size, size_t alignment = sizeof(void*)) = 0;
    
    /**
     * Deallocate previously allocated memory
     * @param ptr: Pointer to memory (must be from this allocator)
     * @param size: Size of the allocation (may be ignored by some allocators)
     */
    virtual void deallocate(void* ptr, size_t size = 0) = 0;
    
    /**
     * Check if this allocator owns the given pointer
     * @param ptr: Pointer to check
     * @return: true if this allocator owns the pointer
     */
    virtual bool owns(void* ptr) const = 0;
    
    /**
     * Get allocation statistics
     */
    struct AllocatorStats {
        size_t total_allocated = 0;      // Total bytes allocated
        size_t total_deallocated = 0;    // Total bytes deallocated  
        size_t current_usage = 0;        // Current memory usage
        size_t allocation_count = 0;     // Number of allocate() calls
        size_t deallocation_count = 0;   // Number of deallocate() calls
        size_t failed_allocations = 0;   // Number of failed allocations
        double fragmentation_ratio = 0.0; // Internal fragmentation ratio
    };
    
    virtual AllocatorStats get_stats() const = 0;
    virtual void reset_stats() = 0;
    
    /**
     * Get name of this allocator (for debugging/benchmarking)
     */
    virtual const char* get_name() const = 0;
    
protected:
    // Helper for thread-safe implementations
    mutable std::mutex mutex_;
};

/**
 * Thread-safe wrapper for any allocator
 * 
 * This class wraps any allocator implementation and makes it thread-safe
 * by adding mutex protection around all operations.
 */
template<typename AllocatorType>
class ThreadSafeAllocator : public AllocatorInterface {
public:
    template<typename... Args>
    explicit ThreadSafeAllocator(Args&&... args) 
        : allocator_(std::forward<Args>(args)...) {}
    
    void* allocate(size_t size, size_t alignment = sizeof(void*)) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocator_.allocate(size, alignment);
    }
    
    void deallocate(void* ptr, size_t size = 0) override {
        std::lock_guard<std::mutex> lock(mutex_);
        allocator_.deallocate(ptr, size);
    }
    
    bool owns(void* ptr) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocator_.owns(ptr);
    }
    
    AllocatorStats get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocator_.get_stats();
    }
    
    void reset_stats() override {
        std::lock_guard<std::mutex> lock(mutex_);
        allocator_.reset_stats();
    }
    
    const char* get_name() const override {
        return allocator_.get_name();
    }
    
private:
    AllocatorType allocator_;
};

} // namespace memplumber
