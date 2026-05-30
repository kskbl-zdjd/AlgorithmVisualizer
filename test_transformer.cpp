#include <QDebug>
#include <QString>
#include "../src/core/CodeParser.h"
#include "../src/core/CodeTransformer.h"

int main() {
    // 测试1：算法函数模式（无 main）
    QString algorithmCode = R"(void bubbleSort(int arr[], int n) {
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                swap(arr[j], arr[j+1]);
            }
        }
    }
})";

    qDebug() << "=== Test 1: Algorithm Function Mode ===";
    qDebug() << "Input code:" << algorithmCode;
    
    CodeTransformer transformer1;
    QString result1 = transformer1.transform(algorithmCode);
    qDebug() << "Transformed code:" << result1;
    qDebug() << "Is complete program?" << transformer1.isCompleteProgram();
    
    // 测试2：完整程序模式（有 main）
    QString completeCode = R"(#include <iostream>
#include <vector>
using namespace std;

int main() {
    vector<int> v = {2,5,6,7,9};
    for (int i = 1; i < v.size(); i++) {
        swap(v[i], v[i-1]);
    }
    return 0;
})";

    qDebug() << "\n=== Test 2: Complete Program Mode ===";
    qDebug() << "Input code:" << completeCode;
    
    CodeTransformer transformer2;
    QString result2 = transformer2.transform(completeCode);
    qDebug() << "Transformed code:" << result2;
    qDebug() << "Is complete program?" << transformer2.isCompleteProgram();
    
    return 0;
}
