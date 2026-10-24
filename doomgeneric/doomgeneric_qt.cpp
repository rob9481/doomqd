#include "doomgeneric.h"
#include "doomkeys.h"

#include <stdio.h>

#include <QBackingStore>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QThread>
#include <QWindow>

#define KEYQUEUE_SIZE 16

class DoomWindow : public QWindow {
public:
  DoomWindow();

  void renderNow();
  int getKey(int *pressed, unsigned char *doomKey);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void exposeEvent(QExposeEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  unsigned char convertToDoomKey(int key);
  void addKeyToQueue(int pressed, int keyCode);
  void quit();

  QBackingStore m_backingStore;

  unsigned short m_keyQueue[KEYQUEUE_SIZE];
  unsigned int m_keyQueueWriteIndex = 0;
  unsigned int m_keyQueueReadIndex = 0;
};

static DoomWindow *window = nullptr;
static qint64 startTime;

void DG_Init() {
  window = new DoomWindow();
  startTime = QDateTime::currentMSecsSinceEpoch();
  QCoreApplication::processEvents();
}

void DG_DrawFrame() {
  window->renderNow();
  QCoreApplication::processEvents(); // pump events
}

void DG_SleepMs(uint32_t ms) { QThread::msleep(ms); }

uint32_t DG_GetTicksMs() {
  return QDateTime::currentMSecsSinceEpoch() - startTime;
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
  return window->getKey(pressed, doomKey);
}

void DG_SetWindowTitle(const char *title) { window->setTitle(title); }

DoomWindow::DoomWindow() : m_backingStore(this) {
  installEventFilter(this);
  resize(QSize(DOOMGENERIC_RESX, DOOMGENERIC_RESY));
  show();
}

void DoomWindow::renderNow() {
  if (!isExposed())
    return;

  if (m_backingStore.size() != size())
    m_backingStore.resize(size());

  // Fit image to window
  qreal doomAspect = qreal(DOOMGENERIC_RESX) / qreal(DOOMGENERIC_RESY);
  qreal winAspect = qreal(width()) / qreal(height());
  QSizeF size;
  if (winAspect > doomAspect) {
    size.setHeight(height());
    size.setWidth(height() * doomAspect);
  } else {
    size.setWidth(width());
    size.setHeight(width() / doomAspect);
  }
  QRectF targetRect(QPointF(0, 0), size);
  targetRect.moveCenter(QPointF(width() / 2.0, height() / 2.0));

  // Wrap DOOM's framebuffer without copying
  auto img = QImage((const uchar *)DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY,
                    QImage::Format_ARGB32)
             .convertToFormat(QImage::Format_RGB32);

  QRect rect(0, 0, width(), height());
  m_backingStore.beginPaint(rect);

  QPainter painter(m_backingStore.paintDevice());
  painter.fillRect(rect, Qt::black);
  painter.drawImage(targetRect, img);
  painter.end();

  m_backingStore.endPaint();
  m_backingStore.flush(rect);
}

int DoomWindow::getKey(int *pressed, unsigned char *doomKey) {
  if (m_keyQueueReadIndex == m_keyQueueWriteIndex) {
    // key queue is empty
    return 0;
  } else {
    unsigned short keyData = m_keyQueue[m_keyQueueReadIndex];
    m_keyQueueReadIndex++;
    m_keyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
  }
}

void DoomWindow::addKeyToQueue(int pressed, int keyCode) {
  unsigned char key = convertToDoomKey(keyCode);
  unsigned short keyData = (pressed << 8) | key;

  m_keyQueue[m_keyQueueWriteIndex] = keyData;
  m_keyQueueWriteIndex++;
  m_keyQueueWriteIndex %= KEYQUEUE_SIZE;
}

unsigned char DoomWindow::convertToDoomKey(int key) {
  switch (key) {
  case Qt::Key_Return:
    return KEY_ENTER;
  case Qt::Key_Escape:
    return KEY_ESCAPE;
  case Qt::Key_Left:
    return KEY_LEFTARROW;
  case Qt::Key_Right:
    return KEY_RIGHTARROW;
  case Qt::Key_Up:
    return KEY_UPARROW;
  case Qt::Key_Down:
    return KEY_DOWNARROW;
  case Qt::Key_Control:
    return KEY_FIRE;
  case Qt::Key_Space:
    return KEY_USE;
  case Qt::Key_Shift:
    return KEY_RSHIFT;
  case Qt::Key_Alt:
    return KEY_LALT;
  case Qt::Key_F2:
    return KEY_F2;
  case Qt::Key_F3:
    return KEY_F3;
  case Qt::Key_F4:
    return KEY_F4;
  case Qt::Key_F5:
    return KEY_F5;
  case Qt::Key_F6:
    return KEY_F6;
  case Qt::Key_F7:
    return KEY_F7;
  case Qt::Key_F8:
    return KEY_F8;
  case Qt::Key_F9:
    return KEY_F9;
  case Qt::Key_F10:
    return KEY_F10;
  case Qt::Key_F11:
    return KEY_F11;
  case Qt::Key_Equal:
  case Qt::Key_Plus:
    return KEY_EQUALS;
  case Qt::Key_Minus:
    return KEY_MINUS;
  }
  return tolower(key);
}

void DoomWindow::quit() {
  puts("Quit requested");
  atexit(QCoreApplication::quit);
  exit(1);
}

bool DoomWindow::eventFilter(QObject *obj, QEvent *event) {
  switch (event->type()) {
  // case QEvent::UpdateRequest:
  //   renderNow();
  //   return true;
  case QEvent::KeyPress: {
    auto keyCombo = static_cast<QKeyEvent *>(event)->keyCombination();
    if ((keyCombo.keyboardModifiers() & Qt::ControlModifier) &&
        keyCombo.key() == Qt::Key_C) {
      quit();
    }

    addKeyToQueue(1, static_cast<QKeyEvent *>(event)->key());
    return true;
  }
  case QEvent::KeyRelease: {
    // Ignore fake releases from auto-repeat; doom tracks key state!
    if (!static_cast<QKeyEvent *>(event)->isAutoRepeat())
      addKeyToQueue(0, static_cast<QKeyEvent *>(event)->key());
    return true;
  }
  case QEvent::Close: {
    quit();
  }
  default: {
    return QWindow::eventFilter(obj, event);
  }
  }
}

void DoomWindow::exposeEvent(QExposeEvent *) {
  if (isExposed())
    renderNow();
}

void DoomWindow::resizeEvent(QResizeEvent *) { m_backingStore.resize(size()); }

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  doomgeneric_Create(argc, argv);

  for (int i = 0;; i++) {
    doomgeneric_Tick();
  }

  return 0;
}
