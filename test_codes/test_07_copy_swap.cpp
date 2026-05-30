// 测试 7：容器级复制与交换
// 覆盖：Copy, ContainerSwap

#include <vector>
#include <algorithm>
using namespace std;

void testCopy(vector<int>& src, vector<int>& dst) {
    copy(src.begin(), src.end(), dst.begin());   // Copy
}

void testSwap(vector<int>& v1, vector<int>& v2) {
    swap(v1, v2);                                // ContainerSwap
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
