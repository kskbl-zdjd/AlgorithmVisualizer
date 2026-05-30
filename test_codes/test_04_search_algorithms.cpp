// 测试 4：查找算法
// 覆盖：BinarySearch, Find

#include <vector>
#include <algorithm>
using namespace std;

void testSearch(vector<int>& v) {
    sort(v.begin(), v.end());                                    // 排序使数组有序
    bool found = binary_search(v.begin(), v.end(), 43);         // BinarySearch
    auto it = find(v.begin(), v.end(), 82);                     // Find
}

int main() {
    vector<int> v = {38, 27, 43, 3, 9, 82, 10};
    testSearch(v);
    return 0;
}
