# -*- coding: utf-8 -*-
import os

filepath = "E:/QTproject/algo-visualizer/src/algorithms/SortingAlgorithms.cpp"

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()
    lines = content.splitlines()

print(f"Total lines: {len(lines)}")

# Find where the first "快速排序" comment is (for color constants)
old_color_start = None
for i, line in enumerate(lines):
    if '快速排序' in line and i < 10:
        old_color_start = i
        break

# Find the second "快速排序" comment (for quick sort implementations)
qs_start = None
for i, line in enumerate(lines):
    if '快速排序' in line and i > 200:
        qs_start = i
        break

print(f"Color constants section starts at line: {old_color_start + 1}")
print(f"Quick sort section starts at line: {qs_start + 1}")

# Find end of makeDashedBox function
old_color_end = None
for i, line in enumerate(lines):
    if i > old_color_start and '规范' in line:
        old_color_end = i
        break

print(f"Old section ends at line: {old_color_end + 1}")

# New quick sort code (plan, adapted to use DashedBox(...) constructor)
new_quicksort = """// ============================================================
// 快速排序（优化版：双框标记 + 双箭头 + 递归路径）
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
    // 红色框 = 当前处理区间，绿色框 = 待处理区间
    int ll = low, hh = high;
    ctrl->addStep({
        [viz, ll, hh, pendingBoxes]() {
            viz->resetAllBlockStates();
            std::vector<DashedBox> boxes;
            boxes.push_back(DashedBox(ll, hh, "处理中", COLOR_RED_BOX));
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

    // 准备左右子区间的待处理标记
    std::vector<DashedBox> leftPending = pendingBoxes;
    std::vector<DashedBox> rightPending = pendingBoxes;

    if (pi + 1 <= high) {
        leftPending.push_back(DashedBox(pi + 1, high, "待处理", COLOR_GREEN_BOX));
    }
    if (low <= pi - 1) {
        rightPending.push_back(DashedBox(low, pi - 1, "待处理", COLOR_GREEN_BOX));
    }

    if (low <= pi - 1) {
        quickSortRecursive(viz, ctrl, arr, low, pi - 1, leftPending, path + " -> 左");
    }
    if (pi + 1 <= high) {
        quickSortRecursive(viz, ctrl, arr, pi + 1, high, rightPending, path + " -> 右");
    }
}

int SortingAlgorithms::partition(
    ArrayVisualizer *viz,
    AnimationController *ctrl,
    std::vector<int> &arr,
    int low, int high,
    const QString &path)
{
    int pivot = arr[high];

    // 步骤：选择基准
    int hh = high;
    ctrl->addStep({
        [viz, hh]() {
            viz->resetAllBlockStates();
            viz->setBlockState(hh, BlockState::Active);
            viz->setArrows({{hh, "pivot"}});
        },
        [viz]() {
            viz->resetAllBlockStates();
            viz->clearArrows();
        },
        QString("%1: 选择基准 arr[%2]=%3").arg(path).arg(hh).arg(pivot)
    });

    int i = low - 1;

    for (int j = low; j < high; ++j) {
        int jj = j;

        // 步骤：j 扫描，显示 pivot 和 j 两个箭头
        ctrl->addStep({
            [viz, jj, hh]() {
                viz->resetAllBlockStates();
                viz->setBlockState(hh, BlockState::Active);
                viz->setBlockState(jj, BlockState::Active);
                viz->setArrows({{hh, "pivot"}, {jj, "j"}});
            },
            [viz]() {
                viz->resetAllBlockStates();
                viz->clearArrows();
            },
            QString("%1: 比较 arr[%2]=%3 与基准 %4")
                .arg(path).arg(jj).arg(arr[jj]).arg(pivot)
        });

        if (arr[j] < pivot) {
            i++;
            if (i != j) {
                int a = i, b = j;
                ctrl->addStep({
                    [viz, a, b, hh]() {
                        viz->swapValues(a, b);
                        viz->setBlockState(hh, BlockState::Active);
                        viz->setBlockState(a, BlockState::Active);
                        viz->setBlockState(b, BlockState::Active);
                        viz->setArrows({{hh, "pivot"}, {b, "j"}});
                    },
                    [viz, a, b]() {
                        viz->swapValues(a, b);
                        viz->resetAllBlockStates();
                        viz->clearArrows();
                    },
                    QString("%1: arr[%2]<%3, 交换 arr[%4]和arr[%5]")
                        .arg(path).arg(arr[b]).arg(pivot).arg(a).arg(b)
                });
                std::swap(arr[i], arr[j]);
            }
        }
    }

    // 步骤：基准归位
    int pivotPos = i + 1;
    if (pivotPos != high) {
        int a = pivotPos, b = high;
        ctrl->addStep({
            [viz, a, b]() {
                viz->swapValues(a, b);
                viz->setBlockState(a, BlockState::Active);
                viz->setBlockState(b, BlockState::Active);
                viz->setArrows({{a, "pivot"}});
            },
            [viz, a, b]() {
                viz->swapValues(a, b);
                viz->resetAllBlockStates();
                viz->clearArrows();
            },
            QString("%1: 基准 %2 归位到 arr[%3]")
                .arg(path).arg(pivot).arg(pivotPos)
        });
        std::swap(arr[pivotPos], arr[high]);
    }

    // 步骤：基准就位
    int pp = pivotPos;
    ctrl->addStep({
        [viz, pp]() {
            viz->resetAllBlockStates();
            viz->setBlockState(pp, BlockState::Active);
            viz->clearArrows();
        },
        nullptr,
        QString("%1: 基准 %2 已就位于 arr[%3]")
            .arg(path).arg(pivot).arg(pp)
    });

    return pivotPos;
}
"""

# Color constants to prepend
color_constants = """// 颜色常量
static const QColor COLOR_RED_BOX("#F44336");   // 红色 - 当前处理区间
static const QColor COLOR_GREEN_BOX("#4CAF50"); // 绿色 - 待处理区间

"""

# Build new content:
# 1. Lines 0 to old_color_start-1 (just #include + blank)
# 2. New color constants
# 3. Lines old_color_end+1 to qs_start-1 (bubble/selection/insertion sort)
# 4. New quick sort code

prefix = '\n'.join(lines[0:old_color_start])
middle = '\n'.join(lines[old_color_end+1:qs_start])

new_content = prefix + '\n' + color_constants + middle + '\n' + new_quicksort

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(new_content)

new_lines = len(new_content.splitlines())
print(f"Done! New file has {new_lines} lines")
print("Quick sort section uses DashedBox(...) constructor, no i-arrow (only pivot + j)")
