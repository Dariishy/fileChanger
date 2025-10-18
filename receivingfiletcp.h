#ifndef RECEIVINGFILETCP_H
#define RECEIVINGFILETCP_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QDataStream>

class ReceivingFileTCP: public QObject
{
    Q_OBJECT
public:
    explicit ReceivingFileTCP(QObject *parent = nullptr);
    ~ReceivingFileTCP();

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    void sendFileRequest();
    bool isConnected() const;

    void setFileMask(const QString &mask);
    void setSavePath(const QString &path);
    void setSaveMode(int mode);//Перезапись/Модификация
    QString getCurrentFileName() const;

signals:
    void fileReceived(const QString &filePath);
    void progressChanged(int value);
    void connected();
    void disconnected();
    void errorOccurred(const QString &errorMessage);

private slots:
    void onReadyRead();
    void onDisconnected();
    void onConnected();
public slots:
    void cancelCurrentOperation();

private:
    QTcpSocket *socket;
    QByteArray data;
    bool isReceivingHeader;
    QString currentFileName;
    qint64 expectedFileSize;
    qint64 receivedFileSize;
    QFile *currentFile;

    QString fileMask;
    QString savePath;
    int saveMode;

    QString getUniqueFileName(const QString &filePath);
    bool checkFileMask(const QString &fileName);
    void skipFileData();
};

#endif // RECEIVINGFILETCP_H
