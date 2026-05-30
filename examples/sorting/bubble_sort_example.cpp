// 冒泡排序示例
// 时间复杂度: O(n²)
// 空间复杂度: O(1)

#include <iostream>
#include <vector>

void bubbleSort(std::vector<int>& arr) {
    int n = arr.size();

    for (int i = 0; i < n - 1; i++) {
        // 标记本轮是否有交换
        bool swapped = false;

        for (int j = 0; j < n - i - 1; j++) {
            // 比较相邻元素
            if (arr[j] > arr[j + 1]) {
                // 交换元素
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
                swapped = true;
            }
        }

        // 如果没有交换，说明数组已经有序
        if (!swapped) {
            break;
        }
    }
}

int main() {
    std::vector<int> arr = {64, 34, 25, 12, 22, 11, 90};

    std::cout << "排序前: ";
    for (int num : arr) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    bubbleSort(arr);

    std::cout << "排序后: ";
    for (int num : arr) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    return 0;
}
