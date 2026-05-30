#ifndef ARRAYVISUALIZER_H
#define ARRAYVISUALIZER_H

#include <QWidget>
#include <vector>
#include <QRectF>

// 色块状态
enum class BlockState {
    Normal,     // 绿色（默认）
    Active,     // 红色（当前父节点）
    Comparing,  // 蓝色（参与比较的子节点）
    Swapping,   // 橙色（正在交换）
    Sorted      // 灰色（已排序）
};

// 箭头信息：指向某个数组下标
struct ArrowMarker {
    int targetIndex;   // 指向的数组下标
    QString label;     // 箭头旁的标签文字（如 "i", "j", "pivot"）
};

// 虚线框信息：框住某个区间
struct DashedBox {
    int startIndex;    // 区间起始下标（包含）
    int endIndex;      // 区间结束下标（包含）
    QString label;     // 框旁的标签文字（如 "待排序", "有序区"）
    QColor color;      // 框的颜色（默认红色）
};

class ArrayVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit ArrayVisualizer(QWidget *parent = nullptr);

    // --- 数据操作 ---
    void setData(const std::vector<int> &data);
    std::vector<int> getData() const;
    void resize(int newSize, const std::vector<int>& newData = {}); // 扩容/缩容动画

    // --- 色块状态 ---
    void setBlockState(int index, BlockState state);
    void resetAllBlockStates();

    // --- 箭头操作 ---
    void setArrows(const std::vector<ArrowMarker> &arrows);
    void clearArrows();

    // --- 虚线框操作 ---
    void setDashedBoxes(const std::vector<DashedBox> &boxes);
    void setDashedBoxes(std::initializer_list<DashedBox> boxes);
    void clearDashedBoxes();

    // --- 交换 ---
    void swapValues(int i, int j);

    // --- 新增：单值写入 ---
    void setValue(int index, int value);

    // --- 新增：临时空间操作 ---
    void clearTempSpace();
    void setTempValue(int index, int value);
    void setTempDashedBoxes(const std::vector<DashedBox> &boxes);
    void clearTempDashedBoxes();
    void setTempArrows(const std::vector<ArrowMarker> &arrows);
    void clearTempArrows();
    void highlightTempBlock(int index);
    void unhighlightTempBlock(int index);

    // --- 新增：clear() 状态 ---
    void showClearState();
    void hideClearState();

    // --- 新增：resize(0) 专用状态 ---
    void showResizeZeroState(const QString& label);
    void hideResizeZeroState();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QSize sizeHint() const override;

private:
    // 数据
    std::vector<int> m_values;
    std::vector<BlockState> m_blockStates;

    // 视觉标记
    std::vector<ArrowMarker> m_arrows;
    std::vector<DashedBox> m_dashedBoxes;

    // 布局参数（根据窗口大小动态计算）
    int m_blockSize;       // 色块边长（正方形）
    int m_blockSpacing;    // 色块间距
    int m_arrowHeight;     // 箭头区域高度
    int m_indexAreaHeight; // 下标区域高度
    int m_verticalOffset;  // 垂直偏移量，使画面整体向下移动

    // 颜色常量
    const QColor COLOR_NORMAL_BLOCK   = QColor("#4CAF50"); // 绿色（默认）
    const QColor COLOR_ACTIVE_BLOCK   = QColor("#F44336"); // 红色（当前父节点）
    const QColor COLOR_COMPARING_BLOCK = QColor("#2196F3"); // 蓝色（参与比较）
    const QColor COLOR_SWAPPING_BLOCK = QColor("#FF9800"); // 橙色（正在交换）
    const QColor COLOR_SORTED_BLOCK  = QColor("#9E9E9E"); // 灰色（已排序）
    const QColor COLOR_ARROW          = QColor("#1565C0"); // 深蓝色箭头
    const QColor COLOR_DASHEDBOX     = QColor("#F44336"); // 红色虚线框
    const QColor COLOR_INDEX_TEXT     = QColor("#333333"); // 下标文字

    void recalcLayout();
    QRectF blockRect(int index) const;     // 获取第index个色块的矩形
    QPointF blockCenterBottom(int index) const; // 色块底部中心点（箭头终点）
    void drawTempSpace(QPainter &painter); // 绘制临时空间

    // 新增成员变量
    std::vector<int> m_tempValues;
    bool m_hasTempSpace = false;
    std::vector<DashedBox> m_tempDashedBoxes;
    std::vector<ArrowMarker> m_tempArrows;
    std::vector<bool> m_tempHighlighted; // 临时空间高亮标记
    bool m_showClear = false;            // clear() 状态
    bool m_showResizeZero = false;       // resize(0) 状态
    QString m_resizeZeroLabel;           // resize(0) 显示的标签文字
};

#endif // ARRAYVISUALIZER_H
