#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QTimer>
#include <QWidget>

#include <memory>

namespace tvg {
class LottieAnimation;
class SwCanvas;
}

class ThorVGWidget final : public QWidget {
public:
  explicit ThorVGWidget(QWidget *parent = nullptr);
  ~ThorVGWidget() override;

  bool setSource(const QString &filePath);
  QString errorString() const;

  void play();
  void pause();
  void stop();
  bool isPlaying() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  bool initializeCanvas();
  bool renderFrame(float frameNumber);
  void updateFrame();
  void setError(const QString &message);

  QTimer timer_;
  QElapsedTimer playbackClock_;
  qint64 pausedElapsedMs_ = 0;
  bool playing_ = false;
  bool thorvgInitialized_ = false;
  QString errorString_;
  QImage frameBuffer_;
  std::unique_ptr<tvg::LottieAnimation> animation_;
  std::unique_ptr<tvg::SwCanvas> canvas_;
  float sourceWidth_ = 0.0f;
  float sourceHeight_ = 0.0f;
};
