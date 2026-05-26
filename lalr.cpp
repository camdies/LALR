#include "lalr.h"
#include <QDebug>
#include <algorithm>
#include <functional>

LALR::LALR() : size(0), lr0size(0) {}

// ===================== qHash 函数 =====================

size_t qHash(const Item& key, size_t seed) {
    size_t h = qHash(key.name, seed);
    for (const auto& s : key.rule)
        h ^= qHash(s, seed);
    for (const auto& s : key.next)
        h ^= qHash(s, seed);
    h ^= qHash(key.pos, seed);
    return h;
}

size_t qHash(const State& key, size_t seed) {
    size_t h = 0;
    for (const auto& item : key.st)
        h ^= qHash(item, seed);
    return h;
}

size_t qHash(const LR0Item& key, size_t seed) {
    size_t h = qHash(key.name, seed);
    for (const auto& s : key.rule)
        h ^= qHash(s, seed);
    h ^= qHash(key.pos, seed);
    return h;
}

size_t qHash(const LR0State& key, size_t seed) {
    size_t h = 0;
    for (const auto& item : key.st)
        h ^= qHash(item, seed);
    return h;
}

// ===================== Item =====================

bool Item::haveSameCore(const Item& i, const Item& j) {
    return i.name == j.name && i.rule == j.rule && i.pos == j.pos;
}

// ===================== State — closure =====================

State State::closure(State I,
    const QHash<QString, QSet<QStringList>>& grammars,
    const QVector<QString>& nonFinalizers,
    QHash<QString, QSet<QString>> firstSet)
{
    State J = I;
    while (true) {
        State oldJ = J;
        for (const Item& item : QSet<Item>(J.st)) {
            if (item.pos < item.rule.size() && nonFinalizers.contains(item.rule[item.pos])) {
                QString B = item.rule[item.pos];
                if (!grammars.contains(B)) continue;
                for (const auto& grammar : grammars[B]) {
                    for (const auto& nextItem : item.next) {
                        QStringList betaRule = item.rule.mid(item.pos + 1);
                        if (nextItem != "#") betaRule.push_back(nextItem);

                        // 计算 FIRST(beta)
                        int k = 0;
                        QSet<QString> tmpSet;
                        while (k < betaRule.size()) {
                            bool hasEpsilon = false;
                            if (firstSet[betaRule[k]].contains("#")) hasEpsilon = true;
                            QSet<QString> fs = firstSet[betaRule[k]];
                            fs.remove("#");
                            tmpSet.unite(fs);
                            if (!hasEpsilon) break;
                            k++;
                        }
                        if (k == betaRule.size()) tmpSet.insert("#");

                        for (const auto& b : tmpSet) {
                            Item newItem;
                            newItem.name = B;
                            newItem.rule = grammar;
                            newItem.next.insert(b);
                            newItem.pos = 0;

                            // 查找同心项合并
                            bool found = false;
                            for (auto it = J.st.begin(); it != J.st.end(); ++it) {
                                if (Item::haveSameCore(*it, newItem)) {
                                    if (!it->next.contains(b)) {
                                        Item merged = *it;
                                        J.st.remove(*it);
                                        merged.next.insert(b);
                                        J.st.insert(merged);
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                J.st.insert(newItem);
                            }
                        }
                    }
                }
            }
        }
        if (oldJ == J) break;
    }
    return J;
}

// ===================== State — change (GOTO) =====================

State State::change(const State& I, const QString& X,
    const QHash<QString, QSet<QStringList>>& grammars,
    const QVector<QString>& nonFinalizers,
    QHash<QString, QSet<QString>> firstSet)
{
    State I_new;
    for (const auto& item : I.st) {
        if (item.pos >= item.rule.size() || item.rule[item.pos] != X) continue;

        Item newItem;
        newItem.name = item.name;
        newItem.rule = item.rule;
        newItem.next = item.next;
        newItem.pos = item.pos + 1;

        bool found = false;
        for (auto it = I_new.st.begin(); it != I_new.st.end(); ++it) {
            if (Item::haveSameCore(*it, newItem)) {
                Item merged = *it;
                I_new.st.remove(*it);
                merged.next.unite(newItem.next);
                I_new.st.insert(merged);
                found = true;
                break;
            }
        }
        if (!found) {
            I_new.st.insert(newItem);
        }
    }
    if (I_new.st.empty()) return I_new;
    return closure(I_new, grammars, nonFinalizers, firstSet);
}

// ===================== State — haveSameCore =====================

bool State::haveSameCore(const State& i, const State& j) {
    if (i.st.size() != j.st.size()) return false;
    auto iList = i.st.values();
    auto jList = j.st.values();
    std::sort(iList.begin(), iList.end());
    std::sort(jList.begin(), jList.end());
    for (int cnt = 0; cnt < iList.size(); cnt++) {
        if (!Item::haveSameCore(iList[cnt], jList[cnt])) {
            return false;
        }
    }
    return true;
}

// ===================== LR0State — closure =====================

LR0State LR0State::closure(LR0State I,
    const QHash<QString, QSet<QStringList>>& grammars,
    const QVector<QString>& nonFinalizers)
{
    LR0State J = I;
    while (true) {
        LR0State oldJ = J;
        for (const LR0Item& item : QSet<LR0Item>(J.st)) {
            if (item.pos < item.rule.size() && nonFinalizers.contains(item.rule[item.pos])) {
                QString B = item.rule[item.pos];
                if (!grammars.contains(B)) continue;
                for (const auto& grammar : grammars[B]) {
                    LR0Item newItem;
                    newItem.name = B;
                    newItem.rule = grammar;
                    newItem.pos = 0;
                    J.st.insert(newItem);
                }
            }
        }
        if (oldJ == J) break;
    }
    return J;
}

// ===================== LR0State — change =====================

LR0State LR0State::change(const LR0State& I, const QString& X,
    const QHash<QString, QSet<QStringList>>& grammars,
    const QVector<QString>& nonFinalizers)
{
    LR0State I_new;
    for (const auto& item : I.st) {
        if (item.pos >= item.rule.size() || item.rule[item.pos] != X) continue;
        LR0Item newItem;
        newItem.name = item.name;
        newItem.rule = item.rule;
        newItem.pos = item.pos + 1;
        I_new.st.insert(newItem);
    }
    if (I_new.st.empty()) return I_new;
    return closure(I_new, grammars, nonFinalizers);
}

// ===================== buildLR0 =====================

void LALR::buildLR0(LR0State startState,
    const QHash<QString, QSet<QStringList>>& grammars,
    const QVector<QString>& nonFinalizers)
{
    lr0StateHash.clear();
    lr0ChangeHash.clear();
    lr0size = 0;

    lr0StateHash[startState] = lr0size++;

    std::function<void(const LR0State&)> dfs = [&](const LR0State& faState) {
        int faStateId = lr0StateHash[faState];
        QSet<QString> changeMethods;
        for (const auto& item : faState.st) {
            if (item.pos < item.rule.size()) {
                changeMethods.insert(item.rule[item.pos]);
            }
        }
        for (const QString& method : changeMethods) {
            LR0State sonState = LR0State::change(faState, method, grammars, nonFinalizers);
            if (sonState.st.empty()) continue;
            if (lr0StateHash.contains(sonState)) {
                lr0ChangeHash[faStateId].insert(method, lr0StateHash[sonState]);
            }
            else {
                lr0StateHash[sonState] = lr0size++;
                lr0ChangeHash[faStateId].insert(method, lr0StateHash[sonState]);
                dfs(sonState);
            }
        }
        };
    dfs(startState);
}

// ===================== buildLR1 =====================

void LALR::buildLR1(State faState,
    const QHash<QString, QSet<QStringList>>& grammars,
    const QVector<QString>& nonFinalizers,
    QHash<QString, QSet<QString>> firstSet,
    const QHash<QString, QSet<QString>>& followSet)
{
    int faStateId = stateHash[faState];

    QSet<QString> changeMethods;
    for (const auto& faItem : faState.st) {
        if (faItem.pos < faItem.rule.size()) {
            changeMethods.insert(faItem.rule[faItem.pos]);
        }
    }

    for (const QString& changeMethod : changeMethods) {
        State sonState = State::change(faState, changeMethod, grammars, nonFinalizers, firstSet);
        if (sonState.st.empty()) continue;
        if (stateHash.contains(sonState)) {
            int sonStateId = stateHash[sonState];
            changeHash[faStateId].insert(changeMethod, sonStateId);
        }
        else {
            stateHash[sonState] = size++;
            changeHash[faStateId].insert(changeMethod, stateHash[sonState]);
            buildLR1(sonState, grammars, nonFinalizers, firstSet, followSet);
        }
    }
}

// ===================== buildLALR1 =====================

void LALR::buildLALR1(const LALR& lr1)
{
    stateHash.clear();
    changeHash.clear();
    size = 0;

    // 并查集
    QVector<int> p(lr1.size);
    for (int i = 0; i < lr1.size; i++) p[i] = i;

    std::function<int(int)> find = [&](int x) -> int {
        if (p[x] != x) p[x] = find(p[x]);
        return p[x];
        };

    QHash<int, State> revlr1StateHash;
    for (auto it = lr1.stateHash.constBegin(); it != lr1.stateHash.constEnd(); ++it) {
        revlr1StateHash[it.value()] = it.key();
    }

    // 合并同心项
    for (int i = 0; i < lr1.size - 1; i++) {
        for (int j = i + 1; j < lr1.size; j++) {
            if (State::haveSameCore(revlr1StateHash[i], revlr1StateHash[j])) {
                p[find(j)] = find(i);
            }
        }
    }

    // 映射
    QHash<int, int> cntChangeSet;
    int idx = 0;
    for (int i = 0; i < lr1.size; i++) {
        if (!cntChangeSet.contains(find(i))) {
            cntChangeSet[find(i)] = idx++;
        }
    }
    size = idx;

    // 建立 LALR 状态集
    QHash<int, State> revlalr1StateHash;
    for (int i = 0; i < lr1.size; i++) {
        int lalrId = cntChangeSet[find(i)];
        if (!revlalr1StateHash.contains(lalrId)) {
            revlalr1StateHash[lalrId] = revlr1StateHash[i];
        }
        else {
            // 合并向前看符号
            State& existing = revlalr1StateHash[lalrId];
            for (const auto& item : revlr1StateHash[i].st) {
                bool merged = false;
                QSet<Item> newSt;
                for (auto lalrItem : existing.st) {
                    if (Item::haveSameCore(item, lalrItem)) {
                        lalrItem.next.unite(item.next);
                        merged = true;
                    }
                    newSt.insert(lalrItem);
                }
                if (!merged) {
                    newSt.insert(item);
                }
                existing.st = newSt;
            }
        }
    }

    for (auto it = revlalr1StateHash.constBegin(); it != revlalr1StateHash.constEnd(); ++it) {
        stateHash[it.value()] = it.key();
    }

    // 建立 LALR 转移集
    for (int i = 0; i < lr1.size; i++) {
        if (!lr1.changeHash.contains(i)) continue;
        for (auto it = lr1.changeHash[i].constBegin(); it != lr1.changeHash[i].constEnd(); ++it) {
            changeHash[cntChangeSet[find(i)]].insert(it.key(), cntChangeSet[find(it.value())]);
        }
    }
}