#include "morse.h"
#include "ui_morse.h"
#include <QDebug>

Morse::Morse(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::Morse)
{
  ui->setupUi(this);

}

Morse::~Morse()
{
  delete ui;
}
