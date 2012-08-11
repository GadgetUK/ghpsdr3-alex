#ifndef MORSE_H
#define MORSE_H

#include <QDialog>

namespace Ui {
  class Morse;
}

class Morse : public QDialog
{
  Q_OBJECT
  
public:
  explicit Morse(QWidget *parent = 0);
  ~Morse();
  
private slots:

private:
  Ui::Morse *ui;
};

#endif // MORSE_H
