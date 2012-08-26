/* File:   Morse.cpp
 * Author: Graeme Jury, ZL2APV
 *
 * Created on 16 August 2012, 20:00
 */

/* Copyright (C)
* 2012 - Graeme Jury, ZL2APV
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include "morse.h"
#include "ui_morse.h"
#include <QDebug>
#include <cctype>
#include <QThread>


Send_Elements::Send_Elements(QObject *parent) :
  QObject(parent)
{
  test = 99;
}

void Send_Elements::doElements(QString buff, unsigned long elTime)
{
  char currentLetter;
  bool curElement; // false = dit, true = dah
  bool wordspaceFlag = false;
  charFrame ltr;

  for (int x = 0; x<buff.length();x++) {
    currentLetter = buff[x].toAscii();
//    ltr = ascii2cw(currentLetter);
    ltr = ascii2cw(currentLetter);
    if (x) { // Don't send a leading letter or word space first time through.
        if (ltr.elementCount == 7) {
          sendCW(_wrdsp, elTime); // Only space has 7 elements
          wordspaceFlag = true; // Indicate we have sent a wordspace so we don't send a letter space as well
        } else {
            if (!wordspaceFlag) sendCW(_ltrsp, elTime);
            wordspaceFlag = false;
        }
    }
    if (ltr.elementCount != 7) { // Don't send the actual word space elments
        for (int cnt=0;cnt<ltr.elementCount;cnt++) {
         curElement = (ltr.letterCode & 0x80); // Only work on the MSB of the 8 bit byte
          ltr.letterCode = ltr.letterCode << 1; // Shift the next bit down to be MSB
      // Transmit current element as either dit or dah
          if (curElement) sendCW(_dahsp, elTime); else sendCW(_ditsp, elTime);
      // Calculate if element space is to be sent
          if (cnt<(ltr.elementCount-1)) sendCW(_elesp, elTime);
        }
    }
  }
}

Send_Elements::charFrame Send_Elements::ascii2cw(char letter)
//Send_Elements::charFrame Send_Elements::charFrame::ascii2cw(char)
//Morse::charFrame Morse::ascii2cw(char letter) // convert an ASCII code to a Morse value
{
  charFrame ltr;

  letter = letter & 0x7f; // Only 7 bit ascii required
  letter = toupper(letter);
  switch(letter)
  {
    case 0x20:
      ltr.elementCount = 7; ltr.letterCode = 0x00; break; // 00000000 Space
    case 0x21:
      ltr.elementCount = 5; ltr.letterCode = 0x40; break; // 01000000 !
    case 0x22:
      ltr.elementCount = 6; ltr.letterCode = 0x48; break; // 01001000 "
    case 0x24:
      ltr.elementCount = 6; ltr.letterCode = 0x14; break; // 00010100 $
    case 0x26:
      ltr.elementCount = 5; ltr.letterCode = 0x40; break; // 01000000 &
    case 0x27:
      ltr.elementCount = 6; ltr.letterCode = 0x78; break; // 01111000 '
    case 0x28:
      ltr.elementCount = 5; ltr.letterCode = 0xB0; break; // 10110000 (
    case 0x29:
      ltr.elementCount = 6; ltr.letterCode = 0xB4; break; // 10110100 )
    case 0x2b:
      ltr.elementCount = 5; ltr.letterCode = 0x50; break; // 01010000 +
    case 0x2c:
      ltr.elementCount = 6; ltr.letterCode = 0xCC; break; // 11001100 ,
    case 0x2d:
      ltr.elementCount = 5; ltr.letterCode = 0x88; break; // 10001000 -
    case 0x2e:
      ltr.elementCount = 6; ltr.letterCode = 0x54; break; // 01010100 .
    case 0x2f:
      ltr.elementCount = 5; ltr.letterCode = 0x90; break; // 10010000 /
    case 0x30:
      ltr.elementCount = 5; ltr.letterCode = 0xF8; break; // 11111000 0
    case 0x31:
      ltr.elementCount = 5; ltr.letterCode = 0x78; break; // 01111000 1
    case 0x32:
      ltr.elementCount = 5; ltr.letterCode = 0x38; break; // 00111000 2
    case 0x33:
      ltr.elementCount = 5; ltr.letterCode = 0x18; break; // 00011000 3
    case 0x34:
      ltr.elementCount = 5; ltr.letterCode = 0x80; break; // 00001000 4
    case 0x35:
      ltr.elementCount = 5; ltr.letterCode = 0x00; break; // 00000000 5
    case 0x36:
      ltr.elementCount = 5; ltr.letterCode = 0x80; break; // 10000000 6
    case 0x37:
      ltr.elementCount = 5; ltr.letterCode = 0xC0; break; // 11000000 7
    case 0x38:
      ltr.elementCount = 5; ltr.letterCode = 0xE0; break; // 11100000 8
    case 0x39:
      ltr.elementCount = 5; ltr.letterCode = 0xF0; break; // 11110000 9
    case 0x40:
      ltr.elementCount = 5; ltr.letterCode = 0x50; break; // 01010000 @
    case 0x41:
      ltr.elementCount = 2; ltr.letterCode = 0x40; break; // 01000000 A
    case 0x42:
      ltr.elementCount = 4; ltr.letterCode = 0x80; break; // 10000000 B
    case 0x43:
      ltr.elementCount = 4; ltr.letterCode = 0xA0; break; // 10100000 C
    case 0x44:
      ltr.elementCount = 3; ltr.letterCode = 0x80; break; // 10000000 D
    case 0x45:
      ltr.elementCount = 1; ltr.letterCode = 0x00; break; // 00000000 E
    case 0x46:
      ltr.elementCount = 4; ltr.letterCode = 0x20; break; // 00100000 F
    case 0x47:
      ltr.elementCount = 3; ltr.letterCode = 0xC0; break; // 11000000 G
    case 0x48:
      ltr.elementCount = 4; ltr.letterCode = 0x00; break; // 00000000 H
    case 0x49:
      ltr.elementCount = 2; ltr.letterCode = 0x00; break; // 00000000 I
    case 0x4a:
      ltr.elementCount = 4; ltr.letterCode = 0x70; break; // 01110000 J
    case 0x4b:
      ltr.elementCount = 3; ltr.letterCode = 0xA0; break; // 10100000 K
    case 0x4c:
      ltr.elementCount = 4; ltr.letterCode = 0x40; break; // 01000000 L
    case 0x4d:
      ltr.elementCount = 2; ltr.letterCode = 0xC0; break; // 11000000 M
    case 0x4e:
      ltr.elementCount = 2; ltr.letterCode = 0x80; break; // 10000000 N
    case 0x4f:
      ltr.elementCount = 3; ltr.letterCode = 0xE0; break; // 11100000 O
    case 0x50:
      ltr.elementCount = 4; ltr.letterCode = 0x60; break; // 01100000 P
    case 0x51:
      ltr.elementCount = 4; ltr.letterCode = 0xD0; break; // 11010000 Q
    case 0x52:
      ltr.elementCount = 3; ltr.letterCode = 0x40; break; // 01000000 R
    case 0x53:
      ltr.elementCount = 3; ltr.letterCode = 0x00; break; // 00000000 S
    case 0x54:
      ltr.elementCount = 1; ltr.letterCode = 0x80; break; // 10000000 T
    case 0x55:
      ltr.elementCount = 3; ltr.letterCode = 0x20; break; // 00100000 U
    case 0x56:
      ltr.elementCount = 4; ltr.letterCode = 0x10; break; // 00010000 V
    case 0x57:
      ltr.elementCount = 3; ltr.letterCode = 0x60; break; // 01100000 W
    case 0x58:
      ltr.elementCount = 4; ltr.letterCode = 0x90; break; // 10010000 X
    case 0x59:
      ltr.elementCount = 4; ltr.letterCode = 0xB0; break; // 10110000 Y
    case 0x5a:
      ltr.elementCount = 4; ltr.letterCode = 0xC0; break; // 11000000 Z
    case 0x5f:
      ltr.elementCount = 6; ltr.letterCode = 0x34; break; // 00110100 _
    default:
      ltr.elementCount = 0xff; ltr.letterCode = 0xff; // Error code to show invalid char
  }
  return ltr;
}

void Send_Elements::sendCW(int el_type, unsigned long elTime)
{
  switch (el_type) {
    case _ditsp:qDebug()<<Q_FUNC_INFO<<"Dit sent";          usleep(elTime);   break;
    case _dahsp:qDebug()<<Q_FUNC_INFO<<"Dah sent";          usleep(elTime*3); break;
    case _elesp:qDebug()<<Q_FUNC_INFO<<"Element space sent";usleep(elTime);   break;
    case _ltrsp:qDebug()<<Q_FUNC_INFO<<"Letter space sent"; usleep(elTime*3); break;
    case _wrdsp:qDebug()<<Q_FUNC_INFO<<"Word space sent";   usleep(elTime*7); break;
    }
}

Morse::Morse(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::Morse)
{
  setGeometry(500,150,600,471);
  ui->setupUi(this);
  cwMode = false;
  wpm = ui->spinBox_wpm->value();
  QThread *cwThread = new QThread;
  Send_Elements *sendEl = new Send_Elements;
  connect(this,SIGNAL(doWork(QString, unsigned long)),sendEl,SLOT(doElements(QString, unsigned long)));
  sendEl->moveToThread(cwThread);
  cwThread->start();
}

Morse::~Morse()
{
  delete ui;
}

void Morse::readSettings(QSettings *settings)
{
  settings->beginGroup("cw");
    setGeometry(settings->value("cwPosition",this->geometry()).toRect());
    ui->plainTextEdit_1->setPlainText(settings->value("cwString1","CQ CQ CQ DE ZL2APV ZL2APV ZL2APV K").toString());
    ui->plainTextEdit_2->setPlainText(settings->value("cwString2","CQ TEST DE ZL2APV ZL2APV K").toString());
    ui->plainTextEdit_3->setPlainText(settings->value("cwString3","TU QRZ DE ZL2APV").toString());
    ui->plainTextEdit_4->setPlainText(settings->value("cwString4","OP GRAEME GRAEME").toString());
    ui->plainTextEdit_5->setPlainText(settings->value("cwString5","QTH NEW PLYMOUTH  NEW PLYMOUTH").toString());
  settings->endGroup();
}

void Morse::writeSettings(QSettings *settings)
{
  settings->beginGroup("cw");
    settings->setValue("cwString1", ui->plainTextEdit_1->toPlainText());
    settings->setValue("cwString2", ui->plainTextEdit_2->toPlainText());
    settings->setValue("cwString3", ui->plainTextEdit_3->toPlainText());
    settings->setValue("cwString4", ui->plainTextEdit_4->toPlainText());
    settings->setValue("cwString5", ui->plainTextEdit_5->toPlainText());
    settings->setValue("cwPosition", this->geometry());
    settings->endGroup();
}



void Morse::keyPressEvent(QKeyEvent *event)
{
  int keyOffset;

  keyOffset = Qt::Key_F1 - 1;
  qDebug()<<"Got here and keyOffset - "<<keyOffset;
  if (cwMode) {
//  if ((UI::mode.getMode()==MODE_CWL)||(UI::mode.getMode()==MODE_CWU)) {
  switch (event->key()) {
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
    case Qt::Key_F5:
    case Qt::Key_F6:
      event->accept();
      sendBuffer(event->key() - keyOffset);
      qDebug() << __FUNCTION__ << "The key pressed was F" << event->key()-keyOffset;
      break;
    }
  }
}

int Morse::sendBuffer(int editBox)
{
  QString buff;
  unsigned long elTime;// todo temp value for cwTime remove when setup

  elTime = 1200000/wpm;// time in nanoseconds todo temp value for cwTime remove when setup
  switch (editBox) {
    case 1:
      buff = ui->plainTextEdit->toPlainText();
      break;
    case 2:
      buff = ui->plainTextEdit_1->toPlainText();
      break;
    case 3:
      buff = ui->plainTextEdit_2->toPlainText();
      break;
    case 4:
      buff = ui->plainTextEdit_3->toPlainText();
    break;
    case 5:
      buff = ui->plainTextEdit_4->toPlainText();
    break;
    case 6:
      buff = ui->plainTextEdit_5->toPlainText();
    break;
    default:
      buff.clear();
  }
  if (buff.isEmpty()) {
    return 1;
  } else {
      emit doWork(buff, elTime);

      return 0;
    }
}

void Morse::on_spinBox_wpm_valueChanged(int arg1)
{
    wpm = arg1;
}
