#ifndef DFAGRAPHWIDGET_H
#define DFAGRAPHWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QHash>
#include <QSet>
#include <cmath>

// 一个 DFA 状态节点
struct DFANode {
    int id;
    QStringList items;       // 项目集内容（每行一个项目）
    QPointF pos;             // 布局位置（中心点）
    bool isStart = false;    // 是否为初始状态
    bool isAccept = false;   // 是否为接受状态（含归约项目）
};

// 一条 DFA 转移边
struct DFAEdge {
    int from;
    int to;
    QString symbol;          // 转移符号
};

class DFAGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DFAGraphWidget(QWidget* parent = nullptr);

    // 设置 DFA 数据
    void setDFA(const QVector<DFANode>& nodes, const QVector<DFAEdge>& edges);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QVector<DFANode> m_nodes;
    QVector<DFAEdge> m_edges;

    // 视图变换
    QPointF m_offset;        // 平移偏移
    double m_scale;          // 缩放比例
    QPoint m_lastMousePos;
    bool m_dragging;
    int m_dragNodeIdx;       // 正在拖拽的节点索引，-1表示拖拽画布

    // 布局
    void autoLayout();

    // 绘制辅助
    QRectF nodeRect(const DFANode& node) const;
    QSizeF nodeSize(const DFANode& node) const;
    QPointF toScreen(const QPointF& worldPos) const;
    QPointF toWorld(const QPointF& screenPos) const;

    void drawArrow(QPainter& painter, QPointF from, QPointF to, const QString& label);
    void drawSelfLoop(QPainter& painter, const QRectF& rect, const QString& label);
    QPointF edgeIntersection(const QRectF& rect, const QPointF& from) const;
};

#endif // DFAGRAPHWIDGET_H