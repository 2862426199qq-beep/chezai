/********************************************************************************
** Form generated from reading UI file 'musicplayer.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MUSICPLAYER_H
#define UI_MUSICPLAYER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MusicPlayer
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_6;
    QHBoxLayout *horizontalLayout_5;
    QVBoxLayout *verticalLayout_4;
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_4;
    QPushButton *pushButton;
    QLabel *label;
    QListWidget *listWidget;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *pBtn_Pre;
    QPushButton *pBtn_Pause;
    QPushButton *pBtn_Next;
    QPushButton *pBtn_OpenSearchWin;
    QVBoxLayout *verticalLayout_5;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_pic;
    QVBoxLayout *verticalLayout;
    QSlider *horizontalSlider;
    QHBoxLayout *horizontalLayout;
    QLabel *label_CurTime;
    QSpacerItem *horizontalSpacer;
    QLabel *label_TotalTime;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *pBtn_SetLove;
    QPushButton *pBtn_loop;
    QPushButton *pBtn_Loud;
    QPushButton *pBtn_Low;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MusicPlayer)
    {
        if (MusicPlayer->objectName().isEmpty())
            MusicPlayer->setObjectName(QString::fromUtf8("MusicPlayer"));
        MusicPlayer->resize(689, 328);
        MusicPlayer->setMinimumSize(QSize(512, 300));
        MusicPlayer->setMaximumSize(QSize(1024, 600));
        MusicPlayer->setStyleSheet(QString::fromUtf8("background-color: rgb(30, 30, 30);"));
        centralwidget = new QWidget(MusicPlayer);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout_6 = new QVBoxLayout(centralwidget);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        pushButton = new QPushButton(centralwidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(pushButton->sizePolicy().hasHeightForWidth());
        pushButton->setSizePolicy(sizePolicy);
        pushButton->setMinimumSize(QSize(60, 60));
        pushButton->setMaximumSize(QSize(60, 60));
        pushButton->setStyleSheet(QString::fromUtf8("background-color: rgb(85, 85, 255);"));

        horizontalLayout_4->addWidget(pushButton);

        label = new QLabel(centralwidget);
        label->setObjectName(QString::fromUtf8("label"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Minimum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy1);
        QFont font;
        font.setPointSize(15);
        font.setBold(true);
        font.setWeight(75);
        label->setFont(font);
        label->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));
        label->setAlignment(Qt::AlignCenter);

        horizontalLayout_4->addWidget(label);


        verticalLayout_3->addLayout(horizontalLayout_4);

        listWidget = new QListWidget(centralwidget);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(listWidget->sizePolicy().hasHeightForWidth());
        listWidget->setSizePolicy(sizePolicy2);
        listWidget->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);\n"
"background-color: rgb(43, 43, 43);"));
        listWidget->setResizeMode(QListView::Fixed);

        verticalLayout_3->addWidget(listWidget);


        verticalLayout_4->addLayout(verticalLayout_3);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        pBtn_Pre = new QPushButton(centralwidget);
        pBtn_Pre->setObjectName(QString::fromUtf8("pBtn_Pre"));
        QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy3.setHorizontalStretch(100);
        sizePolicy3.setVerticalStretch(100);
        sizePolicy3.setHeightForWidth(pBtn_Pre->sizePolicy().hasHeightForWidth());
        pBtn_Pre->setSizePolicy(sizePolicy3);
        pBtn_Pre->setMinimumSize(QSize(100, 100));
        pBtn_Pre->setMaximumSize(QSize(100, 100));
        pBtn_Pre->setAutoFillBackground(false);
        pBtn_Pre->setStyleSheet(QString::fromUtf8(""));
        pBtn_Pre->setIconSize(QSize(100, 100));
        pBtn_Pre->setCheckable(false);

        horizontalLayout_2->addWidget(pBtn_Pre);

        pBtn_Pause = new QPushButton(centralwidget);
        pBtn_Pause->setObjectName(QString::fromUtf8("pBtn_Pause"));
        sizePolicy3.setHeightForWidth(pBtn_Pause->sizePolicy().hasHeightForWidth());
        pBtn_Pause->setSizePolicy(sizePolicy3);
        pBtn_Pause->setMinimumSize(QSize(100, 100));
        pBtn_Pause->setMaximumSize(QSize(100, 100));
        pBtn_Pause->setStyleSheet(QString::fromUtf8(""));
        pBtn_Pause->setCheckable(true);

        horizontalLayout_2->addWidget(pBtn_Pause);

        pBtn_Next = new QPushButton(centralwidget);
        pBtn_Next->setObjectName(QString::fromUtf8("pBtn_Next"));
        sizePolicy3.setHeightForWidth(pBtn_Next->sizePolicy().hasHeightForWidth());
        pBtn_Next->setSizePolicy(sizePolicy3);
        pBtn_Next->setMinimumSize(QSize(100, 100));
        pBtn_Next->setMaximumSize(QSize(100, 100));
        pBtn_Next->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_2->addWidget(pBtn_Next);

        pBtn_OpenSearchWin = new QPushButton(centralwidget);
        pBtn_OpenSearchWin->setObjectName(QString::fromUtf8("pBtn_OpenSearchWin"));
        sizePolicy3.setHeightForWidth(pBtn_OpenSearchWin->sizePolicy().hasHeightForWidth());
        pBtn_OpenSearchWin->setSizePolicy(sizePolicy3);
        pBtn_OpenSearchWin->setMinimumSize(QSize(100, 100));
        pBtn_OpenSearchWin->setMaximumSize(QSize(100, 100));
        pBtn_OpenSearchWin->setMouseTracking(false);
        pBtn_OpenSearchWin->setFocusPolicy(Qt::NoFocus);
        pBtn_OpenSearchWin->setContextMenuPolicy(Qt::NoContextMenu);
        pBtn_OpenSearchWin->setStyleSheet(QString::fromUtf8(""));
        pBtn_OpenSearchWin->setFlat(true);

        horizontalLayout_2->addWidget(pBtn_OpenSearchWin);


        verticalLayout_4->addLayout(horizontalLayout_2);


        horizontalLayout_5->addLayout(verticalLayout_4);

        verticalLayout_5 = new QVBoxLayout();
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        label_pic = new QLabel(centralwidget);
        label_pic->setObjectName(QString::fromUtf8("label_pic"));
        sizePolicy2.setHeightForWidth(label_pic->sizePolicy().hasHeightForWidth());
        label_pic->setSizePolicy(sizePolicy2);
        label_pic->setStyleSheet(QString::fromUtf8("border-image: url(:/music/Music/images/cd.png);"));

        verticalLayout_2->addWidget(label_pic);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalSlider = new QSlider(centralwidget);
        horizontalSlider->setObjectName(QString::fromUtf8("horizontalSlider"));
        sizePolicy1.setHeightForWidth(horizontalSlider->sizePolicy().hasHeightForWidth());
        horizontalSlider->setSizePolicy(sizePolicy1);
        horizontalSlider->setOrientation(Qt::Horizontal);

        verticalLayout->addWidget(horizontalSlider);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label_CurTime = new QLabel(centralwidget);
        label_CurTime->setObjectName(QString::fromUtf8("label_CurTime"));
        QSizePolicy sizePolicy4(QSizePolicy::Preferred, QSizePolicy::Minimum);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(label_CurTime->sizePolicy().hasHeightForWidth());
        label_CurTime->setSizePolicy(sizePolicy4);
        QFont font1;
        font1.setPointSize(10);
        label_CurTime->setFont(font1);
        label_CurTime->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));

        horizontalLayout->addWidget(label_CurTime);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        label_TotalTime = new QLabel(centralwidget);
        label_TotalTime->setObjectName(QString::fromUtf8("label_TotalTime"));
        label_TotalTime->setFont(font1);
        label_TotalTime->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));

        horizontalLayout->addWidget(label_TotalTime);


        verticalLayout->addLayout(horizontalLayout);


        verticalLayout_2->addLayout(verticalLayout);


        verticalLayout_5->addLayout(verticalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        pBtn_SetLove = new QPushButton(centralwidget);
        pBtn_SetLove->setObjectName(QString::fromUtf8("pBtn_SetLove"));
        QSizePolicy sizePolicy5(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(pBtn_SetLove->sizePolicy().hasHeightForWidth());
        pBtn_SetLove->setSizePolicy(sizePolicy5);
        pBtn_SetLove->setMinimumSize(QSize(60, 60));
        pBtn_SetLove->setMaximumSize(QSize(60, 60));
        pBtn_SetLove->setStyleSheet(QString::fromUtf8(""));
        pBtn_SetLove->setCheckable(true);

        horizontalLayout_3->addWidget(pBtn_SetLove);

        pBtn_loop = new QPushButton(centralwidget);
        pBtn_loop->setObjectName(QString::fromUtf8("pBtn_loop"));
        QSizePolicy sizePolicy6(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(pBtn_loop->sizePolicy().hasHeightForWidth());
        pBtn_loop->setSizePolicy(sizePolicy6);
        pBtn_loop->setMinimumSize(QSize(60, 60));
        pBtn_loop->setMaximumSize(QSize(60, 60));
        pBtn_loop->setStyleSheet(QString::fromUtf8(""));
        pBtn_loop->setCheckable(true);

        horizontalLayout_3->addWidget(pBtn_loop);

        pBtn_Loud = new QPushButton(centralwidget);
        pBtn_Loud->setObjectName(QString::fromUtf8("pBtn_Loud"));
        QSizePolicy sizePolicy7(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy7.setHorizontalStretch(20);
        sizePolicy7.setVerticalStretch(20);
        sizePolicy7.setHeightForWidth(pBtn_Loud->sizePolicy().hasHeightForWidth());
        pBtn_Loud->setSizePolicy(sizePolicy7);
        pBtn_Loud->setMinimumSize(QSize(60, 60));
        pBtn_Loud->setMaximumSize(QSize(60, 60));

        horizontalLayout_3->addWidget(pBtn_Loud);

        pBtn_Low = new QPushButton(centralwidget);
        pBtn_Low->setObjectName(QString::fromUtf8("pBtn_Low"));
        sizePolicy5.setHeightForWidth(pBtn_Low->sizePolicy().hasHeightForWidth());
        pBtn_Low->setSizePolicy(sizePolicy5);
        pBtn_Low->setMinimumSize(QSize(60, 60));
        pBtn_Low->setMaximumSize(QSize(60, 60));

        horizontalLayout_3->addWidget(pBtn_Low);


        verticalLayout_5->addLayout(horizontalLayout_3);


        horizontalLayout_5->addLayout(verticalLayout_5);


        verticalLayout_6->addLayout(horizontalLayout_5);

        MusicPlayer->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MusicPlayer);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MusicPlayer->setStatusBar(statusbar);

        retranslateUi(MusicPlayer);

        QMetaObject::connectSlotsByName(MusicPlayer);
    } // setupUi

    void retranslateUi(QMainWindow *MusicPlayer)
    {
        MusicPlayer->setWindowTitle(QApplication::translate("MusicPlayer", "MainWindow", nullptr));
        pushButton->setText(QString());
        label->setText(QApplication::translate("MusicPlayer", "Enjoy Music !", nullptr));
        pBtn_Pre->setText(QString());
        pBtn_Pause->setText(QString());
        pBtn_Next->setText(QString());
        pBtn_OpenSearchWin->setText(QString());
        label_pic->setText(QString());
        label_CurTime->setText(QApplication::translate("MusicPlayer", "00:00", nullptr));
        label_TotalTime->setText(QApplication::translate("MusicPlayer", "03:34", nullptr));
        pBtn_SetLove->setText(QString());
        pBtn_loop->setText(QString());
        pBtn_Loud->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MusicPlayer: public Ui_MusicPlayer {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MUSICPLAYER_H
