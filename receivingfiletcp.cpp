#include "receivingfiletcp.h"
#include <QDebug>
#include <QDir>

ReceivingFileTCP::ReceivingFileTCP(QObject *parent)
    : QObject(parent)
    , socket(new QTcpSocket(this))
    , isReceivingHeader(true)
    , expectedFileSize(0)
    , receivedFileSize(0)
    , currentFile(nullptr)
    , saveMode(0)
{
    connect(socket, &QTcpSocket::readyRead, this, &ReceivingFileTCP::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &ReceivingFileTCP::onDisconnected);
    connect(socket, &QTcpSocket::connected, this, &ReceivingFileTCP::onConnected);
}

ReceivingFileTCP::~ReceivingFileTCP()
{
    if(currentFile)
    {
        currentFile->close();
        delete currentFile;
    }
}

void ReceivingFileTCP::connectToServer(const QString &host, quint16 port)
{
    socket->connectToHost(host, port);
    qDebug() << "Подключение к серверу:" << host << ":" << port;
}

void ReceivingFileTCP::disconnectFromServer()
{
    socket->disconnectFromHost();
}

void ReceivingFileTCP::sendFileRequest()//Запрос на получение файла
{
    if(!socket || socket->state() != QAbstractSocket::ConnectedState)
    {
        qDebug() << "Нет подключения к серверу";
        return;
    }

    data.clear();
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_9);
    out << qint64(0);
    out << QString("GET_FILE");
    out.device()->seek(qint64(0));
    out << qint64(data.size() - sizeof(qint64));

    socket->write(data);
    qDebug() << "Отправил запрос на получение файла";
}

bool ReceivingFileTCP::isConnected() const//Статус подключения
{
    return socket->state() == QAbstractSocket::ConnectedState;
}

void ReceivingFileTCP::setSavePath(const QString &path)//Изменение пути для сохранения
{
    savePath = path;
}

void ReceivingFileTCP::setSaveMode(int mode)//Индекс действия при повторном имени файла
{
    saveMode = mode;
}

QString ReceivingFileTCP::getCurrentFileName() const//Текущее имя файла
{
    return currentFileName;
}

void ReceivingFileTCP::onReadyRead()//Парсинг
{
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_9);

    while(socket->bytesAvailable() > 0)
    {
        if(isReceivingHeader)
        {
            if(socket->bytesAvailable() < static_cast<qint64>(sizeof(quint64)))
            {
                return;
            }
            quint64 headerSize;
            in >> headerSize;
            if(socket->bytesAvailable() < headerSize)
            {
                return;
            }

            in >> currentFileName >> expectedFileSize;

            // Проверка маски файла
            /*if(!fileMask.isEmpty() && !checkFileMask(currentFileName))
            {
                qDebug() << "Имя файла" << currentFileName << "не соответствует маске" << fileMask;
                skipFileData();
                continue;
            }*/

            // Создаем путь к файлу
            QString fullPath = savePath.isEmpty() ? currentFileName : savePath + "/" + currentFileName;

            if(saveMode)
            {
                fullPath = getUniqueFileName(fullPath);
            }

            qDebug() << "Сохраняем файл в:" << fullPath;

            if(expectedFileSize == 0)
            {

                // Создаем пустой файл
                QFile emptyFile(fullPath);
                if (emptyFile.open(QIODevice::WriteOnly))
                {
                    emptyFile.close();
                    qDebug() << "Пустой файл получен:" << currentFileName;
                    emit fileReceived(emptyFile.fileName());
                    emit progressChanged(50);
                }
                else
                {
                    qDebug() << "Не удалось создать пустой файл:" << fullPath;
                }

                isReceivingHeader = true;
                receivedFileSize = 0;
                expectedFileSize = 0;
                continue;
            }

            currentFile = new QFile(fullPath);
            if (!currentFile->open(QIODevice::WriteOnly))
            {
                qDebug() << "Не удалось открыть файл для записи:" << fullPath;
                delete currentFile;
                currentFile = nullptr;
                skipFileData();
                continue;
            }

            isReceivingHeader = false;
            receivedFileSize = 0;
            emit progressChanged(0);
        }
        else
        {
            if(currentFile == nullptr)
            {
                skipFileData();
                if(receivedFileSize >= expectedFileSize)
                {
                    isReceivingHeader = true;
                    receivedFileSize = 0;
                    expectedFileSize = 0;
                }
            }
            else
            {
                if(!currentFile->isOpen())
                {
                    qDebug() << "Файл не открыт для записи, пропускаем данные";
                    skipFileData();
                    return;
                }

                qint64 bytesAvailable = socket->bytesAvailable();
                qint64 bytesToRead = qMin(bytesAvailable, expectedFileSize - receivedFileSize);

                if(bytesToRead > 0)
                {
                    QByteArray buffer = socket->read(bytesToRead);
                    qint64 bytesWritten = currentFile->write(buffer);
                    receivedFileSize += bytesWritten;

                    int loadProgress = static_cast<int>((receivedFileSize * 50) / expectedFileSize);
                    emit progressChanged(loadProgress);

                    qDebug() << "Принято:" << receivedFileSize << "/" << expectedFileSize << "байт";

                    // Файл полностью получен
                    if(receivedFileSize >= expectedFileSize)
                    {
                        currentFile->close();
                        qDebug() << "Файл полностью получен:" << currentFileName;

                        emit fileReceived(currentFile->fileName());
                        emit progressChanged(50);

                        delete currentFile;
                        currentFile = nullptr;
                        isReceivingHeader = true;
                        receivedFileSize = 0;
                        expectedFileSize = 0;
                    }
                }
            }
        }
    }
}

void ReceivingFileTCP::onDisconnected()//Отключение от сервера
{
    qDebug() << "Отключен от сервера";
    if(currentFile)
    {
        currentFile->close();
        delete currentFile;
        currentFile = nullptr;
    }

    isReceivingHeader = true;
    receivedFileSize = 0;
    expectedFileSize = 0;

    emit disconnected();

    //Переподключение
    QTimer::singleShot(3000, this, [this]()
    {
        socket->connectToHost("127.0.0.1", 2323);
        qDebug()<<"Повторное подключение к серверу";
    });
}

void ReceivingFileTCP::onConnected()//Подключение к серверу
{
    qDebug()<<"Успешно подключен к серверу";
    emit connected();
}

QString ReceivingFileTCP::getUniqueFileName(const QString &filePath)//Модифицирование имени входного файла
{
    if(!saveMode)
    {
        return filePath;
    }

    QFileInfo fileInfo(filePath);
    QString path = fileInfo.path();
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();

    QString newFilePath = filePath;
    int count = 1;

    while (QFile::exists(newFilePath)|| QFile::exists(path + "/xor_" + QFileInfo(newFilePath).fileName()))
    {
        newFilePath = path + "/" + baseName + " (" + QString::number(count) + ")." + suffix;
        count++;
    }
    return newFilePath;
}

void ReceivingFileTCP::skipFileData()//Пропуск файла
{
    if(expectedFileSize == 0)
    {
        qDebug() << "Пропуск пустого файла:" << currentFileName;
        isReceivingHeader = true;
        receivedFileSize = 0;
        expectedFileSize = 0;
        currentFileName.clear();
        return;
    }

    while(receivedFileSize < expectedFileSize && socket->bytesAvailable() > 0)
    {
        qint64 bytesAvailable = socket->bytesAvailable();
        qint64 bytesToSkip = qMin(bytesAvailable, expectedFileSize - receivedFileSize);

        if(bytesToSkip > 0)
        {
            QByteArray buffer = socket->read(bytesToSkip);
            receivedFileSize += buffer.size();
            qDebug() << "Пропущено данных:" << buffer.size() << "байт, всего:" << receivedFileSize << "/" << expectedFileSize;
        }
    }

    if(receivedFileSize >= expectedFileSize)
    {
        isReceivingHeader = true;
        receivedFileSize = 0;
        expectedFileSize = 0;
        currentFileName.clear();
    }
}

void ReceivingFileTCP::cancelCurrentOperation()
{

    if(currentFile)
    {
        currentFile->close();
        //Удаляем частично записанный файл
        QString fileName = currentFile->fileName();
        delete currentFile;
        currentFile = nullptr;
        QFile::remove(fileName);
    }

    //Сбрасываем состояние
    isReceivingHeader = true;
    receivedFileSize = 0;
    expectedFileSize = 0;

    //Очищаем буфер сокета
    if(socket && socket->state() == QAbstractSocket::ConnectedState)
    {
        socket->readAll(); // Очищаем буфер
    }
}



