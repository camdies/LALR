#include "settingsdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <cmath>

SettingsDialog::SettingsDialog(double currentDuration, int currentMaxCount, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("通知设置");
    setFixedSize(320, 180);

    QFormLayout* formLayout = new QFormLayout;

    durationEdit = new QLineEdit(QString::number(currentDuration, 'f', 1), this);
    // 只接受数字（浮点数或整数），不满足时自动向下取整到合法值
    durationEdit->setValidator(new QDoubleValidator(0.5, 60.0, 1, this));
    durationEdit->setPlaceholderText("秒，支持小数（如 3.0）");

    maxCountEdit = new QLineEdit(QString::number(currentMaxCount), this);
    maxCountEdit->setValidator(new QIntValidator(1, 20, this));
    maxCountEdit->setPlaceholderText("整数（如 5）");

    formLayout->addRow("消息存在时间（秒）：", durationEdit);
    formLayout->addRow("最多消息数量：", maxCountEdit);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
}

double SettingsDialog::getDuration() const
{
    bool ok;
    double val = durationEdit->text().toDouble(&ok);
    if (!ok || val < 0.5) return 0.5;
    return std::floor(val * 10.0) / 10.0;  // 向下取到一位小数
}

int SettingsDialog::getMaxCount() const
{
    bool ok;
    int val = maxCountEdit->text().toInt(&ok);
    if (!ok || val < 1) return 1;
    return val;  // 整数，QIntValidator 已保证输入合法
}