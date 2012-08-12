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

  void on_pbSave_1_clicked();

private:
  Ui::Morse *ui;
};

#endif // MORSE_H
