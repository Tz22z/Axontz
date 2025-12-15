# Axontz

自己写的 C++ 内存分配器，从零开始不用 malloc，直接 mmap 搞定。

## 简介

该项目实现了几种常见的内存分配算法。不依赖标准库的 malloc/free，全部通过 mmap 系统调用直接和操作系统打交道。

## 实现的分配器

### MemorySource

封装 mmap 系统调用，从 OS 获取内存。所有分配器都基于这个来获取原始内存。

### FreeListAllocator

自由列表分配器，维护一个空闲块链表。

支持任意大小的分配，会自动合并相邻的空闲块减少碎片。分配时采用 first-fit 策略，找到第一个够大的块就分配。

适合分配大小不固定的场景。

### SlabAllocator

固定大小对象分配器，参考了 Linux 内核的 slab 设计。

从大块内存里划分出很多固定大小的槽位，用链表管理空闲槽位。分配和释放都是 O(1) 时间复杂度。

SlabCache 可以管理多个不同大小的 slab，自动选择合适的大小类（16, 32, 64, 128...字节）。

适合小对象的高频分配，比如网络包、游戏实体这种。

### PoolAllocator

对象池，预分配固定数量的对象，用位图记录哪些在用。

相比 slab 更简单，但容量固定。适合已经知道最多需要多少对象的情况。

### CompositeAllocator

组合分配器，会根据申请的大小自动选择最合适的分配器。

小对象（4KB 以下）用 SlabCache
中等对象（4KB 到 256KB）用 FreeListAllocator
大对象（256KB 以上）直接 mmap

这样可以在不同场景下都有比较好的性能。

### BenchmarkSuite

性能测试框架，可以测试不同分配器在各种场景下的表现，和标准 malloc 对比。

测试场景包括顺序分配、随机分配、混合大小、碎片化等。

## 编译运行

需要 Linux 系统和支持 C++17 的编译器。

```bash
make all
make test
```

单独运行某个测试：

```bash
make test-basic
make test-slab
make test-pool
make test-composite
make test-benchmark
```

## 核心设计

### 为什么不用 malloc

主要是为了学习底层原理。通过自己实现能更好理解内存管理的细节，比如碎片怎么产生、怎么处理，不同策略的 trade-off 在哪。

另外针对特定场景可以做优化，比如游戏里大量固定大小对象的分配，用 slab 会比通用的 malloc 快很多。

### 内存碎片处理

外部碎片：FreeListAllocator 会合并相邻的空闲块；SlabAllocator 因为都是固定大小所以没有外部碎片。

内部碎片：SlabCache 提供多个大小类来减少浪费；FreeListAllocator 分配时会拆分过大的块。

### 统计功能

每个分配器都有统计接口，可以看到分配了多少内存、当前使用量、碎片率等信息。

```cpp
auto stats = allocator.get_stats();
std::cout << "已分配: " << stats.total_allocated << "\n";
std::cout << "当前使用: " << stats.current_usage << "\n";
std::cout << "碎片率: " << stats.fragmentation_ratio << "\n";
```

## 性能

在我的机器上测试，分配 10000 个 64 字节对象：

SlabAllocator 最快，平均每次分配 500 纳秒左右
PoolAllocator 差不多，稍微慢一点
CompositeAllocator 对小对象会路由到 Slab，所以也很快
FreeListAllocator 慢一些，因为要遍历链表
标准 malloc 居中

不过实际性能和使用场景关系很大，这些只是参考。

## 学习过程

这个项目断断续续写了两个多月。一开始从最简单的 FreeListAllocator 开始，理解了基本的分配和释放流程。然后加入了块合并、块拆分等优化。

后来看了些内核代码和论文，实现了 Slab。这个设计真的很巧妙，把空闲列表直接存在空闲对象里面，完全不浪费空间。

PoolAllocator 和 Slab 有点像，但用位图管理更直观一些。

CompositeAllocator 是最后加的，把前面的几个整合起来。

BenchmarkSuite 是为了验证性能写的，发现自己实现的分配器在某些场景下确实能比 malloc 快不少。

## 参考

Linux 内核的 SLUB 分配器
jemalloc 和 tcmalloc 的设计
一些经典的内存管理论文

## TODO

线程安全现在只有个简单的 mutex 封装，可以做线程本地缓存优化
想试试 buddy system
可以加个可视化工具看内存使用情况
