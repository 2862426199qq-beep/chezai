/********************************************************************************
** Form generated from reading UI file 'clock.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CLOCK_H
#define UI_CLOCK_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Clock
{
public:

    void setupUi(QWidget *Clock)
    {
        if (Clock->objectName().isEmpty())
            Clock->setObjectName(QString::fromUtf8("Clock"));
        Clock->resize(400, 300);

        retranslateUi(Clock);

        QMetaObject::connectSlotsByName(Clock);
    } // setupUi

    void retranslateUi(QWidget *Clock)
    {
        Clock->setWindowTitle(QApplication::translate("Clock", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Clock: public Ui_Clock {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CLOCK_H
