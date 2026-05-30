#ifndef VIZ_RECORDER_H
#define VIZ_RECORDER_H

#include "VizEvent.h"
#include <vector>

// VizRecorder 仅用于复杂度统计，不再持有动画步骤转换逻辑
// 动画步骤转换已移至 CodeExecutor::convertEventsToSteps()
class VizRecorder {
public:
    VizRecorder() {}

    // 获取复杂度统计
    struct ComplexityStats {
        int comparisons = 0;
        int swaps = 0;
        int arrayAccesses = 0;
        int maxRecursionDepth = 0;
        int currentRecursionDepth = 0;
        int arrayCount = 0;
        int maxArraySize = 0;
        int inputSize = 0;

        // 估算时间/空间复杂度（返回字符串）
        const char* estimateTimeRaw() const;
        const char* estimateSpaceRaw() const;
    };

    // 从事件列表计算复杂度统计
    static ComplexityStats computeStats(const std::vector<VizEvent>& events);
};

#endif // VIZ_RECORDER_H
