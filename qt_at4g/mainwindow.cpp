#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QTime>

void sleep(quint32 msec)
{
    QTime reachTime = QTime::currentTime().addMSecs(msec);

    while(QTime::currentTime() < reachTime)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 初始化控件
    ui->progressBar->setValue(0);
    ui->lineEdit_2->setText("183.15.176.83");
    ui->lineEdit_3->setText("6677");
    ui->lineEdit->setText("当前只能跟本地服务器交互");

    // 列举所有可用串口号
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        QSerialPort serial;
        serial.setPort(info);
        if(serial.open(QIODevice::ReadWrite))
        {
            ui->comboBox->addItem(serial.portName());
            serial.close();
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_5_clicked()
{
    serial = new QSerialPort;

    // 设置串口
    serial->setPortName(ui->comboBox->currentText());
    serial->open(QIODevice::ReadWrite);
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    QObject::connect(serial, &QSerialPort::readyRead, this, &MainWindow::ReadData);
}

quint32 totalBytes = 2000 * 17 * 30;
quint32 sentcnt;
quint32 startDirectly;
quint32 restartDirectly;

void MainWindow::ReadData()
{
    QByteArray buf, sendbuf;
    QString tmp;

    buf = serial->readAll();
    if(!buf.isEmpty())
    {
        // 把接收到的消息显示出来
        QString str = ui->textBrowser->toPlainText();
        str += tr(buf);
        ui->textBrowser->clear();
        ui->textBrowser->append(str);

        if(startDirectly == 1)
        {
            startDirectly = 0;
            goto start_on_middle;
        }

        if(restartDirectly == 1)
        {
            if(buf[0] == 0xEB && buf[1] == 0x90 && buf[3] == 0x09)
            {
                goto restart_dirc;
            }
            restartDirectly = 0;
        }

        // 回复接收到的消息
        if(justSentATCmd == "ATE1")
        {
            if(buf.contains("OK") == true)
            {
                serial->write("AT+CIPMODE=1\r");
                justSentATCmd = "AT+CIPMODE=1";
            }
        }
        else if(justSentATCmd == "AT+CIPMODE=1")
        {
            if(buf.contains("OK") == true)
            {
start_on_middle:
                serial->write("AT+NETOPEN\r");
                justSentATCmd = "AT+NETOPEN";
            }
        }
        else if(justSentATCmd == "AT+NETOPEN")
        {
            if(buf.contains("OK") == true)
            {
                tmp = "AT+CIPOPEN=0,\"TCP\",\"" + serverIP + "\"," + serverPort + "\r";
                serial->write(tmp.toLatin1());
                justSentATCmd = "AT+CIPOPEN";
            }
        }
        else if(justSentATCmd == "AT+CIPOPEN")
        {
            if(buf.contains("CONNECT") == true)
            {
                // 此时可以开始发数据了
                // 先发送<0x09>命令
                sendbuf.resize(4);
                sendbuf[0] = 0xEB;
                sendbuf[1] = 0x90;
                sendbuf[2] = 0x08;
                sendbuf[3] = 0x09;
                serial->write(sendbuf);
            }
            else if(buf[0] == 0xEB && buf[1] == 0x90 && buf[3] == 0x09)
            {
restart_dirc:
                // 再发送<0x05>命令
                sendbuf.resize(4);
                sendbuf[0] = 0xEB;
                sendbuf[1] = 0x90;
                sendbuf[2] = 0x08;
                sendbuf[3] = 0x05;
                serial->write(sendbuf);
            }
            else if(buf[0] == 0xEB && buf[1] == 0x90 && buf[3] == 0x05)
            {
                // 接着发送<0x0B>命令
                sendbuf.resize(4);
                sendbuf[0] = 0xEB;
                sendbuf[1] = 0x90;
                sendbuf[2] = 0x08;
                sendbuf[3] = 0x0B;

                sendbuf[4] = (totalBytes + 32) >> 24;
                sendbuf[5] = (totalBytes + 32) >> 16;
                sendbuf[6] = (totalBytes + 32) >> 8;
                sendbuf[7] = (totalBytes + 32);
                serial->write(sendbuf);
                sleep(1000);

                // 然后数据主体一包一包地发出去
                for(quint32 i = 0; i < totalBytes; i++)
                {
                    if(i % 1360 == 0)
                    {
                        sleep(5);
                    }
                    serial->write("X");
                    ui->progressBar->setValue((i + 1) * 100 / totalBytes);
                }

                // 还有两字节的校验位
                serial->write("Q");
                serial->write("Q");
            }
            else if(buf[0] == 0xEB && buf[1] == 0x90 && buf[3] == 0x0B)
            {
                ui->statusBar->showMessage("ok", 4000);
            }
            else
            {
                ui->statusBar->showMessage("recv err!", 4000);
            }
        }
        else
        {
            // 不识别的数据全部显示出来，不处理
            ui->statusBar->showMessage("recv err2!", 4000);
        }
    }
    buf.clear();
}

void MainWindow::on_pushButton_clicked()
{
    serial->write("AT+CRESET\r");
}

void MainWindow::on_pushButton_2_clicked()
{
    ui->textBrowser->clear();
    ui->progressBar->setValue(0);
    sentcnt = 0;
    startDirectly = 0;
    restartDirectly = 0;
}

void MainWindow::on_pushButton_3_clicked()
{
    // 更新相关变量和控件
    serverIP = ui->lineEdit_2->text();
    serverPort = ui->lineEdit_3->text();

    // 发送AT指令
    serial->write("ATE1\r");
    justSentATCmd = "ATE1";
    waitACK = "OK";
}

void MainWindow::on_pushButton_4_clicked()
{
    sleep(1000);
    serial->write("+++");
    sleep(1000);
    serial->write("AT+NETCLOSE\r");
    startDirectly = 1;
}

void MainWindow::on_pushButton_6_clicked()
{
    QByteArray sendbuf;

    restartDirectly = 1;

    sendbuf.resize(4);
    sendbuf[0] = 0xEB;
    sendbuf[1] = 0x90;
    sendbuf[2] = 0x08;
    sendbuf[3] = 0x09;
    serial->write(sendbuf);
}
