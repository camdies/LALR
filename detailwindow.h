#ifndef DETAILWINDOW_H
#define DETAILWINDOW_H

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QMap>

namespace Ui {
    class DetailWindow;
}

class DetailWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DetailWindow(QWidget* parent = nullptr);
    ~DetailWindow();

    // 获取内部表格（供外部同步刷新）
    QTableWidget* getTable(const QString& tabName);

    // 添加/更新一个页签的表格
    void syncTable(const QString& tabName, QTableWidget* sourceTable);

    // 切换到指定页签
    void switchToTab(const QString& tabName);

private:
    Ui::DetailWindow* ui;
    QTabWidget* tabWidget;
    QMap<QString, QTableWidget*> tables;

    QTableWidget* ensureTab(const QString& tabName);
    void copyTableContent(QTableWidget* source, QTableWidget* dest);
};

#endif // DETAILWINDOW_H