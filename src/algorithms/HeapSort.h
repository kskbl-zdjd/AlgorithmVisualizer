#ifndef HEAPSORT_H
#define HEAPSORT_H

#include "../core/AnimationController.h"
#include "../visualizers/ArrayVisualizer.h"
#include "../visualizers/HeapVisualizer.h"
#include <vector>
#include <QString>

class HeapSort {
public:
    static void heapSort(
        HeapVisualizer *heapViz,
        ArrayVisualizer *arrViz,
        AnimationController *ctrl,
        const std::vector<int> &data
    );

private:
    static void buildHeap(
        HeapVisualizer *heapViz,
        ArrayVisualizer *arrViz,
        AnimationController *ctrl,
        std::vector<int> &arr
    );

    static void siftDown(
        HeapVisualizer *heapViz,
        ArrayVisualizer *arrViz,
        AnimationController *ctrl,
        std::vector<int> &arr,
        int n,
        int i,
        const QString &phase
    );
};

#endif // HEAPSORT_H
