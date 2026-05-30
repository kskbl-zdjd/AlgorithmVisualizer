// 测试 8：综合测试（覆盖全部 20 项 STL 功能）
// 覆盖：PushBack, PopBack, Insert, Erase, Clear, VecResize,
//       EmplaceBack, At, Reverse, Fill, Sort, BinarySearch, Find,
//       Remove, Replace, Rotate, Copy, ContainerSwap, CrossCompare, CrossAssign

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
void testCopySwap(vector<int>& src, vector<int>& dst,
                  vector<int>& v1, vector<int>& v2) {
    copy(src.begin(), src.end(), dst.begin());
    swap(v1, v2);
}

int main() {
    // 测试容器成员函数
    vector<int> v1 = {10, 20, 30, 40, 50};
    testContainerOps(v1);

    // 测试 emplace_back + at
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
