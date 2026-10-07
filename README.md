# ThorVG Qt Widget

A lightweight Qt 6 widget for displaying and controlling Lottie animations
rendered with ThorVG's software canvas.

The project includes a small demo application with playback controls, a frame
timeline, loop toggle, and marker selection.

## Features

- Load Lottie animations from a file.
- Play, pause, and stop playback.
- Enable or disable looping.
- Seek to a frame and read the current frame, frame count, and duration.
- List Lottie markers and play a marker or a custom frame range.
- Scale the animation to fit the widget while preserving its aspect ratio.
- Render into a Qt image buffer using ThorVG's software renderer.

## Requirements

- CMake 3.16 or newer
- C++17 compiler
- Qt 6 Widgets
- ThorVG with the `thorvg-1` pkg-config module
- `pkg-config`

Make sure `pkg-config --modversion thorvg-1` succeeds and that the Qt 6 and
ThorVG development files are discoverable in your environment.

## Build

```sh
cmake -S . -B build
cmake --build build
```

On platforms where CMake creates a regular executable, it is located at
`build/thorvg_widget`. On macOS, it is inside the application bundle:

```sh
./build/thorvg_widget.app/Contents/MacOS/thorvg_widget
```

Pass an animation path to open it immediately; without an argument, use the
demo window's **Open** button:

```sh
./build/thorvg_widget.app/Contents/MacOS/thorvg_widget /path/to/animation.json
```

## Use `ThorVGWidget`

Add `ThorVGWidget.cpp` and `ThorVGWidget.h` to your Qt application, link against
Qt 6 Widgets and ThorVG, then load an animation:

```cpp
#include "ThorVGWidget.h"

auto *animation = new ThorVGWidget(parent);
if (!animation->setSource("/path/to/animation.json")) {
    qWarning() << animation->errorString();
}

layout->addWidget(animation);
```

Playback starts automatically after a successful load. Use the public API to
control playback or select a marker:

```cpp
animation->pause();
animation->setLooping(false);
animation->seekFrame(12);
animation->play();

const QStringList markers = animation->markers();
if (!markers.isEmpty())
    animation->setMarkerSegment(markers.first());
```

`setSegment(beginFrame, endFrame)` selects a frame range, and `clearSegment()`
restores the whole animation. `setTransparentBackground(true)` is available
when the widget is used as an animation overlay.
