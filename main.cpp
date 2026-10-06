#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>

#include "ThorVGWidget.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

   QString filePath = QStringLiteral("/Users/vlad/programming/qt/thorvg_widget/Moods.json");

  ThorVGWidget widget;
  widget.setWindowTitle(QStringLiteral("ThorVG — %1")
                            .arg(QFileInfo(filePath).fileName()));
  widget.resize(640, 480);
  widget.setSource(filePath);
  widget.show();

  return app.exec();
}
