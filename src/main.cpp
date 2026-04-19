#include <QApplication>
#include "just_test.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    JustTestGame view;
    view.setWindowTitle("Just Test - Subway Runner");
    view.show();
    return a.exec();
}