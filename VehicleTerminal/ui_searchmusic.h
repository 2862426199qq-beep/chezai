/********************************************************************************
** Form generated from reading UI file 'searchmusic.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SEARCHMUSIC_H
#define UI_SEARCHMUSIC_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SearchMusic
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_2;
    QLineEdit *SongName;
    QSpinBox *spinBox_SearchNum;
    QHBoxLayout *horizontalLayout;
    QPushButton *pBtn_Search;
    QPushButton *pushButton;
    QListWidget *listWidget;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *SearchMusic)
    {
        if (SearchMusic->objectName().isEmpty())
            SearchMusic->setObjectName(QString::fromUtf8("SearchMusic"));
        SearchMusic->resize(1024, 600);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(SearchMusic->sizePolicy().hasHeightForWidth());
        SearchMusic->setSizePolicy(sizePolicy);
        SearchMusic->setMinimumSize(QSize(1024, 600));
        SearchMusic->setMaximumSize(QSize(1024, 600));
        centralwidget = new QWidget(SearchMusic);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        SongName = new QLineEdit(centralwidget);
        SongName->setObjectName(QString::fromUtf8("SongName"));
        QFont font;
        font.setPointSize(15);
        SongName->setFont(font);
        SongName->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));

        horizontalLayout_2->addWidget(SongName);

        spinBox_SearchNum = new QSpinBox(centralwidget);
        spinBox_SearchNum->setObjectName(QString::fromUtf8("spinBox_SearchNum"));
        spinBox_SearchNum->setFont(font);
        spinBox_SearchNum->setStyleSheet(QString::fromUtf8("color: rgb(85, 170, 255);\n"
"background-color: rgb(47, 47, 47);"));
        spinBox_SearchNum->setMinimum(1);
        spinBox_SearchNum->setMaximum(20);
        spinBox_SearchNum->setValue(5);

        horizontalLayout_2->addWidget(spinBox_SearchNum);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        pBtn_Search = new QPushButton(centralwidget);
        pBtn_Search->setObjectName(QString::fromUtf8("pBtn_Search"));
        pBtn_Search->setFont(font);
        pBtn_Search->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));

        horizontalLayout->addWidget(pBtn_Search);

        pushButton = new QPushButton(centralwidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        pushButton->setFont(font);
        pushButton->setStyleSheet(QString::fromUtf8("color: rgb(85, 255, 255);"));

        horizontalLayout->addWidget(pushButton);


        horizontalLayout_2->addLayout(horizontalLayout);


        verticalLayout->addLayout(horizontalLayout_2);

        listWidget = new QListWidget(centralwidget);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));
        listWidget->setStyleSheet(QString::fromUtf8("color: rgb(85, 170, 255);"));

        verticalLayout->addWidget(listWidget);

        SearchMusic->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(SearchMusic);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        SearchMusic->setStatusBar(statusbar);

        retranslateUi(SearchMusic);

        QMetaObject::connectSlotsByName(SearchMusic);
    } // setupUi

    void retranslateUi(QMainWindow *SearchMusic)
    {
        SearchMusic->setWindowTitle(QApplication::translate("SearchMusic", "MainWindow", nullptr));
        pBtn_Search->setText(QApplication::translate("SearchMusic", "\346\220\234\347\264\242", nullptr));
        pushButton->setText(QApplication::translate("SearchMusic", "\346\267\273\345\212\240", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SearchMusic: public Ui_SearchMusic {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SEARCHMUSIC_H
