#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include "ThorVGWidget.h"

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

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QMainWindow window;
  auto *central = new QWidget(&window);
  auto *layout = new QVBoxLayout(central);
  auto *animation = new ThorVGWidget(central);
  layout->addWidget(animation, 1);

  auto *fileButton = new QPushButton(QStringLiteral("Открыть…"), central);
  auto *playButton = new QPushButton(QStringLiteral("Пауза"), central);
  auto *stopButton = new QPushButton(QStringLiteral("Стоп"), central);
  auto *markerBox = new QComboBox(central);
  auto *loopBox = new QCheckBox(QStringLiteral("Повтор"), central);
  auto *speedBox = new QDoubleSpinBox(central);
  auto *timeline = new QSlider(Qt::Horizontal, central);
  auto *timeLabel = new QLabel(QStringLiteral("00:00.000 / 00:00.000"), central);

  markerBox->addItem(QStringLiteral("Вся анимация"), QString());
  loopBox->setChecked(true);
  speedBox->setRange(0.1, 4.0);
  speedBox->setSingleStep(0.1);
  speedBox->setDecimals(2);
  speedBox->setValue(1.0);
  speedBox->setSuffix(QStringLiteral("×"));
  timeline->setRange(0, 0);

  auto *controls = new QHBoxLayout;
  controls->addWidget(fileButton);
  controls->addWidget(playButton);
  controls->addWidget(stopButton);
  controls->addWidget(markerBox, 1);
  controls->addWidget(loopBox);
  controls->addWidget(new QLabel(QStringLiteral("Скорость:"), central));
  controls->addWidget(speedBox);
  layout->addLayout(controls);

  auto *timelineLayout = new QHBoxLayout;
  timelineLayout->addWidget(timeline, 1);
  timelineLayout->addWidget(timeLabel);
  layout->addLayout(timelineLayout);

  window.setCentralWidget(central);
  window.resize(800, 620);
  window.setWindowTitle(QStringLiteral("ThorVG — просмотр анимации"));

  auto refreshControls = [=] {
    const bool loaded = animation->totalFrames() > 0.0f;
    playButton->setEnabled(loaded);
    stopButton->setEnabled(loaded);
    markerBox->setEnabled(loaded && markerBox->count() > 1);
    loopBox->setEnabled(loaded);
    speedBox->setEnabled(loaded);
    timeline->setEnabled(loaded);
    const int maxFrame =
        loaded ? qMax(0, qCeil(animation->totalFrames()) - 1) : 0;
    timeline->setRange(0, maxFrame);
  };

  auto openFile = [=, &window](const QString &path) {
    if (path.isEmpty())
      return;
    if (!animation->setSource(path)) {
      QMessageBox::warning(&window, QStringLiteral("Ошибка загрузки"),
                           animation->errorString());
      return;
    }

    window.setWindowTitle(QStringLiteral("ThorVG — %1")
                              .arg(QFileInfo(path).fileName()));
    const QSignalBlocker blocker(markerBox);
    markerBox->clear();
    markerBox->addItem(QStringLiteral("Вся анимация"), QString());
    for (const QString &marker : animation->markers())
      markerBox->addItem(marker, marker);
    refreshControls();
  };

  QObject::connect(fileButton, &QPushButton::clicked, &window, [=, &window] {
    const QString path = QFileDialog::getOpenFileName(
        &window, QStringLiteral("Открыть анимацию"), {},
        QStringLiteral("Lottie (*.json *.lot);;Все файлы (*)"));
    openFile(path);
  });
  QObject::connect(playButton, &QPushButton::clicked, &window, [=] {
    if (animation->isPlaying()) {
      animation->pause();
      playButton->setText(QStringLiteral("Продолжить"));
    } else {
      animation->play();
      playButton->setText(QStringLiteral("Пауза"));
    }
  });
  QObject::connect(stopButton, &QPushButton::clicked, &window, [=] {
    animation->stop();
    playButton->setText(QStringLiteral("Продолжить"));
  });
  QObject::connect(loopBox, &QCheckBox::toggled, &window,
                   [=, &window](bool checked) { animation->setLooping(checked); });
  QObject::connect(speedBox,
                   qOverload<double>(&QDoubleSpinBox::valueChanged), &window,
                   [=, &window](double speed) {
                     animation->setPlaybackSpeed(static_cast<float>(speed));
                   });
  QObject::connect(markerBox, &QComboBox::activated, &window,
                   [=, &window](int index) {
                     const QString marker = markerBox->itemData(index).toString();
                     const bool ok = marker.isEmpty()
                                         ? animation->clearSegment()
                                         : animation->setMarkerSegment(marker);
                     if (!ok) {
                       QMessageBox::warning(&window, QStringLiteral("Ошибка"),
                                            animation->errorString());
                       return;
                     }
                     refreshControls();
                     playButton->setText(QStringLiteral("Пауза"));
                     animation->play();
                   });
  QObject::connect(timeline, &QSlider::sliderMoved, &window,
                   [=, &window](int frame) {
    animation->seekFrame(static_cast<float>(frame));
  });
  QObject::connect(timeline, &QSlider::sliderReleased, &window,
                   [=, &window] {
    animation->seekFrame(static_cast<float>(timeline->value()));
  });

  QTimer statusTimer;
  statusTimer.setInterval(50);
  QObject::connect(&statusTimer, &QTimer::timeout, &window, [=, &window] {
    if (animation->totalFrames() <= 0.0f)
      return;
    if (!timeline->isSliderDown()) {
      const QSignalBlocker blocker(timeline);
      timeline->setValue(qRound(animation->currentFrame()));
    }
    const double duration = animation->duration();
    const double currentTime =
        animation->totalFrames() > 0.0f
            ? duration * animation->currentFrame() / animation->totalFrames()
            : 0.0;
    timeLabel->setText(QStringLiteral("%1 / %2")
                           .arg(formatTime(currentTime), formatTime(duration)));
    const QString expectedText =
        animation->isPlaying() ? QStringLiteral("Пауза")
                               : QStringLiteral("Продолжить");
    if (playButton->text() != expectedText)
      playButton->setText(expectedText);
  });
  statusTimer.start();

  if (argc > 1)
    openFile(QString::fromLocal8Bit(argv[1]));
  refreshControls();
  window.show();

  return app.exec();
}
