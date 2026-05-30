#include "MergeSort.h"
#include <QString>

static const QColor COLOR_RED_BOX("#F44336");
static const QColor COLOR_GREEN_BOX("#4CAF50");
static const QColor COLOR_BLUE_BOX("#2196F3");

void MergeSort::mergeSort(
    ArrayVisualizer *arrViz,
    AnimationController *ctrl,
    const std::vector<int> &data)
{
    ctrl->clearSteps();
    arrViz->setData(data);
    arrViz->clearTempSpace();
    
    std::vector<int> arr = data;
    std::vector<DashedBox> pendingBoxes;
    
    mergeSortRecursive(arrViz, ctrl, arr, 0, arr.size() - 1, pendingBoxes);
    
    ctrl->addStep({
        [arrViz]() {
            arrViz->resetAllBlockStates();
            arrViz->clearDashedBoxes();
            arrViz->clearArrows();
            arrViz->clearTempSpace();
        },
        nullptr,
        "归并排序完成"
    });
}

void MergeSort::mergeSortRecursive(
    ArrayVisualizer *arrViz,
    AnimationController *ctrl,
    std::vector<int> &arr,
    int left, int right,
    const std::vector<DashedBox> &pendingBoxes)
{
    if (left >= right) return;
    
    int mid = left + (right - left) / 2;
    
    int ll = left, rr = right, mm = mid;
    ctrl->addStep({
        [arrViz, ll, rr, pendingBoxes]() {
            arrViz->resetAllBlockStates();
            std::vector<DashedBox> boxes;
            boxes.push_back({ll, rr, "处理中", COLOR_RED_BOX});
            for (const auto &box : pendingBoxes) {
                boxes.push_back(box);
            }
            arrViz->setDashedBoxes(boxes);
        },
        nullptr,
        QString("处理区间 [%1, %2]，中点 %3").arg(ll).arg(rr).arg(mm)
    });
    
    // 修正：构建 leftPending 和 rightPending 时，保留所有待处理区
    std::vector<DashedBox> leftPending = pendingBoxes;
    std::vector<DashedBox> rightPending = pendingBoxes;

    // 添加当前层级的待处理区间
    // 左递归时，右半部分 [mid+1, right] 是待处理
    if (mid + 1 <= right) {
        leftPending.push_back({mid + 1, right, "待处理", COLOR_GREEN_BOX});
    }
    // 右递归时，左半部分 [left, mid] 是待处理
    if (left <= mid) {
        rightPending.push_back({left, mid, "待处理", COLOR_GREEN_BOX});
    }

    mergeSortRecursive(arrViz, ctrl, arr, left, mid, leftPending);
    mergeSortRecursive(arrViz, ctrl, arr, mid + 1, right, rightPending);

    // 合并时，传递所有待处理区（过滤掉与当前合并区间重叠的）
    std::vector<DashedBox> mergePending;
    for (const auto &box : pendingBoxes) {
        if (box.endIndex < left || box.startIndex > right) {
            mergePending.push_back(box);
        }
    }
    merge(arrViz, ctrl, arr, left, mid, right, mergePending);
}

void MergeSort::merge(
    ArrayVisualizer *arrViz,
    AnimationController *ctrl,
    std::vector<int> &arr,
    int left, int mid, int right,
    const std::vector<DashedBox> &pendingBoxes)
{
    // 合并阶段：显示合并区间，并保留待处理区
    int ll = left, rr = right, lMid = mid, rStart = mid + 1;
    ctrl->addStep({
        [arrViz, ll, lMid, rStart, rr, pendingBoxes]() {
            std::vector<DashedBox> boxes;
            // 添加合并中的两个子数组（红色）
            boxes.push_back({ll, lMid, "合并中", COLOR_RED_BOX});
            boxes.push_back({rStart, rr, "合并中", COLOR_RED_BOX});
            // 添加待处理区（绿色）
            for (const auto &box : pendingBoxes) {
                boxes.push_back(box);
            }
            arrViz->setDashedBoxes(boxes);
        },
        nullptr,
        QString("开始合并区间 [%1, %2] 和 [%3, %4]").arg(ll).arg(lMid).arg(rStart).arg(rr)
    });

    int n = right - left + 1;
    std::vector<int> temp(n);
    
    for (int i = 0; i < n; i++) {
        int srcIdx = left + i;
        int dstIdx = i;
        ctrl->addStep({
            [arrViz, srcIdx, dstIdx, arr]() {
                arrViz->setBlockState(srcIdx, BlockState::Active);
                arrViz->setTempValue(dstIdx, arr[srcIdx]);
            },
            [arrViz, srcIdx]() {
                arrViz->setBlockState(srcIdx, BlockState::Normal);
            },
            QString("拷贝 arr[%1]=%2 到临时空间[%3]")
                .arg(srcIdx).arg(arr[srcIdx]).arg(dstIdx)
        });
        temp[i] = arr[left + i];
    }
    
    int leftSize = mid - left + 1;
    ctrl->addStep({
        [arrViz, leftSize, n]() {
            arrViz->setTempDashedBoxes({
                {0, leftSize - 1, "左", COLOR_BLUE_BOX},
                {leftSize, n - 1, "右", COLOR_BLUE_BOX}
            });
        },
        nullptr,
        "区分左右子数组"
    });
    
    int i = 0;
    int j = leftSize;
    int k = left;
    
    while (i < leftSize && j < n) {
        int ii = i, jj = j;
        ctrl->addStep({
            [arrViz, ii, jj]() {
                arrViz->setTempArrows({{ii, "i"}, {jj, "j"}});
                arrViz->highlightTempBlock(ii);
                arrViz->highlightTempBlock(jj);
            },
            [arrViz, ii, jj]() {
                arrViz->unhighlightTempBlock(ii);
                arrViz->unhighlightTempBlock(jj);
            },
            QString("比较 temp[i]=%1 和 temp[j]=%2").arg(ii).arg(jj)
        });
        
        if (temp[i] <= temp[j]) {
            int val = temp[i];
            int dstIdx = k;
            ctrl->addStep({
                [arrViz, dstIdx, val]() {
                    arrViz->setValue(dstIdx, val);
                    arrViz->setBlockState(dstIdx, BlockState::Normal);
                },
                nullptr,
                QString("temp[i] 较小，回填到 arr[%1]").arg(dstIdx)
            });
            arr[k++] = temp[i++];
        } else {
            int val = temp[j];
            int dstIdx = k;
            ctrl->addStep({
                [arrViz, dstIdx, val]() {
                    arrViz->setValue(dstIdx, val);
                    arrViz->setBlockState(dstIdx, BlockState::Normal);
                },
                nullptr,
                QString("temp[j] 较小，回填到 arr[%1]").arg(dstIdx)
            });
            arr[k++] = temp[j++];
        }
    }
    
    while (i < leftSize) {
        int val = temp[i];
        int dstIdx = k;
        ctrl->addStep({
            [arrViz, dstIdx, val]() {
                arrViz->setValue(dstIdx, val);
                arrViz->setBlockState(dstIdx, BlockState::Normal);
            },
            nullptr,
            QString("左子数组剩余，回填 arr[%1]=%2").arg(dstIdx).arg(val)
        });
        arr[k++] = temp[i++];
    }
    while (j < n) {
        int val = temp[j];
        int dstIdx = k;
        ctrl->addStep({
            [arrViz, dstIdx, val]() {
                arrViz->setValue(dstIdx, val);
                arrViz->setBlockState(dstIdx, BlockState::Normal);
            },
            nullptr,
            QString("右子数组剩余，回填 arr[%1]=%2").arg(dstIdx).arg(val)
        });
        arr[k++] = temp[j++];
    }
    
    ctrl->addStep({
        [arrViz]() {
            arrViz->clearTempSpace();
            arrViz->clearTempArrows();
            arrViz->clearTempDashedBoxes();
        },
        nullptr,
        QString("区间 [%1, %2] 合并完成").arg(left).arg(right)
    });
}
