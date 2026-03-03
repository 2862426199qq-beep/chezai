/********************************************************************************
** Form generated from reading UI file 'baidumap.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_BAIDUMAP_H
#define UI_BAIDUMAP_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_BaiduMap
{
public:
    QWidget *widget;
    QPushButton *pBtn_ZoomDown;
    QPushButton *pBtn_ZoomUp;
    QPushButton *pBtn_Close;

    void setupUi(QMainWindow *BaiduMap)
    {
        if (BaiduMap->objectName().isEmpty())
            BaiduMap->setObjectName(QString::fromUtf8("BaiduMap"));
        BaiduMap->resize(1149, 728);
        widget = new QWidget(BaiduMap);
        widget->setObjectName(QString::fromUtf8("widget"));
        pBtn_ZoomDown = new QPushButton(widget);
        pBtn_ZoomDown->setObjectName(QString::fromUtf8("pBtn_ZoomDown"));
        pBtn_ZoomDown->setGeometry(QRect(860, 560, 100, 100));
        pBtn_ZoomDown->setMinimumSize(QSize(100, 100));
        pBtn_ZoomDown->setMaximumSize(QSize(100, 100));
        pBtn_ZoomDown->setStyleSheet(QString::fromUtf8("border-image: url(:/map/Map/img/zoom_down.png);"));
        pBtn_ZoomUp = new QPushButton(widget);
        pBtn_ZoomUp->setObjectName(QString::fromUtf8("pBtn_ZoomUp"));
        pBtn_ZoomUp->setGeometry(QRect(580, 510, 100, 100));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(pBtn_ZoomUp->sizePolicy().hasHeightForWidth());
        pBtn_ZoomUp->setSizePolicy(sizePolicy);
        pBtn_ZoomUp->setMinimumSize(QSize(100, 100));
        pBtn_ZoomUp->setMaximumSize(QSize(100, 100));
        pBtn_ZoomUp->setStyleSheet(QString::fromUtf8("border-image: url(:/map/Map/img/zoom_up.png);"));
        pBtn_Close = new QPushButton(widget);
        pBtn_Close->setObjectName(QString::fromUtf8("pBtn_Close"));
        pBtn_Close->setGeometry(QRect(890, 170, 101, 81));
        pBtn_Close->setStyleSheet(QString::fromUtf8("background-color: rgb(114, 159, 207);"));
        BaiduMap->setCentralWidget(widget);

        retranslateUi(BaiduMap);

        QMetaObject::connectSlotsByName(BaiduMap);
    } // setupUi

    void retranslateUi(QMainWindow *BaiduMap)
    {
        BaiduMap->setWindowTitle(QApplication::translate("BaiduMap", "MainWindow", nullptr));
        pBtn_ZoomDown->setText(QString());
        pBtn_ZoomUp->setText(QString());
        pBtn_Close->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class BaiduMap: public Ui_BaiduMap {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_BAIDUMAP_H
