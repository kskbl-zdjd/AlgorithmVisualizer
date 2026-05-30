#include "SortingAlgorithms.h"

// ============================================================
// 颜色常量
static const QColor COLOR_RED_BOX("#F44336");   // 红色 - 当前处理区间
static const QColor COLOR_GREEN_BOX("#4CAF50"); // 绿色 - 待处理区间

// ============================================================
void SortingAlgorithms::bubbleSort(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    const std::vector<int> &data)
{
    ctrl->clearSteps();
    viz->setData(data);
    std::vector<int> arr = data;
    int n = static_cast<int>(arr.size());

    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - i - 1; ++j) {
            // 步骤：箭头指向 j，比较 arr[j] 和 arr[j+1]
            int jj = j;
            ctrl->addStep({
                // execute
                [viz, jj]() {
                    viz->resetAllBlockStates();
                    viz->setBlockState(jj, BlockState::Active);
                    viz->setBlockState(jj + 1, BlockState::Active);
                    viz->setArrows({{jj, "j"}});
                    viz->clearDashedBoxes();
                },
                // undo
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                    viz->clearDashedBoxes();
                },
                QString("比较 arr[%1]=%2 和 arr[%3]=%4")
                    .arg(jj).arg(arr[jj]).arg(jj + 1).arg(arr[jj + 1])
            });

            if (arr[j] > arr[j + 1]) {
                // 步骤：交换
                int a = j, b = j + 1;
                ctrl->addStep({
                    [viz, a, b]() {
                        viz->swapValues(a, b);
                        viz->setBlockState(a, BlockState::Active);
                        viz->setBlockState(b, BlockState::Active);
                    },
                    [viz, a, b]() {
                        viz->swapValues(a, b);
                        viz->resetAllBlockStates();
                    },
                    QString("交换 arr[%1] 和 arr[%2]").arg(a).arg(b)
                });
                std::swap(arr[j], arr[j + 1]);
            }
        }

        // 步骤：第 i 轮结束，arr[n-1-i] 已就位
        int sortedIdx = n - 1 - i;
        ctrl->addStep({
            [viz, sortedIdx]() {
                viz->resetAllBlockStates();
                viz->clearArrows();
                viz->clearDashedBoxes();
            },
            nullptr,
            QString("第 %1 轮结束，arr[%2]=%3 已就位")
                .arg(i + 1).arg(sortedIdx).arg(arr[sortedIdx])
        });
    }

    ctrl->addStep({
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
            viz->clearDashedBoxes();
        },
        nullptr,
        "冒泡排序完成"
    });
}

// ============================================================
// 选择排序
// 规范：红色虚线框=待排序部分，蓝色箭头=当前遍历位置，最小值色块绿→蓝
// ============================================================
void SortingAlgorithms::selectionSort(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    const std::vector<int> &data)
{
    ctrl->clearSteps();
    viz->setData(data);
    std::vector<int> arr = data;
    int n = static_cast<int>(arr.size());

    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;

        // 步骤：标记待排序区域 + 箭头指向起点
        int ii = i;
        ctrl->addStep({
            [viz, ii, n]() {
                viz->resetAllBlockStates();
                viz->setDashedBoxes({{ii, n - 1, "待排序"}});
                viz->setArrows({{ii, "j"}});
                viz->setBlockState(ii, BlockState::Active);
            },
            [viz]() {
                viz->resetAllBlockStates();
                viz->clearArrows();
                viz->clearDashedBoxes();
            },
            QString("开始第 %1 轮，从 arr[%2] 寻找最小值").arg(i + 1).arg(i)
        });

        for (int j = i + 1; j < n; ++j) {
            // 步骤：箭头移到 j，比较
            int jj = j, mi = minIdx;
            ctrl->addStep({
                [viz, jj, mi, ii, n]() {
                    viz->resetAllBlockStates();
                    viz->setDashedBoxes({{ii, n - 1, "待排序"}});
                    viz->setArrows({{jj, "j"}});
                    viz->setBlockState(mi, BlockState::Active); // 当前最小值蓝色
                    viz->setBlockState(jj, BlockState::Active); // 当前比较元素蓝色
                },
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                    viz->clearDashedBoxes();
                },
                QString("比较 arr[%1]=%2（当前最小）和 arr[%3]=%4")
                    .arg(mi).arg(arr[mi]).arg(jj).arg(arr[jj])
            });

            if (arr[j] < arr[minIdx]) {
                minIdx = j;
            }
        }

        if (minIdx != i) {
            // 步骤：交换
            int a = i, b = minIdx;
            ctrl->addStep({
                [viz, a, b, ii, n]() {
                    viz->swapValues(a, b);
                    viz->setDashedBoxes({{ii, n - 1, "待排序"}});
                    viz->setBlockState(a, BlockState::Active);
                    viz->setBlockState(b, BlockState::Active);
                },
                [viz, a, b]() {
                    viz->swapValues(a, b);
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                    viz->clearDashedBoxes();
                },
                QString("交换 arr[%1]=%2 和 arr[%3]=%4")
                    .arg(i).arg(arr[i]).arg(minIdx).arg(arr[minIdx])
            });
            std::swap(arr[i], arr[minIdx]);
        }

        // 步骤：arr[i] 已就位
        int si = i;
        ctrl->addStep({
            [viz, si, n]() {
                viz->resetAllBlockStates();
                viz->clearArrows();
                viz->clearDashedBoxes();
            },
            nullptr,
            QString("arr[%1]=%2 已就位").arg(si).arg(arr[si])
        });
    }

    ctrl->addStep({
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
            viz->clearDashedBoxes();
        },
        nullptr,
        "选择排序完成"
    });
}

// ============================================================
// 插入排序
// 规范：红色虚线框=有序区域，蓝色箭头=有序区间内比较元素，目标色块绿→蓝
// ============================================================
void SortingAlgorithms::insertionSort(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    const std::vector<int> &data)
{
    ctrl->clearSteps();
    viz->setData(data);
    std::vector<int> arr = data;
    int n = static_cast<int>(arr.size());

    // 步骤：arr[0] 视为有序
    ctrl->addStep({
        [viz]() {
            viz->resetAllBlockStates();
            viz->setDashedBoxes({{0, 0, "有序区"}});
            viz->clearArrows();
        },
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearDashedBoxes();
            viz->clearArrows();
        },
        "arr[0] 视为已排序"
    });

    for (int i = 1; i < n; ++i) {
        int key = arr[i];
        int j = i - 1;

        // 步骤：取出目标元素，色块变蓝
        int ii = i;
        ctrl->addStep({
            [viz, ii, j]() {
                viz->resetAllBlockStates();
                viz->setBlockState(ii, BlockState::Active);
                viz->setDashedBoxes({{0, j, "有序区"}});
                viz->setArrows({{ii, "key"}});
            },
            [viz]() {
                viz->resetAllBlockStates();
                viz->clearArrows();
                viz->clearDashedBoxes();
            },
            QString("取出 arr[%1]=%2 准备插入").arg(ii).arg(key)
        });

        while (j >= 0 && arr[j] > key) {
            // 步骤：箭头指向 j，比较 arr[j] 和 key
            int jj = j;
            ctrl->addStep({
                [viz, jj, ii]() {
                    viz->resetAllBlockStates();
                    viz->setBlockState(jj, BlockState::Active);
                    viz->setBlockState(ii, BlockState::Active);
                    viz->setDashedBoxes({{0, jj, "有序区"}});
                    viz->setArrows({{jj, "j"}});
                },
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                    viz->clearDashedBoxes();
                },
                QString("arr[%1]=%2 > %3，后移").arg(jj).arg(arr[jj]).arg(key)
            });

            // 步骤：后移
            int a = j, b = j + 1;
            ctrl->addStep({
                [viz, a, b, jj]() {
                    viz->swapValues(a, b);
                    viz->setBlockState(a, BlockState::Active);
                    viz->setBlockState(b, BlockState::Active);
                    viz->setDashedBoxes({{0, jj - 1 >= 0 ? jj - 1 : 0, "有序区"}});
                },
                [viz, a, b]() {
                    viz->swapValues(a, b);
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                    viz->clearDashedBoxes();
                },
                QString("arr[%1] 后移到 arr[%2]").arg(a).arg(b)
            });
            std::swap(arr[j], arr[j + 1]);
            j--;
        }

        // 步骤：插入完成，更新有序区
        int sortedEnd = i;
        ctrl->addStep({
            [viz, sortedEnd, j]() {
                viz->resetAllBlockStates();
                viz->setDashedBoxes({{0, sortedEnd, "有序区"}});
                viz->clearArrows();
            },
            nullptr,
            QString("%1 插入到位置 %2，有序区扩大").arg(key).arg(j + 1)
        });
    }

    ctrl->addStep({
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
            viz->clearDashedBoxes();
        },
        nullptr,
        "插入排序完成"
    });
}

// ============================================================
// 快速排序（Hoare 双向分区 - j 先遍历）
// ============================================================

void SortingAlgorithms::quickSort(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    const std::vector<int> &data)
{
    ctrl->clearSteps();
    viz->setData(data);
    std::vector<int> arr = data;

    std::vector<DashedBox> pendingBoxes;
    quickSortRecursive(viz, ctrl, arr, 0, static_cast<int>(arr.size()) - 1, pendingBoxes, "快排");

    ctrl->addStep({
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
            viz->clearDashedBoxes();
        },
        nullptr,
        "快速排序完成"
    });
}

void SortingAlgorithms::quickSortRecursive(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    std::vector<int> &arr,
    int low, int high,
    const std::vector<DashedBox> &pendingBoxes,
    const QString &path)
{
    if (low >= high) {
        if (low == high) {
            int ll = low;
            ctrl->addStep({
                [viz, ll]() {
                    viz->resetAllBlockStates();
                    viz->setBlockState(ll, BlockState::Active);
                },
                nullptr,
                QString("%1: arr[%2]=%3 已就位").arg(path).arg(ll).arg(arr[ll])
            });
        }
        return;
    }

    // 步骤：进入递归区间
    int ll = low, hh = high;
    ctrl->addStep({
        [viz, ll, hh, pendingBoxes]() {
            viz->resetAllBlockStates();
            std::vector<DashedBox> boxes;
            boxes.push_back({ll, hh, "处理中", COLOR_RED_BOX});
            for (const auto &box : pendingBoxes) {
                boxes.push_back(box);
            }
            viz->setDashedBoxes(boxes);
            viz->clearArrows();
        },
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
            viz->clearDashedBoxes();
        },
        QString("%1(%2,%3): 进入区间").arg(path).arg(low).arg(high)
    });

    int pi = partition(viz, ctrl, arr, low, high, path);

    // ✅ 准备左右子区间的待处理标记（带边界检查）
    // 处理左子区间 [low, pi] 时，右子区间 [pi+1, high] 是待处理的（绿色框）
    // 处理右子区间 [pi+1, high] 时，左子区间 [low, pi] 已经是处理完的（不需要标记）
    std::vector<DashedBox> leftPending = pendingBoxes;   // 用于左子区间递归
    std::vector<DashedBox> rightPending = pendingBoxes;  // 用于右子区间递归

    // 右子区间 [pi + 1, high] 是待处理的，加入 leftPending
    if (pi + 1 <= high) {
        leftPending.push_back({pi + 1, high, "待处理", COLOR_GREEN_BOX});
    }

    // ✅ 递归调用带边界检查
    // 先处理左子区间 [low, pi]
    if (low <= pi) {
        quickSortRecursive(viz, ctrl, arr, low, pi, leftPending, path + " → 左");
    }
    // 再处理右子区间 [pi + 1, high]
    if (pi + 1 <= high) {
        quickSortRecursive(viz, ctrl, arr, pi + 1, high, rightPending, path + " → 右");
    }
}

int SortingAlgorithms::partition(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    std::vector<int> &arr,
    int low, int high,
    const QString &path)
{
    int pivot = arr[low];
    int i = low + 1;   // ✅ i 从 low+1 开始，不在基准位置
    int j = high;       // ✅ j 从 high 开始，初始位置参与比较
    int ll = low, hh = high;

    // 步骤：选择基准（只有 pivot 变蓝）
    ctrl->addStep({
        [viz, ll]() {
            viz->resetAllBlockStates();          // 所有变绿
            viz->setBlockState(ll, BlockState::Active);  // 只有 pivot 变蓝
            viz->setArrows({{ll, "pivot"}});
        },
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
        },
        QString("%1: 选择基准 arr[%2]=%3").arg(path).arg(ll).arg(pivot)
    });

    // 步骤：初始化双指针（pivot、i、j 变蓝）
    {
        int ii = i, jj = j;
        ctrl->addStep({
            [viz, ii, jj, ll]() {
                viz->resetAllBlockStates();          // 所有变绿
                viz->setBlockState(ll, BlockState::Active);  // pivot 变蓝
                viz->setBlockState(ii, BlockState::Active);  // i 变蓝
                viz->setBlockState(jj, BlockState::Active);  // j 变蓝
                viz->setArrows({{ll, "pivot"}, {ii, "i"}, {jj, "j"}});
            },
            [viz]() {
                viz->resetAllBlockStates();
                viz->clearArrows();
            },
            QString("%1: i=%2, j=%3, 开始双向扫描").arg(path).arg(i).arg(j)
        });
    }

    while (i <= j) {
        // ✅ j 先遍历：从右向左找 < pivot 的元素（初始位置参与比较）
        while (i <= j && arr[j] >= pivot) {
            int ii = i, jj = j;
            ctrl->addStep({
                [viz, ii, jj, ll]() {
                    viz->resetAllBlockStates();
                    viz->setBlockState(ll, BlockState::Active);  // pivot 变蓝
                    viz->setBlockState(ii, BlockState::Active);  // i 变蓝
                    viz->setBlockState(jj, BlockState::Active);  // j 变蓝
                    viz->setArrows({{ll, "pivot"}, {ii, "i"}, {jj, "j"}});
                },
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                },
                QString("%1: j在 %2, arr[%2]=%3 >= pivot=%4, 继续")
                    .arg(path).arg(jj).arg(arr[jj]).arg(pivot)
            });
            j--;
        }

        if (i > j) break;

        // j 找到 < pivot 的元素，停止（pivot、i、j 变蓝）
        {
            int ii = i, jj = j;
            ctrl->addStep({
                [viz, ii, jj, ll]() {
                    viz->resetAllBlockStates();
                    viz->setBlockState(ll, BlockState::Active);  // pivot 变蓝
                    viz->setBlockState(ii, BlockState::Active);  // i 变蓝
                    viz->setBlockState(jj, BlockState::Active);  // j 变蓝
                    viz->setArrows({{ll, "pivot"}, {ii, "i"}, {jj, "j"}});
                },
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                },
                QString("%1: j停在 %2, arr[%2]=%3 < pivot=%4")
                    .arg(path).arg(jj).arg(arr[jj]).arg(pivot)
            });
        }

        // ✅ i 再遍历：从左向右找 > pivot 的元素（初始位置参与比较）
        while (i <= j && arr[i] <= pivot) {
            int ii = i, jj = j;
            ctrl->addStep({
                [viz, ii, jj, ll]() {
                    viz->resetAllBlockStates();          // 所有变绿
                    viz->setBlockState(ll, BlockState::Active);  // pivot 变蓝
                    viz->setBlockState(ii, BlockState::Active);  // i 变蓝
                    viz->setBlockState(jj, BlockState::Active);  // j 变蓝
                    viz->setArrows({{ll, "pivot"}, {ii, "i"}, {jj, "j"}});
                },
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                },
                QString("%1: i在 %2, arr[%2]=%3 <= pivot=%4, 继续")
                    .arg(path).arg(ii).arg(arr[ii]).arg(pivot)
            });
            i++;
        }

        if (i > j) break;

        // i 找到 > pivot 的元素，停止（pivot、i、j 变蓝）
        {
            int ii = i, jj = j;
            ctrl->addStep({
                [viz, ii, jj, ll]() {
                    viz->resetAllBlockStates();
                    viz->setBlockState(ll, BlockState::Active);  // pivot 变蓝
                    viz->setBlockState(ii, BlockState::Active);  // i 变蓝
                    viz->setBlockState(jj, BlockState::Active);  // j 变蓝
                    viz->setArrows({{ll, "pivot"}, {ii, "i"}, {jj, "j"}});
                },
                [viz]() {
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                },
                QString("%1: i停在 %2, arr[%2]=%3 > pivot=%4")
                    .arg(path).arg(ii).arg(arr[ii]).arg(pivot)
            });
        }

        // 交换 arr[i] 和 arr[j]（pivot、i、j 变蓝，交换的两个也变蓝）
        {
            int a = i, b = j;
            ctrl->addStep({
                [viz, a, b, ll]() {
                    viz->swapValues(a, b);
                    viz->resetAllBlockStates();
                    viz->setBlockState(ll, BlockState::Active);  // pivot 变蓝
                    viz->setBlockState(a, BlockState::Active);   // i 变蓝
                    viz->setBlockState(b, BlockState::Active);   // j 变蓝
                    viz->setArrows({{ll, "pivot"}, {a, "i"}, {b, "j"}});
                },
                [viz, a, b]() {
                    viz->swapValues(a, b);
                    viz->resetAllBlockStates();
                    viz->clearArrows();
                },
                QString("%1: 交换 arr[%2]=%3 和 arr[%4]=%5")
                    .arg(path).arg(a).arg(arr[a]).arg(b).arg(arr[b])
            });
            std::swap(arr[i], arr[j]);
        }

        // ✅ 交换后 i 和 j 保持不动，继续 while 循环比较当前位置的值
        // 不执行 i++ 和 j--
    }

    // ✅ 步骤：pivot 归位，交换 arr[low] 和 arr[j]
    // 注意：j 可能小于 low（当所有元素都 >= pivot 时），需要保护
    int pivotPos = j;

    // 确保 pivotPos 在有效范围内
    if (pivotPos < low) pivotPos = low;
    if (pivotPos > high) pivotPos = high;

    if (pivotPos != low) {
        int a = low, b = pivotPos;
        ctrl->addStep({
            [viz, a, b]() {
                viz->swapValues(a, b);
                viz->resetAllBlockStates();
                viz->setBlockState(b, BlockState::Active);  // pivot 新位置变蓝
                viz->setArrows({{b, "pivot"}});
            },
            [viz, a, b]() {
                viz->swapValues(a, b);
                viz->resetAllBlockStates();
                viz->clearArrows();
            },
            QString("%1: 基准 %2 归位到 arr[%3]")
                .arg(path).arg(pivot).arg(pivotPos)
        });
        std::swap(arr[low], arr[pivotPos]);
    }

    // 步骤：分区完成（只有 pivot 变蓝）
    {
        int pp = pivotPos;
        ctrl->addStep({
            [viz, pp]() {
                viz->resetAllBlockStates();
                viz->setBlockState(pp, BlockState::Active);  // pivot 变蓝
                viz->clearArrows();
            },
            nullptr,
            QString("%1: 基准 %2 已就位于 arr[%3]")
                .arg(path).arg(pivot).arg(pp)
        });
    }

    return pivotPos;
}

