#ifndef LALR_H
#define LALR_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QList>

// ==================== 单条 LR 项目 ====================
struct Item {
    QString name;       // 产生式左侧非终结符
    QStringList rule;   // 产生式右侧符号列表
    QSet<QString> next; // 向前看符号集合
    int pos;            // 圆点位置

    static bool haveSameCore(const Item& i, const Item& j);

    friend bool operator==(const Item& a, const Item& b) {
        return a.name == b.name && a.rule == b.rule && a.next == b.next && a.pos == b.pos;
    }

    friend bool operator<(const Item& a, const Item& b) {
        if (a.name != b.name) return a.name < b.name;
        if (a.rule != b.rule) return a.rule < b.rule;
        if (a.pos != b.pos) return a.pos < b.pos;
        // next 不参与排序核心比较，但为了全序加上
        auto aList = a.next.values();
        auto bList = b.next.values();
        std::sort(aList.begin(), aList.end());
        std::sort(bList.begin(), bList.end());
        return aList < bList;
    }
};

size_t qHash(const Item& key, size_t seed = 0);

// ==================== 单个状态 ====================
struct State {
    QSet<Item> st;

    static State closure(State I,
        const QHash<QString, QSet<QStringList>>& grammars,
        const QVector<QString>& nonFinalizers,
        QHash<QString, QSet<QString>> firstSet);

    static State change(const State& I, const QString& X,
        const QHash<QString, QSet<QStringList>>& grammars,
        const QVector<QString>& nonFinalizers,
        QHash<QString, QSet<QString>> firstSet);

    static bool haveSameCore(const State& i, const State& j);

    friend bool operator==(const State& a, const State& b) {
        return a.st == b.st;
    }
};

size_t qHash(const State& key, size_t seed = 0);

// ==================== LR(0) 项目（无向前看符号） ====================
struct LR0Item {
    QString name;
    QStringList rule;
    int pos;

    friend bool operator==(const LR0Item& a, const LR0Item& b) {
        return a.name == b.name && a.rule == b.rule && a.pos == b.pos;
    }
    friend bool operator<(const LR0Item& a, const LR0Item& b) {
        if (a.name != b.name) return a.name < b.name;
        if (a.rule != b.rule) return a.rule < b.rule;
        return a.pos < b.pos;
    }
};

size_t qHash(const LR0Item& key, size_t seed = 0);

struct LR0State {
    QSet<LR0Item> st;

    static LR0State closure(LR0State I,
        const QHash<QString, QSet<QStringList>>& grammars,
        const QVector<QString>& nonFinalizers);

    static LR0State change(const LR0State& I, const QString& X,
        const QHash<QString, QSet<QStringList>>& grammars,
        const QVector<QString>& nonFinalizers);

    friend bool operator==(const LR0State& a, const LR0State& b) {
        return a.st == b.st;
    }
};

size_t qHash(const LR0State& key, size_t seed = 0);

// ==================== 分析表结点 ====================
struct LALR1TableItem {
    int kind;   // 1=移进, 2=规约, 3=非终结符GOTO, 4=接受
    int idx;
};

// ==================== 分析过程栈元素 ====================
struct AnalysisItem {
    int kind;       // 1=状态, 2=符号
    int state;
    QString ch;
};

// ==================== LALR 主类 ====================
class LALR
{
public:
    LALR();

    int size;
    QHash<State, int> stateHash;
    QHash<int, QHash<QString, int>> changeHash;

    // LR(0) DFA
    int lr0size;
    QHash<LR0State, int> lr0StateHash;
    QHash<int, QHash<QString, int>> lr0ChangeHash;

    // 构建 LR(0)
    void buildLR0(LR0State startState,
        const QHash<QString, QSet<QStringList>>& grammars,
        const QVector<QString>& nonFinalizers);

    // 构建 LR(1)
    void buildLR1(State faState,
        const QHash<QString, QSet<QStringList>>& grammars,
        const QVector<QString>& nonFinalizers,
        QHash<QString, QSet<QString>> firstSet,
        const QHash<QString, QSet<QString>>& followSet);

    // 构建 LALR(1) — 从 LR(1) 合并同心项
    void buildLALR1(const LALR& lr1);
};

#endif // LALR_H