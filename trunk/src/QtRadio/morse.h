#ifndef MORSE_H
#define MORSE_H

#include <QDialog>
#include <QSettings>

namespace Ui {
  class Morse;
}

class Morse : public QDialog
{
  Q_OBJECT
  
public:
  explicit Morse(QWidget *parent = 0);
  ~Morse();
// Functions
  void readSettings(QSettings* settings);
  void writeSettings(QSettings* settings);
  
private slots:

private:
  Ui::Morse *ui;
  struct charFrame {
    unsigned char elementCount;
    unsigned char letterCode;
  };
// Functions
  charFrame ascii2cw(char letter);
};

#endif // MORSE_H
