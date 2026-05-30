#ifndef HEAPVISUALIZER_H
#define HEAPVISUALIZER_H

#include <QWidget>
#include <QPainter>
#include <vector>
#include <QColor>

// 堆节点状态
enum class HeapNodeState {
    Normal,     // 绿色 - 默认状态
    Active,     // 红色 - 当前操作的父节点
    Comparing,  // 蓝色 - 参与比较的子节点
    Swapping,   // 橙色 - 正在交换
    Sorted      // 灰色 - 已排序
};

// 堆节点结构
struct HeapNode {
    int value;
    int index;
    HeapNodeState state = HeapNodeState::Normal;
    bool connected = true;  // 是否有连线（排序后断开）
};

class HeapVisualizer : public QWidget {
    Q_OBJECT

public:
    explicit HeapVisualizer(QWidget *parent = nullptr);

    void setData(const std::vector<int> &data);
    void setNodeState(int index, HeapNodeState state);
    void resetAllNodeStates();
    void disconnectNode(int index);
    void swapNodes(int i, int j);

    // 显示/隐藏索引
    void setShowIndices(bool show) { m_showIndices = show; update(); }
    bool showIndices() const { return m_showIndices; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::vector<HeapNode> m_nodes;
    bool m_showIndices = true;

    QPointF getNodePosition(int index) const;
    void drawNode(QPainter &painter, const HeapNode &node, const QPointF &pos);
    void drawConnections(QPainter &painter);
};

#endif // HEAPVISUALIZER_H
