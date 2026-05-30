#ifndef SORTINGALGORITHMS_H
#define SORTINGALGORITHMS_H

#include "../core/AnimationController.h"
#include "../visualizers/ArrayVisualizer.h"
#include <vector>

class SortingAlgorithms
{
public:
    static void bubbleSort(
        ArrayVisualizer *viz,
        AnimationController *ctrl,
        const std::vector<int> &data
    );

    static void selectionSort(
        ArrayVisualizer *viz,
        AnimationController *ctrl,
        const std::vector<int> &data
    );

    static void insertionSort(
        ArrayVisualizer *viz,
        AnimationController *ctrl,
        const std::vector<int> &data
    );

    static void quickSort(
        ArrayVisualizer *viz,
        AnimationController *ctrl,
        const std::vector<int> &data
    );

private:
    static void quickSortRecursive(
        ArrayVisualizer *viz,
        AnimationController *ctrl,
        std::vector<int> &arr,
        int low, int high,
        const std::vector<DashedBox> &pendingBoxes,
        const QString &path
    );

    static int partition(
        ArrayVisualizer *viz,
        AnimationController *ctrl,
        std::vector<int> &arr,
        int low, int high,
        const QString &path
    );
};

#endif // SORTINGALGORITHMS_H
