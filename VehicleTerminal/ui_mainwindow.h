/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout_3;
    QStackedWidget *stackedWidget;
    QWidget *page;
    QGridLayout *gridLayout;
    QPushButton *pBtn_Weather;
    QPushButton *pBtn_Music;
    QPushButton *pBtn_Map;
    QPushButton *pBtn_Monitor;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout;
    QLabel *label_Time;
    QLabel *label_Date;
    QHBoxLayout *horizontalLayout;
    QLabel *label_4;
    QLabel *label_temp;
    QLabel *label_2;
    QLabel *label_humi;
    QWidget *clock_Widget;
    QWidget *page_2;
    QGridLayout *gridLayout_2;
    QPushButton *pushButton_5;
    QPushButton *pushButton_2;
    QSpacerItem *horizontalSpacer;
    QPushButton *pushButton_3;
    QPushButton *pushButton_4;
    QPushButton *pushButton_6;
    QPushButton *pBtn_Setting;
    QSpacerItem *horizontalSpacer_2;
    QSpacerItem *verticalSpacer;
    QSpacerItem *verticalSpacer_2;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1024, 600);
        MainWindow->setMinimumSize(QSize(1024, 600));
        MainWindow->setMaximumSize(QSize(1024, 600));
        MainWindow->setStyleSheet(QString::fromUtf8("background-color: rgb(81, 81, 81);"));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout_3 = new QHBoxLayout(centralwidget);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        stackedWidget = new QStackedWidget(centralwidget);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(stackedWidget->sizePolicy().hasHeightForWidth());
        stackedWidget->setSizePolicy(sizePolicy);
        page = new QWidget();
        page->setObjectName(QString::fromUtf8("page"));
        gridLayout = new QGridLayout(page);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        pBtn_Weather = new QPushButton(page);
        pBtn_Weather->setObjectName(QString::fromUtf8("pBtn_Weather"));
        sizePolicy.setHeightForWidth(pBtn_Weather->sizePolicy().hasHeightForWidth());
        pBtn_Weather->setSizePolicy(sizePolicy);
        pBtn_Weather->setStyleSheet(QString::fromUtf8("border-image: url(:/img/weather.jpg);"));

        gridLayout->addWidget(pBtn_Weather, 1, 1, 1, 1);

        pBtn_Music = new QPushButton(page);
        pBtn_Music->setObjectName(QString::fromUtf8("pBtn_Music"));
        sizePolicy.setHeightForWidth(pBtn_Music->sizePolicy().hasHeightForWidth());
        pBtn_Music->setSizePolicy(sizePolicy);
        pBtn_Music->setStyleSheet(QString::fromUtf8("border-image: url(:/img/music.png);"));

        gridLayout->addWidget(pBtn_Music, 0, 3, 1, 1);

        pBtn_Map = new QPushButton(page);
        pBtn_Map->setObjectName(QString::fromUtf8("pBtn_Map"));
        sizePolicy.setHeightForWidth(pBtn_Map->sizePolicy().hasHeightForWidth());
        pBtn_Map->setSizePolicy(sizePolicy);
        pBtn_Map->setStyleSheet(QString::fromUtf8("border-image: url(:/img/map.jpg);"));

        gridLayout->addWidget(pBtn_Map, 1, 3, 1, 1);

        pBtn_Monitor = new QPushButton(page);
        pBtn_Monitor->setObjectName(QString::fromUtf8("pBtn_Monitor"));
        sizePolicy.setHeightForWidth(pBtn_Monitor->sizePolicy().hasHeightForWidth());
        pBtn_Monitor->setSizePolicy(sizePolicy);
        pBtn_Monitor->setStyleSheet(QString::fromUtf8("border-image: url(:/img/monitor.png);"));

        gridLayout->addWidget(pBtn_Monitor, 1, 2, 1, 1);

        groupBox = new QGroupBox(page);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        sizePolicy.setHeightForWidth(groupBox->sizePolicy().hasHeightForWidth());
        groupBox->setSizePolicy(sizePolicy);
        groupBox->setMaximumSize(QSize(350, 16777215));
        groupBox->setStyleSheet(QString::fromUtf8("background-color: rgb(93, 93, 140);\n"
"border-color: rgb(255, 0, 0);"));
        verticalLayout = new QVBoxLayout(groupBox);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label_Time = new QLabel(groupBox);
        label_Time->setObjectName(QString::fromUtf8("label_Time"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label_Time->sizePolicy().hasHeightForWidth());
        label_Time->setSizePolicy(sizePolicy1);
        QFont font;
        font.setPointSize(30);
        font.setBold(true);
        font.setItalic(false);
        font.setWeight(75);
        label_Time->setFont(font);
        label_Time->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_Time);

        label_Date = new QLabel(groupBox);
        label_Date->setObjectName(QString::fromUtf8("label_Date"));
        sizePolicy1.setHeightForWidth(label_Date->sizePolicy().hasHeightForWidth());
        label_Date->setSizePolicy(sizePolicy1);
        QFont font1;
        font1.setPointSize(20);
        font1.setBold(true);
        font1.setItalic(true);
        font1.setWeight(75);
        label_Date->setFont(font1);
        label_Date->setStyleSheet(QString::fromUtf8("color: rgb(255, 170, 0);"));
        label_Date->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_Date);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label_4 = new QLabel(groupBox);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(label_4->sizePolicy().hasHeightForWidth());
        label_4->setSizePolicy(sizePolicy2);
        label_4->setMaximumSize(QSize(50, 50));
        label_4->setPixmap(QPixmap(QString::fromUtf8(":/img/temperature.png")));
        label_4->setScaledContents(true);
        label_4->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(label_4);

        label_temp = new QLabel(groupBox);
        label_temp->setObjectName(QString::fromUtf8("label_temp"));
        QSizePolicy sizePolicy3(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(label_temp->sizePolicy().hasHeightForWidth());
        label_temp->setSizePolicy(sizePolicy3);
        QFont font2;
        font2.setPointSize(20);
        font2.setBold(true);
        font2.setWeight(75);
        label_temp->setFont(font2);
        label_temp->setStyleSheet(QString::fromUtf8("color: rgb(255, 0, 0);"));
        label_temp->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(label_temp);

        label_2 = new QLabel(groupBox);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        sizePolicy2.setHeightForWidth(label_2->sizePolicy().hasHeightForWidth());
        label_2->setSizePolicy(sizePolicy2);
        label_2->setMaximumSize(QSize(50, 50));
        label_2->setPixmap(QPixmap(QString::fromUtf8(":/img/humidity.png")));
        label_2->setScaledContents(true);
        label_2->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(label_2);

        label_humi = new QLabel(groupBox);
        label_humi->setObjectName(QString::fromUtf8("label_humi"));
        sizePolicy3.setHeightForWidth(label_humi->sizePolicy().hasHeightForWidth());
        label_humi->setSizePolicy(sizePolicy3);
        label_humi->setFont(font2);
        label_humi->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 127);"));
        label_humi->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(label_humi);


        verticalLayout->addLayout(horizontalLayout);


        gridLayout->addWidget(groupBox, 0, 2, 1, 1);

        clock_Widget = new QWidget(page);
        clock_Widget->setObjectName(QString::fromUtf8("clock_Widget"));
        sizePolicy.setHeightForWidth(clock_Widget->sizePolicy().hasHeightForWidth());
        clock_Widget->setSizePolicy(sizePolicy);

        gridLayout->addWidget(clock_Widget, 0, 1, 1, 1);

        stackedWidget->addWidget(page);
        page_2 = new QWidget();
        page_2->setObjectName(QString::fromUtf8("page_2"));
        gridLayout_2 = new QGridLayout(page_2);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        pushButton_5 = new QPushButton(page_2);
        pushButton_5->setObjectName(QString::fromUtf8("pushButton_5"));
        sizePolicy.setHeightForWidth(pushButton_5->sizePolicy().hasHeightForWidth());
        pushButton_5->setSizePolicy(sizePolicy);
        pushButton_5->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 85, 255);"));

        gridLayout_2->addWidget(pushButton_5, 2, 2, 1, 1);

        pushButton_2 = new QPushButton(page_2);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));
        sizePolicy.setHeightForWidth(pushButton_2->sizePolicy().hasHeightForWidth());
        pushButton_2->setSizePolicy(sizePolicy);
        pushButton_2->setStyleSheet(QString::fromUtf8("background-color: rgb(85, 255, 127);"));

        gridLayout_2->addWidget(pushButton_2, 0, 2, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Minimum);

        gridLayout_2->addItem(horizontalSpacer, 0, 1, 1, 1);

        pushButton_3 = new QPushButton(page_2);
        pushButton_3->setObjectName(QString::fromUtf8("pushButton_3"));
        sizePolicy.setHeightForWidth(pushButton_3->sizePolicy().hasHeightForWidth());
        pushButton_3->setSizePolicy(sizePolicy);
        pushButton_3->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 170, 0);"));

        gridLayout_2->addWidget(pushButton_3, 0, 4, 1, 1);

        pushButton_4 = new QPushButton(page_2);
        pushButton_4->setObjectName(QString::fromUtf8("pushButton_4"));
        sizePolicy.setHeightForWidth(pushButton_4->sizePolicy().hasHeightForWidth());
        pushButton_4->setSizePolicy(sizePolicy);
        pushButton_4->setStyleSheet(QString::fromUtf8("background-color: rgb(170, 255, 255);"));

        gridLayout_2->addWidget(pushButton_4, 2, 0, 1, 1);

        pushButton_6 = new QPushButton(page_2);
        pushButton_6->setObjectName(QString::fromUtf8("pushButton_6"));
        sizePolicy.setHeightForWidth(pushButton_6->sizePolicy().hasHeightForWidth());
        pushButton_6->setSizePolicy(sizePolicy);
        pushButton_6->setStyleSheet(QString::fromUtf8("background-color: rgb(85, 170, 255);"));

        gridLayout_2->addWidget(pushButton_6, 2, 4, 1, 1);

        pBtn_Setting = new QPushButton(page_2);
        pBtn_Setting->setObjectName(QString::fromUtf8("pBtn_Setting"));
        sizePolicy.setHeightForWidth(pBtn_Setting->sizePolicy().hasHeightForWidth());
        pBtn_Setting->setSizePolicy(sizePolicy);
        pBtn_Setting->setStyleSheet(QString::fromUtf8("border-image: url(:/img/settting.png);"));

        gridLayout_2->addWidget(pBtn_Setting, 0, 0, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Minimum);

        gridLayout_2->addItem(horizontalSpacer_2, 0, 3, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Minimum);

        gridLayout_2->addItem(verticalSpacer, 1, 0, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Minimum);

        gridLayout_2->addItem(verticalSpacer_2, 1, 2, 1, 1);

        stackedWidget->addWidget(page_2);

        horizontalLayout_3->addWidget(stackedWidget);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        stackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "MainWindow", nullptr));
        pBtn_Weather->setText(QString());
        pBtn_Music->setText(QString());
        pBtn_Map->setText(QString());
        pBtn_Monitor->setText(QString());
        groupBox->setTitle(QString());
        label_Time->setText(QApplication::translate("MainWindow", "TextLabel", nullptr));
        label_Date->setText(QApplication::translate("MainWindow", "TextLabel", nullptr));
        label_4->setText(QString());
        label_temp->setText(QString());
        label_2->setText(QString());
        label_humi->setText(QString());
        pushButton_5->setText(QString());
        pushButton_2->setText(QString());
        pushButton_3->setText(QString());
        pushButton_4->setText(QString());
        pushButton_6->setText(QString());
        pBtn_Setting->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
