#include "toastmanager.h"
#include <QApplication>

// ======================== ToastWidget ========================

ToastWidget::ToastWidget(const QString& message, bool isSuccess, QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(350, 50);
    setWindowFlags(Qt::FramelessWindowHint | Qt::SubWindow);
    setAttribute(Qt::WA_TranslucentBackground, false);

    QString bgColor = isSuccess ? "#d4edda" : "#f8d7da";   // 淡绿 / 淡红
    QString borderColor = isSuccess ? "#c3e6cb" : "#f5c6cb";
    QString textColor = isSuccess ? "#155724" : "#721c24";

    setStyleSheet(QString(
        "background-color: %1; border: 1px solid %2; border-radius: 6px;"
    ).arg(bgColor, borderColor));

    label = new QLabel(message, this);
    label->setStyleSheet(QString("color: %1; font-size: 13px; padding: 4px 10px; border: none; background: transparent;").arg(textColor));
    label->setWordWrap(true);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->addWidget(label);

    opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(1.0);
    setGraphicsEffect(opacityEffect);

    fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
}

void ToastWidget::startFadeOut(int durationMs)
{
    fadeAnimation->setDuration(durationMs);
    fadeAnimation->setStartValue(1.0);
    fadeAnimation->setEndValue(0.0);
    connect(fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        emit fadedOut(this);
        });
    fadeAnimation->start();
}

// ======================== ToastManager ========================

ToastManager::ToastManager(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
    , m_durationSec(3.0)
    , m_maxCount(5)
{
}

void ToastManager::showToast(const QString& message, bool isSuccess)
{
    // 超出最大数量时移除最旧的
    while (m_toasts.size() >= m_maxCount) {
        ToastWidget* oldest = m_toasts.first();
        m_toasts.removeFirst();
        oldest->deleteLater();
    }

    ToastWidget* toast = new ToastWidget(message, isSuccess, m_parentWidget);
    m_toasts.append(toast);
    toast->show();
    repositionToasts();

    // 定时开始淡出
    int stayMs = static_cast<int>(m_durationSec * 1000);
    int fadeMs = 800; // 淡出动画时长

    QTimer::singleShot(stayMs, toast, [toast, fadeMs]() {
        toast->startFadeOut(fadeMs);
        });

    connect(toast, &ToastWidget::fadedOut, this, [this](ToastWidget* w) {
        m_toasts.removeOne(w);
        w->deleteLater();
        repositionToasts();
        });
}

void ToastManager::repositionToasts()
{
    if (!m_parentWidget) return;
    int parentW = m_parentWidget->width();
    int parentH = m_parentWidget->height();

    // 从底部向上排列
    int y = parentH - TOAST_BOTTOM_MARGIN;
    for (int i = m_toasts.size() - 1; i >= 0; i--) {
        y -= TOAST_HEIGHT;
        m_toasts[i]->move(parentW - TOAST_WIDTH - TOAST_RIGHT_MARGIN, y);
        y -= TOAST_MARGIN;
    }
}

void ToastManager::setDuration(double seconds)
{
    if (seconds < 0.5) seconds = 0.5;
    m_durationSec = seconds;
}

void ToastManager::setMaxCount(int count)
{
    if (count < 1) count = 1;
    m_maxCount = count;
}