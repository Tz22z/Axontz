#include "axontzz/benchmark_suite.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace memplumber {

BenchmarkSuite::BenchmarkSuite() {
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::test_sequential_allocation(
    AllocatorInterface& allocator,
    size_t num_objects,
    size_t object_size) {

    BenchmarkResult result;
    result.allocator_name = allocator.get_name();
    result.test_name = "顺序分配";
    result.operations = num_objects * 2; // 分配 + 释放
    result.success = true;

    std::vector<void*> pointers;
    pointers.reserve(num_objects);

    Timer timer;

    // 分配阶段
    for (size_t i = 0; i < num_objects; ++i) {
        void* ptr = allocator.allocate(object_size);
        if (ptr == nullptr) {
            result.success = false;
            result.error_message = "分配失败";
            return result;
        }
        pointers.push_back(ptr);
    }

    auto stats_after_alloc = allocator.get_stats();
    result.peak_memory_usage = stats_after_alloc.current_usage;

    // 释放阶段
    for (void* ptr : pointers) {
        allocator.deallocate(ptr, object_size);
    }

    result.duration_ms = timer.elapsed_ms();
    result.avg_latency_ns = (timer.elapsed_ns() / result.operations);
    result.throughput_ops_per_sec = (result.operations / result.duration_ms) * 1000.0;

    auto final_stats = allocator.get_stats();
    result.final_memory_usage = final_stats.current_usage;
    result.fragmentation_ratio = final_stats.fragmentation_ratio;

    return result;
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::test_random_allocation(
    AllocatorInterface& allocator,
    size_t num_operations,
    size_t min_size,
    size_t max_size) {

    BenchmarkResult result;
    result.allocator_name = allocator.get_name();
    result.test_name = "随机分配";
    result.operations = num_operations;
    result.success = true;

    std::vector<std::pair<void*, size_t>> allocated;
    Timer timer;

    for (size_t i = 0; i < num_operations; ++i) {
        // 50% 概率分配，50% 概率释放
        if (allocated.empty() || (rand() % 2 == 0 && allocated.size() < num_operations / 2)) {
            // 分配
            size_t size = random_size(min_size, max_size);
            void* ptr = allocator.allocate(size);
            if (ptr != nullptr) {
                allocated.push_back({ptr, size});
            }
        } else {
            // 释放随机对象
            size_t index = rand() % allocated.size();
            allocator.deallocate(allocated[index].first, allocated[index].second);
            allocated.erase(allocated.begin() + index);
        }
    }

    auto stats_peak = allocator.get_stats();
    result.peak_memory_usage = stats_peak.current_usage;

    // 清理剩余对象
    for (const auto& alloc : allocated) {
        allocator.deallocate(alloc.first, alloc.second);
    }

    result.duration_ms = timer.elapsed_ms();
    result.avg_latency_ns = (timer.elapsed_ns() / result.operations);
    result.throughput_ops_per_sec = (result.operations / result.duration_ms) * 1000.0;

    auto final_stats = allocator.get_stats();
    result.final_memory_usage = final_stats.current_usage;
    result.fragmentation_ratio = final_stats.fragmentation_ratio;

    return result;
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::test_mixed_sizes(
    AllocatorInterface& allocator,
    size_t num_objects) {

    BenchmarkResult result;
    result.allocator_name = allocator.get_name();
    result.test_name = "混合大小";
    result.operations = num_objects * 2;
    result.success = true;

    // 不同大小的分配
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    std::vector<std::pair<void*, size_t>> pointers;
    Timer timer;

    for (size_t i = 0; i < num_objects; ++i) {
        size_t size = sizes[i % num_sizes];
        void* ptr = allocator.allocate(size);
        if (ptr != nullptr) {
            pointers.push_back({ptr, size});
        }
    }

    auto stats_after_alloc = allocator.get_stats();
    result.peak_memory_usage = stats_after_alloc.current_usage;

    for (const auto& alloc : pointers) {
        allocator.deallocate(alloc.first, alloc.second);
    }

    result.duration_ms = timer.elapsed_ms();
    result.avg_latency_ns = (timer.elapsed_ns() / result.operations);
    result.throughput_ops_per_sec = (result.operations / result.duration_ms) * 1000.0;

    auto final_stats = allocator.get_stats();
    result.final_memory_usage = final_stats.current_usage;
    result.fragmentation_ratio = final_stats.fragmentation_ratio;

    return result;
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::test_fragmentation(
    AllocatorInterface& allocator,
    size_t num_cycles) {

    BenchmarkResult result;
    result.allocator_name = allocator.get_name();
    result.test_name = "碎片化测试";
    result.operations = num_cycles * 20;
    result.success = true;

    Timer timer;

    for (size_t cycle = 0; cycle < num_cycles; ++cycle) {
        std::vector<void*> pointers;

        // 分配10个对象
        for (int i = 0; i < 10; ++i) {
            void* ptr = allocator.allocate(128);
            if (ptr != nullptr) {
                pointers.push_back(ptr);
            }
        }

        // 释放奇数索引的对象（创建"洞"）
        for (size_t i = 1; i < pointers.size(); i += 2) {
            allocator.deallocate(pointers[i], 128);
            pointers[i] = nullptr;
        }

        // 释放剩余的对象
        for (void* ptr : pointers) {
            if (ptr != nullptr) {
                allocator.deallocate(ptr, 128);
            }
        }
    }

    result.duration_ms = timer.elapsed_ms();
    result.avg_latency_ns = (timer.elapsed_ns() / result.operations);
    result.throughput_ops_per_sec = (result.operations / result.duration_ms) * 1000.0;

    auto final_stats = allocator.get_stats();
    result.final_memory_usage = final_stats.current_usage;
    result.peak_memory_usage = result.final_memory_usage;
    result.fragmentation_ratio = final_stats.fragmentation_ratio;

    return result;
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::test_bulk_allocation(
    AllocatorInterface& allocator,
    size_t num_objects,
    size_t object_size) {

    BenchmarkResult result;
    result.allocator_name = allocator.get_name();
    result.test_name = "批量分配";
    result.operations = num_objects * 2;
    result.success = true;

    std::vector<void*> pointers;
    pointers.reserve(num_objects);

    Timer timer;

    // 快速分配大量对象
    for (size_t i = 0; i < num_objects; ++i) {
        void* ptr = allocator.allocate(object_size);
        if (ptr != nullptr) {
            pointers.push_back(ptr);
        } else {
            result.success = false;
            result.error_message = "批量分配失败";
        }
    }

    auto stats_after_alloc = allocator.get_stats();
    result.peak_memory_usage = stats_after_alloc.current_usage;

    // 快速释放
    for (void* ptr : pointers) {
        allocator.deallocate(ptr, object_size);
    }

    result.duration_ms = timer.elapsed_ms();
    result.avg_latency_ns = (timer.elapsed_ns() / result.operations);
    result.throughput_ops_per_sec = (result.operations / result.duration_ms) * 1000.0;

    auto final_stats = allocator.get_stats();
    result.final_memory_usage = final_stats.current_usage;
    result.fragmentation_ratio = final_stats.fragmentation_ratio;

    return result;
}

std::vector<BenchmarkSuite::BenchmarkResult> BenchmarkSuite::run_full_suite(
    AllocatorInterface& allocator,
    const BenchmarkConfig& config) {

    std::vector<BenchmarkResult> results;

    std::cout << "运行完整基准测试套件：" << allocator.get_name() << std::endl;

    // 顺序分配
    if (config.verbose) std::cout << "  - 顺序分配测试..." << std::endl;
    results.push_back(test_sequential_allocation(allocator, config.num_iterations, 128));

    // 随机分配
    if (config.verbose) std::cout << "  - 随机分配测试..." << std::endl;
    results.push_back(test_random_allocation(allocator, config.num_iterations,
                                            config.min_size, config.max_size));

    // 混合大小
    if (config.verbose) std::cout << "  - 混合大小测试..." << std::endl;
    results.push_back(test_mixed_sizes(allocator, config.num_iterations));

    // 碎片化测试
    if (config.verbose) std::cout << "  - 碎片化测试..." << std::endl;
    results.push_back(test_fragmentation(allocator, config.num_iterations / 10));

    // 批量分配
    if (config.verbose) std::cout << "  - 批量分配测试..." << std::endl;
    results.push_back(test_bulk_allocation(allocator, config.num_iterations, 256));

    return results;
}

void BenchmarkSuite::print_result(const BenchmarkResult& result) {
    std::cout << "\n=== " << result.test_name << " ===" << std::endl;
    std::cout << "分配器：" << result.allocator_name << std::endl;
    std::cout << "操作数：" << result.operations << std::endl;
    std::cout << "耗时：" << std::fixed << std::setprecision(2) << result.duration_ms << " ms" << std::endl;
    std::cout << "吞吐量：" << std::fixed << std::setprecision(0)
              << result.throughput_ops_per_sec << " ops/sec" << std::endl;
    std::cout << "平均延迟：" << std::fixed << std::setprecision(0)
              << result.avg_latency_ns << " ns" << std::endl;
    std::cout << "峰值内存：" << result.peak_memory_usage << " 字节" << std::endl;
    std::cout << "最终内存：" << result.final_memory_usage << " 字节" << std::endl;
    std::cout << "碎片率：" << std::fixed << std::setprecision(2)
              << (result.fragmentation_ratio * 100.0) << "%" << std::endl;

    if (!result.success) {
        std::cout << "错误：" << result.error_message << std::endl;
    }
}

void BenchmarkSuite::print_comparison(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) return;

    std::cout << "\n╔═══════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                         基准测试对比结果                              ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝" << std::endl;

    std::cout << std::left << std::setw(20) << "分配器"
              << std::setw(15) << "测试"
              << std::right << std::setw(12) << "吞吐量(K/s)"
              << std::setw(12) << "延迟(ns)"
              << std::setw(12) << "碎片率(%)" << std::endl;
    std::cout << std::string(71, '-') << std::endl;

    for (const auto& result : results) {
        std::cout << std::left << std::setw(20) << result.allocator_name
                  << std::setw(15) << result.test_name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1)
                  << (result.throughput_ops_per_sec / 1000.0)
                  << std::setw(12) << std::fixed << std::setprecision(0)
                  << result.avg_latency_ns
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << (result.fragmentation_ratio * 100.0) << std::endl;
    }
}

void BenchmarkSuite::compare_allocators(
    AllocatorInterface& allocator1,
    AllocatorInterface& allocator2,
    const BenchmarkConfig& config) {

    std::cout << "\n╔═══════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    分配器性能对比                                      ║" << std::endl;
    std::cout << "║  " << std::left << std::setw(20) << allocator1.get_name()
              << " vs " << std::setw(20) << allocator2.get_name() << "                 ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝" << std::endl;

    BenchmarkSuite suite;

    auto results1 = suite.run_full_suite(allocator1, config);
    auto results2 = suite.run_full_suite(allocator2, config);

    std::vector<BenchmarkResult> all_results;
    all_results.insert(all_results.end(), results1.begin(), results1.end());
    all_results.insert(all_results.end(), results2.begin(), results2.end());

    print_comparison(all_results);
}

size_t BenchmarkSuite::random_size(size_t min_size, size_t max_size) {
    if (min_size >= max_size) return min_size;
    return min_size + (rand() % (max_size - min_size + 1));
}

// MallocAllocator 实现

void* MallocAllocator::allocate(size_t size, size_t alignment) {
    (void)alignment; // 忽略对齐参数
    void* ptr = malloc(size);
    if (ptr != nullptr) {
        stats_.total_allocated += size;
        stats_.current_usage += size;
        stats_.allocation_count++;
    } else {
        stats_.failed_allocations++;
    }
    return ptr;
}

void MallocAllocator::deallocate(void* ptr, size_t size) {
    if (ptr != nullptr) {
        free(ptr);
        stats_.total_deallocated += size;
        if (stats_.current_usage >= size) {
            stats_.current_usage -= size;
        }
        stats_.deallocation_count++;
    }
}

} // namespace memplumber
