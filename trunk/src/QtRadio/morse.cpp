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

void Morse::readSettings(QSettings *settings)
{
  settings->beginGroup("cw");

  qDebug() << __FUNCTION__ << settings->value("cwString1","CQ CQ CQ DE ZL2APV ZL2APV ZL2APV K").toString();
  ui->plainTextEdit_1->setPlainText(settings->value("cwString1","CQ CQ CQ DE ZL2APV ZL2APV ZL2APV K").toString());
  settings->endGroup();
}

void Morse::writeSettings(QSettings *settings)
{
  settings->beginGroup("cw");
  settings->setValue("cwString1", ui->plainTextEdit_1->toPlainText());
  settings->endGroup();
  qDebug() << __FUNCTION__ << ui->plainTextEdit_1->toPlainText();
}

void Morse::on_pbSave_1_clicked()
{
    QSettings settings("G0ORX","QtRadio");

    writeSettings(&settings);
    readSettings(&settings);
}
