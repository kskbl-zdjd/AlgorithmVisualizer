#ifndef CODE_EXAMPLES_H
#define CODE_EXAMPLES_H

#include <QString>

namespace CodeExamples {

inline const QString BUBBLE_SORT = R"cpp(
void bubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                // 交换 arr[j] 和 arr[j+1]
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
        // 第 i 轮结束，arr[n-1-i] 已就位
    }
}
)cpp";

inline const QString SELECTION_SORT = R"cpp(
void selectionSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j] < arr[minIdx]) {
                minIdx = j;
            }
        }
        // 交换 arr[i] 和 arr[minIdx]
        int temp = arr[i];
        arr[i] = arr[minIdx];
        arr[minIdx] = temp;
        // arr[i] 已就位
    }
}
)cpp";

inline const QString INSERTION_SORT = R"cpp(
void insertionSort(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        // 将 key 插入到有序区间的正确位置
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];  // 后移
            j--;
        }
        arr[j + 1] = key;  // 插入
    }
}
)cpp";

inline const QString QUICK_SORT = R"cpp(
void quickSort(int arr[], int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

int partition(int arr[], int low, int high) {
    int pivot = arr[high];  // 选择基准
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            // 交换 arr[i] 和 arr[j]
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    // 基准归位
    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    return i + 1;
}
)cpp";

inline const QString HEAP_SORT_CODE = R"cpp(
// 堆排序主函数
void heapSort(vector<int>& arr) {
    int n = arr.size();

    // 建堆阶段：从最后一个父节点开始，自底向上建堆
    for (int i = n / 2 - 1; i >= 0; i--) {
        siftDown(arr, n, i);
    }

    // 排序阶段：逐个将堆顶元素移到末尾
    for (int i = n - 1; i > 0; i--) {
        swap(arr[0], arr[i]);    // 堆顶与末尾交换
        siftDown(arr, i, 0);     // 对堆顶下沉调整
    }
}

// 下沉调整函数
void siftDown(vector<int>& arr, int n, int i) {
    int largest = i;           // 假设当前节点最大
    int left = 2 * i + 1;      // 左子节点索引
    int right = 2 * i + 2;     // 右子节点索引

    // 找出父节点、左子节点、右子节点中的最大值
    if (left < n && arr[left] > arr[largest]) {
        largest = left;
    }
    if (right < n && arr[right] > arr[largest]) {
        largest = right;
    }

    // 若最大值不是父节点，交换并继续下沉
    if (largest != i) {
        swap(arr[i], arr[largest]);
        siftDown(arr, n, largest);  // 递归下沉
    }
}
)cpp";

inline const QString MERGE_SORT_CODE = R"cpp(
void mergeSort(int arr[], int left, int right) {
    if (left >= right) return;
    
    int mid = left + (right - left) / 2;
    
    mergeSort(arr, left, mid);
    mergeSort(arr, mid + 1, right);
    merge(arr, left, mid, right);
}

void merge(int arr[], int left, int mid, int right) {
    int n = right - left + 1;
    int* temp = new int[n];
    
    for (int i = 0; i < n; i++) {
        temp[i] = arr[left + i];
    }
    
    int i = 0, j = mid - left + 1, k = left;
    while (i <= mid - left && j < n) {
        if (temp[i] <= temp[j]) {
            arr[k++] = temp[i++];
        } else {
            arr[k++] = temp[j++];
        }
    }
    
    while (i <= mid - left) arr[k++] = temp[i++];
    while (j < n) arr[k++] = temp[j++];
    
    delete[] temp;
}
)cpp";

} // namespace CodeExamples

#endif // CODE_EXAMPLES_H
