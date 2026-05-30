// 测试 2：emplace_back + at()
// 覆盖：EmplaceBack, At

#include <vector>
using namespace std;

void testEmplaceAt(vector<int>& v) {
    v.emplace_back(77);                   // EmplaceBack
    int x = v.at(2);                      // At
}

int main() {
    vector<int> v = {10, 20, 30, 40, 50};
    testEmplaceAt(v);
    return 0;
}
