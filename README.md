# AlgorithmVisualizer

算法及数据结构可视化教学应用 —— 基于 C++/Qt6 的交互式算法演示工具。

支持六大经典排序算法的逐步动画演示，并提供**开发者模式**：自由编写任意 C++ 代码，由 Clang AST 自动分析、插桩并可视化执行过程。

---

## 功能特性

### 入门模式（Beginner Mode）

面向算法学习者的开箱即用模式：

- **6 种排序算法**：冒泡、选择、插入、快速、堆、归并排序
- **可视化元素**：双色块对比 / 箭头标记 / 虚线区间框 / 临时空间 / 树形堆结构
- **播放控制**：开始 / 暂停 / 停止 / 单步前进 / 单步后退
- **速度调节**：0.1× ~ 5.0× 可调速
- **数据输入**：手动输入或一键随机生成

### 开发者模式（Developer Mode）

面向开发者的自由编码模式，支持任意 C++ 代码的可视化执行：

- **自动插桩**：Clang AST 解析用户代码，自动注入可视化 API 调用
- **24 种操作支持**：swap / compare / assign / read / push_back / pop_back / insert / erase / clear / resize / sort / reverse / fill / binary_search / find / copy / remove / replace / rotate / emplace_back / at / cross-compare / cross-assign / container-swap
- **多数组可视化**：支持同时可视化多个 `vector<int>` 和 `int[]` 数组，按变量名自动创建独立面板
- **引用参数别名路由**：自定义函数引用参数自动映射到调用者变量
- **复杂度统计**：自动统计比较次数、交换次数，估算时间/空间复杂度
- **DLL 编译执行**：用户代码编译为 DLL 加载运行，事件驱动回放

---

## 技术栈

| 组件 | 版本 | 用途 |
|------|------|------|
| **C++** | C++17 | 核心语言标准 |
| **Qt** | 6.8.3 (MinGW 64-bit) | GUI 框架、事件循环 |
| **MinGW** | 13.1.0 | C++ 编译器 |
| **CMake** | ≥ 3.16 | 跨平台构建系统 |
| **Ninja** | 任意版本 | 构建后端 |
| **Clang** | ≥ 3.2 | AST 语法分析（开发者模式） |

**架构设计**：事件驱动，Swap / Compare / VizEvent 事件流驱动动画更新，DLL 执行与 Qt 回放两阶段解耦。

---

## 运行方式

### 环境要求

- Windows 10 / 11 64-bit
- Qt 6.8.3（MinGW 64-bit 组件）
- Clang（开发者模式需要）

> 详细环境配置步骤请查看 [`docs/环境配置说明书.html`](docs/环境配置说明书.html)

### 快速构建

```powershell
# 1. 配置 CMakeLists.txt 中的 Qt 路径（改为你的 Qt 安装路径）
#    - Qt6_DIR
#    - CMAKE_PREFIX_PATH
#    - build.ps1 中的 $QtRoot

# 2. 一键构建
.\build.ps1
```

### 手动构建

```powershell
# 设置环境变量
$env:PATH = "C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;$env:PATH"

# 构建
mkdir build; cd build
cmake .. -G "Ninja"
ninja
```

编译产物位于 `build/bin/AlgorithmVisualizer.exe`。首次运行前需将 Qt 运行时 DLL 复制到 exe 同目录（`build.ps1` 自动完成此步骤）。

### 运行

双击 `build/bin/AlgorithmVisualizer.exe`，通过顶部工具栏切换入门模式 / 开发者模式。

---

## 目录结构

```
algo-visualizer/
├── CMakeLists.txt                  # CMake 构建配置
├── build.ps1                       # 一键构建脚本
├── README.md
├── .gitignore
├── docs/
│   └── 环境配置说明书.html          # 详细环境配置指南
├── src/
│   ├── main.cpp                    # 程序入口
│   ├── app/
│   │   └── MainWindow.h/.cpp       # 主窗口（QStackedWidget 页面管理）
│   ├── modes/
│   │   ├── BeginnerMode.h/.cpp     # 入门模式（算法选择 → 播放演示）
│   │   └── DeveloperMode.h/.cpp    # 开发者模式（自由编码 → 自动插桩）
│   ├── core/
│   │   ├── AnimationController.h/.cpp  # 动画引擎（步骤队列 / 播放控制）
│   │   ├── AstAnalyzer.h/.cpp          # Clang AST 分析器（24 种操作识别）
│   │   ├── CodeInserter.h/.cpp         # 代码插桩器（注入 viz:: 调用）
│   │   └── CodeExecutor.h/.cpp         # 代码执行引擎（DLL 编译 / 加载 / 执行）
│   ├── algorithms/
│   │   ├── SortingAlgorithms.h/.cpp    # 冒泡 / 选择 / 插入 / 快速排序
│   │   ├── HeapSort.h/.cpp             # 堆排序 + 堆可视化
│   │   └── MergeSort.h/.cpp            # 归并排序 + 临时空间绘制
│   ├── visualizers/
│   │   ├── ArrayVisualizer.h/.cpp      # 数组可视化（色块 / 箭头 / 虚线框）
│   │   └── HeapVisualizer.h/.cpp       # 堆树形可视化
│   ├── recorder/
│   │   ├── VizEvent.h                  # VizEvent POD 结构体（17 种事件类型）
│   │   ├── VizRecorder.h/.cpp          # 复杂度统计器
│   │   └── viz_api.h/.cpp              # 可视化 C++ API 命名空间
│   └── resources/
│       └── code_examples.h             # 内置代码示例（6 种排序算法源码）
├── examples/
│   └── sorting/
│       └── bubble_sort_example.cpp     # 冒泡排序独立示例
└── test_codes/
    ├── STL_Test_Suite.md               # 测试套件说明
    └── test_01~08_*.cpp                # 24 种操作类型的完整测试用例
```

---

## 系统架构

```
┌─────────────────────────────────────────────────────┐
│                 AlgorithmVisualizer                 │
├─────────────────────────┬───────────────────────────┤
│      入门模式             │       开发者模式           │
│  预置算法 → 一键演示       │  自由编码 → 自动插桩        │
├─────────────────────────┴───────────────────────────┤
│              AnimationController                    │
│         事件驱动 · 步骤队列 · 播放控制                  │
├─────────────────────────┬───────────────────────────┤
│   ArrayVisualizer        │      HeapVisualizer       │
│  （色块 / 箭头 / 虚线框）  │     （二叉树节点可视化）     │
└─────────────────────────┴───────────────────────────┘
```

**开发者模式执行流程**：

```
用户 C++ 代码 → Clang AST 分析 → 自动插桩(viz::调用) → DLL 编译
→ QLibrary 加载执行 → VizEvent 事件收集 → 转换为 AnimationStep → 动画回放
```

---

## License

MIT

---

## 环境配置

完整环境配置指南请查看 [`docs/环境配置说明书.html`](docs/环境配置说明书.html)，包含图文教程、常见问题排查、Clang 安装备选方案等。
