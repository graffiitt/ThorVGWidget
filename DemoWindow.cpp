#include "DemoWindow.h"
#include "ThorVGWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QString formatTime(double seconds) {
  const int totalMilliseconds = qMax(0, qRound(seconds * 1000.0));
  const int minutes = totalMilliseconds / 60000;
  const int wholeSeconds = (totalMilliseconds / 1000) % 60;
  const int milliseconds = totalMilliseconds % 1000;
  return QStringLiteral("%1:%2.%3")
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(wholeSeconds, 2, 10, QLatin1Char('0'))
      .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

} // namespace

DemoWindow::DemoWindow(QWidget *parent) : QMainWindow(parent) {
  createControls();
  resize(800, 620);
  setWindowTitle(QStringLiteral("ThorVG — просмотр анимации"));

  statusTimer_.setInterval(50);
  connect(&statusTimer_, &QTimer::timeout, this,
          [this] { refreshPlaybackStatus(); });
  statusTimer_.start();
}

void DemoWindow::createControls() {
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  animation_ = new ThorVGWidget(central);
  layout->addWidget(animation_, 1);

  auto *fileButton = new QPushButton(QStringLiteral("Открыть…"), central);
  playButton_ = new QPushButton(QStringLiteral("Пауза"), central);
  stopButton_ = new QPushButton(QStringLiteral("Стоп"), central);
  markerBox_ = new QComboBox(central);
  loopBox_ = new QCheckBox(QStringLiteral("Повтор"), central);
  timeline_ = new QSlider(Qt::Horizontal, central);
  timeLabel_ = new QLabel(QStringLiteral("00:00.000 / 00:00.000"), central);

  markerBox_->addItem(QStringLiteral("Вся анимация"), QString());
  loopBox_->setChecked(true);
  timeline_->setRange(0, 0);

  auto *controls = new QHBoxLayout;
  controls->addWidget(fileButton);
  controls->addWidget(playButton_);
  controls->addWidget(stopButton_);
  controls->addWidget(markerBox_, 1);
  controls->addWidget(loopBox_);
  layout->addLayout(controls);

  auto *timelineLayout = new QHBoxLayout;
  timelineLayout->addWidget(timeline_, 1);
  timelineLayout->addWidget(timeLabel_);
  layout->addLayout(timelineLayout);
  setCentralWidget(central);

  connect(fileButton, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Открыть анимацию"),
        QApplication::applicationDirPath(),
        QStringLiteral("Lottie (*.json *.lot);;Все файлы (*)"));
    openFile(path);
  });
  connect(playButton_, &QPushButton::clicked, this, [this] {
    if (animation_->isPlaying())
      animation_->pause();
    else
      animation_->play();
    refreshPlaybackStatus();
  });
  connect(stopButton_, &QPushButton::clicked, animation_, [this] {
    animation_->stop();
    refreshPlaybackStatus();
  });
  connect(loopBox_, &QCheckBox::toggled, animation_,
          [this](bool checked) { animation_->setLooping(checked); });
  connect(markerBox_, &QComboBox::activated, this, [this](int index) {
    const QString marker = markerBox_->itemData(index).toString();
    const bool ok = marker.isEmpty() ? animation_->clearSegment()
                                     : animation_->setMarkerSegment(marker);
    if (!ok) {
      QMessageBox::warning(this, QStringLiteral("Ошибка"),
                           animation_->errorString());
      return;
    }
    animation_->play();
    refreshPlaybackStatus();
  });
  connect(timeline_, &QSlider::sliderMoved, animation_, [this](int frame) {
    animation_->seekFrame(static_cast<float>(frame));
  });
  connect(timeline_, &QSlider::sliderReleased, animation_, [this] {
    animation_->seekFrame(static_cast<float>(timeline_->value()));
  });

  refreshControls();
}

void DemoWindow::openFile(const QString &filePath) {
  if (filePath.isEmpty())
    return;
  if (!animation_->setSource(filePath)) {
    QMessageBox::warning(this, QStringLiteral("Ошибка загрузки"),
                         animation_->errorString());
    return;
  }

  setWindowTitle(
      QStringLiteral("ThorVG — %1").arg(QFileInfo(filePath).fileName()));
  const QSignalBlocker blocker(markerBox_);
  markerBox_->clear();
  markerBox_->addItem(QStringLiteral("Вся анимация"), QString());
  for (const QString &marker : animation_->markers())
    markerBox_->addItem(marker, marker);
  refreshControls();
  refreshPlaybackStatus();
}

void DemoWindow::refreshControls() {
  const bool loaded = animation_ && animation_->totalFrames() > 0.0f;
  playButton_->setEnabled(loaded);
  stopButton_->setEnabled(loaded);
  markerBox_->setEnabled(loaded && markerBox_->count() > 1);
  loopBox_->setEnabled(loaded);
  timeline_->setEnabled(loaded);

  const int maxFrame =
      loaded ? qMax(0, qCeil(animation_->totalFrames()) - 1) : 0;
  timeline_->setRange(0, maxFrame);
}

void DemoWindow::refreshPlaybackStatus() {
  if (!animation_ || animation_->totalFrames() <= 0.0f)
    return;

  if (!timeline_->isSliderDown()) {
    const QSignalBlocker blocker(timeline_);
    timeline_->setValue(qRound(animation_->currentFrame()));
  }

  const double duration = animation_->duration();
  const double currentTime =
      duration * animation_->currentFrame() / animation_->totalFrames();
  timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(currentTime),
                                                    formatTime(duration)));
  playButton_->setText(animation_->isPlaying() ? QStringLiteral("Пауза")
                                               : QStringLiteral("Продолжить"));
}
