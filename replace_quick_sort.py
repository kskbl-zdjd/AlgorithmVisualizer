# -*- coding: utf-8 -*-
"""替换快速排序部分为 Lomuto 分区实现"""

import sys
import codecs

# 设置 UTF-8 输出
sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer if hasattr(sys.stdout, 'buffer') else sys.stdout)

INPUT_FILE = r'E:\QTproject\algo-visualizer\src\algorithms\SortingAlgorithms.cpp'
NEW_CODE_FILE = r'E:\QTproject\algo-visualizer\src\algorithms\quick_sort_new.cpp'

# 读取原始文件
with codecs.open(INPUT_FILE, 'r', 'utf-8') as f:
    lines = f.readlines()

# 读取新代码
with codecs.open(NEW_CODE_FILE, 'r', 'utf-8') as f:
    new_code = f.read()

# 找到快速排序部分的起始位置（quickSort 函数定义）
start_marker = 'void SortingAlgorithms::quickSort('
start_idx = None
for i, line in enumerate(lines):
    if start_marker in line:
        start_idx = i
        break

if start_idx is None:
    print("错误：未找到快速排序函数的起始位置")
    sys.exit(1)

print(f"找到快速排序函数：第 {start_idx + 1} 行")

# 找到 partition 函数的结束位置
# 查找后面的非快速排序代码（例如下一个函数定义或文件结尾）
end_idx = None
for i in range(start_idx + 1, len(lines)):
    # 查找下一个函数定义或文件末尾
    if i > start_idx and lines[i].startswith('void ') and 'AlgorithmVisualizer' not in lines[i]:
        # 检查是否是别的函数（不是 quickSort/quickSortRecursive/partition）
        if 'quickSort' not in lines[i] and 'partition' not in lines[i]:
            end_idx = i
            break

if end_idx is None:
    end_idx = len(lines)

print(f"找到快速排序部分结束：第 {end_idx + 1} 行")

# 替换代码
new_lines = lines[:start_idx] + [new_code + '\n\n'] + lines[end_idx:]

# 写回文件
with codecs.open(INPUT_FILE, 'w', 'utf-8') as f:
    f.writelines(new_lines)

print(f"成功替换：第 {start_idx + 1} 行到第 {end_idx} 行")
print("新代码已写入文件。")
