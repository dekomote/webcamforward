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

    connect(videoDevicesGroup, SIGNAL(triggered(QAction*)), SLOT(updateCameraDevice(QAction*)));
    connect(ui->actionRegister, SIGNAL(triggered()), this, SLOT(website()));

    busyReceiving = false;
    messageBuffer = new QString();

    heartbeatTimer = new QTimer(this);
    connect(heartbeatTimer, SIGNAL(timeout()), SLOT(on_heartbeatTimer_timeout()));

    imageStreamTimer = new QTimer(this);
    connect(imageStreamTimer, SIGNAL(timeout()), SLOT(on_imageStreamTimer_timeout()));

    pSocket = new QTcpSocket(this);
    connect(pSocket, SIGNAL(connected()), this, SLOT(on_pSocket_connected()));
    connect(pSocket, SIGNAL(disconnected()), this, SLOT(on_pSocket_disconnected()));
    connect(pSocket, SIGNAL(readyRead()), this, SLOT(on_pSocket_readyRead()));
    connect(pSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(on_pSocket_error()));

    setCamera(cameraDevice);

    QSettings settings;
    ui->clientSecretEdit->setText(settings.value("user/clientSecret", "").toString());

    statusIcon = new QLabel();
    statusIcon->setGeometry(0,0, 12, 12);
    statusIcon->setPixmap(QPixmap("://icon/red_orb.png"));
    statusIcon->setToolTip("Idle");
    statusBar()->addPermanentWidget(statusIcon);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_connectButton_clicked()
{
    if(pSocket->state() == QTcpSocket::UnconnectedState)
    {
        QSettings settings;
        statusBar()->showMessage("Connecting");
        pSocket->connectToHost(settings.value("global/hostName", HOSTNAME).toString(), settings.value("global/hostPort", PORT).toInt());

        if(ui->clientSecretEdit->text().length() > 0)
            settings.setValue("user/clientSecret", ui->clientSecretEdit->text());
    }
    else
    {
        send_message(QString("disconnect"), QString(""));
        pSocket->disconnectFromHost();
    }
}

void MainWindow::on_pSocket_connected()
{
    statusBar()->showMessage("Connection established");
    ui->connectButton->setText("Disconnect");
    ui->clientSecretEdit->setEnabled(false);
    ui->menuDevices->setEnabled(false);
    ui->menuSettings->setEnabled(false);
    statusIcon->setPixmap(QPixmap("://icon/blue_orb.png"));
    statusIcon->setToolTip("Connected");
}

void MainWindow::on_pSocket_disconnected()
{

    ui->connectButton->setText("Connect");
    ui->clientSecretEdit->setEnabled(true);
    ui->menuDevices->setEnabled(true);
    ui->menuSettings->setEnabled(true);
    statusIcon->setPixmap(QPixmap("://icon/red_orb.png"));
    statusIcon->setToolTip("Idle");

    if(heartbeatTimer->isActive())
        heartbeatTimer->stop();

    if(imageStreamTimer->isActive())
        imageStreamTimer->stop();

}

void MainWindow::on_pSocket_error()
{
    statusBar()->showMessage("Connection error");
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


void MainWindow::on_start_stream(QString payload)
{
    statusIcon->setPixmap(QPixmap("://icon/green_orb.png"));
    statusIcon->setToolTip("Streaming");
    statusBar()->showMessage("Server requested the image stream. Starting.");
    imageStreamTimer->start(300);
}

void MainWindow::on_imageStreamTimer_timeout()
{
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    ui->viewFinder->grab().save(&buffer, "JPG");
    send_message(QString("image"), QString(buffer.data().toBase64()));
}


void MainWindow::on_stop_stream(QString payload)
{
    statusIcon->setPixmap(QPixmap("://icon/blue_orb.png"));
    statusIcon->setToolTip("Connected");
    statusBar()->showMessage("Server requested stopping the image stream. Stopping.");
    if(imageStreamTimer->isActive())
        imageStreamTimer->stop();
}


void MainWindow::setCamera(const QByteArray &cameraDevice)
{
    delete imageCapture;

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
    camera->stop();
    camera->unload();
    setCamera(action->data().toByteArray());
}


void MainWindow::displayCameraError()
{
    statusBar()->showMessage("Camera error.");
}

void MainWindow::closing()
{
    if(pSocket->state() != QTcpSocket::UnconnectedState)
    {
        send_message(QString("disconnect"), QString(""));
        pSocket->disconnectFromHost();
    }
}


void MainWindow::website()
{
    QUrl website_url = QUrl();
    website_url.setHost(QString(HOSTNAME));
    website_url.setScheme("http");
    QDesktopServices::openUrl(website_url);
}
