#pragma once

#include "allocator_interface.h"
#include <cstddef>
#include <chrono>
#include <vector>
#include <string>

namespace memplumber {

/**
 * 基准测试套件
 *
 * 用于测量和对比不同分配器的性能特征
 *
 * 测试场景：
 * - 顺序分配/释放（测试吞吐量）
 * - 随机分配/释放（测试真实场景）
 * - 混合大小分配（测试适应性）
 * - 碎片化测试（测试内存效率）
 *
 * 性能指标：
 * - 吞吐量（操作/秒）
 * - 平均延迟（纳秒）
 * - 内存使用效率
 * - 碎片率
 */
class BenchmarkSuite {
public:
    /**
     * 基准测试结果
     */
    struct BenchmarkResult {
        std::string allocator_name;           // 分配器名称
        std::string test_name;                // 测试名称

        size_t operations;                    // 总操作次数
        double duration_ms;                   // 总耗时（毫秒）
        double throughput_ops_per_sec;        // 吞吐量（操作/秒）
        double avg_latency_ns;                // 平均延迟（纳秒）

        size_t peak_memory_usage;             // 峰值内存使用
        size_t final_memory_usage;            // 最终内存使用
        double fragmentation_ratio;           // 碎片率

        bool success;                         // 是否成功完成
        std::string error_message;            // 错误信息（如果失败）
    };

    /**
     * 测试配置
     */
    struct BenchmarkConfig {
        size_t num_iterations;       // 迭代次数
        size_t min_size;             // 最小分配大小
        size_t max_size;             // 最大分配大小
        bool verbose;                // 是否输出详细信息

        BenchmarkConfig()
            : num_iterations(10000)
            , min_size(16)
            , max_size(4096)
            , verbose(false) {}
    };

    BenchmarkSuite();
    ~BenchmarkSuite() = default;

    /**
     * 顺序分配测试
     * 分配 N 个对象，然后按相同顺序释放
     */
    BenchmarkResult test_sequential_allocation(
        AllocatorInterface& allocator,
        size_t num_objects,
        size_t object_size);

    /**
     * 随机分配测试
     * 随机分配和释放对象
     */
    BenchmarkResult test_random_allocation(
        AllocatorInterface& allocator,
        size_t num_operations,
        size_t min_size,
        size_t max_size);

    /**
     * 混合大小测试
     * 分配不同大小的对象
     */
    BenchmarkResult test_mixed_sizes(
        AllocatorInterface& allocator,
        size_t num_objects);

    /**
     * 碎片化测试
     * 模拟导致碎片的分配模式
     */
    BenchmarkResult test_fragmentation(
        AllocatorInterface& allocator,
        size_t num_cycles);

    /**
     * 批量分配测试
     * 一次性分配大量对象
     */
    BenchmarkResult test_bulk_allocation(
        AllocatorInterface& allocator,
        size_t num_objects,
        size_t object_size);

    /**
     * 运行完整测试套件
     */
    std::vector<BenchmarkResult> run_full_suite(
        AllocatorInterface& allocator,
        const BenchmarkConfig& config = BenchmarkConfig());

    /**
     * 打印基准测试结果
     */
    static void print_result(const BenchmarkResult& result);

    /**
     * 打印对比表格
     */
    static void print_comparison(const std::vector<BenchmarkResult>& results);

    /**
     * 对比两个分配器
     */
    static void compare_allocators(
        AllocatorInterface& allocator1,
        AllocatorInterface& allocator2,
        const BenchmarkConfig& config = BenchmarkConfig());

private:
    /**
     * 计时器辅助类
     */
    class Timer {
    public:
        Timer() : start_(std::chrono::high_resolution_clock::now()) {}

        double elapsed_ms() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
            return duration.count() / 1000.0;
        }

        double elapsed_ns() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
            return static_cast<double>(duration.count());
        }

    private:
        std::chrono::high_resolution_clock::time_point start_;
    };

    /**
     * 生成随机大小
     */
    static size_t random_size(size_t min_size, size_t max_size);
};

/**
 * malloc 包装器，用于基准测试对比
 */
class MallocAllocator : public AllocatorInterface {
public:
    MallocAllocator() : stats_{} {}
    ~MallocAllocator() override = default;

    void* allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    bool owns(void* ptr) const override { return true; } // 假设所有指针都有效
    AllocatorStats get_stats() const override { return stats_; }
    void reset_stats() override { stats_ = AllocatorStats{}; }
    const char* get_name() const override { return "malloc"; }

private:
    AllocatorStats stats_;
};

} // namespace memplumber
