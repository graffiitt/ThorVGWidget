#pragma once

#include <QImage>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <memory>

namespace tvg {
struct LottieAnimation;
struct SwCanvas;
}

class ThorVGWidget final : public QWidget {
public:
  explicit ThorVGWidget(QWidget *parent = nullptr);
  ~ThorVGWidget() override;

  bool setSource(const QString &filePath);
  QString errorString() const;
  void setTransparentBackground(bool enabled);

  void play();
  void pause();
  void stop();
  bool isPlaying() const;
  void setLooping(bool enabled);
  bool isLooping() const;
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
  void updateTimerInterval();
  void updateFrame();
  void setError(const QString &message);

  QTimer timer_;
  double playbackFrame_ = 0.0;
  bool playing_ = false;
  bool looping_ = true;
  bool thorvgInitialized_ = false;
  bool transparentBackground_ = false;
  QString errorString_;
  QImage frameBuffer_;
  std::unique_ptr<tvg::LottieAnimation> animation_;
  std::unique_ptr<tvg::SwCanvas> canvas_;
  float sourceWidth_ = 0.0f;
  float sourceHeight_ = 0.0f;
};
