#include "VizRecorder.h"
#include <algorithm>
#include <cmath>

VizRecorder::ComplexityStats VizRecorder::computeStats(const std::vector<VizEvent>& events) {
    ComplexityStats stats;

    for (const auto& e : events) {
        switch (e.type) {
        case VizEventType::ArrayCreate:
            stats.arrayCount++;
            stats.maxArraySize = std::max(stats.maxArraySize, e.arrayDataCount);
            if (stats.inputSize == 0) {
                stats.inputSize = e.arrayDataCount;
            }
            break;
        case VizEventType::SetValue:
            stats.arrayAccesses++;
            break;
        case VizEventType::Swap:
            stats.swaps++;
            stats.arrayAccesses += 2;
            break;
        case VizEventType::Compare:
            stats.comparisons++;
            stats.arrayAccesses += 2;
            break;
        case VizEventType::Mark:
            stats.arrayAccesses++;
            break;
        case VizEventType::RecurseEnter:
            stats.currentRecursionDepth++;
            stats.maxRecursionDepth = std::max(stats.maxRecursionDepth, stats.currentRecursionDepth);
            break;
        case VizEventType::RecurseLeave:
            if (stats.currentRecursionDepth > 0)
                stats.currentRecursionDepth--;
            break;
        default:
            break;
        }
    }

    return stats;
}

const char* VizRecorder::ComplexityStats::estimateTimeRaw() const {
    if (inputSize == 0) return "N/A";
    double ratio = (double)(comparisons + swaps) / inputSize;
    if (ratio <= 1.5) return "O(n)";
    if (ratio <= inputSize * 1.5) return "O(n log n) 或更高";
    return "O(n^2)";
}

const char* VizRecorder::ComplexityStats::estimateSpaceRaw() const {
    if (maxRecursionDepth > 0) return "O(n) [递归]";
    return "O(1) [原地]";
}
