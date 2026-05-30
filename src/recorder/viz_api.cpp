#include "viz_api.h"
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace viz {
    // 全局变量定义
    std::vector<VizEvent>* g_eventBuffer = nullptr;
    int g_stepCount = 0;
    const int MAX_STEPS = 10000;

    // 引用参数别名表 + 旧值保存（支持嵌套调用恢复）
    static std::map<std::string, std::string> g_aliases;
    static std::map<std::string, std::string> g_aliasSaved;

    void pushAlias(const char* paramName, const char* actualName) {
        if (!paramName || !actualName) return;
        std::string key(paramName);
        // 保存旧值（如果有），用于 popAlias 恢复
        auto it = g_aliases.find(key);
        if (it != g_aliases.end()) {
            g_aliasSaved[key] = it->second;
        } else {
            g_aliasSaved[key] = "";  // 标记 "之前不存在"
        }
        g_aliases[key] = std::string(actualName);
    }

    void popAlias(const char* paramName) {
        if (!paramName) return;
        std::string key(paramName);
        auto it = g_aliasSaved.find(key);
        if (it != g_aliasSaved.end()) {
            if (it->second.empty()) {
                g_aliases.erase(key);
            } else {
                g_aliases[key] = it->second;
            }
            g_aliasSaved.erase(it);
        }
    }

    // 解析别名：如果有映射则返回 actualName，否则返回原始 name
    static const char* resolveName(const char* name) {
        if (!name || !name[0]) return name;
        auto it = g_aliases.find(std::string(name));
        if (it != g_aliases.end()) {
            // 返回别名映射值（C++ string 的生命周期由 g_aliases 保证）
            return it->second.c_str();
        }
        return name;
    }

    bool checkStepLimit() {
        if (++g_stepCount > MAX_STEPS) {
            return false;
        }
        return true;
    }

    void setEventBuffer(std::vector<VizEvent>* buffer) {
        g_eventBuffer = buffer;
        g_stepCount = 0;  // 重置计数器
    }

    void array(int* arr, int size, const char* name) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }

        VizEvent e;
        e.type = VizEventType::ArrayCreate;
        e.setArrayData(arr, size);
        e.setArrayName(resolveName(name));
        g_eventBuffer->push_back(e);
    }

    void array(const std::vector<int>& arr, const char* name) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }

        VizEvent e;
        e.type = VizEventType::ArrayCreate;
        e.setArrayData(arr.data(), static_cast<int>(arr.size()));
        e.setArrayName(resolveName(name));
        g_eventBuffer->push_back(e);
    }

    // —— helper: 从事件缓冲区查找指定数组的当前大小 ——
    static int lookupCurSize(const char* varName) {
        if (!g_eventBuffer) return 0;
        const char* resolved = resolveName(varName);
        for (auto it = g_eventBuffer->rbegin(); it != g_eventBuffer->rend(); ++it) {
            // 只匹配同名数组的事件
            bool match = (strcmp(it->arrayName, resolved) == 0);
            if (it->type == VizEventType::ArrayCreate && match) {
                return it->arrayDataCount;
            }
            if (it->type == VizEventType::Clear && match) {
                return 0;
            }
            if ((it->type == VizEventType::Resize || it->type == VizEventType::VecResize) && match) {
                return it->newSize;
            }
        }
        return 0;
    }

    // —— 旧版（无 varName，兼容）——
    void resize(int newSize) {
        resize(newSize, "");
    }
    void resize(int newSize, int newValue) {
        resize(newSize, newValue, "");
    }
    void clearContainer() {
        clearContainer("");
    }
    void vecResize(int newSize, int fillVal, const char* label) {
        vecResize(newSize, fillVal, label, "");
    }

    // —— 新版（带 varName）——
    void resize(int newSize, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        int oldSize = lookupCurSize(varName);
        VizEvent e;
        e.type = VizEventType::Resize;
        e.oldSize = oldSize;
        e.newSize = newSize;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    void resize(int newSize, int newValue, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        int oldSize = lookupCurSize(varName);
        VizEvent e;
        e.type = VizEventType::Resize;
        e.oldSize = oldSize;
        e.newSize = newSize;
        e.value = newValue;
        e.setArrayName(resolveName(varName));
        fprintf(stderr, "[viz::resize] var=%s oldSize=%d newSize=%d value=%d\n", varName, oldSize, newSize, newValue);
        g_eventBuffer->push_back(e);
    }

    void clearContainer(const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        int oldSize = lookupCurSize(varName);
        VizEvent e;
        e.type = VizEventType::Clear;
        e.oldSize = oldSize;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    void vecResize(int newSize, int fillVal, const char* label, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        int oldSize = lookupCurSize(varName);
        VizEvent e;
        e.type = VizEventType::VecResize;
        e.oldSize = oldSize;
        e.newSize = newSize;
        e.value   = fillVal;
        e.setComment(label);
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    // —— 旧版 setValue ——
    void setValue(int index, int value) {
        setValue(index, value, "");
    }
    void setValue(int* arr, int index, int value) {
        setValue(arr, index, value, "");
    }

    // —— 新版 setValue ——
    void setValue(int index, int value, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        VizEvent e;
        e.type = VizEventType::SetValue;
        e.index1 = index;
        e.value = value;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    void setValue(int* arr, int index, int value, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        arr[index] = value;
        VizEvent e;
        e.type = VizEventType::SetValue;
        e.index1 = index;
        e.value = value;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    // —— 旧版 swap ——
    void swap(int* arr, int i, int j) {
        swap(arr, i, j, "");
    }

    // —— 新版 swap ——
    void swap(int* arr, int i, int j, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        std::swap(arr[i], arr[j]);
        VizEvent e;
        e.type = VizEventType::Swap;
        e.index1 = i;
        e.index2 = j;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    // —— 旧版 compare ——
    void compare(int i, int j) {
        compare(i, j, "");
    }
    int compare(int* arr, int i, int j) {
        return compare(arr, i, j, "");
    }

    // —— 新版 compare ——
    void compare(int i, int j, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        VizEvent e;
        e.type = VizEventType::Compare;
        e.index1 = i;
        e.index2 = j;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    int compare(int* arr, int i, int j, const char* varName) {
        if (!g_eventBuffer) return arr[i] - arr[j];
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        VizEvent e;
        e.type = VizEventType::Compare;
        e.index1 = i;
        e.index2 = j;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
        return arr[i] - arr[j];
    }

    // —— 跨数组比较 ——
    void crossCompare(int* arr1, int i1, int* arr2, int i2,
                      const char* varName1, const char* varName2) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }
        // 记录两个数组的 compare 事件
        {
            VizEvent e;
            e.type = VizEventType::Compare;
            e.index1 = i1;
            e.index2 = i1;
            e.setArrayName(resolveName(varName1));
            e.setComment(resolveName(varName2));  // 在 comment 中存储对方数组名
            g_eventBuffer->push_back(e);
        }
        {
            VizEvent e;
            e.type = VizEventType::Compare;
            e.index1 = i2;
            e.index2 = i2;
            e.setArrayName(resolveName(varName2));
            e.setComment(resolveName(varName1));  // 在 comment 中存储对方数组名
            g_eventBuffer->push_back(e);
        }
    }

    void string(const char* data, int size, const char* name) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }

        VizEvent e;
        e.type = VizEventType::ArrayCreate;
        // 逐元素复制 char → int
        e.arrayDataCount = (size > VizEvent::MAX_ARRAY_SIZE) ? VizEvent::MAX_ARRAY_SIZE : size;
        for (int i = 0; i < e.arrayDataCount; i++) {
            e.arrayData[i] = static_cast<int>(static_cast<unsigned char>(data[i]));
        }
        e.setArrayName(name);
        g_eventBuffer->push_back(e);
    }

    void mark(int index, const char* label) {
        mark(index, label, "");
    }

    void mark(int index, const char* label, const char* varName) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }

        VizEvent e;
        e.type = VizEventType::Mark;
        e.index1 = index;
        e.setLabel(label);
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }

    void unmark(int index) {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::Unmark;
        e.index1 = index;
        g_eventBuffer->push_back(e);
    }

    void unmarkAll() {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::UnmarkAll;
        g_eventBuffer->push_back(e);
    }

    void recurseEnter(int left, int right) {
        if (!g_eventBuffer) return;
        if (!checkStepLimit()) {
            throw std::runtime_error("Execution steps exceeded limit (10000), possible infinite loop");
        }

        VizEvent e;
        e.type = VizEventType::RecurseEnter;
        e.index1 = left;
        e.index2 = right;
        g_eventBuffer->push_back(e);
    }

    void recursePending(int left, int right) {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::RecursePending;
        e.index1 = left;
        e.index2 = right;
        g_eventBuffer->push_back(e);
    }

    void recurseLeave() {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::RecurseLeave;
        g_eventBuffer->push_back(e);
    }

    void highlight(int index, const char* color) {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::Highlight;
        e.index1 = index;
        e.setLabel(color);
        g_eventBuffer->push_back(e);
    }

    void reset() {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::Reset;
        g_eventBuffer->push_back(e);
    }

    void comment(const char* text) {
        if (!g_eventBuffer) return;

        VizEvent e;
        e.type = VizEventType::Comment;
        e.setComment(text);
        g_eventBuffer->push_back(e);
    }

    void destroyArray(const char* varName) {
        if (!g_eventBuffer || !checkStepLimit()) return;

        VizEvent e;
        e.type = VizEventType::ArrayDestroy;
        e.setArrayName(resolveName(varName));
        g_eventBuffer->push_back(e);
    }
}
