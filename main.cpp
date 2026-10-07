#include <QApplication>

#include "DemoWindow.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  DemoWindow window;

  if (argc > 1)
    window.openFile(QString::fromLocal8Bit(argv[1]));
  window.show();

  return app.exec();
}
