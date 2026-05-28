#ifndef TOASTMANAGER_H
#define TOASTMANAGER_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QList>
#include <QGraphicsOpacityEffect>

// 单条 Toast 通知
class ToastWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToastWidget(const QString& message, bool isSuccess, QWidget* parent = nullptr);
    void startFadeOut(int durationMs);

signals:
    void fadedOut(ToastWidget* self);

private:
    QLabel* label;
    QGraphicsOpacityEffect* opacityEffect;
    QPropertyAnimation* fadeAnimation;
};

// Toast 管理器 —— 负责在父窗口右下角堆叠通知
class ToastManager : public QObject
{
    Q_OBJECT
public:
    explicit ToastManager(QWidget* parentWidget, QObject* parent = nullptr);

    void showToast(const QString& message, bool isSuccess);

    // 设置项
    void setDuration(double seconds);   // 消息存在时间（秒）
    void setMaxCount(int count);        // 最多同时显示的消息数量
    double duration() const { return m_durationSec; }
    int maxCount() const { return m_maxCount; }

private:
    void repositionToasts();

    QWidget* m_parentWidget;
    QList<ToastWidget*> m_toasts;
    double m_durationSec;
    int m_maxCount;

    static const int TOAST_WIDTH = 350;
    static const int TOAST_HEIGHT = 50;
    static const int TOAST_MARGIN = 8;
    static const int TOAST_RIGHT_MARGIN = 16;
    static const int TOAST_BOTTOM_MARGIN = 16;
};

#endif // TOASTMANAGER_H