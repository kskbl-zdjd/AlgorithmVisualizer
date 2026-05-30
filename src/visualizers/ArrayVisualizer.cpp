#include "ArrayVisualizer.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

ArrayVisualizer::ArrayVisualizer(QWidget *parent)
    : QWidget(parent)
    , m_blockSize(50)
    , m_blockSpacing(8)
    , m_arrowHeight(5)       // 顶部箭头区域（不再使用，保留兼容）
    , m_indexAreaHeight(45)  // 底部区域高度：25(索引) + 20(箭头)
    , m_verticalOffset(30)   // 垂直偏移量，使画面整体向下移动
{
    setMinimumHeight(200);
}

// ========== 数据操作 ==========

void ArrayVisualizer::setData(const std::vector<int> &data)
{
    m_values = data;
    m_blockStates.assign(data.size(), BlockState::Normal);
    recalcLayout();
    update();
}

std::vector<int> ArrayVisualizer::getData() const
{
    return m_values;
}

void ArrayVisualizer::resize(int newSize, const std::vector<int>& newData)
{
    int oldSize = static_cast<int>(m_values.size());

    if (newSize > oldSize) {
        // 扩容：添加新元素
        if (!newData.empty() && static_cast<int>(newData.size()) >= newSize) {
            // newData 包含完整数组（如 push_back 后传入），直接复制
            m_values = newData;
        } else if (!newData.empty()) {
            // newData 仅包含新增元素（如 {newValue}），附加到尾部
            m_values.resize(newSize, 0);
            for (int i = oldSize; i < newSize && i - oldSize < static_cast<int>(newData.size()); ++i) {
                m_values[i] = newData[i - oldSize];
            }
        } else {
            // 无 newData，用默认值 0
            m_values.resize(newSize, 0);
        }
        m_blockStates.resize(newSize, BlockState::Active);  // 新增元素高亮
    } else if (newSize < oldSize) {
        // 缩容：删除尾部元素
        for (int i = newSize; i < oldSize; ++i) {
            m_blockStates[i] = BlockState::Active;  // 被删除的元素高亮
        }
        update();  // 先显示高亮再延迟删除（由外部控制步骤）
        // 实际删除在下一步 undo 之前的 execute 中完成
        // 为简化，这里直接 resize
        m_values.resize(newSize);
        m_blockStates.resize(newSize, BlockState::Normal);
    } else {
        return;  // 大小不变
    }

    recalcLayout();
    update();
}

// ========== 色块状态 ==========

void ArrayVisualizer::setBlockState(int index, BlockState state)
{
    if (index >= 0 && index < static_cast<int>(m_blockStates.size())) {
        m_blockStates[index] = state;
        update();
    }
}

void ArrayVisualizer::resetAllBlockStates()
{
    for (auto &s : m_blockStates) {
        s = BlockState::Normal;
    }
    update();
}

// ========== 箭头操作 ==========

void ArrayVisualizer::setArrows(const std::vector<ArrowMarker> &arrows)
{
    m_arrows = arrows;
    update();
}

void ArrayVisualizer::clearArrows()
{
    m_arrows.clear();
    update();
}

// ========== 虚线框操作 ==========

void ArrayVisualizer::setDashedBoxes(const std::vector<DashedBox> &boxes)
{
    m_dashedBoxes = boxes;
    update();
}

void ArrayVisualizer::setDashedBoxes(std::initializer_list<DashedBox> boxes)
{
    m_dashedBoxes = std::vector<DashedBox>(boxes);
    update();
}

void ArrayVisualizer::clearDashedBoxes()
{
    m_dashedBoxes.clear();
    update();
}

// ========== 交换 ==========

void ArrayVisualizer::swapValues(int i, int j)
{
    if (i >= 0 && i < static_cast<int>(m_values.size()) &&
        j >= 0 && j < static_cast<int>(m_values.size())) {
        std::swap(m_values[i], m_values[j]);
        std::swap(m_blockStates[i], m_blockStates[j]);
        update();
    }
}

// ========== 布局计算 ==========

void ArrayVisualizer::recalcLayout()
{
    if (m_values.empty()) return;

    int availableWidth = width() - 40; // 左右各留20边距
    int totalSpacing = (static_cast<int>(m_values.size()) - 1) * m_blockSpacing;
    m_blockSize = (availableWidth - totalSpacing) / static_cast<int>(m_values.size());
    m_blockSize = std::clamp(m_blockSize, 30, 60);
}

QRectF ArrayVisualizer::blockRect(int index) const
{
    int totalWidth = static_cast<int>(m_values.size()) * m_blockSize
                   + (static_cast<int>(m_values.size()) - 1) * m_blockSpacing;
    int startX = (width() - totalWidth) / 2;
    int x = startX + index * (m_blockSize + m_blockSpacing);
    int y = m_arrowHeight + m_verticalOffset; // 添加垂直偏移量
    return QRectF(x, y, m_blockSize, m_blockSize);
}

QPointF ArrayVisualizer::blockCenterBottom(int index) const
{
    QRectF rect = blockRect(index);
    return QPointF(rect.center().x(), rect.top());
}

// ========== 绘制 ==========

void ArrayVisualizer::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_values.empty()) {
        if (m_showResizeZero) {
            // resize(0) 状态：与 clear 类似，但显示 label 文字（如 "resize(0)"）
            // 使用更宽的色块以容纳完整文字
            QFontMetrics fm(painter.font());
            QString dispLabel = m_resizeZeroLabel.isEmpty() ? "resize(0)" : m_resizeZeroLabel;
            int textW = fm.horizontalAdvance(dispLabel) + 40;
            int blockW = std::max(140, textW);
            int blockH = 60;
            int x = (width() - blockW) / 2;
            int y = (height() - blockH) / 2;
            QRectF rzRect(x, y, blockW, blockH);

            // 背景色块（橙褐色，区别于 CLEAR 的蓝灰色）
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#795548"));
            painter.drawRoundedRect(rzRect, 8, 8);

            // 边框
            painter.setPen(QPen(QColor("#4E342E"), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rzRect, 8, 8);

            // 标签文字
            painter.setPen(QColor("#FFFFFF"));
            QFont rzFont = painter.font();
            rzFont.setPointSize(14);
            rzFont.setBold(true);
            painter.setFont(rzFont);
            painter.drawText(rzRect, Qt::AlignCenter, dispLabel);
        } else if (m_showClear) {
            // clear() 状态：显示单个色块，内含 "CLEAR" 文字
            int blockW = 120;
            int blockH = 60;
            int x = (width() - blockW) / 2;
            int y = (height() - blockH) / 2;
            QRectF clearRect(x, y, blockW, blockH);

            // 背景色块
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#607D8B"));  // 蓝灰色
            painter.drawRoundedRect(clearRect, 8, 8);

            // 边框
            painter.setPen(QPen(QColor("#455A64"), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(clearRect, 8, 8);

            // "CLEAR" 文字
            painter.setPen(QColor("#FFFFFF"));
            QFont clearFont = painter.font();
            clearFont.setPointSize(18);
            clearFont.setBold(true);
            painter.setFont(clearFont);
            painter.drawText(clearRect, Qt::AlignCenter, "CLEAR");
        } else {
            painter.drawText(rect(), Qt::AlignCenter, "暂无数据");
        }
        return;
    }

    // --- 1. 绘制虚线框（在色块下层） ---
    for (const auto &box : m_dashedBoxes) {
        QRectF firstRect = blockRect(box.startIndex);
        QRectF lastRect  = blockRect(box.endIndex);
        QRectF boxRect(
            firstRect.left() - 6,
            firstRect.top() - 6,
            lastRect.right() - firstRect.left() + 12,
            lastRect.bottom() - firstRect.top() + 12
        );

        // 使用 box.color（如果未设置则回退到红色）
        QColor boxColor = box.color.isValid() ? box.color : COLOR_DASHEDBOX;
        QPen dashedPen(boxColor, 2, Qt::DashLine);
        painter.setPen(dashedPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(boxRect);

        // 标签
        if (!box.label.isEmpty()) {
            painter.setPen(boxColor);
            QFont font = painter.font();
            font.setPointSize(9);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(
                static_cast<int>(boxRect.left()),
                static_cast<int>(boxRect.top()) - 4,
                box.label
            );
        }
    }

    // --- 2. 绘制色块 ---
    for (int i = 0; i < static_cast<int>(m_values.size()); ++i) {
        QRectF rect = blockRect(i);
            QColor color;
            switch (m_blockStates[i]) {
                case BlockState::Normal:    color = COLOR_NORMAL_BLOCK; break;
                case BlockState::Active:    color = COLOR_ACTIVE_BLOCK; break;
                case BlockState::Comparing: color = COLOR_COMPARING_BLOCK; break;
                case BlockState::Swapping:  color = COLOR_SWAPPING_BLOCK; break;
                case BlockState::Sorted:    color = COLOR_SORTED_BLOCK; break;
            }

        // 色块填充
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(rect, 4, 4);

        // 色块边框
        painter.setPen(QPen(color.darker(120), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, 4, 4);

        // 色块内数值
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(11);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(rect, Qt::AlignCenter, QString::number(m_values[i]));
    }

    // --- 3. 绘制数组下标 ---
    painter.setPen(COLOR_INDEX_TEXT);
    QFont indexFont = painter.font();
    indexFont.setPointSize(9);
    indexFont.setBold(false);
    painter.setFont(indexFont);

    for (int i = 0; i < static_cast<int>(m_values.size()); ++i) {
        QRectF rect = blockRect(i);
        painter.drawText(
            QRectF(rect.left(), rect.bottom() + 2, rect.width(), m_indexAreaHeight),
            Qt::AlignCenter,
            QString("[%1]").arg(i)
        );
    }

    // --- 4. 绘制蓝色箭头（在色块下方，向上指着节点） ---
    for (const auto &arrow : m_arrows) {
        QRectF block = blockRect(arrow.targetIndex);
        
        // 箭头在色块下方
        qreal centerX = block.center().x();
        qreal bottomY = block.bottom();  // 色块底部
        qreal arrowLen = 25;  // 箭头线长度
        
        // 箭头线：从下方指向色块底部
        QPointF lineBase(centerX, bottomY + arrowLen);
        QPointF lineTip(centerX, bottomY + 2);
        
        QPen arrowPen(COLOR_ARROW, 2);
        painter.setPen(arrowPen);
        painter.drawLine(lineBase, lineTip);
        
        // 箭头头部（向上指的三角形）
        qreal arrowSize = 6;
        QPainterPath arrowHead;
        arrowHead.moveTo(centerX, bottomY + 2);              // 三角形尖端（指向色块）
        arrowHead.lineTo(centerX - arrowSize, bottomY + arrowSize * 2 + 2);  // 左下角
        arrowHead.lineTo(centerX + arrowSize, bottomY + arrowSize * 2 + 2);  // 右下角
        arrowHead.closeSubpath();
        
        painter.setBrush(COLOR_ARROW);
        painter.setPen(Qt::NoPen);
        painter.drawPath(arrowHead);
        
        // 箭头标签（在箭头线下方）
        if (!arrow.label.isEmpty()) {
            painter.setPen(COLOR_ARROW);
            QFont labelFont = painter.font();
            labelFont.setPointSize(8);
            labelFont.setBold(true);
            painter.setFont(labelFont);
            painter.drawText(
                QRectF(centerX - 25, bottomY + arrowLen + 2, 50, 15),
                Qt::AlignCenter,
                arrow.label
            );
        }
    }

    // --- 5. 绘制临时空间（归并排序用） ---
    if (m_hasTempSpace) {
        drawTempSpace(painter);
    }
}

void ArrayVisualizer::resizeEvent(QResizeEvent *event)
{
    recalcLayout();
    QWidget::resizeEvent(event);
}

QSize ArrayVisualizer::sizeHint() const
{
    int totalWidth = static_cast<int>(m_values.size()) * (m_blockSize + m_blockSpacing);
    int totalHeight = m_arrowHeight + m_blockSize + m_indexAreaHeight;
    return QSize(totalWidth + 40, totalHeight);
}

// ========== 新增：单值写入 ==========

void ArrayVisualizer::setValue(int index, int value)
{
    if (index >= 0 && index < static_cast<int>(m_values.size())) {
        m_values[index] = value;
        update();
    }
}

// ========== 新增：clear() 状态 ==========

void ArrayVisualizer::showClearState()
{
    m_values.clear();
    m_blockStates.clear();
    m_arrows.clear();
    m_dashedBoxes.clear();
    m_showClear = true;
    update();
}

void ArrayVisualizer::hideClearState()
{
    m_showClear = false;
    update();
}

void ArrayVisualizer::showResizeZeroState(const QString& label)
{
    m_values.clear();
    m_blockStates.clear();
    m_arrows.clear();
    m_dashedBoxes.clear();
    m_showClear = false;
    m_showResizeZero = true;
    m_resizeZeroLabel = label;
    update();
}

void ArrayVisualizer::hideResizeZeroState()
{
    m_showResizeZero = false;
    m_resizeZeroLabel.clear();
    update();
}

// ========== 新增：临时空间操作 ==========

void ArrayVisualizer::clearTempSpace()
{
    m_tempValues.clear();
    m_hasTempSpace = false;
    m_tempDashedBoxes.clear();
    m_tempArrows.clear();
    m_tempHighlighted.clear();
    update();
}

void ArrayVisualizer::setTempValue(int index, int value)
{
    if (index >= static_cast<int>(m_tempValues.size())) {
        m_tempValues.resize(index + 1, -1);
        m_tempHighlighted.resize(index + 1, false);
    }
    m_tempValues[index] = value;
    m_hasTempSpace = true;
    update();
}

void ArrayVisualizer::setTempDashedBoxes(const std::vector<DashedBox> &boxes)
{
    m_tempDashedBoxes = boxes;
    update();
}

void ArrayVisualizer::clearTempDashedBoxes()
{
    m_tempDashedBoxes.clear();
    update();
}

void ArrayVisualizer::setTempArrows(const std::vector<ArrowMarker> &arrows)
{
    m_tempArrows = arrows;
    update();
}

void ArrayVisualizer::clearTempArrows()
{
    m_tempArrows.clear();
    update();
}

void ArrayVisualizer::highlightTempBlock(int index)
{
    if (index >= 0 && index < static_cast<int>(m_tempHighlighted.size())) {
        m_tempHighlighted[index] = true;
        update();
    }
}

void ArrayVisualizer::unhighlightTempBlock(int index)
{
    if (index >= 0 && index < static_cast<int>(m_tempHighlighted.size())) {
        m_tempHighlighted[index] = false;
        update();
    }
}

// ========== 新增：绘制临时空间 ==========

void ArrayVisualizer::drawTempSpace(QPainter &painter)
{
    if (m_tempValues.empty()) return;

    int totalWidth = static_cast<int>(m_tempValues.size()) * m_blockSize
                   + (static_cast<int>(m_tempValues.size()) - 1) * m_blockSpacing;
    int startX = (width() - totalWidth) / 2;

    // 临时空间绘制在主数组下方，留出索引区域后再向下 10px
    int tempY = m_arrowHeight + m_blockSize + m_indexAreaHeight + 10;

    // 绘制"临时空间"标题
    painter.setPen(QColor("#616161"));
    QFont titleFont = painter.font();
    titleFont.setPointSize(8);
    titleFont.setBold(false);
    painter.setFont(titleFont);
    painter.drawText(startX, tempY - 2, "temp[]");

    for (int i = 0; i < static_cast<int>(m_tempValues.size()); i++) {
        int x = startX + i * (m_blockSize + m_blockSpacing);
        QRectF rect(x, tempY, m_blockSize, m_blockSize);

        // 高亮色或普通黄色背景
        bool highlighted = (i < static_cast<int>(m_tempHighlighted.size())) && m_tempHighlighted[i];
        QColor bgColor = highlighted ? QColor("#FF9800") : QColor("#FFEB3B");

        painter.setBrush(bgColor);
        painter.setPen(QPen(QColor("#9E9E9E"), 1, Qt::DashLine));
        painter.drawRect(rect);

        // 绘制数值
        if (m_tempValues[i] >= 0) {
            painter.setPen(Qt::black);
            QFont numFont = painter.font();
            numFont.setPointSize(11);
            numFont.setBold(true);
            painter.setFont(numFont);
            painter.drawText(rect, Qt::AlignCenter, QString::number(m_tempValues[i]));
        }

        // 下标
        painter.setPen(QColor("#333333"));
        QFont idxFont = painter.font();
        idxFont.setPointSize(9);
        idxFont.setBold(false);
        painter.setFont(idxFont);
        painter.drawText(
            QRectF(x, tempY + m_blockSize + 2, m_blockSize, 16),
            Qt::AlignCenter,
            QString("[%1]").arg(i)
        );
    }

    // 绘制临时空间的虚线框
    for (const auto &box : m_tempDashedBoxes) {
        if (box.startIndex < 0 || box.endIndex >= static_cast<int>(m_tempValues.size())) continue;
        int x0 = startX + box.startIndex * (m_blockSize + m_blockSpacing);
        int x1 = startX + box.endIndex   * (m_blockSize + m_blockSpacing) + m_blockSize;
        QRectF boxRect(x0 - 4, tempY - 4, x1 - x0 + 8, m_blockSize + 8);
        QColor boxColor = box.color.isValid() ? box.color : QColor("#F44336");
        painter.setPen(QPen(boxColor, 2, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(boxRect);
        if (!box.label.isEmpty()) {
            painter.setPen(boxColor);
            QFont lf = painter.font();
            lf.setPointSize(8);
            lf.setBold(true);
            painter.setFont(lf);
            painter.drawText(
                static_cast<int>(boxRect.left()),
                static_cast<int>(boxRect.top()) - 3,
                box.label
            );
        }
    }

    // 绘制临时空间的箭头
    for (const auto &arrow : m_tempArrows) {
        if (arrow.targetIndex < 0 || arrow.targetIndex >= static_cast<int>(m_tempValues.size())) continue;
        int x = startX + arrow.targetIndex * (m_blockSize + m_blockSpacing);
        qreal cx = x + m_blockSize / 2.0;
        qreal topY = tempY;
        qreal arrowLen = 18;

        QPen arrowPen(QColor("#1565C0"), 2);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(cx, topY - arrowLen), QPointF(cx, topY - 2));

        qreal arrowSize = 5;
        QPainterPath arrowHead;
        arrowHead.moveTo(cx, topY - 2);
        arrowHead.lineTo(cx - arrowSize, topY - 2 - arrowSize * 1.5);
        arrowHead.lineTo(cx + arrowSize, topY - 2 - arrowSize * 1.5);
        arrowHead.closeSubpath();
        painter.setBrush(QColor("#1565C0"));
        painter.setPen(Qt::NoPen);
        painter.drawPath(arrowHead);

        if (!arrow.label.isEmpty()) {
            painter.setPen(QColor("#1565C0"));
            QFont lf = painter.font();
            lf.setPointSize(8);
            lf.setBold(true);
            painter.setFont(lf);
            painter.drawText(
                QRectF(cx - 20, topY - arrowLen - 14, 40, 14),
                Qt::AlignCenter,
                arrow.label
            );
        }
    }
}
