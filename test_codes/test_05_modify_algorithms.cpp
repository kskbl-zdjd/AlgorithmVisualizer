// 测试 5：修改算法
// 覆盖：Remove, Replace, Rotate

#include <vector>
#include <algorithm>
using namespace std;

void testModifyAlgorithms(vector<int>& v) {
    remove(v.begin(), v.end(), 3);              // Remove
    replace(v.begin(), v.end(), 9, 99);         // Replace
    rotate(v.begin(), v.begin() + 2, v.end());  // Rotate
}

int main() {
    vector<int> v = {3, 9, 27, 3, 38, 82, 10};
    testModifyAlgorithms(v);
    return 0;
}
