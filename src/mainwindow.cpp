#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QScreen>
#include <QRect>
#include <QString>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QDateTime>
#include <QColor>
#include <QStack>
#include <QHeaderView>
#include <QRegularExpression>
#include <algorithm>
#include <functional>

#include "lalr.h"

// ============ 消息设置按钮 ============
#include "settingsdialog.h"

// ====================== 辅助：设置 DFA 表格垂直表头颜色 ======================
static void setDFAVerticalHeaders(QTableWidget* table, int totalRows)
{
    for (int i = 0; i < totalRows; i++) {
        auto* headerItem = new QTableWidgetItem(QString::number(i));
        if (i == 0) {
            headerItem->setForeground(QColor(0, 160, 0));
        }
        else if (i == totalRows - 1) {
            headerItem->setForeground(QColor(255, 0, 0));
        }
        table->setVerticalHeaderItem(i, headerItem);
    }
}

// ====================== 辅助：规范化文法行 ======================
static QString normalizeGrammarLine(const QString& line)
{
    QString result = line.trimmed();
    if (result.isEmpty()) return result;

    // 第一步：在 "->" 两侧确保有空格
    result.replace("->", " -> ");

    // 第二步：在 "|" 两侧确保有空格
    result.replace("|", " | ");

    // 第三步：清理多余空格
    result = result.simplified();

    return result;
}

// 新增：智能分词函数，用于将产生式右侧（可能无空格）正确分割
// 核心思路：如果有空格就按空格分词；如果没有空格则逐字符分词
// 但需识别多字符的非终结符名（已在 nonFinalizers 中注册的）
static QStringList smartTokenize(const QString& rhs, const QVector<QString>& knownNonTerminals)
{
    // 先尝试按空格分割
    QStringList parts = rhs.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    // 如果按空格分词后每个 token 都是已知符号或单字符，直接返回
    // 否则需要对无空格的 token 进行进一步拆分
    QStringList result;
    for (const QString& part : parts) {
        if (part == "@" || part == "|" || part == "->") {
            result.append(part);
            continue;
        }
        // 检查是否是已知的非终结符
        if (knownNonTerminals.contains(part)) {
            result.append(part);
            continue;
        }
        // 如果是单字符，直接加入
        if (part.size() == 1) {
            result.append(part);
            continue;
        }
        // 尝试贪心匹配已知非终结符，否则逐字符拆分
        int i = 0;
        while (i < part.size()) {
            bool matched = false;
            // 从最长的已知非终结符开始匹配
            for (int len = part.size() - i; len > 0; len--) {
                QString sub = part.mid(i, len);
                if (knownNonTerminals.contains(sub)) {
                    result.append(sub);
                    i += len;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                // 单字符作为终结符
                result.append(QString(part[i]));
                i++;
            }
        }
    }
    return result;
}
/*
static QString normalizeGrammarLine(const QString& line)
{
    QString result = line.trimmed();
    if (result.isEmpty()) return result;

    // 1. 处理 -> 使得两边有空格
    result.replace("->", " -> ");

    // 2. 处理 | 使得两边有空格（关键修复！）
    // 需要确保 | 两侧都有空格，这样 "* |/" 变成 "* | /"
    result.replace("|", " | ");

    // 3. 清理多余空格
    result = result.simplified();
    return result;
}
*/
// ====================== 构造函数 ======================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , canSentence(false)
{
    ui->setupUi(this);
    this->setWindowTitle("LALR(1)语法分析生成器");
    // 设置合适的窗口大小而非最大化
    this->resize(1280, 720);

    //初始化消息通知
    toastManager = new ToastManager(this, this);

    // ============ 使用说明按钮 ============
    connect(ui->helpButton, &QPushButton::clicked, this, [this]() {
        QMessageBox helpBox(this);
        helpBox.setWindowTitle("LALR(1)分析生成器 - 使用说明");
        helpBox.setTextFormat(Qt::RichText);
        helpBox.setText(
            "<h3>LALR(1)分析生成器使用说明</h3>"
            "<p><b>1.</b> 输入文法规则，符号之间用空格隔开，左侧非终结符后跟 <code>-></code> 符号。</p>"
            "<p>&nbsp;&nbsp;&nbsp;支持的写法：<code>E -> E + T</code> 或 <code>E->E+T</code> 或 <code>exp ->exp addop term</code></p>"
            "<p><b>2.</b> 用 <code>#</code> 表示空串，用 <code>|</code> 分隔同一非终结符的不同产生式。</p>"
            "<p><b>3.</b> 第一个非终结符默认为文法开始符号。</p>"
            "<p><b>4.</b> 点击「文法分析」生成 First/Follow 集合、LR(0)/LR(1)/LALR(1) DFA 及分析表。</p>"
            "<p><b>5.</b> 输入句子后点击「分析句子」查看分析过程。</p>"
            "<p><b>6.</b> 点击「通知设置」可调整右下角消息提示的显示时间和最大数量。</p>"
            "<p><b style='color:red;'>7. 注意: 编译原理的文法定义中 | 是保留字符，不应出现在符号名中。</b></p>"
            "<hr>"
            "<p style='color:green;'>DFA表中：<b>绿色编号</b> = 初始状态(0)</p>"
            "<p style='color:red;'>DFA表中：<b>红色编号</b> = 末状态</p>"
            "<p>Follow集合中的终结符 <code>$</code> 表示输入结束符。</p>"
        );
        helpBox.setStandardButtons(QMessageBox::Ok);
        helpBox.exec();
        });

// 在构造函数中，helpButton 的 connect 之后添加：
    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(toastManager->duration(), toastManager->maxCount(), this);
        if (dlg.exec() == QDialog::Accepted) {
            toastManager->setDuration(dlg.getDuration());
            toastManager->setMaxCount(dlg.getMaxCount());
        }
        });

    // ============ 打开文法文件 ============
    connect(ui->openButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "选择文法规则文件", "", "文本文件 (*.txt);;所有文件 (*)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                ui->grammarEdit->setText(in.readAll());
                file.close();
            }
        }
        });

    // ============ 保存文法文件 ============
    connect(ui->saveButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, "保存文法规则文件", "grammar.txt", "文本文件 (*.txt);;所有文件 (*)");
        if (fileName.isEmpty()) return;
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << ui->grammarEdit->toPlainText();
            file.close();
            //QMessageBox::information(this, "提示", "文件保存成功: " + fileName);
            toastManager->showToast("提示: 文件保存成功: " + fileName, true);

        }
        else {
            //QMessageBox::warning(this, "提示", "文件保存失败!");
            toastManager->showToast("提示: 文件保存失败!", false);
        }
        });

    // ============ 文法分析 ============
    connect(ui->analysisButton, &QPushButton::clicked, this, [this]() {
        // 清空
        nonFinalizers.clear();
        grammars.clear();
        firstSet.clear();
        followSet.clear();
        recursion.clear();
        LALR1_table.clear();
        canSentence = false;

        ui->firstTableWidget->clearContents();
        ui->firstTableWidget->setRowCount(0);
        ui->followTableWidget->clearContents();
        ui->followTableWidget->setRowCount(0);
        ui->lr0TableWidget->clearContents();
        ui->lr0TableWidget->setRowCount(0);
        ui->lrTableWidget->clearContents();
        ui->lrTableWidget->setRowCount(0);
        ui->lalrTableWidget->clearContents();
        ui->lalrTableWidget->setRowCount(0);
        ui->lalrAnalysisTableWidget->clearContents();
        ui->lalrAnalysisTableWidget->setRowCount(0);
        ui->resultTableWidget->clearContents();
        ui->resultTableWidget->setRowCount(0);
        ui->slr1DetailLabel->setText("");

        // -------- 获取文法规则 --------
        QString content = ui->grammarEdit->toPlainText().trimmed();
        if (content.isEmpty()) {
            toastManager->showToast("提示: 请先输入文法规则!", false);
            return;
        }
        QStringList rawGrammarList = content.split("\n", Qt::SkipEmptyParts);

        // -------- 预处理：规范化每一行 --------
        QStringList grammarList;
        for (const auto& line : rawGrammarList) {
            QString normalized = normalizeGrammarLine(line);
            if (!normalized.isEmpty()) {
                grammarList.append(normalized);
            }
        }

        // -------- 获取非终结符 --------
// 修改文法解析部分，在获取非终结符之后、分词之前
// 先第一遍收集非终结符（不变）
// 第二遍分词时使用 smartTokenize
        // -------- 第一遍：获取非终结符 --------
        for (const auto& grammar : grammarList) {
            QStringList wordList = grammar.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (wordList.size() < 3 || wordList[1] != "->") {
                toastManager->showToast("文法格式错误! 要求: 非终结符 -> 产生式\n错误行: " + grammar, false);
                return;
            }
            if (!nonFinalizers.contains(wordList[0]))
                nonFinalizers.push_back(wordList[0]);
        }

        // -------- 获取起始符 --------
        startString = nonFinalizers[0];

        // -------- 第二遍：使用 smartTokenize 分词并构建文法规则 --------
        for (const auto& grammar : grammarList) {
            int arrowPos = grammar.indexOf("->");
            if (arrowPos < 0) continue;
            QString nonfinal = grammar.left(arrowPos).trimmed();
            QString rhsRaw = grammar.mid(arrowPos + 2).trimmed();

            // 按 | 分割各候选式
            QStringList alternatives = rhsRaw.split(QRegularExpression("\\s*\\|\\s*"), Qt::SkipEmptyParts);

            for (const QString& alt : alternatives) {
                QStringList tokens = smartTokenize(alt.trimmed(), nonFinalizers);
                if (!tokens.isEmpty()) {
                    grammars[nonfinal].insert(tokens);
                    // 同时注册终结符到 firstSet
                    for (const QString& tok : tokens) {
                        if (!nonFinalizers.contains(tok) && tok != "#") {
                            firstSet[tok].insert(tok);
                        }
                    }
                }
            }
        }

        // -------- 消除直接左递归 --------
        int len = nonFinalizers.size();
        for (int i = 0; i < len; i++) {
            QString nonFinal = nonFinalizers[i];
            QSet<QStringList> leftGrammar, rightGrammar;
            for (const QStringList& g : grammars[nonFinal]) {
                if (!g.isEmpty() && g[0] == nonFinal) {
                    leftGrammar.insert(g.mid(1));
                }
                else {
                    rightGrammar.insert(g);
                }
            }
            if (rightGrammar.isEmpty() && !leftGrammar.isEmpty()) {
                //QMessageBox::warning(this, "提示", "文法包含死递归!");
                toastManager->showToast("提示: 文法包含死递归!", false);
                return;
            }
            if (!leftGrammar.isEmpty()) {
                grammars[nonFinal].clear();
                QString newName = nonFinal + "_nonleft";
                while (nonFinalizers.contains(newName)) newName += "_";
                for (auto rs : rightGrammar) {
                    rs.append(newName);
                    grammars[nonFinal].insert(rs);
                }
                nonFinalizers.append(newName);
                for (auto ls : leftGrammar) {
                    ls.append(newName);
                    grammars[newName].insert(ls);
                }
                grammars[newName].insert(QStringList() << "#");
            }
        }

        // -------- 计算 FIRST 集合 --------
        QSet<QString> computing;

        std::function<QSet<QString>(const QString&)> getFirst = [&](const QString& s) -> QSet<QString> {
            if (firstSet.contains(s) && !firstSet[s].isEmpty()) {
                return firstSet[s];
            }
            if (!nonFinalizers.contains(s)) {
                firstSet[s].insert(s);
                return firstSet[s];
            }
            if (computing.contains(s)) {
                return firstSet[s];  // 正在计算中，返回当前已有的（可能为空）
            }
            computing.insert(s);
            for (const auto& g : grammars[s]) {
                int k = 0;
                while (k < g.size()) {
                    QSet<QString> fk = getFirst(g[k]);
                    bool hasEpsilon = fk.contains("#");
                    QSet<QString> fkNoEps = fk;
                    fkNoEps.remove("#");
                    firstSet[s].unite(fkNoEps);
                    if (!hasEpsilon) break;
                    k++;
                }
                if (k == g.size()) firstSet[s].insert("#");
            }
            computing.remove(s);
            return firstSet[s];
            };
        /*
        std::function<QSet<QString>(const QString&)> getFirst = [&](const QString& s) -> QSet<QString> {
            //写保护
            if (firstSet.contains(s) && !firstSet[s].isEmpty()) {
                return firstSet[s];
            }

            if (!nonFinalizers.contains(s)) {
                firstSet[s].insert(s);
                return firstSet[s];
            }
            for (const auto& g : grammars[s]) {
                int k = 0;
                while (k < g.size()) {
                    QSet<QString> fk = getFirst(g[k]);
                    bool hasEpsilon = fk.contains("#");
                    QSet<QString> fkNoEps = fk;
                    fkNoEps.remove("#");
                    firstSet[s].unite(fkNoEps);
                    if (!hasEpsilon) break;
                    k++;
                }
                if (k == g.size()) firstSet[s].insert("#");
            }
            return firstSet[s];
            };
        */
        for (const auto& nf : nonFinalizers) {
            getFirst(nf);
        }

        // -------- 计算 FOLLOW 集合 --------
        followSet[startString].insert("$");
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto it = grammars.constBegin(); it != grammars.constEnd(); ++it) {
                const QString& key = it.key();
                for (const auto& g : it.value()) {
                    for (int i = 0; i < g.size(); i++) {
                        if (!nonFinalizers.contains(g[i])) continue;

                        int k = i + 1;
                        QSet<QString> tmpSet;
                        while (k < g.size()) {
                            QSet<QString> fk = firstSet[g[k]];
                            bool hasEps = fk.contains("#");
                            QSet<QString> fkNoEps = fk;
                            fkNoEps.remove("#");
                            tmpSet.unite(fkNoEps);
                            if (!hasEps) break;
                            k++;
                        }
                        bool canBeEmpty = (k == g.size());

                        QSet<QString> toAdd = tmpSet;
                        toAdd.subtract(followSet[g[i]]);
                        if (!toAdd.isEmpty()) {
                            followSet[g[i]].unite(toAdd);
                            changed = true;
                        }

                        if (canBeEmpty) {
                            QSet<QString> toAdd2 = followSet[key];
                            toAdd2.subtract(followSet[g[i]]);
                            if (!toAdd2.isEmpty()) {
                                followSet[g[i]].unite(toAdd2);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        // -------- 可视化 FIRST 和 FOLLOW 集合（不带大括号） --------
        ui->firstTableWidget->setColumnCount(2);
        ui->firstTableWidget->setHorizontalHeaderLabels(QStringList() << "非终结符" << "First集合");
        ui->firstTableWidget->horizontalHeader()->setStretchLastSection(true);
        ui->firstTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

        int firstCount = 0;
        for (const auto& nf : nonFinalizers) {
            if (firstSet.contains(nf)) firstCount++;
        }
        ui->firstTableWidget->setRowCount(firstCount);
        int row = 0;
        for (const auto& nf : nonFinalizers) {
            if (!firstSet.contains(nf)) continue;
            ui->firstTableWidget->setItem(row, 0, new QTableWidgetItem(nf));
            QStringList vals = firstSet[nf].values();
            std::sort(vals.begin(), vals.end());
            ui->firstTableWidget->setItem(row, 1, new QTableWidgetItem(vals.join(", ")));
            row++;
        }

        ui->followTableWidget->setColumnCount(2);
        ui->followTableWidget->setHorizontalHeaderLabels(QStringList() << "非终结符" << "Follow集合");
        ui->followTableWidget->horizontalHeader()->setStretchLastSection(true);
        ui->followTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

        int followCount = 0;
        for (const auto& nf : nonFinalizers) {
            if (followSet.contains(nf)) followCount++;
        }
        ui->followTableWidget->setRowCount(followCount);
        row = 0;
        for (const auto& nf : nonFinalizers) {
            if (!followSet.contains(nf)) continue;
            ui->followTableWidget->setItem(row, 0, new QTableWidgetItem(nf));
            QStringList vals = followSet[nf].values();
            std::sort(vals.begin(), vals.end());
            ui->followTableWidget->setItem(row, 1, new QTableWidgetItem(vals.join(", ")));
            row++;
        }

        // ======== 扩充文法 ========
        firstSet["$"] = QSet<QString>({ "$" });
        {
            QString newStart = startString + "_new";
            while (nonFinalizers.contains(newStart)) newStart += "_";
            grammars[newStart].insert(QStringList() << startString);
            nonFinalizers.prepend(newStart);
            firstSet[newStart] = firstSet[startString];
            followSet[newStart].insert("$");
            startString = newStart;
        }

        // ======== 构建 LR(0) DFA ========
        LALR lr0dfa;
        {
            LR0Item lr0FirstItem;
            lr0FirstItem.name = startString;
            lr0FirstItem.rule = *grammars[startString].begin();
            lr0FirstItem.pos = 0;
            LR0State lr0FirstState;
            lr0FirstState.st.insert(lr0FirstItem);
            lr0FirstState = LR0State::closure(lr0FirstState, grammars, nonFinalizers);
            lr0dfa.buildLR0(lr0FirstState, grammars, nonFinalizers);
        }

        // ======== 判断 LR(0) 冲突 ========
        bool isLR0 = true;
        QString lr0ConflictMsg;
        {
            QHash<int, LR0State> revLR0;
            for (auto it = lr0dfa.lr0StateHash.constBegin(); it != lr0dfa.lr0StateHash.constEnd(); ++it) {
                revLR0[it.value()] = it.key();
            }
            QStringList lr0Conflicts;
            for (int i = 0; i < lr0dfa.lr0size; i++) {
                if (!revLR0.contains(i)) continue;
                const LR0State& state = revLR0[i];

                int reduceCount = 0;
                bool hasShift = false;
                QStringList reduceNames;

                for (const auto& item : state.st) {
                    bool isReduce = (item.pos == item.rule.size()) ||
                        (item.rule.size() == 1 && item.rule[0] == "#" && item.pos == 0);
                    if (isReduce) {
                        reduceCount++;
                        reduceNames.append(item.name + " -> " + item.rule.join(" "));
                    }
                    else {
                        hasShift = true;
                    }
                }

                if (reduceCount > 1) {
                    lr0Conflicts.append(QString("状态I%1: 归约-归约冲突 (多个归约项目: %2)")
                        .arg(i).arg(reduceNames.join("; ")));
                }
                if (reduceCount > 0 && hasShift) {
                    lr0Conflicts.append(QString("状态I%1: 移进-归约冲突")
                        .arg(i));
                }
            }
            if (!lr0Conflicts.isEmpty()) {
                isLR0 = false;
                lr0ConflictMsg = "<b style='color:red;'>该文法不是 LR(0) 文法</b><br>原因:<br>";
                for (const auto& c : lr0Conflicts) {
                    lr0ConflictMsg += "• " + c + "<br>";
                }
            }
        }

        // LR(0) DFA 可视化
        {
            QSet<QString> lr0ChangeSymbols;
            for (auto it = lr0dfa.lr0ChangeHash.constBegin(); it != lr0dfa.lr0ChangeHash.constEnd(); ++it) {
                for (auto jt = it.value().constBegin(); jt != it.value().constEnd(); ++jt) {
                    lr0ChangeSymbols.insert(jt.key());
                }
            }
            QStringList lr0Columns = lr0ChangeSymbols.values();
            std::sort(lr0Columns.begin(), lr0Columns.end());

            ui->lr0TableWidget->setRowCount(lr0dfa.lr0size);
            ui->lr0TableWidget->setColumnCount(lr0Columns.size());
            ui->lr0TableWidget->setHorizontalHeaderLabels(lr0Columns);
            ui->lr0TableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

            QHash<int, LR0State> revLR0;
            for (auto it = lr0dfa.lr0StateHash.constBegin(); it != lr0dfa.lr0StateHash.constEnd(); ++it) {
                revLR0[it.value()] = it.key();
            }

            // 不再调用 setDFAVerticalHeaders，改为与 LR(1)/LALR(1) 一致的样式
            for (int i = 0; i < lr0dfa.lr0size; i++) {
                QString desc = QString::number(i) + ":\n";
                if (revLR0.contains(i)) {
                    for (const auto& item : revLR0[i].st) {
                        desc += item.name + " -> ";
                        for (int p = 0; p < item.rule.size(); p++) {
                            if (p == item.pos) desc += ". ";
                            desc += item.rule[p] + " ";
                        }
                        if (item.pos == item.rule.size()) desc += ". ";
                        desc += "\n";
                    }
                }
                auto* vhItem = new QTableWidgetItem(desc);
                if (i == 0) {
                    vhItem->setForeground(QColor(0, 160, 0));     // 绿色=初始状态
                }
                else if (i == lr0dfa.lr0size - 1) {
                    vhItem->setForeground(QColor(255, 0, 0));      // 红色=末状态
                }
                ui->lr0TableWidget->setVerticalHeaderItem(i, vhItem);

                for (int c = 0; c < lr0Columns.size(); c++) {
                    if (lr0dfa.lr0ChangeHash.contains(i) && lr0dfa.lr0ChangeHash[i].contains(lr0Columns[c])) {
                        ui->lr0TableWidget->setItem(i, c, new QTableWidgetItem(
                            QString::number(lr0dfa.lr0ChangeHash[i][lr0Columns[c]])));
                    }
                }
            }
            ui->lr0TableWidget->resizeRowsToContents();
            ui->lr0TableWidget->resizeColumnsToContents();
        }

        // ======== 判断 SLR(1) — 写入独立页签 ========
        {
            QString slr1Result = checkSLR1(lr0dfa);
            // 如果不是LR(0)，在SLR(1)页签前面加上LR(0)的冲突信息
            QString fullResult;
            if (!isLR0) {
                fullResult = lr0ConflictMsg + "<hr>" + slr1Result;
            }
            else {
                fullResult = "<b style='color:green;'>该文法是 LR(0) 文法</b><hr>" + slr1Result;
            }
            ui->slr1DetailLabel->setText(fullResult);
        }
        /*
        // ======== 判断 SLR(1) ========
        {
            QString slr1Result = checkSLR1(lr0dfa);
            ui->slr1DetailLabel->setText(slr1Result);
        }
        */


        // ======== 构建 LR(1) DFA ========
        LALR lr1;
        {
            Item firstItem;
            firstItem.name = startString;
            firstItem.rule = *grammars[startString].begin();
            firstItem.next = QSet<QString>({ "$" });
            firstItem.pos = 0;
            State firstState;
            firstState.st.insert(firstItem);
            firstState = State::closure(firstState, grammars, nonFinalizers, firstSet);
            lr1.stateHash[firstState] = lr1.size++;
            lr1.buildLR1(firstState, grammars, nonFinalizers, firstSet, followSet);
        }

        // LR(1) DFA 可视化 - 按照样式：垂直表头显示项目集内容
        {
            QSet<QString> lrChangeSymbols;
            for (auto it = lr1.changeHash.constBegin(); it != lr1.changeHash.constEnd(); ++it) {
                for (auto jt = it.value().constBegin(); jt != it.value().constEnd(); ++jt) {
                    lrChangeSymbols.insert(jt.key());
                }
            }
            QStringList lrColumns = lrChangeSymbols.values();
            std::sort(lrColumns.begin(), lrColumns.end());

            ui->lrTableWidget->setRowCount(lr1.size);
            ui->lrTableWidget->setColumnCount(lrColumns.size());
            ui->lrTableWidget->setHorizontalHeaderLabels(lrColumns);
            ui->lrTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

            QHash<int, State> revLR1;
            for (auto it = lr1.stateHash.constBegin(); it != lr1.stateHash.constEnd(); ++it) {
                revLR1[it.value()] = it.key();
            }

            for (int i = 0; i < lr1.size; i++) {
                // 构建项目集描述字符串，直接放入垂直表头
                QString desc = QString::number(i) + ":\n";
                if (revLR1.contains(i)) {
                    for (const auto& item : revLR1[i].st) {
                        desc += item.name + " -> ";
                        for (int p = 0; p < item.rule.size(); p++) {
                            if (p == item.pos) desc += ". ";
                            desc += item.rule[p] + " ";
                        }
                        if (item.pos == item.rule.size()) desc += ". ";
                        desc += ", ";
                        QStringList lookaheads = item.next.values();
                        std::sort(lookaheads.begin(), lookaheads.end());
                        desc += lookaheads.join("、");
                        desc += "\n";
                    }
                }
                auto* vhItem = new QTableWidgetItem(desc);
                if (i == 0) {
                    vhItem->setForeground(QColor(0, 160, 0));
                }
                else if (i == lr1.size - 1) {
                    vhItem->setForeground(QColor(255, 0, 0));
                }
                ui->lrTableWidget->setVerticalHeaderItem(i, vhItem);

                for (int c = 0; c < lrColumns.size(); c++) {
                    if (lr1.changeHash.contains(i) && lr1.changeHash[i].contains(lrColumns[c])) {
                        ui->lrTableWidget->setItem(i, c, new QTableWidgetItem(
                            QString::number(lr1.changeHash[i][lrColumns[c]])));
                    }
                }
            }
            ui->lrTableWidget->resizeRowsToContents();
            ui->lrTableWidget->resizeColumnsToContents();
        }

        // ======== 构建 LALR(1) DFA ========
        LALR lalr1;
        lalr1.buildLALR1(lr1);



        // LALR(1) DFA 可视化 - 样式
        {
            QSet<QString> lalrChangeSymbols;
            for (auto it = lalr1.changeHash.constBegin(); it != lalr1.changeHash.constEnd(); ++it) {
                for (auto jt = it.value().constBegin(); jt != it.value().constEnd(); ++jt) {
                    lalrChangeSymbols.insert(jt.key());
                }
            }
            QStringList lalrColumns = lalrChangeSymbols.values();
            std::sort(lalrColumns.begin(), lalrColumns.end());

            ui->lalrTableWidget->setRowCount(lalr1.size);
            ui->lalrTableWidget->setColumnCount(lalrColumns.size());
            ui->lalrTableWidget->setHorizontalHeaderLabels(lalrColumns);
            ui->lalrTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

            QHash<int, State> revLALR1;
            for (auto it = lalr1.stateHash.constBegin(); it != lalr1.stateHash.constEnd(); ++it) {
                revLALR1[it.value()] = it.key();
            }

            for (int i = 0; i < lalr1.size; i++) {
                QString desc = QString::number(i) + ":\n";
                if (revLALR1.contains(i)) {
                    for (const auto& item : revLALR1[i].st) {
                        desc += item.name + " -> ";
                        for (int p = 0; p < item.rule.size(); p++) {
                            if (p == item.pos) desc += ". ";
                            desc += item.rule[p] + " ";
                        }
                        if (item.pos == item.rule.size()) desc += ". ";
                        desc += ", ";
                        QStringList lookaheads = item.next.values();
                        std::sort(lookaheads.begin(), lookaheads.end());
                        desc += lookaheads.join("、");
                        desc += "\n";
                    }
                }
                auto* vhItem = new QTableWidgetItem(desc);
                if (i == 0) {
                    vhItem->setForeground(QColor(0, 160, 0));
                }
                else if (i == lalr1.size - 1) {
                    vhItem->setForeground(QColor(255, 0, 0));
                }
                ui->lalrTableWidget->setVerticalHeaderItem(i, vhItem);

                for (int c = 0; c < lalrColumns.size(); c++) {
                    if (lalr1.changeHash.contains(i) && lalr1.changeHash[i].contains(lalrColumns[c])) {
                        ui->lalrTableWidget->setItem(i, c, new QTableWidgetItem(
                            QString::number(lalr1.changeHash[i][lalrColumns[c]])));
                    }
                }
            }
            ui->lalrTableWidget->resizeRowsToContents();
            ui->lalrTableWidget->resizeColumnsToContents();
        }

        // ======== 判断 LALR(1) 冲突 ========
        {
            QHash<int, State> revLALR1;
            for (auto it = lalr1.stateHash.constBegin(); it != lalr1.stateHash.constEnd(); ++it) {
                revLALR1[it.value()] = it.key();
            }
            bool hasReduceReduce = false;
            bool hasShiftReduce = false;
            for (int i = 0; i < lalr1.size; i++) {
                if (!revLALR1.contains(i)) continue;
                const State& state = revLALR1[i];
                QSet<QString> reduceSymbols;
                for (const auto& item : state.st) {
                    if (item.pos == item.rule.size() || (item.rule.size() == 1 && item.rule[0] == "#")) {
                        for (const auto& n : item.next) {
                            if (reduceSymbols.contains(n)) {
                                hasReduceReduce = true;
                            }
                            reduceSymbols.insert(n);
                        }
                    }
                }
                QSet<QString> shiftSymbols;
                for (const auto& item : state.st) {
                    if (item.pos < item.rule.size() && !(item.rule.size() == 1 && item.rule[0] == "#")) {
                        shiftSymbols.insert(item.rule[item.pos]);
                    }
                }
                if (!shiftSymbols.intersect(reduceSymbols).isEmpty()) {
                    hasShiftReduce = true;
                }
            }
            if (hasReduceReduce) {
                //QMessageBox::warning(this, "警告", "该文法在 LALR(1) 中出现了规约-规约冲突，不是 LALR(1) 文法");
                toastManager->showToast("警告: 该文法在 LALR(1) 中出现了规约-规约冲突,不是 LALR(1) 文法!", false);
                return;
            }
            if (hasShiftReduce) {
                //QMessageBox::warning(this, "警告", "该文法在 LALR(1) 中出现了移进-规约冲突。后续将优先移进来解决二义性。");
                toastManager->showToast("警告: 该文法在 LALR(1) 中出现了移进-规约冲突。后续将优先移进来解决二义性。", false);
            }
        }

        // ======== 构建 LALR(1) 分析表 ========
        {
            QSet<QString> terminalSet;
            for (int i = 0; i < lalr1.size; i++) {
                if (!lalr1.changeHash.contains(i)) continue;
                for (auto it = lalr1.changeHash[i].constBegin(); it != lalr1.changeHash[i].constEnd(); ++it) {
                    if (!nonFinalizers.contains(it.key())) {
                        terminalSet.insert(it.key());
                    }
                }
            }
            QStringList terminalList = terminalSet.values();
            std::sort(terminalList.begin(), terminalList.end());
            terminalList.append("$");

            QStringList nonTermList;
            for (const auto& nf : nonFinalizers) {
                if (nf != startString) nonTermList.append(nf);
            }
            std::sort(nonTermList.begin(), nonTermList.end());

            QStringList header;
            header.append(terminalList);
            header.append(nonTermList);

            ui->lalrAnalysisTableWidget->setColumnCount(header.size());
            ui->lalrAnalysisTableWidget->setHorizontalHeaderLabels(header);
            ui->lalrAnalysisTableWidget->setRowCount(lalr1.size);
            ui->lalrAnalysisTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

            QHash<QString, int> headerHash;
            for (int i = 0; i < header.size(); i++) {
                headerHash[header[i]] = i;
            }

            QHash<int, State> revHash;
            for (auto it = lalr1.stateHash.constBegin(); it != lalr1.stateHash.constEnd(); ++it) {
                revHash[it.value()] = it.key();
            }

            setDFAVerticalHeaders(ui->lalrAnalysisTableWidget, lalr1.size);

            // 先填规约/接受
            for (int i = 0; i < lalr1.size; i++) {
                if (!revHash.contains(i)) continue;
                const State& state = revHash[i];
                for (const Item& item : state.st) {
                    bool isReduce = (item.pos == item.rule.size()) ||
                        (item.rule.size() == 1 && item.rule[0] == "#" && item.pos == 0);
                    if (!isReduce) continue;

                    if (item.name == startString) {
                        for (const auto& next : item.next) {
                            if (next == "$" && headerHash.contains("$")) {
                                LALR1TableItem ti;
                                ti.kind = 4;
                                ti.idx = 0;
                                LALR1_table[i].insert(next, ti);
                                ui->lalrAnalysisTableWidget->setItem(i, headerHash["$"],
                                    new QTableWidgetItem("acc"));
                            }
                        }
                    }
                    else {
                        for (const auto& next : item.next) {
                            if (!headerHash.contains(next)) continue;
                            int ruleIdx = -1;
                            for (int r = 0; r < recursion.size(); r++) {
                                if (Item::haveSameCore(recursion[r], item)) {
                                    ruleIdx = r;
                                    break;
                                }
                            }
                            if (ruleIdx < 0) {
                                recursion.push_back(item);
                                ruleIdx = recursion.size() - 1;
                            }
                            LALR1TableItem ti;
                            ti.kind = 2;
                            ti.idx = ruleIdx;
                            if (!LALR1_table[i].contains(next)) {
                                LALR1_table[i].insert(next, ti);
                                ui->lalrAnalysisTableWidget->setItem(i, headerHash[next],
                                    new QTableWidgetItem("r" + QString::number(ruleIdx)));
                            }
                        }
                    }
                }
            }

            // 再填移进（移进优先覆盖规约）
            for (int i = 0; i < lalr1.size; i++) {
                if (!lalr1.changeHash.contains(i)) continue;
                for (auto it = lalr1.changeHash[i].constBegin(); it != lalr1.changeHash[i].constEnd(); ++it) {
                    const QString& sym = it.key();
                    int target = it.value();
                    if (!headerHash.contains(sym)) continue;
                    if (nonFinalizers.contains(sym)) {
                        LALR1TableItem ti;
                        ti.kind = 3;
                        ti.idx = target;
                        LALR1_table[i].insert(sym, ti);
                        ui->lalrAnalysisTableWidget->setItem(i, headerHash[sym],
                            new QTableWidgetItem(QString::number(target)));
                    }
                    else {
                        LALR1TableItem ti;
                        ti.kind = 1;
                        ti.idx = target;
                        LALR1_table[i].insert(sym, ti);
                        ui->lalrAnalysisTableWidget->setItem(i, headerHash[sym],
                            new QTableWidgetItem("s" + QString::number(target)));
                    }
                }
            }

            ui->lalrAnalysisTableWidget->resizeColumnsToContents();
        }

        canSentence = true;

        //QMessageBox::information(this, "提示", "文法分析完成!");
        toastManager->showToast("提示：提示文法分析完成!", true);
    });

    // ============ 句子分析 - 按照图片3的样式：分析栈（状态+符号交替）和符号栈（剩余输入） ============
    connect(ui->sentenceButton, &QPushButton::clicked, this, [this]() {
        if (!canSentence) {
            //QMessageBox::warning(this, "警告", "请先进行文法分析!");
            toastManager->showToast("警告：请先进行文法分析!", false);
            return;
        }

        ui->resultTableWidget->clearContents();
        ui->resultTableWidget->setRowCount(0);
        ui->resultTableWidget->setColumnCount(2);
        ui->resultTableWidget->setHorizontalHeaderLabels(
            QStringList() << "分析栈" << "符号栈");
        ui->resultTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->resultTableWidget->horizontalHeader()->setStretchLastSection(true);

        QString sentence = ui->sentenceEdit->text().trimmed();
        QString originalStr = sentence;
        if (sentence.isEmpty()) {
            //QMessageBox::warning(this, "警告", "请输入待分析的句子!");
            toastManager->showToast("警告：请输入待分析的句子!", false);
            return;
        }

        QStringList tokens;
        for (int i = 0; i < sentence.size(); i++) {
            tokens.append(QString(sentence[i]));
        }
        tokens.append("$");

        int tokenIdx = 0;
        QStack<AnalysisItem> analysisStack;
        AnalysisItem firstItem;
        firstItem.kind = 1;
        firstItem.state = 0;
        analysisStack.push(firstItem);
        int step = 0;

        // 打印函数 - 按图片3格式：行号在垂直表头，分析栈=状态+符号交替，符号栈=剩余输入
        auto printStep = [&]() {
            QString analysisString;
            for (const auto& ai : analysisStack) {
                if (ai.kind == 1) {
                    analysisString += QString::number(ai.state) + " ";
                }
                else {
                    analysisString += ai.ch + " ";
                }
            }
            QString remaining;
            for (int i = tokenIdx; i < tokens.size(); i++) {
                remaining += tokens[i];
            }

            int r = ui->resultTableWidget->rowCount();
            ui->resultTableWidget->setRowCount(r + 1);
            ui->resultTableWidget->setVerticalHeaderItem(r, new QTableWidgetItem(QString::number(step + 1)));
            ui->resultTableWidget->setItem(r, 0, new QTableWidgetItem(analysisString.trimmed()));
            ui->resultTableWidget->setItem(r, 1, new QTableWidgetItem(remaining));
            step++;
            };

        printStep();

        while (true) {
            if (analysisStack.empty()) {
                //QMessageBox::warning(this, "错误", "分析栈为空，句子 \"" + originalStr + "\" 不属于该文法");
                toastManager->showToast("错误: 分析栈为空，句子 \"" + originalStr + "\" 不属于该文法!", false);
                return;
            }
            AnalysisItem top = analysisStack.top();
            if (top.kind != 1) {
                //QMessageBox::warning(this, "错误", "分析栈顶不是状态，句子 \"" + originalStr + "\" 不属于该文法");
                toastManager->showToast("错误: 分析栈顶不是状态，句子 \"" + originalStr + "\" 不属于该文法!", false);
                return;
            }

            QString currentToken = (tokenIdx < tokens.size()) ? tokens[tokenIdx] : "$";

            if (!LALR1_table[top.state].contains(currentToken)) {
                if (LALR1_table[top.state].contains("#")) {
                    currentToken = "#";
                }
                else {
                    //QMessageBox::warning(this, "错误", "分析表中无对应项 [" + QString::number(top.state) + ", " + currentToken + "],\n句子 \"" + originalStr + "\" 不属于该文法");
                    toastManager->showToast("错误: 分析表中无对应项 [" + QString::number(top.state) + ", " + currentToken + "],\n句子 \"" + originalStr + "\" 不属于该文法!", false);
                    return;
                }
            }

            LALR1TableItem action = LALR1_table[top.state][currentToken];

            if (action.kind == 1) {
                AnalysisItem chItem;
                chItem.kind = 2;
                chItem.ch = tokens[tokenIdx];
                tokenIdx++;

                AnalysisItem stItem;
                stItem.kind = 1;
                stItem.state = action.idx;

                analysisStack.push(chItem);
                analysisStack.push(stItem);
                printStep();

            }
            else if (action.kind == 2) {
                Item ruleItem = recursion[action.idx];
                int popCount = ruleItem.pos * 2;
                if (ruleItem.rule.size() == 1 && ruleItem.rule[0] == "#") {
                    popCount = 0;
                }

                for (int i = 0; i < popCount; i++) {
                    if (analysisStack.empty()) {
                        //QMessageBox::warning(this, "错误", "规约时栈为空，句子不匹配");
                        toastManager->showToast("错误: 规约时栈为空，句子不匹配!", false);
                        return;
                    }
                    analysisStack.pop();
                }

                if (analysisStack.empty()) {
                    //QMessageBox::warning(this, "错误", "规约后栈为空，句子不匹配");
                    toastManager->showToast("错误: 规约时栈为空，句子不匹配!", false);
                    return;
                }

                AnalysisItem topAfterPop = analysisStack.top();
                if (topAfterPop.kind != 1) {
                    //QMessageBox::warning(this, "错误", "规约后栈顶不是状态");
                    toastManager->showToast("错误: 规约后栈顶不是状态!", false);
                    return;
                }

                if (!LALR1_table[topAfterPop.state].contains(ruleItem.name)) {
                    //QMessageBox::warning(this, "错误, ", "GOTO表中无 [" + QString::number(topAfterPop.state) + ", " + ruleItem.name + "]");
                    toastManager->showToast("错误: GOTO表中无 [" + QString::number(topAfterPop.state) + ", " + ruleItem.name + "]!", false);
                    return;
                }

                AnalysisItem chItem;
                chItem.kind = 2;
                chItem.ch = ruleItem.name;

                AnalysisItem stItem;
                stItem.kind = 1;
                stItem.state = LALR1_table[topAfterPop.state][ruleItem.name].idx;

                analysisStack.push(chItem);
                analysisStack.push(stItem);
                printStep();

            }
            else if (action.kind == 4) {
                ui->resultTableWidget->resizeColumnsToContents();
                //QMessageBox::information(this, "提醒", "分析完毕，" + originalStr + "属于该文法的句子");
                toastManager->showToast("分析完毕，" + originalStr + " 属于该文法的句子!", true);
                return;

            }
            else {
                //QMessageBox::warning(this, "错误", "遇到未知动作，句子不匹配");
                toastManager->showToast("错误: 遇到未知动作，句子不匹配!", false);
                return;
            }

            if (step > 10000) {
                //QMessageBox::warning(this, "错误", "分析步骤超过10000步，可能存在死循环");
                toastManager->showToast("错误: 分析步骤超过10000步，可能存在死循环!", false);
                return;
            }
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ====================== 判断 SLR(1) ======================
QString MainWindow::checkSLR1(const LALR& lr0dfa)
{
    QHash<int, LR0State> revLR0;
    for (auto it = lr0dfa.lr0StateHash.constBegin(); it != lr0dfa.lr0StateHash.constEnd(); ++it) {
        revLR0[it.value()] = it.key();
    }

    QStringList conflicts;

    for (int i = 0; i < lr0dfa.lr0size; i++) {
        if (!revLR0.contains(i)) continue;
        const LR0State& state = revLR0[i];

        QList<LR0Item> reduceItems;
        QSet<QString> shiftSymbols;

        for (const auto& item : state.st) {
            bool isReduce = (item.pos == item.rule.size()) ||
                (item.rule.size() == 1 && item.rule[0] == "#" && item.pos == 0);
            if (isReduce) {
                reduceItems.append(item);
            }
            else {
                shiftSymbols.insert(item.rule[item.pos]);
            }
        }

        if (reduceItems.size() > 1) {
            for (int a = 0; a < reduceItems.size() - 1; a++) {
                for (int b = a + 1; b < reduceItems.size(); b++) {
                    QSet<QString> followA = followSet.value(reduceItems[a].name);
                    QSet<QString> followB = followSet.value(reduceItems[b].name);
                    QSet<QString> intersection = followA;
                    intersection.intersect(followB);
                    if (!intersection.isEmpty()) {
                        conflicts.append(QString("状态I%1: 规约-规约冲突 (%2 与 %3 的FOLLOW集合有交集: {%4})")
                            .arg(i).arg(reduceItems[a].name).arg(reduceItems[b].name)
                            .arg(intersection.values().join(", ")));
                    }
                }
            }
        }

        for (const auto& rItem : reduceItems) {
            QSet<QString> followR = followSet.value(rItem.name);
            QSet<QString> intersection = shiftSymbols;
            intersection.intersect(followR);
            if (!intersection.isEmpty()) {
                conflicts.append(QString("状态I%1: 移进-规约冲突 (%2 的FOLLOW集合 与 移进符号有交集: {%3})")
                    .arg(i).arg(rItem.name).arg(intersection.values().join(", ")));
            }
        }
    }

    if (conflicts.isEmpty()) {
        return "<b style='color:green;'>该文法是 SLR(1) 文法</b>";
    }
    else {
        QString result = "<b style='color:red;'>该文法不是 SLR(1) 文法</b><br>原因:<br>";
        for (const auto& c : conflicts) {
            result += "• " + c + "<br>";
        }
        return result;
    }
}