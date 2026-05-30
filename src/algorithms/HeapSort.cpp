#include "HeapSort.h"
#include <cmath>

// 颜色常量
static const QColor COLOR_RED("#F44336");
static const QColor COLOR_BLUE("#2196F3");
static const QColor COLOR_ORANGE("#FF9800");
static const QColor COLOR_GREEN("#4CAF50");
static const QColor COLOR_GREY("#9E9E9E");

void HeapSort::heapSort(
    HeapVisualizer *heapViz,
    ArrayVisualizer *arrViz,
    AnimationController *ctrl,
    const std::vector<int> &data)
{
    ctrl->clearSteps();
    heapViz->setData(data);
    arrViz->setData(data);
    std::vector<int> arr = data;
    int n = static_cast<int>(arr.size());

    // 步骤：初始状态
    ctrl->addStep({
        [heapViz, arrViz]() {
            heapViz->resetAllNodeStates();
            arrViz->resetAllBlockStates();
            arrViz->clearArrows();
            arrViz->clearDashedBoxes();
        },
        nullptr,
        "堆排序：初始状态"
    });

    // 建堆阶段
    buildHeap(heapViz, arrViz, ctrl, arr);

    // 排序阶段
    for (int i = n -1; i > 0; i--) {
        // 红框框出当前待处理区间 [0, i]
        int ii = i;
        ctrl->addStep({
            [arrViz, ii]() {
                arrViz->resetAllBlockStates();
                arrViz->setDashedBoxes({{0, ii, "待排序", COLOR_RED}});
                arrViz->clearArrows();
            },
            [arrViz]() {
                arrViz->clearDashedBoxes();
                arrViz->clearArrows();
            },
            QString("排序阶段：待处理区间 [0,%1]").arg(ii)
        });

        // 交换堆顶(arr[0])与最末元素(arr[i])
        int a = 0, b = i;
        ctrl->addStep({
            [heapViz, arrViz, a, b]() {
                // 堆可视化：两节点变橙色
                heapViz->setNodeState(a, HeapNodeState::Swapping);
                heapViz->setNodeState(b, HeapNodeState::Swapping);
                // 数组：两色块变橙（用 Active 代替，因为数组没有 Swapping 状态）
                arrViz->setBlockState(a, BlockState::Active);
                arrViz->setBlockState(b, BlockState::Active);
                arrViz->swapValues(a, b);
                heapViz->swapNodes(a, b);
            },
            [heapViz, arrViz, a, b]() {
                arrViz->swapValues(a, b);
                heapViz->swapNodes(a, b);
            },
            QString("交换堆顶 %1 与最末元素 %2").arg(a).arg(b)
        });
        std::swap(arr[0], arr[i]);

        // 最末节点变灰（已排序），断开连线
        int sortedIdx = i;
        ctrl->addStep({
            [heapViz, arrViz, sortedIdx]() {
                heapViz->setNodeState(sortedIdx, HeapNodeState::Sorted);
                heapViz->disconnectNode(sortedIdx);
                arrViz->setBlockState(sortedIdx, BlockState::Active); // 用 Active 模拟灰色效果
            },
            [heapViz, arrViz, sortedIdx]() {
                heapViz->setNodeState(sortedIdx, HeapNodeState::Normal);
                heapViz->disconnectNode(sortedIdx); // 保持断开
            },
            QString("元素 %1 已就位（已排序）").arg(arr[sortedIdx])
        });

        // 对堆顶执行下沉调整（无序区大小为 i）
        if (i > 1) {
            siftDown(heapViz, arrViz, ctrl, arr, i, 0, "排序");
        }
    }

    // 排序完成
    ctrl->addStep({
        [heapViz, arrViz]() {
            heapViz->resetAllNodeStates();
            arrViz->resetAllBlockStates();
            arrViz->clearArrows();
            arrViz->clearDashedBoxes();
        },
        nullptr,
        "堆排序完成"
    });
}

void HeapSort::buildHeap(
    HeapVisualizer *heapViz,
    ArrayVisualizer *arrViz,
    AnimationController *ctrl,
    std::vector<int> &arr)
{
    int n = static_cast<int>(arr.size());

    // 从最后一个父节点开始，自底向上建堆
    for (int i = n / 2 - 1; i >= 0; i--) {
        // 步骤：箭头指向当前父节点
        int parentIdx = i;
        ctrl->addStep({
            [heapViz, arrViz, parentIdx]() {
                // ✅ 先重置所有颜色
                heapViz->resetAllNodeStates();
                arrViz->resetAllBlockStates();
                // 设置箭头和父节点颜色
                arrViz->setArrows({{parentIdx, "parent"}});
                arrViz->setBlockState(parentIdx, BlockState::Active);
                heapViz->setNodeState(parentIdx, HeapNodeState::Active);
            },
            [heapViz, arrViz, parentIdx]() {
                arrViz->clearArrows();
                arrViz->setBlockState(parentIdx, BlockState::Normal);
                heapViz->setNodeState(parentIdx, HeapNodeState::Normal);
            },
            QString("建堆：处理父节点 %1 (值 %2)").arg(parentIdx).arg(arr[parentIdx])
        });

        // 执行下沉调整
        siftDown(heapViz, arrViz, ctrl, arr, n, i, "建堆");
    }

    // 建堆完成
    ctrl->addStep({
        [heapViz, arrViz]() {
            heapViz->resetAllNodeStates();
            arrViz->resetAllBlockStates();
            arrViz->clearArrows();
            arrViz->clearDashedBoxes();
        },
        nullptr,
        "建堆完成：满足最大堆性质"
    });
}

void HeapSort::siftDown(
    HeapVisualizer *heapViz,
    ArrayVisualizer *arrViz,
    AnimationController *ctrl,
    std::vector<int> &arr,
    int n,
    int i,
    const QString &phase)
{
    int largest = i;

    // 步骤：比较左子节点
    int left = 2 * i + 1;
    if (left < n) {
        int leftIdx = left;
        int parentIdx = i;
        ctrl->addStep({
            [heapViz, arrViz, leftIdx, parentIdx]() {
                // ✅ 先重置所有颜色
                heapViz->resetAllNodeStates();
                arrViz->resetAllBlockStates();
                // 父节点变红
                heapViz->setNodeState(parentIdx, HeapNodeState::Active);
                arrViz->setBlockState(parentIdx, BlockState::Active);
                // 左子节点变蓝
                heapViz->setNodeState(leftIdx, HeapNodeState::Comparing);
                arrViz->setBlockState(leftIdx, BlockState::Active);
            },
            [heapViz, arrViz, leftIdx, parentIdx]() {
                heapViz->setNodeState(leftIdx, HeapNodeState::Normal);
                arrViz->setBlockState(leftIdx, BlockState::Normal);
            },
            QString("%1：比较父节点 %2 与左子节点 %3 (值 %4)")
                .arg(phase).arg(i).arg(leftIdx).arg(arr[leftIdx])
        });
        if (arr[left] > arr[largest]) largest = left;
    }

    // 步骤：比较右子节点
    int right = 2 * i + 2;
    if (right < n) {
        int rightIdx = right;
        int parentIdx = i;
        ctrl->addStep({
            [heapViz, arrViz, rightIdx, parentIdx]() {
                // ✅ 先重置所有颜色
                heapViz->resetAllNodeStates();
                arrViz->resetAllBlockStates();
                // 父节点变红
                heapViz->setNodeState(parentIdx, HeapNodeState::Active);
                arrViz->setBlockState(parentIdx, BlockState::Active);
                // 右子节点变蓝
                heapViz->setNodeState(rightIdx, HeapNodeState::Comparing);
                arrViz->setBlockState(rightIdx, BlockState::Active);
            },
            [heapViz, arrViz, rightIdx, parentIdx]() {
                heapViz->setNodeState(rightIdx, HeapNodeState::Normal);
                arrViz->setBlockState(rightIdx, BlockState::Normal);
            },
            QString("%1：比较父节点 %2 与右子节点 %3 (值 %4)")
                .arg(phase).arg(i).arg(rightIdx).arg(arr[rightIdx])
        });
        if (arr[right] > arr[largest]) largest = right;
    }

    // 若需要交换
    if (largest != i) {
        int parentIdx = i;
        int childIdx = largest;
        ctrl->addStep({
            [heapViz, arrViz, parentIdx, childIdx]() {
                // ✅ 先重置所有颜色
                heapViz->resetAllNodeStates();
                arrViz->resetAllBlockStates();
                // 交换的两个节点变橙色
                heapViz->setNodeState(parentIdx, HeapNodeState::Swapping);
                heapViz->setNodeState(childIdx, HeapNodeState::Swapping);
                arrViz->setBlockState(parentIdx, BlockState::Active);
                arrViz->setBlockState(childIdx, BlockState::Active);
                // 执行交换
                arrViz->swapValues(parentIdx, childIdx);
                heapViz->swapNodes(parentIdx, childIdx);
            },
            [heapViz, arrViz, parentIdx, childIdx]() {
                arrViz->swapValues(parentIdx, childIdx);
                heapViz->swapNodes(parentIdx, childIdx);
            },
            QString("%1：交换父节点 %2 (值 %3) 与子节点 %4 (值 %5)")
                .arg(phase)
                .arg(parentIdx).arg(arr[parentIdx])
                .arg(childIdx).arg(arr[childIdx])
        });
        std::swap(arr[i], arr[largest]);

        // 继续下沉（递归）
        siftDown(heapViz, arrViz, ctrl, arr, n, largest, phase);
    } else {
        // 不需要交换，堆性质已满足
        int currIdx = i;
        ctrl->addStep({
            [heapViz, arrViz, currIdx]() {
                heapViz->resetAllNodeStates();
                arrViz->resetAllBlockStates();
                arrViz->clearArrows();
            },
            nullptr,
            QString("%1：节点 %2 满足堆性质").arg(phase).arg(currIdx)
        });
    }
}
