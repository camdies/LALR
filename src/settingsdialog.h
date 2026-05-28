#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDoubleValidator>
#include <QIntValidator>

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(double currentDuration, int currentMaxCount, QWidget* parent = nullptr);

    double getDuration() const;
    int getMaxCount() const;

private:
    QLineEdit* durationEdit;
    QLineEdit* maxCountEdit;
};

#endif // SETTINGSDIALOG_H