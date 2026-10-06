#include "ThorVGWidget.h"

#include <QFile>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QDebug>

#include <thorvg.h>
#include <thorvg_lottie.h>

#include <algorithm>
#include <cmath>

ThorVGWidget::ThorVGWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(64, 64);
  setAttribute(Qt::WA_OpaquePaintEvent);

  const auto result = tvg::Initializer::init(0);
  if (result != tvg::Result::Success) {
    setError(QStringLiteral("Не удалось инициализировать ThorVG (код %1)")
                 .arg(static_cast<int>(result)));
    return;
  }
  thorvgInitialized_ = true;

  timer_.setInterval(16);
  connect(&timer_, &QTimer::timeout, this, [this] { updateFrame(); });
}

ThorVGWidget::~ThorVGWidget() {
  timer_.stop();
  if (canvas_ && canvas_->sync() != tvg::Result::Success)
    qWarning() << "Не удалось завершить задачи ThorVG перед закрытием виджета";
  canvas_.reset();
  animation_.reset();
  if (thorvgInitialized_) {
    const auto result = tvg::Initializer::term();
    if (result == tvg::Result::InsufficientCondition) {
      qInfo() << "ThorVG уже был завершён";
    } else if (result != tvg::Result::Success) {
      qWarning() << "Не удалось завершить ThorVG, код:"
                 << static_cast<int>(result);
    }
  }
}

bool ThorVGWidget::setSource(const QString &filePath) {
  pause();
  playheadSeconds_ = 0.0;
  canvas_.reset();
  animation_.reset();
  frameBuffer_ = {};
  sourceWidth_ = 0.0f;
  sourceHeight_ = 0.0f;

  if (!thorvgInitialized_)
    return false;
  if (filePath.isEmpty()) {
    setError(QStringLiteral("Путь к файлу анимации не задан"));
    return false;
  }

  animation_.reset(tvg::LottieAnimation::gen());
  if (!animation_) {
    setError(QStringLiteral("Не удалось создать анимацию ThorVG"));
    return false;
  }

  const QByteArray encodedPath = QFile::encodeName(filePath);
  auto *picture = animation_->picture();
  const auto loadResult = picture->load(encodedPath.constData());
  if (loadResult != tvg::Result::Success) {
    animation_.reset();
    setError(QStringLiteral("Не удалось загрузить «%1» (код ThorVG: %2)")
                 .arg(filePath)
                 .arg(static_cast<int>(loadResult)));
    return false;
  }

  const auto sizeResult = picture->size(&sourceWidth_, &sourceHeight_);
  if (sizeResult != tvg::Result::Success) {
    animation_.reset();
    setError(QStringLiteral("Не удалось получить размер анимации (код %1)")
                 .arg(static_cast<int>(sizeResult)));
    return false;
  }
  if (sourceWidth_ <= 0.0f || sourceHeight_ <= 0.0f ||
      animation_->totalFrame() <= 0.0f || animation_->duration() <= 0.0f) {
    animation_.reset();
    setError(QStringLiteral("Файл не содержит корректную анимацию"));
    return false;
  }

  errorString_.clear();
  if (!renderFrame(0.0f))
    return false;
  play();
  return true;
}

QString ThorVGWidget::errorString() const { return errorString_; }

void ThorVGWidget::play() {
  if (!animation_ || playing_)
    return;

  if (!looping_ && playheadSeconds_ >= animation_->duration()) {
    playheadSeconds_ = 0.0;
    if (!renderFrame(0.0f))
      return;
  }
  playing_ = true;
  playbackBaseMs_ = 0;
  playbackClock_.restart();
  timer_.start();
}

void ThorVGWidget::pause() {
  if (!playing_)
    return;

  playbackBaseMs_ += playbackClock_.elapsed();
  if (animation_)
    playheadSeconds_ = std::min(
        static_cast<double>(animation_->duration()),
        playheadSeconds_ +
            static_cast<double>(playbackBaseMs_) * playbackSpeed_ / 1000.0);
  playbackBaseMs_ = 0;
  timer_.stop();
  playing_ = false;
}

void ThorVGWidget::stop() {
  timer_.stop();
  playing_ = false;
  playheadSeconds_ = 0.0;
  if (animation_)
    renderFrame(0.0f);
}

bool ThorVGWidget::isPlaying() const { return playing_; }

void ThorVGWidget::setLooping(bool enabled) { looping_ = enabled; }

bool ThorVGWidget::isLooping() const { return looping_; }

bool ThorVGWidget::setPlaybackSpeed(float speed) {
  if (!std::isfinite(speed) || speed <= 0.0f) {
    setError(QStringLiteral("Скорость воспроизведения должна быть больше нуля"));
    return false;
  }

  if (playing_) {
    playbackBaseMs_ += playbackClock_.elapsed();
    playheadSeconds_ +=
        static_cast<double>(playbackBaseMs_) * playbackSpeed_ / 1000.0;
    playbackBaseMs_ = 0;
    playbackClock_.restart();
  }
  playbackSpeed_ = speed;
  return true;
}

float ThorVGWidget::playbackSpeed() const { return playbackSpeed_; }

bool ThorVGWidget::seekFrame(float frameNumber) {
  if (!animation_ || !std::isfinite(frameNumber)) {
    setError(QStringLiteral("Нельзя перейти к кадру: анимация не загружена"));
    return false;
  }

  const float maxFrame = std::max(0.0f, animation_->totalFrame() - 1.0f);
  const float clampedFrame = std::clamp(frameNumber, 0.0f, maxFrame);
  playheadSeconds_ = animation_->totalFrame() > 0.0f
                         ? static_cast<double>(animation_->duration()) *
                               clampedFrame / animation_->totalFrame()
                         : 0.0;
  if (playing_) {
    playbackBaseMs_ = 0;
    playbackClock_.restart();
  }
  return renderFrame(clampedFrame);
}

float ThorVGWidget::currentFrame() const {
  return animation_ ? animation_->curFrame() : 0.0f;
}

float ThorVGWidget::totalFrames() const {
  return animation_ ? animation_->totalFrame() : 0.0f;
}

float ThorVGWidget::duration() const {
  return animation_ ? animation_->duration() : 0.0f;
}

QStringList ThorVGWidget::markers() const {
  QStringList names;
  if (!animation_)
    return names;

  const uint32_t count = animation_->markersCnt();
  for (uint32_t index = 0; index < count; ++index) {
    const char *name = animation_->marker(index, nullptr, nullptr);
    if (name)
      names.append(QString::fromUtf8(name));
  }
  return names;
}

bool ThorVGWidget::setSegment(float beginFrame, float endFrame) {
  if (!animation_ || !std::isfinite(beginFrame) ||
      !std::isfinite(endFrame)) {
    setError(QStringLiteral("Неверный диапазон сегмента анимации"));
    return false;
  }

  pause();
  const auto result =
      static_cast<tvg::Animation *>(animation_.get())->segment(beginFrame,
                                                                endFrame);
  if (result != tvg::Result::Success) {
    setError(QStringLiteral("Не удалось задать сегмент (код ThorVG: %1)")
                 .arg(static_cast<int>(result)));
    return false;
  }
  playheadSeconds_ = 0.0;
  return renderFrame(0.0f);
}

bool ThorVGWidget::setMarkerSegment(const QString &markerName) {
  if (!animation_ || markerName.isEmpty()) {
    setError(QStringLiteral("Не удалось выбрать маркер анимации"));
    return false;
  }

  pause();
  const QByteArray encodedName = markerName.toUtf8();
  const auto result = animation_->segment(encodedName.constData());
  if (result != tvg::Result::Success) {
    setError(QStringLiteral("Не удалось выбрать маркер «%1» (код ThorVG: %2)")
                 .arg(markerName)
                 .arg(static_cast<int>(result)));
    return false;
  }
  playheadSeconds_ = 0.0;
  return renderFrame(0.0f);
}

bool ThorVGWidget::clearSegment() {
  if (!animation_) {
    setError(QStringLiteral("Анимация не загружена"));
    return false;
  }

  pause();
  const auto result = animation_->segment(static_cast<const char *>(nullptr));
  if (result != tvg::Result::Success) {
    setError(QStringLiteral("Не удалось сбросить сегмент (код ThorVG: %1)")
                 .arg(static_cast<int>(result)));
    return false;
  }
  playheadSeconds_ = 0.0;
  return renderFrame(0.0f);
}

void ThorVGWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.fillRect(rect(), palette().window());
  if (!frameBuffer_.isNull()) {
    const QSizeF logicalSize(frameBuffer_.width() / frameBuffer_.devicePixelRatio(),
                             frameBuffer_.height() / frameBuffer_.devicePixelRatio());
    const QRectF target(QPointF((width() - logicalSize.width()) / 2.0,
                                (height() - logicalSize.height()) / 2.0),
                        logicalSize);
    painter.drawImage(target, frameBuffer_);
    return;
  }

  const QString message =
      errorString_.isEmpty()
          ? QStringLiteral("Выберите файл анимации Lottie (.json или .lot)")
          : errorString_;
  painter.setPen(palette().text().color());
  painter.drawText(rect().adjusted(16, 16, -16, -16),
                   Qt::AlignCenter | Qt::TextWordWrap, message);
}

void ThorVGWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (animation_)
    renderFrame(animation_->curFrame());
}

bool ThorVGWidget::initializeCanvas() {
  const qreal devicePixelRatio = devicePixelRatioF();
  const int pixelWidth = std::max(1, qRound(width() * devicePixelRatio));
  const int pixelHeight = std::max(1, qRound(height() * devicePixelRatio));
  if (frameBuffer_.size() == QSize(pixelWidth, pixelHeight) &&
      qFuzzyCompare(frameBuffer_.devicePixelRatio(), devicePixelRatio) && canvas_)
    return true;

  QImage newFrameBuffer(pixelWidth, pixelHeight,
                        QImage::Format_ARGB32_Premultiplied);
  newFrameBuffer.setDevicePixelRatio(devicePixelRatio);
  if (newFrameBuffer.isNull()) {
    setError(QStringLiteral("Не удалось выделить буфер для ThorVG"));
    return false;
  }

  const bool createCanvas = !canvas_;
  if (createCanvas) {
    canvas_.reset(tvg::SwCanvas::gen());
    if (!canvas_) {
      setError(QStringLiteral("Не удалось создать software canvas ThorVG"));
      return false;
    }
  }

  const auto targetResult = canvas_->target(
      reinterpret_cast<uint32_t *>(newFrameBuffer.bits()),
      static_cast<uint32_t>(newFrameBuffer.bytesPerLine() / 4), pixelWidth,
      pixelHeight, tvg::ColorSpace::ARGB8888);
  if (targetResult != tvg::Result::Success) {
    if (createCanvas)
      canvas_.reset();
    setError(QStringLiteral("Не удалось настроить буфер ThorVG (код %1)")
                 .arg(static_cast<int>(targetResult)));
    return false;
  }

  frameBuffer_ = std::move(newFrameBuffer);
  if (!createCanvas)
    return true;

  auto *picture = animation_->picture();
  if (picture->ref() == 0) {
    canvas_.reset();
    setError(QStringLiteral("Не удалось увеличить счетчик ссылок ThorVG"));
    return false;
  }
  const auto addResult = canvas_->add(picture);
  if (addResult != tvg::Result::Success) {
    picture->unref();
    canvas_.reset();
    setError(QStringLiteral("Не удалось добавить анимацию на canvas (код %1)")
                 .arg(static_cast<int>(addResult)));
    return false;
  }

  return true;
}

bool ThorVGWidget::renderFrame(float frameNumber) {
  if (!animation_ || !initializeCanvas())
    return false;

  const int pixelWidth = frameBuffer_.width();
  const int pixelHeight = frameBuffer_.height();
  const float scale =
      std::min(pixelWidth / sourceWidth_, pixelHeight / sourceHeight_);
  const float renderedWidth = sourceWidth_ * scale;
  const float renderedHeight = sourceHeight_ * scale;
  auto *picture = animation_->picture();

  auto result = picture->size(renderedWidth, renderedHeight);
  if (result != tvg::Result::Success) {
    setError(QStringLiteral("Не удалось изменить размер анимации (код %1)")
                 .arg(static_cast<int>(result)));
    return false;
  }
  result = picture->translate((pixelWidth - renderedWidth) / 2.0f,
                              (pixelHeight - renderedHeight) / 2.0f);
  if (result != tvg::Result::Success) {
    setError(QStringLiteral("Не удалось разместить анимацию (код %1)")
                 .arg(static_cast<int>(result)));
    return false;
  }
  result = animation_->frame(frameNumber);
  if (result != tvg::Result::Success &&
      result != tvg::Result::InsufficientCondition) {
    setError(QStringLiteral("Не удалось установить кадр анимации (код %1)")
                 .arg(static_cast<int>(result)));
    return false;
  }

  const auto updateResult = canvas_->update();
  if (updateResult != tvg::Result::Success) {
    setError(QStringLiteral("Ошибка обновления кадра ThorVG (код %1)")
                 .arg(static_cast<int>(updateResult)));
    return false;
  }
  const auto drawResult = canvas_->draw(true);
  if (drawResult != tvg::Result::Success) {
    setError(QStringLiteral("Ошибка отрисовки ThorVG (код %1)")
                 .arg(static_cast<int>(drawResult)));
    return false;
  }
  const auto syncResult = canvas_->sync();
  if (syncResult != tvg::Result::Success) {
    setError(QStringLiteral("Ошибка завершения отрисовки ThorVG (код %1)")
                 .arg(static_cast<int>(syncResult)));
    return false;
  }

  errorString_.clear();
  update();
  return true;
}

void ThorVGWidget::updateFrame() {
  if (!animation_)
    return;

  const float duration = animation_->duration();
  if (duration <= 0.0f)
    return;

  const double elapsedSeconds =
      static_cast<double>(playbackBaseMs_ + playbackClock_.elapsed()) *
      playbackSpeed_ / 1000.0;
  const double position = playheadSeconds_ + elapsedSeconds;
  if (!looping_ && position >= duration) {
    const float finalFrame = std::max(0.0f, animation_->totalFrame() - 1.0f);
    if (renderFrame(finalFrame)) {
      playheadSeconds_ = duration;
      pause();
    }
    return;
  }

  const double localPosition = looping_ ? std::fmod(position, duration) : position;
  const float frameNumber = static_cast<float>(
      localPosition / duration * animation_->totalFrame());
  if (!renderFrame(frameNumber))
    pause();
}

void ThorVGWidget::setError(const QString &message) {
  errorString_ = message;
  qWarning().noquote() << message;
  update();
}
