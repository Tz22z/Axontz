#include "axontzz/benchmark_suite.h"
#include "axontzz/memory_source.h"
#include "axontzz/free_list_allocator.h"
#include "axontzz/slab_allocator.h"
#include "axontzz/composite_allocator.h"
#include <iostream>
#include <iomanip>

using namespace memplumber;

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    MemPlumber 内存分配器基准测试                       ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝" << std::endl;

    MemorySource mem_source;
    BenchmarkSuite suite;

    // 配置
    BenchmarkSuite::BenchmarkConfig config;
    config.num_iterations = 1000;  // 1000次迭代（避免内存不足）
    config.verbose = true;

    std::cout << "\n测试配置：" << std::endl;
    std::cout << "  迭代次数：" << config.num_iterations << std::endl;
    std::cout << "  大小范围：" << config.min_size << " - " << config.max_size << " 字节" << std::endl;

    // 1. malloc 基准
    std::cout << "\n\n【1/4】测试 malloc（系统分配器）" << std::endl;
    std::cout << std::string(71, '=') << std::endl;
    MallocAllocator malloc_alloc;
    auto malloc_results = suite.run_full_suite(malloc_alloc, config);

    // 2. FreeListAllocator
    std::cout << "\n\n【2/4】测试 FreeListAllocator（通用可变大小分配器）" << std::endl;
    std::cout << std::string(71, '=') << std::endl;
    FreeListAllocator freelist_alloc(mem_source, 1024 * 1024);
    auto freelist_results = suite.run_full_suite(freelist_alloc, config);

    // 3. SlabCache
    std::cout << "\n\n【3/4】测试 SlabCache（固定大小分配器）" << std::endl;
    std::cout << std::string(71, '=') << std::endl;
    SlabCache slab_alloc(mem_source);
    auto slab_results = suite.run_full_suite(slab_alloc, config);

    // 4. CompositeAllocator
    std::cout << "\n\n【4/4】测试 CompositeAllocator（智能组合分配器）" << std::endl;
    std::cout << std::string(71, '=') << std::endl;
    CompositeAllocator composite_alloc(mem_source);
    auto composite_results = suite.run_full_suite(composite_alloc, config);

    // 汇总结果
    std::cout << "\n\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                           性能对比汇总                                 ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝" << std::endl;

    std::vector<BenchmarkSuite::BenchmarkResult> all_results;
    all_results.insert(all_results.end(), malloc_results.begin(), malloc_results.end());
    all_results.insert(all_results.end(), freelist_results.begin(), freelist_results.end());
    all_results.insert(all_results.end(), slab_results.begin(), slab_results.end());
    all_results.insert(all_results.end(), composite_results.begin(), composite_results.end());

    BenchmarkSuite::print_comparison(all_results);

    // 计算平均性能提升
    std::cout << "\n\n性能分析：" << std::endl;
    std::cout << std::string(71, '=') << std::endl;

    double malloc_avg_throughput = 0.0;
    double composite_avg_throughput = 0.0;

    for (const auto& result : malloc_results) {
        malloc_avg_throughput += result.throughput_ops_per_sec;
    }
    malloc_avg_throughput /= malloc_results.size();

    for (const auto& result : composite_results) {
        composite_avg_throughput += result.throughput_ops_per_sec;
    }
    composite_avg_throughput /= composite_results.size();

    double improvement = ((composite_avg_throughput - malloc_avg_throughput) / malloc_avg_throughput) * 100.0;

    std::cout << "malloc 平均吞吐量：" << std::fixed << std::setprecision(0)
              << malloc_avg_throughput << " ops/sec" << std::endl;
    std::cout << "CompositeAllocator 平均吞吐量：" << std::fixed << std::setprecision(0)
              << composite_avg_throughput << " ops/sec" << std::endl;

    if (improvement > 0) {
        std::cout << "\n✓ CompositeAllocator 比 malloc 快 " << std::fixed << std::setprecision(1)
                  << improvement << "%！" << std::endl;
    } else {
        std::cout << "\nCompositeAllocator 比 malloc 慢 " << std::fixed << std::setprecision(1)
                  << (-improvement) << "%" << std::endl;
    }

    // 碎片率分析
    std::cout << "\n碎片率分析：" << std::endl;
    std::cout << std::string(71, '-') << std::endl;

    double malloc_avg_frag = 0.0;
    double composite_avg_frag = 0.0;

    for (const auto& result : malloc_results) {
        malloc_avg_frag += result.fragmentation_ratio;
    }
    malloc_avg_frag /= malloc_results.size();

    for (const auto& result : composite_results) {
        composite_avg_frag += result.fragmentation_ratio;
    }
    composite_avg_frag /= composite_results.size();

    std::cout << "malloc 平均碎片率：" << std::fixed << std::setprecision(2)
              << (malloc_avg_frag * 100.0) << "%" << std::endl;
    std::cout << "CompositeAllocator 平均碎片率：" << std::fixed << std::setprecision(2)
              << (composite_avg_frag * 100.0) << "%" << std::endl;

    std::cout << "\n\n╔═══════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                        基准测试完成！                                  ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝" << std::endl;

    return 0;
}
