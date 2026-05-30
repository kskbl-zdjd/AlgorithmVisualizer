#ifndef MERGESORT_H
#define MERGESORT_H

#include "../core/AnimationController.h"
#include "../visualizers/ArrayVisualizer.h"
#include <vector>

class MergeSort {
public:
    static void mergeSort(
        ArrayVisualizer *arrViz,
        AnimationController *ctrl,
        const std::vector<int> &data
    );

private:
    static void mergeSortRecursive(
        ArrayVisualizer *arrViz,
        AnimationController *ctrl,
        std::vector<int> &arr,
        int left, int right,
        const std::vector<DashedBox> &pendingBoxes
    );
    
    static void merge(
        ArrayVisualizer *arrViz,
        AnimationController *ctrl,
        std::vector<int> &arr,
        int left, int mid, int right,
        const std::vector<DashedBox> &pendingBoxes
    );
};

#endif // MERGESORT_H
