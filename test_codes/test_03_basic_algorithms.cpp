// 测试 3：STL 基础算法
// 覆盖：Reverse, Fill, Sort

#include <vector>
#include <algorithm>
using namespace std;

void testStlAlgorithms(vector<int>& v) {
    reverse(v.begin(), v.end());              // Reverse
    fill(v.begin() + 1, v.begin() + 5, 0);    // Fill
    sort(v.begin(), v.end());                 // Sort
}

int main() {
    vector<int> v = {38, 27, 43, 3, 9, 82, 10};
    testStlAlgorithms(v);
    for (int i = 0;i < v.size();i++) {
        cout << v[i]<< " ";
    }
    return 0;
}
