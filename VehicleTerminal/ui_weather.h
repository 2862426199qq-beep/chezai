/********************************************************************************
** Form generated from reading UI file 'weather.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WEATHER_H
#define UI_WEATHER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Weather
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_6;
    QGroupBox *groupBox_Total;
    QVBoxLayout *verticalLayout_7;
    QSplitter *splitter_2;
    QWidget *horizontalLayoutWidget_2;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label;
    QLabel *label_Time;
    QSpacerItem *horizontalSpacer;
    QLabel *label_37;
    QLabel *label_location;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pushButton;
    QPushButton *pushButton_2;
    QSplitter *splitter;
    QGroupBox *groupBox_5;
    QVBoxLayout *verticalLayout_5;
    QLabel *label_now_img;
    QLabel *label_now_tmp;
    QLabel *label_now_des;
    QLabel *label_now_win;
    QLabel *label_now_winSpeed;
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QFrame *line_5;
    QGroupBox *groupBox_4;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_day1_week;
    QLabel *label_day1_data;
    QLabel *label_day1_img;
    QLabel *label_day1_tmp;
    QLabel *label_day1_des;
    QLabel *label_day1_win;
    QLabel *label_day1_winSpeed;
    QFrame *line;
    QGroupBox *groupBox_3;
    QVBoxLayout *verticalLayout_3;
    QLabel *label_day2_week;
    QLabel *label_day2_data;
    QLabel *label_day2_img;
    QLabel *label_day2_tmp;
    QLabel *label_day2_des;
    QLabel *label_day2_win;
    QLabel *label_day2_winSpeed;
    QFrame *line_2;
    QGroupBox *groupBox_2;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_day3_week;
    QLabel *label_day3_data;
    QLabel *label_day3_img;
    QLabel *label_day3_tmp;
    QLabel *label_day3_des;
    QLabel *label_day3_win;
    QLabel *label_day3_winSpeed;
    QFrame *line_3;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout;
    QLabel *label_day4_week;
    QLabel *label_day4_data;
    QLabel *label_day4_img;
    QLabel *label_day4_tmp;
    QLabel *label_day4_des;
    QLabel *label_day4_win;
    QLabel *label_day4_winSpeed;
    QFrame *line_4;

    void setupUi(QMainWindow *Weather)
    {
        if (Weather->objectName().isEmpty())
            Weather->setObjectName(QString::fromUtf8("Weather"));
        Weather->resize(800, 600);
        centralwidget = new QWidget(Weather);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout_6 = new QVBoxLayout(centralwidget);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        groupBox_Total = new QGroupBox(centralwidget);
        groupBox_Total->setObjectName(QString::fromUtf8("groupBox_Total"));
        groupBox_Total->setAutoFillBackground(true);
        groupBox_Total->setStyleSheet(QString::fromUtf8(""));
        verticalLayout_7 = new QVBoxLayout(groupBox_Total);
        verticalLayout_7->setObjectName(QString::fromUtf8("verticalLayout_7"));
        splitter_2 = new QSplitter(groupBox_Total);
        splitter_2->setObjectName(QString::fromUtf8("splitter_2"));
        splitter_2->setOrientation(Qt::Vertical);
        horizontalLayoutWidget_2 = new QWidget(splitter_2);
        horizontalLayoutWidget_2->setObjectName(QString::fromUtf8("horizontalLayoutWidget_2"));
        horizontalLayout_2 = new QHBoxLayout(horizontalLayoutWidget_2);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(horizontalLayoutWidget_2);
        label->setObjectName(QString::fromUtf8("label"));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy);
        label->setMaximumSize(QSize(80, 16777215));
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        label->setFont(font);

        horizontalLayout_2->addWidget(label);

        label_Time = new QLabel(horizontalLayoutWidget_2);
        label_Time->setObjectName(QString::fromUtf8("label_Time"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label_Time->sizePolicy().hasHeightForWidth());
        label_Time->setSizePolicy(sizePolicy1);
        QFont font1;
        font1.setPointSize(15);
        font1.setBold(true);
        font1.setWeight(75);
        label_Time->setFont(font1);
        label_Time->setStyleSheet(QString::fromUtf8("color: rgb(85, 170, 255);"));
        label_Time->setAlignment(Qt::AlignCenter);

        horizontalLayout_2->addWidget(label_Time);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        label_37 = new QLabel(horizontalLayoutWidget_2);
        label_37->setObjectName(QString::fromUtf8("label_37"));
        label_37->setMaximumSize(QSize(80, 16777215));
        label_37->setFont(font);

        horizontalLayout_2->addWidget(label_37);

        label_location = new QLabel(horizontalLayoutWidget_2);
        label_location->setObjectName(QString::fromUtf8("label_location"));
        label_location->setFont(font1);
        label_location->setStyleSheet(QString::fromUtf8("color: rgb(85, 170, 255);"));
        label_location->setAlignment(Qt::AlignCenter);

        horizontalLayout_2->addWidget(label_location);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_2);

        pushButton = new QPushButton(horizontalLayoutWidget_2);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(pushButton->sizePolicy().hasHeightForWidth());
        pushButton->setSizePolicy(sizePolicy2);
        pushButton->setMinimumSize(QSize(50, 50));
        pushButton->setFont(font1);

        horizontalLayout_2->addWidget(pushButton);

        pushButton_2 = new QPushButton(horizontalLayoutWidget_2);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));
        sizePolicy2.setHeightForWidth(pushButton_2->sizePolicy().hasHeightForWidth());
        pushButton_2->setSizePolicy(sizePolicy2);
        pushButton_2->setFont(font1);

        horizontalLayout_2->addWidget(pushButton_2);

        splitter_2->addWidget(horizontalLayoutWidget_2);
        splitter = new QSplitter(splitter_2);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        groupBox_5 = new QGroupBox(splitter);
        groupBox_5->setObjectName(QString::fromUtf8("groupBox_5"));
        groupBox_5->setMinimumSize(QSize(200, 0));
        groupBox_5->setMaximumSize(QSize(160, 16777215));
        verticalLayout_5 = new QVBoxLayout(groupBox_5);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        label_now_img = new QLabel(groupBox_5);
        label_now_img->setObjectName(QString::fromUtf8("label_now_img"));
        QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(label_now_img->sizePolicy().hasHeightForWidth());
        label_now_img->setSizePolicy(sizePolicy3);
        label_now_img->setMinimumSize(QSize(160, 160));
        QFont font2;
        font2.setPointSize(12);
        label_now_img->setFont(font2);
        label_now_img->setLayoutDirection(Qt::LeftToRight);
        label_now_img->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_now_img);

        label_now_tmp = new QLabel(groupBox_5);
        label_now_tmp->setObjectName(QString::fromUtf8("label_now_tmp"));
        sizePolicy3.setHeightForWidth(label_now_tmp->sizePolicy().hasHeightForWidth());
        label_now_tmp->setSizePolicy(sizePolicy3);
        label_now_tmp->setMinimumSize(QSize(160, 160));
        QFont font3;
        font3.setPointSize(20);
        font3.setBold(true);
        font3.setWeight(75);
        label_now_tmp->setFont(font3);
        label_now_tmp->setLayoutDirection(Qt::LeftToRight);
        label_now_tmp->setStyleSheet(QString::fromUtf8("background-image: url(:/img/images/tmp.png);"));
        label_now_tmp->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_now_tmp);

        label_now_des = new QLabel(groupBox_5);
        label_now_des->setObjectName(QString::fromUtf8("label_now_des"));
        label_now_des->setFont(font1);
        label_now_des->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_now_des);

        label_now_win = new QLabel(groupBox_5);
        label_now_win->setObjectName(QString::fromUtf8("label_now_win"));
        QFont font4;
        font4.setPointSize(12);
        font4.setBold(true);
        font4.setWeight(75);
        label_now_win->setFont(font4);
        label_now_win->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_now_win);

        label_now_winSpeed = new QLabel(groupBox_5);
        label_now_winSpeed->setObjectName(QString::fromUtf8("label_now_winSpeed"));
        label_now_winSpeed->setFont(font2);
        label_now_winSpeed->setStyleSheet(QString::fromUtf8("background-color: rgb(85, 255, 0);\n"
"color: rgb(85, 0, 0);"));
        label_now_winSpeed->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_now_winSpeed);

        splitter->addWidget(groupBox_5);
        horizontalLayoutWidget = new QWidget(splitter);
        horizontalLayoutWidget->setObjectName(QString::fromUtf8("horizontalLayoutWidget"));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        line_5 = new QFrame(horizontalLayoutWidget);
        line_5->setObjectName(QString::fromUtf8("line_5"));
        line_5->setFrameShape(QFrame::VLine);
        line_5->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_5);

        groupBox_4 = new QGroupBox(horizontalLayoutWidget);
        groupBox_4->setObjectName(QString::fromUtf8("groupBox_4"));
        groupBox_4->setMaximumSize(QSize(140, 16777215));
        groupBox_4->setLayoutDirection(Qt::LeftToRight);
        groupBox_4->setAlignment(Qt::AlignCenter);
        verticalLayout_4 = new QVBoxLayout(groupBox_4);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        label_day1_week = new QLabel(groupBox_4);
        label_day1_week->setObjectName(QString::fromUtf8("label_day1_week"));
        label_day1_week->setFont(font4);
        label_day1_week->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day1_week->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_week);

        label_day1_data = new QLabel(groupBox_4);
        label_day1_data->setObjectName(QString::fromUtf8("label_day1_data"));
        label_day1_data->setFont(font4);
        label_day1_data->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day1_data->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_data);

        label_day1_img = new QLabel(groupBox_4);
        label_day1_img->setObjectName(QString::fromUtf8("label_day1_img"));
        sizePolicy3.setHeightForWidth(label_day1_img->sizePolicy().hasHeightForWidth());
        label_day1_img->setSizePolicy(sizePolicy3);
        label_day1_img->setMinimumSize(QSize(120, 120));
        label_day1_img->setFont(font2);
        label_day1_img->setLayoutDirection(Qt::LeftToRight);
        label_day1_img->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_img);

        label_day1_tmp = new QLabel(groupBox_4);
        label_day1_tmp->setObjectName(QString::fromUtf8("label_day1_tmp"));
        label_day1_tmp->setFont(font4);
        label_day1_tmp->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));
        label_day1_tmp->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_tmp);

        label_day1_des = new QLabel(groupBox_4);
        label_day1_des->setObjectName(QString::fromUtf8("label_day1_des"));
        QFont font5;
        font5.setPointSize(9);
        font5.setBold(true);
        font5.setWeight(75);
        label_day1_des->setFont(font5);
        label_day1_des->setStyleSheet(QString::fromUtf8("color: rgb(0, 255, 255);"));
        label_day1_des->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_des);

        label_day1_win = new QLabel(groupBox_4);
        label_day1_win->setObjectName(QString::fromUtf8("label_day1_win"));
        QFont font6;
        font6.setPointSize(10);
        font6.setBold(true);
        font6.setWeight(75);
        label_day1_win->setFont(font6);
        label_day1_win->setStyleSheet(QString::fromUtf8("color: rgb(255, 170, 127);"));
        label_day1_win->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_win);

        label_day1_winSpeed = new QLabel(groupBox_4);
        label_day1_winSpeed->setObjectName(QString::fromUtf8("label_day1_winSpeed"));
        QFont font7;
        font7.setPointSize(8);
        label_day1_winSpeed->setFont(font7);
        label_day1_winSpeed->setStyleSheet(QString::fromUtf8("background-color: argb(255, 170, 127,150);\n"
"color: rgb(255, 85, 0);"));
        label_day1_winSpeed->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day1_winSpeed);


        horizontalLayout->addWidget(groupBox_4);

        line = new QFrame(horizontalLayoutWidget);
        line->setObjectName(QString::fromUtf8("line"));
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line);

        groupBox_3 = new QGroupBox(horizontalLayoutWidget);
        groupBox_3->setObjectName(QString::fromUtf8("groupBox_3"));
        groupBox_3->setMaximumSize(QSize(140, 16777215));
        groupBox_3->setLayoutDirection(Qt::LeftToRight);
        groupBox_3->setAlignment(Qt::AlignCenter);
        verticalLayout_3 = new QVBoxLayout(groupBox_3);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        label_day2_week = new QLabel(groupBox_3);
        label_day2_week->setObjectName(QString::fromUtf8("label_day2_week"));
        label_day2_week->setFont(font4);
        label_day2_week->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day2_week->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_week);

        label_day2_data = new QLabel(groupBox_3);
        label_day2_data->setObjectName(QString::fromUtf8("label_day2_data"));
        label_day2_data->setFont(font4);
        label_day2_data->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day2_data->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_data);

        label_day2_img = new QLabel(groupBox_3);
        label_day2_img->setObjectName(QString::fromUtf8("label_day2_img"));
        sizePolicy3.setHeightForWidth(label_day2_img->sizePolicy().hasHeightForWidth());
        label_day2_img->setSizePolicy(sizePolicy3);
        label_day2_img->setMinimumSize(QSize(120, 120));
        label_day2_img->setFont(font2);
        label_day2_img->setLayoutDirection(Qt::LeftToRight);
        label_day2_img->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_img);

        label_day2_tmp = new QLabel(groupBox_3);
        label_day2_tmp->setObjectName(QString::fromUtf8("label_day2_tmp"));
        label_day2_tmp->setFont(font4);
        label_day2_tmp->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));
        label_day2_tmp->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_tmp);

        label_day2_des = new QLabel(groupBox_3);
        label_day2_des->setObjectName(QString::fromUtf8("label_day2_des"));
        label_day2_des->setFont(font5);
        label_day2_des->setStyleSheet(QString::fromUtf8("color: rgb(0, 255, 255);"));
        label_day2_des->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_des);

        label_day2_win = new QLabel(groupBox_3);
        label_day2_win->setObjectName(QString::fromUtf8("label_day2_win"));
        label_day2_win->setFont(font6);
        label_day2_win->setStyleSheet(QString::fromUtf8("color: rgb(255, 170, 127);"));
        label_day2_win->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_win);

        label_day2_winSpeed = new QLabel(groupBox_3);
        label_day2_winSpeed->setObjectName(QString::fromUtf8("label_day2_winSpeed"));
        label_day2_winSpeed->setFont(font7);
        label_day2_winSpeed->setStyleSheet(QString::fromUtf8("background-color: argb(255, 170, 127,150);\n"
"color: rgb(255, 85, 0);"));
        label_day2_winSpeed->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day2_winSpeed);


        horizontalLayout->addWidget(groupBox_3);

        line_2 = new QFrame(horizontalLayoutWidget);
        line_2->setObjectName(QString::fromUtf8("line_2"));
        line_2->setFrameShape(QFrame::VLine);
        line_2->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_2);

        groupBox_2 = new QGroupBox(horizontalLayoutWidget);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        groupBox_2->setMaximumSize(QSize(140, 16777215));
        groupBox_2->setLayoutDirection(Qt::LeftToRight);
        groupBox_2->setAlignment(Qt::AlignCenter);
        verticalLayout_2 = new QVBoxLayout(groupBox_2);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        label_day3_week = new QLabel(groupBox_2);
        label_day3_week->setObjectName(QString::fromUtf8("label_day3_week"));
        label_day3_week->setFont(font4);
        label_day3_week->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day3_week->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_week);

        label_day3_data = new QLabel(groupBox_2);
        label_day3_data->setObjectName(QString::fromUtf8("label_day3_data"));
        label_day3_data->setFont(font4);
        label_day3_data->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day3_data->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_data);

        label_day3_img = new QLabel(groupBox_2);
        label_day3_img->setObjectName(QString::fromUtf8("label_day3_img"));
        sizePolicy3.setHeightForWidth(label_day3_img->sizePolicy().hasHeightForWidth());
        label_day3_img->setSizePolicy(sizePolicy3);
        label_day3_img->setMinimumSize(QSize(120, 120));
        label_day3_img->setFont(font2);
        label_day3_img->setLayoutDirection(Qt::LeftToRight);
        label_day3_img->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_img);

        label_day3_tmp = new QLabel(groupBox_2);
        label_day3_tmp->setObjectName(QString::fromUtf8("label_day3_tmp"));
        label_day3_tmp->setFont(font4);
        label_day3_tmp->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));
        label_day3_tmp->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_tmp);

        label_day3_des = new QLabel(groupBox_2);
        label_day3_des->setObjectName(QString::fromUtf8("label_day3_des"));
        label_day3_des->setFont(font5);
        label_day3_des->setStyleSheet(QString::fromUtf8("color: rgb(0, 255, 255);"));
        label_day3_des->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_des);

        label_day3_win = new QLabel(groupBox_2);
        label_day3_win->setObjectName(QString::fromUtf8("label_day3_win"));
        label_day3_win->setFont(font6);
        label_day3_win->setStyleSheet(QString::fromUtf8("color: rgb(255, 170, 127);"));
        label_day3_win->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_win);

        label_day3_winSpeed = new QLabel(groupBox_2);
        label_day3_winSpeed->setObjectName(QString::fromUtf8("label_day3_winSpeed"));
        label_day3_winSpeed->setFont(font7);
        label_day3_winSpeed->setStyleSheet(QString::fromUtf8("background-color: argb(255, 170, 127,150);\n"
"color: rgb(255, 85, 0);"));
        label_day3_winSpeed->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day3_winSpeed);


        horizontalLayout->addWidget(groupBox_2);

        line_3 = new QFrame(horizontalLayoutWidget);
        line_3->setObjectName(QString::fromUtf8("line_3"));
        line_3->setFrameShape(QFrame::VLine);
        line_3->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_3);

        groupBox = new QGroupBox(horizontalLayoutWidget);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        groupBox->setMaximumSize(QSize(140, 16777215));
        groupBox->setLayoutDirection(Qt::LeftToRight);
        groupBox->setAlignment(Qt::AlignCenter);
        verticalLayout = new QVBoxLayout(groupBox);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label_day4_week = new QLabel(groupBox);
        label_day4_week->setObjectName(QString::fromUtf8("label_day4_week"));
        label_day4_week->setFont(font4);
        label_day4_week->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day4_week->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_week);

        label_day4_data = new QLabel(groupBox);
        label_day4_data->setObjectName(QString::fromUtf8("label_day4_data"));
        label_day4_data->setFont(font4);
        label_day4_data->setStyleSheet(QString::fromUtf8("background-color: argb(85, 255, 127,0);"));
        label_day4_data->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_data);

        label_day4_img = new QLabel(groupBox);
        label_day4_img->setObjectName(QString::fromUtf8("label_day4_img"));
        sizePolicy3.setHeightForWidth(label_day4_img->sizePolicy().hasHeightForWidth());
        label_day4_img->setSizePolicy(sizePolicy3);
        label_day4_img->setMinimumSize(QSize(120, 120));
        label_day4_img->setFont(font2);
        label_day4_img->setLayoutDirection(Qt::LeftToRight);
        label_day4_img->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_img);

        label_day4_tmp = new QLabel(groupBox);
        label_day4_tmp->setObjectName(QString::fromUtf8("label_day4_tmp"));
        label_day4_tmp->setFont(font4);
        label_day4_tmp->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));
        label_day4_tmp->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_tmp);

        label_day4_des = new QLabel(groupBox);
        label_day4_des->setObjectName(QString::fromUtf8("label_day4_des"));
        label_day4_des->setFont(font5);
        label_day4_des->setStyleSheet(QString::fromUtf8("color: rgb(0, 255, 255);"));
        label_day4_des->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_des);

        label_day4_win = new QLabel(groupBox);
        label_day4_win->setObjectName(QString::fromUtf8("label_day4_win"));
        label_day4_win->setFont(font6);
        label_day4_win->setStyleSheet(QString::fromUtf8("color: rgb(255, 170, 127);"));
        label_day4_win->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_win);

        label_day4_winSpeed = new QLabel(groupBox);
        label_day4_winSpeed->setObjectName(QString::fromUtf8("label_day4_winSpeed"));
        label_day4_winSpeed->setFont(font7);
        label_day4_winSpeed->setStyleSheet(QString::fromUtf8("background-color: argb(255, 170, 127,150);\n"
"color: rgb(255, 85, 0);"));
        label_day4_winSpeed->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day4_winSpeed);


        horizontalLayout->addWidget(groupBox);

        line_4 = new QFrame(horizontalLayoutWidget);
        line_4->setObjectName(QString::fromUtf8("line_4"));
        line_4->setFrameShape(QFrame::VLine);
        line_4->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_4);

        splitter->addWidget(horizontalLayoutWidget);
        splitter_2->addWidget(splitter);

        verticalLayout_7->addWidget(splitter_2);


        verticalLayout_6->addWidget(groupBox_Total);

        Weather->setCentralWidget(centralwidget);

        retranslateUi(Weather);

        QMetaObject::connectSlotsByName(Weather);
    } // setupUi

    void retranslateUi(QMainWindow *Weather)
    {
        Weather->setWindowTitle(QApplication::translate("Weather", "MainWindow", nullptr));
        groupBox_Total->setTitle(QString());
        label->setStyleSheet(QApplication::translate("Weather", "color: rgb(85, 255, 255);", nullptr));
        label->setText(QApplication::translate("Weather", "\346\233\264\346\226\260\346\227\266\351\227\264\357\274\232", nullptr));
        label_Time->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_37->setStyleSheet(QApplication::translate("Weather", "color: rgb(85, 255, 255);", nullptr));
        label_37->setText(QApplication::translate("Weather", "\345\275\223\345\211\215\345\234\260\347\202\271\357\274\232", nullptr));
        label_location->setText(QApplication::translate("Weather", "\346\233\264\346\226\260\346\227\266\351\227\264", nullptr));
        pushButton->setText(QApplication::translate("Weather", "\346\233\264\346\226\260", nullptr));
        pushButton_2->setText(QApplication::translate("Weather", "\350\277\224\345\233\236", nullptr));
        groupBox_5->setTitle(QString());
        label_now_img->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_now_tmp->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_now_des->setStyleSheet(QApplication::translate("Weather", "background-color: argb(170, 255, 255,0);", nullptr));
        label_now_des->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_now_win->setStyleSheet(QApplication::translate("Weather", "background-color: argb(170, 255, 255,0);", nullptr));
        label_now_win->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_now_winSpeed->setText(QApplication::translate("Weather", "26 \344\274\230", nullptr));
        groupBox_4->setTitle(QString());
        label_day1_week->setText(QApplication::translate("Weather", "\345\221\250\344\270\211", nullptr));
        label_day1_data->setText(QApplication::translate("Weather", "07\346\234\21031\346\227\245", nullptr));
        label_day1_img->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_day1_tmp->setText(QApplication::translate("Weather", "26~32\342\204\203", nullptr));
        label_day1_des->setText(QApplication::translate("Weather", "\351\230\264\350\275\254\351\233\267\351\230\265\351\233\250", nullptr));
        label_day1_win->setText(QApplication::translate("Weather", "\344\270\234\351\243\2163-4\347\272\247", nullptr));
        label_day1_winSpeed->setText(QApplication::translate("Weather", "26 \344\274\230", nullptr));
        groupBox_3->setTitle(QString());
        label_day2_week->setText(QApplication::translate("Weather", "\345\221\250\344\270\211", nullptr));
        label_day2_data->setText(QApplication::translate("Weather", "07\346\234\21031\346\227\245", nullptr));
        label_day2_img->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_day2_tmp->setText(QApplication::translate("Weather", "26~32\342\204\203", nullptr));
        label_day2_des->setText(QApplication::translate("Weather", "\351\230\264\350\275\254\351\233\267\351\230\265\351\233\250", nullptr));
        label_day2_win->setText(QApplication::translate("Weather", "\344\270\234\351\243\2163-4\347\272\247", nullptr));
        label_day2_winSpeed->setText(QApplication::translate("Weather", "26 \344\274\230", nullptr));
        groupBox_2->setTitle(QString());
        label_day3_week->setText(QApplication::translate("Weather", "\345\221\250\344\270\211", nullptr));
        label_day3_data->setText(QApplication::translate("Weather", "07\346\234\21031\346\227\245", nullptr));
        label_day3_img->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_day3_tmp->setText(QApplication::translate("Weather", "26~32\342\204\203", nullptr));
        label_day3_des->setText(QApplication::translate("Weather", "\351\230\264\350\275\254\351\233\267\351\230\265\351\233\250", nullptr));
        label_day3_win->setText(QApplication::translate("Weather", "\344\270\234\351\243\2163-4\347\272\247", nullptr));
        label_day3_winSpeed->setText(QApplication::translate("Weather", "26 \344\274\230", nullptr));
        groupBox->setTitle(QString());
        label_day4_week->setText(QApplication::translate("Weather", "\345\221\250\344\270\211", nullptr));
        label_day4_data->setText(QApplication::translate("Weather", "07\346\234\21031\346\227\245", nullptr));
        label_day4_img->setText(QApplication::translate("Weather", "TextLabel", nullptr));
        label_day4_tmp->setText(QApplication::translate("Weather", "26~32\342\204\203", nullptr));
        label_day4_des->setText(QApplication::translate("Weather", "\351\230\264\350\275\254\351\233\267\351\230\265\351\233\250", nullptr));
        label_day4_win->setText(QApplication::translate("Weather", "\344\270\234\351\243\2163-4\347\272\247", nullptr));
        label_day4_winSpeed->setText(QApplication::translate("Weather", "26 \344\274\230", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Weather: public Ui_Weather {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WEATHER_H
