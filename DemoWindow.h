#pragma once

#include <QMainWindow>
#include <QString>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class ThorVGWidget;

class DemoWindow final : public QMainWindow {
public:
  explicit DemoWindow(QWidget *parent = nullptr);
  void openFile(const QString &filePath);

private:
  void createControls();
  void refreshControls();
  void refreshPlaybackStatus();

  ThorVGWidget *animation_ = nullptr;
  QPushButton *playButton_ = nullptr;
  QPushButton *stopButton_ = nullptr;
  QComboBox *markerBox_ = nullptr;
  QCheckBox *loopBox_ = nullptr;
  QSlider *timeline_ = nullptr;
  QLabel *timeLabel_ = nullptr;
  QTimer statusTimer_;
};
