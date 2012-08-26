#ifndef MORSE_H
#define MORSE_H

#include <QDialog>
#include <QSettings>

enum {_ditsp, _dahsp, _elesp, _ltrsp, _wrdsp};

namespace Ui {
  class Morse;
}

class Send_Elements : public QObject
{
  Q_OBJECT
public:
  explicit Send_Elements(QObject *parent = 0);
  int test;

public slots:
  void doElements(QString);

private:
  struct charFrame {
    unsigned char elementCount;
    unsigned char letterCode;
  };
  // Private Functions
    charFrame ascii2cw(char letter);
    void sendCW(int el_type);

signals:
};

class Morse : public QDialog
{
  Q_OBJECT

public:
  explicit Morse(QWidget *parent = 0);
  ~Morse();
// Public Functions
  void readSettings(QSettings* settings);
  void writeSettings(QSettings* settings);
  void keyPressEvent(QKeyEvent *event);
// Public Variables
  bool cwMode;

private slots:

private:
  Ui::Morse *ui;

// Private Functions
  int sendBuffer(int editBox);

signals:
  void doWork(QString);
};

#endif // MORSE_H
