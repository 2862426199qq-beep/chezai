/********************************************************************************
** Form generated from reading UI file 'settingwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGWINDOW_H
#define UI_SETTINGWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDateEdit>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SettingWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout_7;
    QToolBox *toolBox;
    QWidget *setTime;
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_2;
    QVBoxLayout *verticalLayout;
    QLabel *label_setting_time;
    QHBoxLayout *horizontalLayout;
    QSpinBox *spinBox_hour;
    QSpinBox *spinBox_Min;
    QSpinBox *spinBox_Sec;
    QPushButton *pBtn_ModifyTime;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_setting_date;
    QDateEdit *dateEdit;
    QPushButton *pBtn_ModifyDate;
    QWidget *setNet;
    QVBoxLayout *verticalLayout_4;
    QHBoxLayout *horizontalLayout_4;
    QPushButton *pushButton_3;
    QPushButton *pushButton_4;
    QListWidget *listWidget;
    QHBoxLayout *horizontalLayout_5;
    QLineEdit *lineEdit_password;
    QPushButton *pushButton_5;
    QVBoxLayout *verticalLayout_5;
    QLabel *label_ShowGif;
    QHBoxLayout *horizontalLayout_6;
    QPushButton *pBtn_PauseGif;
    QPushButton *pBtn_SwitchGif;
    QPushButton *pushButton_8;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *SettingWindow)
    {
        if (SettingWindow->objectName().isEmpty())
            SettingWindow->setObjectName(QString::fromUtf8("SettingWindow"));
        SettingWindow->resize(800, 600);
        SettingWindow->setMinimumSize(QSize(100, 0));
        centralwidget = new QWidget(SettingWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout_7 = new QHBoxLayout(centralwidget);
        horizontalLayout_7->setObjectName(QString::fromUtf8("horizontalLayout_7"));
        toolBox = new QToolBox(centralwidget);
        toolBox->setObjectName(QString::fromUtf8("toolBox"));
        setTime = new QWidget();
        setTime->setObjectName(QString::fromUtf8("setTime"));
        setTime->setGeometry(QRect(0, 0, 386, 468));
        verticalLayout_3 = new QVBoxLayout(setTime);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label_setting_time = new QLabel(setTime);
        label_setting_time->setObjectName(QString::fromUtf8("label_setting_time"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(label_setting_time->sizePolicy().hasHeightForWidth());
        label_setting_time->setSizePolicy(sizePolicy);
        label_setting_time->setMinimumSize(QSize(200, 0));
        QFont font;
        font.setPointSize(12);
        label_setting_time->setFont(font);
        label_setting_time->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_setting_time);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        spinBox_hour = new QSpinBox(setTime);
        spinBox_hour->setObjectName(QString::fromUtf8("spinBox_hour"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(spinBox_hour->sizePolicy().hasHeightForWidth());
        spinBox_hour->setSizePolicy(sizePolicy1);
        spinBox_hour->setMinimumSize(QSize(0, 80));
        spinBox_hour->setFont(font);
        spinBox_hour->setAlignment(Qt::AlignCenter);
        spinBox_hour->setMaximum(23);

        horizontalLayout->addWidget(spinBox_hour);

        spinBox_Min = new QSpinBox(setTime);
        spinBox_Min->setObjectName(QString::fromUtf8("spinBox_Min"));
        sizePolicy1.setHeightForWidth(spinBox_Min->sizePolicy().hasHeightForWidth());
        spinBox_Min->setSizePolicy(sizePolicy1);
        spinBox_Min->setMinimumSize(QSize(0, 80));
        spinBox_Min->setFont(font);
        spinBox_Min->setAlignment(Qt::AlignCenter);
        spinBox_Min->setMaximum(59);

        horizontalLayout->addWidget(spinBox_Min);

        spinBox_Sec = new QSpinBox(setTime);
        spinBox_Sec->setObjectName(QString::fromUtf8("spinBox_Sec"));
        sizePolicy1.setHeightForWidth(spinBox_Sec->sizePolicy().hasHeightForWidth());
        spinBox_Sec->setSizePolicy(sizePolicy1);
        spinBox_Sec->setMinimumSize(QSize(0, 80));
        spinBox_Sec->setFont(font);
        spinBox_Sec->setAlignment(Qt::AlignCenter);
        spinBox_Sec->setMaximum(59);

        horizontalLayout->addWidget(spinBox_Sec);


        verticalLayout->addLayout(horizontalLayout);


        horizontalLayout_2->addLayout(verticalLayout);

        pBtn_ModifyTime = new QPushButton(setTime);
        pBtn_ModifyTime->setObjectName(QString::fromUtf8("pBtn_ModifyTime"));
        sizePolicy.setHeightForWidth(pBtn_ModifyTime->sizePolicy().hasHeightForWidth());
        pBtn_ModifyTime->setSizePolicy(sizePolicy);
        QFont font1;
        font1.setPointSize(20);
        pBtn_ModifyTime->setFont(font1);

        horizontalLayout_2->addWidget(pBtn_ModifyTime);


        verticalLayout_3->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        label_setting_date = new QLabel(setTime);
        label_setting_date->setObjectName(QString::fromUtf8("label_setting_date"));
        QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(label_setting_date->sizePolicy().hasHeightForWidth());
        label_setting_date->setSizePolicy(sizePolicy2);
        label_setting_date->setMinimumSize(QSize(200, 0));
        label_setting_date->setFont(font);
        label_setting_date->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_setting_date);

        dateEdit = new QDateEdit(setTime);
        dateEdit->setObjectName(QString::fromUtf8("dateEdit"));
        QSizePolicy sizePolicy3(QSizePolicy::Minimum, QSizePolicy::Expanding);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(dateEdit->sizePolicy().hasHeightForWidth());
        dateEdit->setSizePolicy(sizePolicy3);
        QFont font2;
        font2.setPointSize(15);
        dateEdit->setFont(font2);
        dateEdit->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(dateEdit);


        horizontalLayout_3->addLayout(verticalLayout_2);

        pBtn_ModifyDate = new QPushButton(setTime);
        pBtn_ModifyDate->setObjectName(QString::fromUtf8("pBtn_ModifyDate"));
        sizePolicy.setHeightForWidth(pBtn_ModifyDate->sizePolicy().hasHeightForWidth());
        pBtn_ModifyDate->setSizePolicy(sizePolicy);
        pBtn_ModifyDate->setFont(font1);

        horizontalLayout_3->addWidget(pBtn_ModifyDate);


        verticalLayout_3->addLayout(horizontalLayout_3);

        toolBox->addItem(setTime, QString::fromUtf8("\350\256\276\347\275\256\346\227\266\351\227\264"));
        setNet = new QWidget();
        setNet->setObjectName(QString::fromUtf8("setNet"));
        setNet->setGeometry(QRect(0, 0, 386, 468));
        verticalLayout_4 = new QVBoxLayout(setNet);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        pushButton_3 = new QPushButton(setNet);
        pushButton_3->setObjectName(QString::fromUtf8("pushButton_3"));
        pushButton_3->setMinimumSize(QSize(0, 50));
        pushButton_3->setFont(font2);

        horizontalLayout_4->addWidget(pushButton_3);

        pushButton_4 = new QPushButton(setNet);
        pushButton_4->setObjectName(QString::fromUtf8("pushButton_4"));
        pushButton_4->setMinimumSize(QSize(0, 50));
        pushButton_4->setFont(font2);

        horizontalLayout_4->addWidget(pushButton_4);


        verticalLayout_4->addLayout(horizontalLayout_4);

        listWidget = new QListWidget(setNet);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));

        verticalLayout_4->addWidget(listWidget);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        lineEdit_password = new QLineEdit(setNet);
        lineEdit_password->setObjectName(QString::fromUtf8("lineEdit_password"));
        lineEdit_password->setMinimumSize(QSize(0, 50));
        lineEdit_password->setFont(font2);

        horizontalLayout_5->addWidget(lineEdit_password);

        pushButton_5 = new QPushButton(setNet);
        pushButton_5->setObjectName(QString::fromUtf8("pushButton_5"));
        pushButton_5->setMinimumSize(QSize(0, 50));
        pushButton_5->setFont(font2);

        horizontalLayout_5->addWidget(pushButton_5);


        verticalLayout_4->addLayout(horizontalLayout_5);

        toolBox->addItem(setNet, QString::fromUtf8("\350\256\276\347\275\256\347\275\221\347\273\234"));

        horizontalLayout_7->addWidget(toolBox);

        verticalLayout_5 = new QVBoxLayout();
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        label_ShowGif = new QLabel(centralwidget);
        label_ShowGif->setObjectName(QString::fromUtf8("label_ShowGif"));
        sizePolicy2.setHeightForWidth(label_ShowGif->sizePolicy().hasHeightForWidth());
        label_ShowGif->setSizePolicy(sizePolicy2);

        verticalLayout_5->addWidget(label_ShowGif);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        pBtn_PauseGif = new QPushButton(centralwidget);
        pBtn_PauseGif->setObjectName(QString::fromUtf8("pBtn_PauseGif"));
        QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(pBtn_PauseGif->sizePolicy().hasHeightForWidth());
        pBtn_PauseGif->setSizePolicy(sizePolicy4);
        pBtn_PauseGif->setMaximumSize(QSize(16777215, 100));
        pBtn_PauseGif->setFont(font1);
        pBtn_PauseGif->setCheckable(true);
        pBtn_PauseGif->setChecked(false);

        horizontalLayout_6->addWidget(pBtn_PauseGif);

        pBtn_SwitchGif = new QPushButton(centralwidget);
        pBtn_SwitchGif->setObjectName(QString::fromUtf8("pBtn_SwitchGif"));
        sizePolicy4.setHeightForWidth(pBtn_SwitchGif->sizePolicy().hasHeightForWidth());
        pBtn_SwitchGif->setSizePolicy(sizePolicy4);
        pBtn_SwitchGif->setMaximumSize(QSize(16777215, 100));
        pBtn_SwitchGif->setFont(font1);

        horizontalLayout_6->addWidget(pBtn_SwitchGif);

        pushButton_8 = new QPushButton(centralwidget);
        pushButton_8->setObjectName(QString::fromUtf8("pushButton_8"));
        sizePolicy4.setHeightForWidth(pushButton_8->sizePolicy().hasHeightForWidth());
        pushButton_8->setSizePolicy(sizePolicy4);
        pushButton_8->setMaximumSize(QSize(16777215, 100));
        pushButton_8->setFont(font1);

        horizontalLayout_6->addWidget(pushButton_8);


        verticalLayout_5->addLayout(horizontalLayout_6);


        horizontalLayout_7->addLayout(verticalLayout_5);

        SettingWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(SettingWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        SettingWindow->setStatusBar(statusbar);

        retranslateUi(SettingWindow);

        toolBox->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(SettingWindow);
    } // setupUi

    void retranslateUi(QMainWindow *SettingWindow)
    {
        SettingWindow->setWindowTitle(QApplication::translate("SettingWindow", "MainWindow", nullptr));
        label_setting_time->setText(QApplication::translate("SettingWindow", "TextLabel", nullptr));
        pBtn_ModifyTime->setText(QApplication::translate("SettingWindow", "\344\277\256\346\224\271\346\227\266\351\227\264", nullptr));
        label_setting_date->setText(QApplication::translate("SettingWindow", "TextLabel", nullptr));
        pBtn_ModifyDate->setText(QApplication::translate("SettingWindow", "\344\277\256\346\224\271\346\227\245\346\234\237", nullptr));
        toolBox->setItemText(toolBox->indexOf(setTime), QApplication::translate("SettingWindow", "\350\256\276\347\275\256\346\227\266\351\227\264", nullptr));
        pushButton_3->setText(QApplication::translate("SettingWindow", "\345\274\200\345\220\257Wifi", nullptr));
        pushButton_4->setText(QApplication::translate("SettingWindow", "\346\237\245\350\257\242\351\231\204\350\277\221\347\275\221\347\273\234", nullptr));
        lineEdit_password->setText(QApplication::translate("SettingWindow", "\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
        pushButton_5->setText(QApplication::translate("SettingWindow", "\350\277\236\346\216\245Wifi", nullptr));
        toolBox->setItemText(toolBox->indexOf(setNet), QApplication::translate("SettingWindow", "\350\256\276\347\275\256\347\275\221\347\273\234", nullptr));
        label_ShowGif->setText(QString());
        pBtn_PauseGif->setText(QApplication::translate("SettingWindow", "\346\232\202\345\201\234", nullptr));
        pBtn_SwitchGif->setText(QApplication::translate("SettingWindow", "\345\210\207\346\215\242", nullptr));
        pushButton_8->setText(QApplication::translate("SettingWindow", "\350\277\224\345\233\236", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SettingWindow: public Ui_SettingWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGWINDOW_H
