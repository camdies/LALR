#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QVector>
#include <QStringList>
#include <QHash>

#include "lalr.h"
#include "detailwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow* ui;

    // 起始符
    QString startString;

    // 存储非终结符
    QVector<QString> nonFinalizers;

    // 存储文法规则
    QHash<QString, QSet<QStringList>> grammars;

    // first集合与follow集合
    QHash<QString, QSet<QString>> firstSet;
    QHash<QString, QSet<QString>> followSet;

    // 规约规则存储
    QVector<Item> recursion;

    // LALR1_table
    QHash<int, QHash<QString, LALR1TableItem>> LALR1_table;

    // 是否可以分析句子
    bool canSentence;

    // 详细窗口
    DetailWindow* detailWindow;

    // 同步所有表格到详细窗口
    void syncAllTablesToDetail();

    // 判断SLR(1)
    QString checkSLR1(const LALR& lr0dfa);
};

#endif // MAINWINDOW_H