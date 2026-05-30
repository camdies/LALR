#include "dfagraphwidget.h"
#include <QPainterPath>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

static const double NODE_PADDING_X = 18.0;
static const double NODE_PADDING_Y = 12.0;
static const double MIN_NODE_WIDTH = 100.0;
static const double MIN_NODE_HEIGHT = 50.0;
static const double ARROW_SIZE = 10.0;
static const double SELF_LOOP_RADIUS = 30.0;

DFAGraphWidget::DFAGraphWidget(QWidget* parent)
    : QWidget(parent)
    , m_offset(0, 0)
    , m_scale(1.0)
    , m_dragging(false)
    , m_dragNodeIdx(-1)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void DFAGraphWidget::setDFA(const QVector<DFANode>& nodes, const QVector<DFAEdge>& edges)
{
    m_nodes = nodes;
    m_edges = edges;
    m_offset = QPointF(50, 50);
    m_scale = 1.0;
    autoLayout();
    update();
}

void DFAGraphWidget::clear()
{
    m_nodes.clear();
    m_edges.clear();
    update();
}

// ====== 自动布局：分层布局（BFS层次） ======
void DFAGraphWidget::autoLayout()
{
    if (m_nodes.isEmpty()) return;

    int n = m_nodes.size();

    // BFS 分层
    QVector<int> level(n, -1);
    QVector<QVector<int>> layers;
    QHash<int, int> idToIdx;
    for (int i = 0; i < n; i++) {
        idToIdx[m_nodes[i].id] = i;
    }

    // 找到起始状态
    int startIdx = 0;
    for (int i = 0; i < n; i++) {
        if (m_nodes[i].isStart) { startIdx = i; break; }
    }

    // 构建邻接表
    QHash<int, QVector<int>> adj;
    for (const auto& e : m_edges) {
        if (e.from != e.to) { // 排除自环
            adj[e.from].append(e.to);
        }
    }

    // BFS
    QVector<int> queue;
    queue.append(m_nodes[startIdx].id);
    level[startIdx] = 0;

    int head = 0;
    while (head < queue.size()) {
        int cur = queue[head++];
        int curIdx = idToIdx[cur];
        for (int next : adj[cur]) {
            int nextIdx = idToIdx.value(next, -1);
            if (nextIdx >= 0 && level[nextIdx] < 0) {
                level[nextIdx] = level[curIdx] + 1;
                queue.append(next);
            }
        }
    }

    // 未访问到的节点放到最后一层
    int maxLevel = 0;
    for (int i = 0; i < n; i++) {
        if (level[i] < 0) level[i] = maxLevel + 1;
        if (level[i] > maxLevel) maxLevel = level[i];
    }

    // 按层分组
    layers.resize(maxLevel + 1);
    for (int i = 0; i < n; i++) {
        layers[level[i]].append(i);
    }

    // 计算每个节点的尺寸
    QVector<QSizeF> sizes(n);
    for (int i = 0; i < n; i++) {
        sizes[i] = nodeSize(m_nodes[i]);
    }

    // 布局
    double xOffset = 40;
    double layerSpacingX = 80;  // 层间水平间距

    double currentX = xOffset;
    for (int l = 0; l <= maxLevel; l++) {
        double maxWidth = 0;
        for (int idx : layers[l]) {
            if (sizes[idx].width() > maxWidth) maxWidth = sizes[idx].width();
        }

        double nodeSpacingY = 40;
        double totalHeight = 0;
        for (int i = 0; i < layers[l].size(); i++) {
            totalHeight += sizes[layers[l][i]].height();
            if (i > 0) totalHeight += nodeSpacingY;
        }

        double startY = 40;
        double currentY = startY;
        for (int i = 0; i < layers[l].size(); i++) {
            int idx = layers[l][i];
            m_nodes[idx].pos = QPointF(currentX + maxWidth / 2.0, currentY + sizes[idx].height() / 2.0);
            currentY += sizes[idx].height() + nodeSpacingY;
        }

        currentX += maxWidth + layerSpacingX;
    }
}

QSizeF DFAGraphWidget::nodeSize(const DFANode& node) const
{
    QFont font("Microsoft YaHei", 9);
    QFontMetrics fm(font);

    double maxTextWidth = fm.horizontalAdvance("I" + QString::number(node.id) + ":");
    for (const auto& item : node.items) {
        double w = fm.horizontalAdvance(item);
        if (w > maxTextWidth) maxTextWidth = w;
    }

    double width = maxTextWidth + NODE_PADDING_X * 2;
    double height = fm.height() * (node.items.size() + 1) + NODE_PADDING_Y * 2;

    if (width < MIN_NODE_WIDTH) width = MIN_NODE_WIDTH;
    if (height < MIN_NODE_HEIGHT) height = MIN_NODE_HEIGHT;

    return QSizeF(width, height);
}

QRectF DFAGraphWidget::nodeRect(const DFANode& node) const
{
    QSizeF s = nodeSize(node);
    return QRectF(node.pos.x() - s.width() / 2, node.pos.y() - s.height() / 2, s.width(), s.height());
}

QPointF DFAGraphWidget::toScreen(const QPointF& worldPos) const
{
    return worldPos * m_scale + m_offset;
}

QPointF DFAGraphWidget::toWorld(const QPointF& screenPos) const
{
    return (screenPos - m_offset) / m_scale;
}

// ====== 绘制 ======
void DFAGraphWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);

    if (m_nodes.isEmpty()) {
        painter.setPen(Qt::gray);
        painter.setFont(QFont("Microsoft YaHei", 12));
        painter.drawText(rect(), Qt::AlignCenter, "请先进行文法分析以生成DFA图");
        return;
    }

    painter.save();
    painter.translate(m_offset);
    painter.scale(m_scale, m_scale);

    QFont nodeFont("Microsoft YaHei", 9);
    QFont labelFont("Microsoft YaHei", 9, QFont::Bold);
    QFontMetrics fm(nodeFont);

    // 先画边
    for (const auto& edge : m_edges) {
        int fromIdx = -1, toIdx = -1;
        for (int i = 0; i < m_nodes.size(); i++) {
            if (m_nodes[i].id == edge.from) fromIdx = i;
            if (m_nodes[i].id == edge.to) toIdx = i;
        }
        if (fromIdx < 0 || toIdx < 0) continue;

        if (edge.from == edge.to) {
            // 自环
            QRectF r = nodeRect(m_nodes[fromIdx]);
            drawSelfLoop(painter, r, edge.symbol);
        }
        else {
            QRectF fromRect = nodeRect(m_nodes[fromIdx]);
            QRectF toRect = nodeRect(m_nodes[toIdx]);
            QPointF fromCenter = m_nodes[fromIdx].pos;
            QPointF toCenter = m_nodes[toIdx].pos;
            QPointF start = edgeIntersection(fromRect, toCenter);
            QPointF end = edgeIntersection(toRect, fromCenter);
            drawArrow(painter, start, end, edge.symbol);
        }
    }

    // 再画节点
    for (int i = 0; i < m_nodes.size(); i++) {
        const DFANode& node = m_nodes[i];
        QRectF r = nodeRect(node);

        // 背景
        if (node.isStart) {
            painter.setBrush(QColor(220, 255, 220));  // 淡绿
        }
        else if (node.isAccept) {
            painter.setBrush(QColor(255, 240, 220));  // 淡橙
        }
        else {
            painter.setBrush(QColor(245, 245, 255));  // 淡蓝灰
        }

        painter.setPen(QPen(Qt::black, 1.5));
        painter.drawRoundedRect(r, 8, 8);

        // 如果是接受状态，画双框
        if (node.isAccept) {
            QRectF inner = r.adjusted(3, 3, -3, -3);
            painter.drawRoundedRect(inner, 6, 6);
        }

        // 绘制状态编号（圆圈）
        double circleR = 12;
        QPointF circleCenter(r.right() - circleR - 4, r.bottom() - circleR - 4);
        painter.setPen(QPen(Qt::black, 1.2));
        painter.setBrush(Qt::white);
        painter.drawEllipse(circleCenter, circleR, circleR);
        painter.setFont(QFont("Microsoft YaHei", 8, QFont::Bold));
        painter.drawText(QRectF(circleCenter.x() - circleR, circleCenter.y() - circleR, circleR * 2, circleR * 2),
            Qt::AlignCenter, QString::number(node.id));

        // 绘制项目集内容
        painter.setFont(nodeFont);
        painter.setPen(Qt::black);
        double textY = r.top() + NODE_PADDING_Y + fm.ascent();
        for (const auto& item : node.items) {
            painter.drawText(QPointF(r.left() + NODE_PADDING_X, textY), item);
            textY += fm.height();
        }

        // 起始状态：画一个指向它的箭头
        if (node.isStart) {
            QPointF arrowEnd(r.left(), r.center().y());
            QPointF arrowStart(r.left() - 35, r.center().y());
            drawArrow(painter, arrowStart, arrowEnd, "");
        }
    }

    painter.restore();
}

void DFAGraphWidget::drawArrow(QPainter& painter, QPointF from, QPointF to, const QString& label)
{
    painter.setPen(QPen(Qt::black, 1.5));

    QLineF line(from, to);
    painter.drawLine(line);

    // 箭头
    double angle = std::atan2(-(to.y() - from.y()), to.x() - from.x());
    QPointF arrowP1 = to + QPointF(-ARROW_SIZE * std::cos(angle - M_PI / 6),
        ARROW_SIZE * std::sin(angle - M_PI / 6));
    QPointF arrowP2 = to + QPointF(-ARROW_SIZE * std::cos(angle + M_PI / 6),
        ARROW_SIZE * std::sin(angle + M_PI / 6));
    QPainterPath arrowHead;
    arrowHead.moveTo(to);
    arrowHead.lineTo(arrowP1);
    arrowHead.lineTo(arrowP2);
    arrowHead.closeSubpath();
    painter.setBrush(Qt::black);
    painter.drawPath(arrowHead);

    // 标签
    if (!label.isEmpty()) {
        QPointF mid = (from + to) / 2.0;
        // 偏移标签避免与线重叠
        double perpX = -(to.y() - from.y());
        double perpY = to.x() - from.x();
        double perpLen = std::sqrt(perpX * perpX + perpY * perpY);
        if (perpLen > 0) {
            perpX /= perpLen;
            perpY /= perpLen;
        }
        QPointF labelPos = mid + QPointF(perpX * 12, perpY * 12);

        painter.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
        painter.setPen(QColor(0, 0, 180));
        painter.setBrush(Qt::NoBrush);
        QFontMetrics fm(painter.font());
        QRectF textRect(labelPos.x() - fm.horizontalAdvance(label) / 2.0 - 2,
            labelPos.y() - fm.height() / 2.0,
            fm.horizontalAdvance(label) + 4, fm.height());
        painter.fillRect(textRect, QColor(255, 255, 255, 200));
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
}

void DFAGraphWidget::drawSelfLoop(QPainter& painter, const QRectF& rect, const QString& label)
{
    painter.setPen(QPen(Qt::black, 1.5));

    // 在节点左侧画自环
    QPointF top(rect.left() + rect.width() * 0.3, rect.top());
    QPointF loopCenter(rect.left() - 5, rect.top() - SELF_LOOP_RADIUS);

    QPainterPath path;
    path.moveTo(top);
    path.cubicTo(QPointF(top.x() - 30, top.y() - SELF_LOOP_RADIUS * 2),
        QPointF(top.x() - 60, top.y() - SELF_LOOP_RADIUS),
        QPointF(rect.left(), rect.top() + rect.height() * 0.3));

    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // 箭头
    QPointF endPt(rect.left(), rect.top() + rect.height() * 0.3);
    QPointF prevPt(rect.left() - 10, rect.top());
    double angle = std::atan2(-(endPt.y() - prevPt.y()), endPt.x() - prevPt.x());
    QPointF arrowP1 = endPt + QPointF(-ARROW_SIZE * std::cos(angle - M_PI / 6),
        ARROW_SIZE * std::sin(angle - M_PI / 6));
    QPointF arrowP2 = endPt + QPointF(-ARROW_SIZE * std::cos(angle + M_PI / 6),
        ARROW_SIZE * std::sin(angle + M_PI / 6));
    QPainterPath arrowHead;
    arrowHead.moveTo(endPt);
    arrowHead.lineTo(arrowP1);
    arrowHead.lineTo(arrowP2);
    arrowHead.closeSubpath();
    painter.setBrush(Qt::black);
    painter.drawPath(arrowHead);

    // 标签
    if (!label.isEmpty()) {
        painter.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
        painter.setPen(QColor(0, 0, 180));
        QPointF labelPos(rect.left() - 45, rect.top() - SELF_LOOP_RADIUS + 5);
        painter.drawText(labelPos, label);
    }
}

QPointF DFAGraphWidget::edgeIntersection(const QRectF& rect, const QPointF& from) const
{
    QPointF center = rect.center();
    double dx = from.x() - center.x();
    double dy = from.y() - center.y();

    if (std::abs(dx) < 0.001 && std::abs(dy) < 0.001) {
        return center;
    }

    double halfW = rect.width() / 2.0;
    double halfH = rect.height() / 2.0;

    double scaleX = (std::abs(dx) > 0.001) ? halfW / std::abs(dx) : 1e9;
    double scaleY = (std::abs(dy) > 0.001) ? halfH / std::abs(dy) : 1e9;
    double s = std::min(scaleX, scaleY);

    return QPointF(center.x() + dx * s, center.y() + dy * s);
}

// ====== 交互：拖拽和缩放 ======
void DFAGraphWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF worldPos = toWorld(event->pos());
        m_dragNodeIdx = -1;

        // 检查是否点击了某个节点
        for (int i = 0; i < m_nodes.size(); i++) {
            QRectF r = nodeRect(m_nodes[i]);
            if (r.contains(worldPos)) {
                m_dragNodeIdx = i;
                break;
            }
        }

        m_dragging = true;
        m_lastMousePos = event->pos();
    }
}

void DFAGraphWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        if (m_dragNodeIdx >= 0) {
            // 拖拽节点
            m_nodes[m_dragNodeIdx].pos += QPointF(delta.x() / m_scale, delta.y() / m_scale);
        }
        else {
            // 拖拽画布
            m_offset += QPointF(delta);
        }
        m_lastMousePos = event->pos();
        update();
    }
}

void DFAGraphWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragNodeIdx = -1;
    }
}

void DFAGraphWidget::wheelEvent(QWheelEvent* event)
{
    double oldScale = m_scale;
    if (event->angleDelta().y() > 0) {
        m_scale *= 1.15;
    }
    else {
        m_scale /= 1.15;
    }
    m_scale = std::max(0.2, std::min(m_scale, 5.0));

    // 以鼠标位置为中心缩放
    QPointF mousePos = event->position();
    m_offset = mousePos - (mousePos - m_offset) * (m_scale / oldScale);

    update();
}