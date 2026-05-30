#ifndef VIZ_API_H
#define VIZ_API_H

#include "VizEvent.h"
#include <vector>

// 前向声明
namespace viz {
    // 全局事件缓冲区指针（DLL 和主程序共享）
    extern std::vector<VizEvent>* g_eventBuffer;

    // 步骤计数器（用于无限循环保护）
    extern int g_stepCount;
    extern const int MAX_STEPS;

    // 检查步骤上限
    bool checkStepLimit();

    // 设置事件缓冲区
    void setEventBuffer(std::vector<VizEvent>* buffer);

    // 引用参数别名：函数调用点包裹，使参数名事件路由到调用者变量
    void pushAlias(const char* paramName, const char* actualName);
    void popAlias(const char* paramName);

    // ===== 数组操作 =====
    void array(int* arr, int size, const char* name = "");
    void array(const std::vector<int>& arr, const char* name = "");

    // --- 带 varName 参数的版本（推荐）---
    void resize(int newSize, const char* varName);                        // 通知可视化层数组大小变化
    void resize(int newSize, int newValue, const char* varName);          // 带新增元素值的扩容
    void clearContainer(const char* varName);                              // 清空容器
    void vecResize(int newSize, int fillVal, const char* label, const char* varName);
    void setValue(int index, int value, const char* varName);
    void setValue(int* arr, int index, int value, const char* varName);
    void swap(int* arr, int i, int j, const char* varName);
    void compare(int i, int j, const char* varName);
    int compare(int* arr, int i, int j, const char* varName);             // 返回 arr[i] - arr[j]
    void mark(int index, const char* label, const char* varName);

    // --- 旧版 API（向后兼容，varName 默认为空）---
    void resize(int newSize);                        // 旧版，等价于 resize(newSize, "")
    void resize(int newSize, int newValue);          // 旧版，等价于 resize(newSize, newValue, "")
    void clearContainer();                           // 旧版，等价于 clearContainer("")
    void vecResize(int newSize, int fillVal, const char* label);   // 旧版
    void setValue(int index, int value);
    void setValue(int* arr, int index, int value);  // 旧版
    void swap(int* arr, int i, int j);              // 旧版
    void compare(int i, int j);                     // 旧版
    int compare(int* arr, int i, int j);            // 旧版
    void string(const char* data, int size, const char* name = "");  // 字符串注册

    // ===== 跨数组操作 =====
    void crossCompare(int* arr1, int i1, int* arr2, int i2,
                      const char* varName1, const char* varName2);

    // ===== 指针/标记 =====
    void mark(int index, const char* label);
    void unmark(int index);
    void unmarkAll();

    // ===== 递归可视化 =====
    void recurseEnter(int left, int right);
    void recursePending(int left, int right);
    void recurseLeave();

    // ===== 通用 =====
    void highlight(int index, const char* color = "active");
    void reset();
    void comment(const char* text);

    // 值参数生命周期：函数退出时销毁数组面板
    void destroyArray(const char* varName);
}

#endif // VIZ_API_H
