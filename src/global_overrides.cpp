#include <cstddef>
#include <cstdlib>

// Placeholder global new/delete overrides
// These will be properly implemented once we have the complete allocator

void* operator new(std::size_t size) {
    // For now, fall back to malloc to avoid infinite recursion
    // This will be replaced with our custom allocator
    return malloc(size);
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    operator delete(ptr);
}
