# C++自定义内存分配器

从零开始构建的高性能内存分配框架，用于探索操作系统内存管理和C++系统编程

## 目标

无需使用 malloc/free 即可实施自定义内存分配器
通过系统调用直接管理操作系统内存
与标准分配器的性能比较
线程安全并发分配

## 预期功能

实现基于mmap的直接内存源
实现自由列表分配器的
实现相同大小对象的板块分配器
实现线程安全和并发性
实现性能基准套件
综合分析及可视化

## Build

```bash
make all
make test
```

---
*Project in development - targeting advanced computer science coursework*
