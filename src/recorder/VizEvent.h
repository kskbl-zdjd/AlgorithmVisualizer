#ifndef VIZ_EVENT_H
#define VIZ_EVENT_H

#include <cstring>

// 事件类型枚举（纯 C++）
enum class VizEventType : int {
    ArrayCreate = 1,     // 创建数组
    SetValue = 2,        // 更新值
    Swap = 3,            // 交换
    Compare = 4,         // 比较
    Mark = 5,            // 放置箭头
    Unmark = 6,          // 移除箭头
    UnmarkAll = 7,       // 移除所有箭头
    RecurseEnter = 8,    // 进入递归
    RecursePending = 9,  // 待处理区间
    RecurseLeave = 10,   // 离开递归
    Highlight = 11,      // 高亮
    Reset = 12,          // 重置
    Comment = 13,        // 文字说明
    Resize = 14,         // 数组扩容/缩容
    Clear = 15,          // 清空容器（vv.clear()）
    VecResize = 16,      // vector::resize(n) 或 resize(n, val)（专用，携带标注文字）
    ArrayDestroy = 17,   // 销毁数组面板（值参数函数退出时）
};

// 事件结构体（纯 C++，无 Qt）
struct VizEvent {
    VizEventType type;
    int index1;
    int index2;
    int value;
    char label[64];       // 固定长度字符串，替代 QString
    char comment[256];    // 固定长度字符串
    
    // Resize 事件专用字段
    int oldSize;          // 扩容/缩容前的大小
    int newSize;          // 扩容/缩容后的大小
    
    // 使用固定大小数组替代 std::vector，避免跨 DLL 边界内存问题
    static const int MAX_ARRAY_SIZE = 256;
    int arrayData[MAX_ARRAY_SIZE];  // 数组数据（固定大小，直接存储在结构体内）
    int arrayDataCount;              // 实际数据个数
    
    char arrayName[64];

    // 构造函数，初始化所有字段
    VizEvent() : type(VizEventType::Comment), index1(0), index2(0), 
                 value(0), arrayDataCount(0), oldSize(0), newSize(0) {
        label[0] = '\0';
        comment[0] = '\0';
        arrayName[0] = '\0';
    }

    // 辅助函数：安全设置字符串
    void setLabel(const char* s) {
        if (s) {
            strncpy(label, s, 63);
            label[63] = '\0';
        }
    }

    void setComment(const char* s) {
        if (s) {
            strncpy(comment, s, 255);
            comment[255] = '\0';
        }
    }

    void setArrayName(const char* s) {
        if (s) {
            strncpy(arrayName, s, 63);
            arrayName[63] = '\0';
        }
    }
    
    // 设置数组数据（深复制到固定数组，安全跨 DLL 边界）
    void setArrayData(const int* data, int count) {
        arrayDataCount = (count > MAX_ARRAY_SIZE) ? MAX_ARRAY_SIZE : count;
        for (int i = 0; i < arrayDataCount; i++) {
            arrayData[i] = data[i];
        }
    }
};

#endif // VIZ_EVENT_H
