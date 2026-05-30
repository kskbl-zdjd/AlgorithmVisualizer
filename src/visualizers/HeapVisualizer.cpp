#include "HeapVisualizer.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QFontMetrics>
#include <cmath>

HeapVisualizer::HeapVisualizer(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
}

void HeapVisualizer::setData(const std::vector<int> &data)
{
    m_nodes.clear();
    m_nodes.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        HeapNode node;
        node.value = data[i];
        node.index = static_cast<int>(i);
        node.state = HeapNodeState::Normal;
        node.connected = true;
        m_nodes.push_back(node);
    }
    update();
}

void HeapVisualizer::setNodeState(int index, HeapNodeState state)
{
    if (index >= 0 && index < static_cast<int>(m_nodes.size())) {
        m_nodes[index].state = state;
        update();
    }
}

void HeapVisualizer::resetAllNodeStates()
{
    for (auto &node : m_nodes) {
        if (node.state != HeapNodeState::Sorted) {
            node.state = HeapNodeState::Normal;
        }
    }
    update();
}

void HeapVisualizer::disconnectNode(int index)
{
    if (index >= 0 && index < static_cast<int>(m_nodes.size())) {
        m_nodes[index].connected = false;
        update();
    }
}

void HeapVisualizer::swapNodes(int i, int j)
{
    if (i >= 0 && i < static_cast<int>(m_nodes.size()) &&
        j >= 0 && j < static_cast<int>(m_nodes.size())) {
        std::swap(m_nodes[i].value, m_nodes[j].value);
        update();
    }
}

QSize HeapVisualizer::sizeHint() const
{
    return QSize(600, 300);
}

QPointF HeapVisualizer::getNodePosition(int index) const
{
    if (m_nodes.empty()) return QPointF(width() / 2.0, 40);

    int n = static_cast<int>(m_nodes.size());

    // 计算节点所在层深
    int depth = 0;
    int temp = index + 1;
    while (temp > 1) {
        temp /= 2;
        depth++;
    }

    // 该层节点数 = 2^depth
    int nodesInLevel = 1 << depth;

    // 该层第一个节点的数组索引
    int firstInLevel = (1 << depth) - 1;

    // 节点在层中的位置 (0-based)
    int posInLevel = index - firstInLevel;

    // 计算坐标
    double margin = 20.0;
    double availWidth = width() - 2 * margin;
    double levelHeight = (height() - 40) / (static_cast<double>(log2(n) + 1) + 1);

    double y = 40 + levelHeight * depth;

    // 层内均匀分配水平空间
    double xSpacing = availWidth / (nodesInLevel + 1);
    double x = margin + xSpacing * (posInLevel + 1);

    return QPointF(x, y);
}

void HeapVisualizer::drawConnections(QPainter &painter)
{
    painter.setPen(QPen(QColor("#000000"), 2));
    painter.setBrush(Qt::NoBrush);

    int n = static_cast<int>(m_nodes.size());
    for (int i = 0; i < n; ++i) {
        if (!m_nodes[i].connected) continue;

        QPointF parentPos = getNodePosition(i);

        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < n && m_nodes[left].connected) {
            QPointF childPos = getNodePosition(left);
            painter.drawLine(parentPos, childPos);
        }
        if (right < n && m_nodes[right].connected) {
            QPointF childPos = getNodePosition(right);
            painter.drawLine(parentPos, childPos);
        }
    }
}

void HeapVisualizer::drawNode(QPainter &painter, const HeapNode &node, const QPointF &pos)
{
    // 根据状态选择颜色
    QColor color;
    switch (node.state) {
        case HeapNodeState::Normal:    color = QColor("#4CAF50"); break;
        case HeapNodeState::Active:    color = QColor("#F44336"); break;
        case HeapNodeState::Comparing: color = QColor("#2196F3"); break;
        case HeapNodeState::Swapping:  color = QColor("#FF9800"); break;
        case HeapNodeState::Sorted:    color = QColor("#9E9E9E"); break;
    }

    double radius = 20;

    // 绘制圆形节点
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(pos, radius, radius);

    // 绘制数值（白色文字）
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);

    QString text = QString::number(node.value);
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text);
    int textHeight = fm.height();
    painter.drawText(QRectF(pos.x() - textWidth / 2.0,
                          pos.y() - textHeight / 2.0 + 2,
                          textWidth, textHeight),
                    Qt::AlignCenter, text);

    // 绘制索引（如果启用）
    if (m_showIndices) {
        painter.setPen(Qt::black);
        font.setBold(false);
        font.setPointSize(8);
        painter.setFont(font);
        QString idxText = QString::number(node.index);
        textWidth = fm.horizontalAdvance(idxText);
        painter.drawText(QRectF(pos.x() - textWidth / 2.0,
                                pos.y() + radius + 5,
                                textWidth, 20),
                        Qt::AlignCenter, idxText);
    }
}

void HeapVisualizer::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 白色背景
    painter.fillRect(rect(), Qt::white);

    if (m_nodes.empty()) return;

    drawConnections(painter);

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        QPointF pos = getNodePosition(static_cast<int>(i));
        drawNode(painter, m_nodes[i], pos);
    }
}

void HeapVisualizer::resizeEvent(QResizeEvent * /*event*/)
{
    update();
}
