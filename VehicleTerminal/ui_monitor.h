/********************************************************************************
** Form generated from reading UI file 'monitor.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MONITOR_H
#define UI_MONITOR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Monitor
{
public:
    QMenuBar *menubar;
    QWidget *centralwidget;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *Monitor)
    {
        if (Monitor->objectName().isEmpty())
            Monitor->setObjectName(QString::fromUtf8("Monitor"));
        Monitor->resize(800, 600);
        menubar = new QMenuBar(Monitor);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        Monitor->setMenuBar(menubar);
        centralwidget = new QWidget(Monitor);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        Monitor->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(Monitor);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        Monitor->setStatusBar(statusbar);

        retranslateUi(Monitor);

        QMetaObject::connectSlotsByName(Monitor);
    } // setupUi

    void retranslateUi(QMainWindow *Monitor)
    {
        Monitor->setWindowTitle(QApplication::translate("Monitor", "MainWindow", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Monitor: public Ui_Monitor {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MONITOR_H
