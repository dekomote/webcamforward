#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    camera(0),
    imageCapture(0)
{
    ui->setupUi(this);

    QByteArray cameraDevice;

    QActionGroup *videoDevicesGroup = new QActionGroup(this);
    videoDevicesGroup->setExclusive(true);
    foreach(const QByteArray &deviceName, QCamera::availableDevices()) {
        QString description = camera->deviceDescription(deviceName);
        QAction *videoDeviceAction = new QAction(description, videoDevicesGroup);
        videoDeviceAction->setCheckable(true);
        videoDeviceAction->setData(QVariant(deviceName));
        if (cameraDevice.isEmpty()) {
            cameraDevice = deviceName;
            videoDeviceAction->setChecked(true);
        }
        ui->menuDevices->addAction(videoDeviceAction);
    }

    setCamera(cameraDevice);
    connect(videoDevicesGroup, SIGNAL(triggered(QAction*)), SLOT(updateCameraDevice(QAction*)));


    busyReceiving = false;
    messageBuffer = new QString();

    heartbeatTimer = new QTimer(this);
    connect(heartbeatTimer, SIGNAL(timeout()), SLOT(on_heartbeatTimer_timeout()));

    imageStreamTimer = new QTimer(this);

    pSocket = new QTcpSocket(this);
    connect(pSocket, SIGNAL(connected()), this, SLOT(on_pSocket_connected()));
    connect(pSocket, SIGNAL(disconnected()), this, SLOT(on_pSocket_disconnected()));
    connect(pSocket, SIGNAL(readyRead()), this, SLOT(on_pSocket_readyRead()));
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_connectButton_clicked()
{
    if(pSocket->state() == QTcpSocket::UnconnectedState)
    {
        pSocket->connectToHost(HOSTNAME, PORT);
    }
    else
    {
        pSocket->disconnectFromHost();
    }
}

void MainWindow::on_pSocket_connected()
{
    statusBar()->showMessage("Connection established");
    ui->connectButton->setText("Disconnect");
    ui->clientSecretEdit->setEnabled(false);
    ui->menuDevices->setEnabled(false);
}

void MainWindow::on_pSocket_disconnected()
{

    ui->connectButton->setText("Connect");
    ui->clientSecretEdit->setEnabled(true);
    ui->menuDevices->setEnabled(true);

    if(heartbeatTimer->isActive())
        heartbeatTimer->stop();

    if(imageStreamTimer->isActive())
        imageStreamTimer->stop();

}

void MainWindow::on_pSocket_readyRead()
{
    QString data = (QString)pSocket->readAll();
    QStringList parts;
    QString message;


    if(busyReceiving)
    {
        messageBuffer->append(data);
    }
    else{
        busyReceiving = true;
        messageBuffer->append(data);

        while(messageBuffer->length() > 0){
            parts = messageBuffer->split(JSON_DATA_BOUNDARY);
            message = parts[0];
            if(parts.length() > 1)
                messageBuffer = new QString(parts[1]);
            else
                messageBuffer = new QString();
            on_message(message);
        }
        busyReceiving = false;
    }
    return;
}


void MainWindow::on_message(QString &message)
{

    try
    {
        QJsonDocument d = QJsonDocument::fromJson(message.toUtf8());
        QString command = "on_" + d.object().value(QString("command")).toString();
        QString payload = d.object().value(QString("payload")).toString();
        QMetaObject::invokeMethod((QObject*)this, command.toLocal8Bit().data(), Qt::QueuedConnection, Q_ARG(QString, payload));
    }
    catch(QJsonParseError& e){
        qWarning() << "Can't parse message " << message << " " << e.errorString();
    }
    catch(...){
        qWarning() << "I don't understand' " << message;
    }
}


void MainWindow::send_message(QString command, QString payload)
{
    QJsonDocument d = QJsonDocument();
    QJsonObject o = QJsonObject();
    o.insert("command", command);
    o.insert("payload", payload);
    d.setObject(o);
    pSocket->write(d.toJson().append(JSON_DATA_BOUNDARY));
    pSocket->flush();
}


void MainWindow::on_authenticate(QString payload)
{
    statusBar()->showMessage("Recieved Authentication Request. Sending Authentication Data.");
    send_message(QString("authenticate"), ui->clientSecretEdit->text());
}

void MainWindow::on_authenticated(QString payload)
{
    statusBar()->showMessage("Client authenticated. Starting the Heartbeat.");
    heartbeatTimer->start(10000);
}

void MainWindow::on_forbidden(QString payload)
{
    statusBar()->showMessage("Client failed at authentication! Check your client secret.");
    pSocket->disconnectFromHost();
}


void MainWindow::on_heartbeatTimer_timeout()
{
    statusBar()->showMessage("Sending a heartbeat.");
    send_message(QString("heartbeat"), QString(""));
}


void MainWindow::setCamera(const QByteArray &cameraDevice)
{
    delete imageCapture;
    delete camera;

    if (cameraDevice.isEmpty())
        camera = new QCamera;
    else
        camera = new QCamera(cameraDevice);

    connect(camera, SIGNAL(error(QCamera::Error)), this, SLOT(displayCameraError()));

    imageCapture = new QCameraImageCapture(camera);

    camera->setViewfinder(ui->viewFinder);

    connect(imageCapture, SIGNAL(readyForCaptureChanged(bool)), this, SLOT(readyForCapture(bool)));

    camera->start();
}


void MainWindow::readyForCapture(bool ready)
{
    if(ready){
        statusBar()->showMessage("Ready for capture.");
        ui->connectButton->setEnabled(true);
    }
    else
        statusBar()->showMessage("Camera not ready.");
}


void MainWindow::updateCameraDevice(QAction *action)
{
    setCamera(action->data().toByteArray());
}


void MainWindow::displayCameraError()
{
    statusBar()->showMessage("Camera error.");
}
