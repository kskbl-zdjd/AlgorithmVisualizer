// 测试 6：跨数组比较与赋值
// 覆盖：CrossCompare, CrossAssign

#include <vector>
using namespace std;

void testCrossArray(vector<int>& arr, vector<int>& brr) {
    if (arr[0] > brr[1]) {          // CrossCompare
        arr[2] = brr[3];            // CrossAssign
    }
}

int main() {
    vector<int> arr = {70, 5, 30, 40};
    vector<int> brr = {10, 20, 30, 99};
    testCrossArray(arr, brr);
    return 0;
}
