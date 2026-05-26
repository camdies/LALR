#include "detailwindow.h"
#include "ui_detailwindow.h"
#include <QHeaderView>

DetailWindow::DetailWindow(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::DetailWindow)
{
    ui->setupUi(this);
    setWindowTitle("详细分析结果");
    resize(1400, 900);

    tabWidget = new QTabWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(tabWidget);
    setLayout(layout);
}

DetailWindow::~DetailWindow()
{
    delete ui;
}

QTableWidget* DetailWindow::ensureTab(const QString& tabName)
{
    if (tables.contains(tabName)) {
        return tables[tabName];
    }
    QTableWidget* table = new QTableWidget(this);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    tabWidget->addTab(table, tabName);
    tables[tabName] = table;
    return table;
}

void DetailWindow::copyTableContent(QTableWidget* source, QTableWidget* dest)
{
    if (!source || !dest) return;

    dest->clear();
    dest->setRowCount(source->rowCount());
    dest->setColumnCount(source->columnCount());

    // 复制水平表头
    QStringList hHeaders;
    for (int c = 0; c < source->columnCount(); c++) {
        QTableWidgetItem* hItem = source->horizontalHeaderItem(c);
        hHeaders << (hItem ? hItem->text() : QString::number(c));
    }
    dest->setHorizontalHeaderLabels(hHeaders);

    // 复制垂直表头
    for (int r = 0; r < source->rowCount(); r++) {
        QTableWidgetItem* vItem = source->verticalHeaderItem(r);
        if (vItem) {
            QTableWidgetItem* newVItem = new QTableWidgetItem(vItem->text());
            newVItem->setForeground(vItem->foreground());
            dest->setVerticalHeaderItem(r, newVItem);
        }
    }

    // 复制单元格
    for (int r = 0; r < source->rowCount(); r++) {
        for (int c = 0; c < source->columnCount(); c++) {
            QTableWidgetItem* srcItem = source->item(r, c);
            if (srcItem) {
                QTableWidgetItem* newItem = new QTableWidgetItem(srcItem->text());
                newItem->setForeground(srcItem->foreground());
                newItem->setBackground(srcItem->background());
                dest->setItem(r, c, newItem);
            }
        }
    }

    dest->resizeRowsToContents();
    dest->resizeColumnsToContents();
}

void DetailWindow::syncTable(const QString& tabName, QTableWidget* sourceTable)
{
    QTableWidget* dest = ensureTab(tabName);
    copyTableContent(sourceTable, dest);
}

void DetailWindow::switchToTab(const QString& tabName)
{
    if (tables.contains(tabName)) {
        int idx = tabWidget->indexOf(tables[tabName]);
        if (idx >= 0) tabWidget->setCurrentIndex(idx);
    }
}

QTableWidget* DetailWindow::getTable(const QString& tabName)
{
    if (tables.contains(tabName)) return tables[tabName];
    return nullptr;
}