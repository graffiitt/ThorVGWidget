#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QStringList>
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
  void setLooping(bool enabled);
  bool isLooping() const;
  bool setPlaybackSpeed(float speed);
  float playbackSpeed() const;
  bool seekFrame(float frameNumber);
  float currentFrame() const;
  float totalFrames() const;
  float duration() const;
  QStringList markers() const;
  bool setSegment(float beginFrame, float endFrame);
  bool setMarkerSegment(const QString &markerName);
  bool clearSegment();

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
  qint64 playbackBaseMs_ = 0;
  double playheadSeconds_ = 0.0;
  float playbackSpeed_ = 1.0f;
  bool playing_ = false;
  bool looping_ = true;
  bool thorvgInitialized_ = false;
  QString errorString_;
  QImage frameBuffer_;
  std::unique_ptr<tvg::LottieAnimation> animation_;
  std::unique_ptr<tvg::SwCanvas> canvas_;
  float sourceWidth_ = 0.0f;
  float sourceHeight_ = 0.0f;
};
