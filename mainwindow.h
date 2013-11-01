#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork/QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QCameraImageCapture>
#include <QtMultimedia>
#include <QVideoWidget>

#define JSON_DATA_BOUNDARY ((const char *) "---jsonrpcprotocolboundary---")
#define HOSTNAME "localhost"
#define PORT 9000

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool busyReceiving;

    QString *messageBuffer;
    QTcpSocket *pSocket;
    QTimer *heartbeatTimer;
    QTimer *imageStreamTimer;
    QCamera *camera;
    QCameraImageCapture *imageCapture;

    void send_message(QString command, QString payload);


private slots:

    void displayCameraError();
    void on_connectButton_clicked();

    void on_pSocket_connected();
    void on_pSocket_disconnected();
    void on_pSocket_readyRead();
    void on_message(QString &message);

    void on_heartbeatTimer_timeout();

    void readyForCapture(bool ready);
    void setCamera(const QByteArray &cameraDevice);
    void updateCameraDevice(QAction *action);

    //methods called remotely
    void on_authenticate(QString payload);
    void on_authenticated(QString payload);
    void on_forbidden(QString payload);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
