#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ftpserver.h"
#include "debuglogdialog.h"

#include <QCoreApplication>
#include <QSettings>
#include <QFileDialog>
#include <QIntValidator>

#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->lineEditPort->setValidator(new QIntValidator(1, 65535, this));

#if defined(Q_OS_ANDROID)
    // Fix for the bug android keyboard bug - see
    // http://stackoverflow.com/q/21074012/492336.
    foreach (QLineEdit *lineEdit, findChildren<QLineEdit*>()) {
        connect(lineEdit, SIGNAL(editingFinished()), QGuiApplication::inputMethod(), SLOT(hide()));
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)) && (QT_VERSION < QT_VERSION_CHECK(5, 3, 0))
    // Fix for bug in Qt 5.2 - Virtual keyboard keeps switching to
    // uppercase when I type in a QLineEdit, see
    // http://stackoverflow.com/q/25263561/492336.
    // The bug is fixed in Qt 5.4, I don't know about Qt 5.3.
    foreach (QLineEdit *lineEdit, findChildren<QLineEdit*>()) {
        lineEdit->setInputMethodHints(Qt::ImhNoAutoUppercase);
    }
#endif
#else
    // The exit button is needed only for Android. Hide it for other builds.
    ui->pushButtonExit->hide();
#endif // Q_OS_ANDROID

    // Set window icon.
    setWindowIcon(QIcon(":/icons/appicon"));

    // debug
#ifdef QT_DEBUG
    if (this->configPath.isEmpty())
        this->configPath = QDir::currentPath() + "/config.json";
#endif

    loadSettings();
    server = 0;
    startServer();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::setOrientation(ScreenOrientation orientation)
{
#if defined(Q_OS_SYMBIAN)
    // If the version of Qt on the device is < 4.7.2, that attribute won't work
    if (orientation != ScreenOrientationAuto) {
        const QStringList v = QString::fromAscii(qVersion()).split(QLatin1Char('.'));
        if (v.count() == 3 && (v.at(0).toInt() << 16 | v.at(1).toInt() << 8 | v.at(2).toInt()) < 0x040702) {
            qWarning("Screen orientation locking only supported with Qt 4.7.2 and above");
            return;
        }
    }
#endif // Q_OS_SYMBIAN

    Qt::WidgetAttribute attribute;
    switch (orientation) {
#if QT_VERSION < 0x040702 || QT_VERSION >= 0x050000
    // Qt < 4.7.2 does not yet have the Qt::WA_*Orientation attributes
    // Qt 5 has removed them.
    case ScreenOrientationLockPortrait:
        attribute = static_cast<Qt::WidgetAttribute>(128);
        break;
    case ScreenOrientationLockLandscape:
        attribute = static_cast<Qt::WidgetAttribute>(129);
        break;
    default:
    case ScreenOrientationAuto:
        attribute = static_cast<Qt::WidgetAttribute>(130);
        break;
#else // QT_VERSION < 0x040702
    case ScreenOrientationLockPortrait:
        attribute = Qt::WA_LockPortraitOrientation;
        break;
    case ScreenOrientationLockLandscape:
        attribute = Qt::WA_LockLandscapeOrientation;
        break;
    default:
    case ScreenOrientationAuto:
        attribute = Qt::WA_AutoOrientation;
        break;
#endif // QT_VERSION < 0x040702
    };
    setAttribute(attribute, true);
}

void MainWindow::showExpanded()
{
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    showFullScreen();
#elif defined(Q_WS_MAEMO_5) || defined (Q_OS_ANDROID)
    showMaximized();
#else
    show();
#endif
}

void MainWindow::loadSettings()
{
    // try to load config path
    QJsonObject config, ftp;
    QFile file;
    file.setFileName(this->configPath);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString jsonString = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        config = doc.object();

        if (config.contains("ftp") && config["ftp"].isObject())
            ftp = config["ftp"].toObject();
    }

    // init default value if not exists
    if (!ftp.contains("port")) {
    // UNIX-derived systems such as Linux and Android don't allow access to
    // port 21 for non-root programs, so we will use port 2121 instead.
#if defined(Q_OS_UNIX)
    ftp["port"] = "2121";
#else
    ftp["port"] = "21";
#endif
    }

    if (!ftp.contains("username"))
        ftp["username"] = "admin";
    if (!ftp.contains("password"))
        ftp["password"] = "qt";
    if (!ftp.contains("rootpath"))
        ftp["rootpath"] = QDir::rootPath();
    if (!ftp.contains("anonymous"))
        ftp["anonymous"] = false;
    if (!ftp.contains("readonly"))
        ftp["readonly"] = false;
    if (!ftp.contains("oneip"))
        ftp["oneip"] = true;

    ui->lineEditPort->setText(ftp["port"].toString());
    ui->lineEditUserName->setText(ftp["username"].toString());
    ui->lineEditPassword->setText(ftp["password"].toString());
    ui->lineEditRootPath->setText(ftp["rootpath"].toString());
    ui->checkBoxAnonymous->setChecked(ftp["anonymous"].toBool());
    ui->checkBoxReadOnly->setChecked(ftp["readonly"].toBool());
    ui->checkBoxOnlyOneIpAllowed->setChecked(ftp["oneip"].toBool());
}

void MainWindow::saveSettings()
{
    QJsonObject config, ftp;

    ftp["port"] = ui->lineEditPort->text();
    ftp["username"] = ui->lineEditUserName->text();
    ftp["password"] = ui->lineEditPassword->text();
    ftp["rootpath"] = ui->lineEditRootPath->text();
    ftp["anonymous"] = ui->checkBoxAnonymous->isChecked();
    ftp["readonly"] = ui->checkBoxReadOnly->isChecked();
    ftp["oneip"] = ui->checkBoxOnlyOneIpAllowed->isChecked();

    // save settings to json file
    QFile file;
    file.setFileName(this->configPath);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString jsonString = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        config = doc.object();
    }

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Couldn't open the file.");
        return;
    }

    // overwrite
    config["ftp"] = ftp;

    QJsonDocument doc;
    doc.setObject(config);
    QByteArray json = doc.toJson();
    file.write(json);
    file.close();
}

void MainWindow::startServer()
{
    QString userName;
    QString password;
    if (!ui->checkBoxAnonymous->isChecked()) {
        userName = ui->lineEditUserName->text();
        password = ui->lineEditPassword->text();
    }
    delete server;
    server = new FtpServer(this, ui->lineEditRootPath->text(), ui->lineEditPort->text().toInt(), userName,
                           password, ui->checkBoxReadOnly->isChecked(), ui->checkBoxOnlyOneIpAllowed->isChecked());
    connect(server, SIGNAL(newPeerIp(QString)), SLOT(onPeerIpChanged(QString)));
    if (server->isListening()) {
        ui->statusBar->showMessage("Listening at " + FtpServer::lanIp());
    } else {
        ui->statusBar->showMessage("Not listening");
    }
}

void MainWindow::on_pushButtonRestartServer_clicked()
{
    startServer();
}

void MainWindow::on_toolButtonBrowse_clicked()
{
    QString rootPath;
#ifdef Q_OS_ANDROID
    // In Android, the file dialog is not shown maximized by the static
    // function, which looks weird, since the dialog doesn't have borders or
    // anything. To make sure it's shown maximized, we won't be using
    // QFileDialog::getExistingDirectory().
    QFileDialog dialog;
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.showMaximized();
    dialog.exec();
    if (!dialog.selectedFiles().isEmpty()) {
        rootPath = dialog.selectedFiles().front();
    }
#else
    rootPath = QFileDialog::getExistingDirectory(this, QString(), ui->lineEditRootPath->text());
#endif
    if (rootPath.isEmpty()) {
        return;
    }
    ui->lineEditRootPath->setText(rootPath);
}

void MainWindow::onPeerIpChanged(const QString &peerIp)
{
    ui->statusBar->showMessage("Connected to " + peerIp);
}

void MainWindow::on_pushButtonShowDebugLog_clicked()
{
    DebugLogDialog *dlg = new DebugLogDialog;
    dlg->setAttribute( Qt::WA_DeleteOnClose, true );
    dlg->setModal(true);
    dlg->showExpanded();
}

void MainWindow::on_pushButtonExit_clicked()
{
    close();
}

void MainWindow::setConfigPath(QString path) {
    this->configPath = path;
}
