# AlgorithmVisualizer 全面功能测试套件

以下测试代码覆盖所有 24 种 AST 操作类型。每份代码可直接粘贴到「开发者模式」代码编辑器中运行。

---

## 测试清单

| # | 测试名称 | 覆盖 OpType | 功能点 |
|---|---------|------------|--------|
| 1 | 容器成员函数 | PushBack, PopBack, Insert, Erase, Clear, VecResize | 6 |
| 2 | emplace_back + at | EmplaceBack, At | 2 |
| 3 | STL 基础算法 | Reverse, Fill, Sort | 3 |
| 4 | 查找算法 | BinarySearch, Find | 2 |
| 5 | 修改算法 | Remove, Replace, Rotate | 3 |
| 6 | 跨数组操作 | CrossCompare, CrossAssign | 2 |
| 7 | 容器级复制与交换 | Copy, ContainerSwap | 2 |
| 8 | 综合测试（全功能） | 以上所有 | 20 |

> 已覆盖的 6 种排序算法（冒泡/选择/插入/快速/堆/归并）仍在初学者模式中可用，无需在此重复。

---

## 测试 1：STL 容器成员函数

**覆盖**: StlPushBack, StlPopBack, StlInsert, StlErase, StlClear, StlVecResize

```cpp
#include <vector>
using namespace std;

void testContainerOps(vector<int>& v) {
    v.push_back(99);                      // PushBack
    v.pop_back();                         // PopBack
    v.insert(v.begin() + 1, 55);          // Insert: 在位置1插入55
    v.erase(v.begin() + 2);               // Erase: 删除位置2的元素
    v.resize(10);                         // VecResize: 扩容到10
    v.clear();                            // Clear: 清空容器
}

int main() {
    vector<int> v = {10, 20, 30, 40, 50};
    testContainerOps(v);
    return 0;
}
```

---

## 测试 2：emplace_back + at()

**覆盖**: StlEmplaceBack, StlAt

```cpp
#include <vector>
using namespace std;

void testEmplaceAt(vector<int>& v) {
    v.emplace_back(77);                   // EmplaceBack
    int x = v.at(2);                      // At: 访问索引2
}

int main() {
    vector<int> v = {10, 20, 30, 40, 50};
    testEmplaceAt(v);
    return 0;
}
```

---

## 测试 3：STL 基础算法

**覆盖**: StlReverse, StlFill, StlSort

```cpp
#include <vector>
#include <algorithm>
using namespace std;

void testStlAlgorithms(vector<int>& v) {
    reverse(v.begin(), v.end());              // Reverse: 反转数组
    fill(v.begin() + 1, v.begin() + 5, 0);    // Fill: 部分填充
    sort(v.begin(), v.end());                 // Sort: 升序排序
}

int main() {
    vector<int> v = {38, 27, 43, 3, 9, 82, 10};
    testStlAlgorithms(v);
    return 0;
}
```

---

## 测试 4：查找算法

**覆盖**: StlBinarySearch, StlFind

```cpp
#include <vector>
#include <algorithm>
using namespace std;

void testSearch(vector<int>& v) {
    // sort 是为了让 binary_search 在有序数组上运行
    sort(v.begin(), v.end());
    bool found = binary_search(v.begin(), v.end(), 43);   // BinarySearch
    auto it = find(v.begin(), v.end(), 82);                // Find
}

int main() {
    vector<int> v = {38, 27, 43, 3, 9, 82, 10};
    testSearch(v);
    return 0;
}
```

---

## 测试 5：修改算法

**覆盖**: StlRemove, StlReplace, StlRotate

```cpp
#include <vector>
#include <algorithm>
using namespace std;

void testModifyAlgorithms(vector<int>& v) {
    remove(v.begin(), v.end(), 3);              // Remove: 移除值3
    replace(v.begin(), v.end(), 9, 99);         // Replace: 将9替换为99
    rotate(v.begin(), v.begin() + 2, v.end());  // Rotate: 左旋2个位置
}

int main() {
    vector<int> v = {3, 9, 27, 3, 38, 82, 10};
    testModifyAlgorithms(v);
    return 0;
}
```

---

## 测试 6：跨数组比较与赋值

**覆盖**: CrossCompare, CrossAssign

```cpp
#include <vector>
using namespace std;

void testCrossArray(vector<int>& arr, vector<int>& brr) {
    if (arr[0] > brr[1]) {          // CrossCompare: 跨数组比较
        arr[2] = brr[3];            // CrossAssign: 跨数组赋值
    }
}

int main() {
    vector<int> arr = {70, 5, 30, 40};
    vector<int> brr = {10, 20, 30, 99};
    testCrossArray(arr, brr);
    return 0;
}
```

---

## 测试 7：容器级复制与交换

**覆盖**: StlCopy, StlContainerSwap

```cpp
#include <vector>
#include <algorithm>
using namespace std;

void testCopy(vector<int>& src, vector<int>& dst) {
    copy(src.begin(), src.end(), dst.begin());   // Copy: 复制src到dst
}

void testSwap(vector<int>& v1, vector<int>& v2) {
    swap(v1, v2);                                // ContainerSwap: 交换整个容器
}

int main() {
    vector<int> src = {11, 22, 33, 44, 55};
    vector<int> dst = {0, 0, 0, 0, 0};
    testCopy(src, dst);

    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {9, 8, 7};
    testSwap(v1, v2);
    return 0;
}
```

---

## 测试 8：综合测试（全功能）

**覆盖**: PushBack, PopBack, Insert, Erase, Clear, VecResize, EmplaceBack, At, Reverse, Fill, Sort, BinarySearch, Find, Remove, Replace, Rotate, Copy, ContainerSwap, CrossCompare, CrossAssign（共 20 项）

```cpp
#include <vector>
#include <algorithm>
using namespace std;

// 容器成员函数
void testContainerOps(vector<int>& v) {
    v.push_back(99);
    v.pop_back();
    v.insert(v.begin() + 1, 55);
    v.erase(v.begin() + 2);
}

// emplace_back + at
void testEmplaceAt(vector<int>& v) {
    v.emplace_back(77);
    int x = v.at(1);
}

// 基础算法
void testBasicAlgorithms(vector<int>& v) {
    reverse(v.begin(), v.end());
    fill(v.begin(), v.begin() + 3, 0);
    sort(v.begin(), v.end());
}

// 查找算法
void testSearch(vector<int>& v) {
    sort(v.begin(), v.end());
    bool found = binary_search(v.begin(), v.end(), 43);
    auto it = find(v.begin(), v.end(), 82);
}

// 修改算法
void testModifyAlgorithms(vector<int>& v) {
    remove(v.begin(), v.end(), 9);
    replace(v.begin(), v.end(), 43, 99);
    rotate(v.begin(), v.begin() + 3, v.end());
}

// 跨数组操作
void testCrossArray(vector<int>& arr, vector<int>& brr) {
    if (arr[0] > brr[1]) {
        arr[2] = brr[3];
    }
}

// 容器复制与交换
void testCopySwap(vector<int>& src, vector<int>& dst, vector<int>& v1, vector<int>& v2) {
    copy(src.begin(), src.end(), dst.begin());
    swap(v1, v2);
}

int main() {
    // 测试容器成员函数
    vector<int> v1 = {10, 20, 30, 40, 50};
    testContainerOps(v1);

    // 测试 emplace_back + at
    // 小技巧：resize + 重新赋值 模拟新向量
    v1.clear();
    v1.push_back(10);
    v1.push_back(20);
    v1.push_back(30);
    testEmplaceAt(v1);

    // 测试基础算法
    vector<int> v2 = {38, 27, 43, 3, 9, 82, 10};
    testBasicAlgorithms(v2);

    // 测试查找算法
    vector<int> v3 = {38, 27, 43, 3, 9, 82, 10};
    testSearch(v3);

    // 测试修改算法
    vector<int> v4 = {3, 9, 27, 10, 38, 82, 43};
    testModifyAlgorithms(v4);

    // 测试跨数组操作
    vector<int> arr = {70, 5, 30, 40};
    vector<int> brr = {10, 20, 30, 99};
    testCrossArray(arr, brr);

    // 测试容器复制与交换
    vector<int> src = {11, 22, 33, 44, 55};
    vector<int> dst = {0, 0, 0, 0, 0};
    vector<int> vx = {1, 2, 3};
    vector<int> vy = {9, 8, 7};
    testCopySwap(src, dst, vx, vy);

    return 0;
}
```

---

## 使用说明

1. 启动 AlgorithmVisualizer，切换到「开发者模式」
2. 从上方复制任意测试代码，粘贴到左侧代码编辑器中
3. 点击「▶ 运行」按钮
4. 观察中间可视化区域的动画效果
5. 使用右侧控制按钮（播放/暂停/单步）逐步骤查看
6. 底部复杂度分析面板会显示统计信息

> **提示**: 建议先逐个运行测试 1~7，确认各功能独立工作正常后，再运行测试 8 进行综合验证。
