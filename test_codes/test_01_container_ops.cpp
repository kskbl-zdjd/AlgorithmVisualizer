// 测试 1：STL 容器成员函数
// 覆盖：push_back, pop_back, insert, erase, clear, resize

#include <vector>
using namespace std;

void testContainerOps(vector<int>& v) {
    v.push_back(99);                      // PushBack
    v.pop_back();                         // PopBack
    v.insert(v.begin() + 1, 55);          // Insert
    v.erase(v.begin() + 2);               // Erase
    v.resize(10);                         // VecResize
    v.clear();                            // Clear
}

int main() {
    vector<int> v = {10, 20, 30, 40, 50};
    testContainerOps(v);
    return 0;
}
