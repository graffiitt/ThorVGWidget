#include "ThorVGWidget.h"

#include <QFile>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QDebug>

#include <thorvg.h>

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
  canvas_.reset();
  animation_.reset();
  if (thorvgInitialized_ && tvg::Initializer::term() != tvg::Result::Success)
    qWarning() << "Не удалось корректно завершить ThorVG";
}

bool ThorVGWidget::setSource(const QString &filePath) {
  pause();
  pausedElapsedMs_ = 0;
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

  animation_.reset(tvg::Animation::gen());
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

  playing_ = true;
  playbackClock_.restart();
  timer_.start();
}

void ThorVGWidget::pause() {
  if (!playing_)
    return;

  pausedElapsedMs_ += playbackClock_.elapsed();
  timer_.stop();
  playing_ = false;
}

void ThorVGWidget::stop() {
  timer_.stop();
  playing_ = false;
  pausedElapsedMs_ = 0;
  if (animation_)
    renderFrame(0.0f);
}

bool ThorVGWidget::isPlaying() const { return playing_; }

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

  canvas_.reset();
  frameBuffer_ = QImage(pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied);
  frameBuffer_.setDevicePixelRatio(devicePixelRatio);
  if (frameBuffer_.isNull()) {
    setError(QStringLiteral("Не удалось выделить буфер для ThorVG"));
    return false;
  }

  canvas_.reset(tvg::SwCanvas::gen());
  if (!canvas_) {
    frameBuffer_ = {};
    setError(QStringLiteral("Не удалось создать software canvas ThorVG"));
    return false;
  }

  const auto targetResult = canvas_->target(
      reinterpret_cast<uint32_t *>(frameBuffer_.bits()),
      static_cast<uint32_t>(frameBuffer_.bytesPerLine() / 4), pixelWidth,
      pixelHeight, tvg::ColorSpace::ARGB8888);
  if (targetResult != tvg::Result::Success) {
    canvas_.reset();
    frameBuffer_ = {};
    setError(QStringLiteral("Не удалось настроить буфер ThorVG (код %1)")
                 .arg(static_cast<int>(targetResult)));
    return false;
  }

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

  const qint64 elapsedMs = pausedElapsedMs_ + playbackClock_.elapsed();
  const float duration = animation_->duration();
  const float elapsedSeconds = static_cast<float>(elapsedMs) / 1000.0f;
  const float progress = std::fmod(elapsedSeconds, duration) / duration;
  if (!renderFrame(progress * animation_->totalFrame()))
    pause();
}

void ThorVGWidget::setError(const QString &message) {
  errorString_ = message;
  qWarning().noquote() << message;
  update();
}
