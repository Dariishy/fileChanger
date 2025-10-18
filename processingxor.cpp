#include "processingxor.h"
#include <QDebug>
#include <QRegularExpression>

ProcessingXOR::ProcessingXOR(QObject *parent)
    : QObject(parent)
{

}

ProcessingXOR::~ProcessingXOR()
{

}

void ProcessingXOR::processFile(const QString &inputPath, const QString &outputPath, const QVector<unsigned char> &xorKeys, bool deleteOriginal)//Обработка XOR
{
    QFile inputFile(inputPath);

    if(QThread::currentThread()->isInterruptionRequested())
    {
        emit error("Обработка отменена");
        return;
    }

    if(!inputFile.exists())
    {
        emit error("Нет файла: " + inputPath);
        return;
    }

    if (xorKeys.size() != 8)
    {
        emit error("Размер ключа должен быть 8 байт");
        return;
    }

    // Проверка маски файла
    if(!fileMask.isEmpty() && !checkFileMask(inputFile.fileName()))
    {
        qDebug() << "Имя файла" << inputFile.fileName() << "не соответствует маске" << fileMask;
        emit progressChanged(100);
        inputFile.remove();
        emit finished("-");
        return;
    }

    if(inputFile.size() == 0)
    {
        qDebug() << "Обработка пустого файла:" << inputPath;

        QFile outputFile(outputPath);
        if(!outputFile.open(QIODevice::WriteOnly))
        {
            emit error("Ошибка открытия файла: " + outputFile.errorString());
            return;
        }
        outputFile.close();

        //Удаляем исходный файл если нужно
        if(deleteOriginal)
        {
            if(!inputFile.remove())
            {
                emit error("Не удалось удалить исходный файл: " + inputFile.errorString());
            }
        }
        emit finished(outputPath);
        emit progressChanged(100);
        return;
    }

    if (!inputFile.open(QIODevice::ReadOnly))
    {
        emit error("Ошибка открытия входного файла: " + inputFile.errorString());
        return;
    }
    QFile outputFile(outputPath);

    if (!outputFile.open(QIODevice::WriteOnly))
    {
        emit error("Ошибка открытия выходного файла: " + outputFile.errorString());
        inputFile.close();
        return;
    }

    //Обрабатываем файл частями
    bool success = processChunk(inputFile, outputFile, xorKeys, inputFile.size());

    inputFile.close();
    outputFile.close();

    if (!success)
    {
        outputFile.remove();
        emit error("Ошибка обработки файла");
        return;
    }

    //Удаляем исходный файл если нужно
    if(deleteOriginal)
    {
        if(!inputFile.remove())
        {
            emit error("Не удалось удалить исходный файл: " + inputFile.errorString());
        }
    }

    emit finished(outputPath);
    emit progressChanged(100);
}

bool ProcessingXOR::processChunk(QFile &inputFile, QFile &outputFile, const QVector<unsigned char> &xorKeys, qint64 totalSize)
{
    if (totalSize == 0)
    {
        return true;
    }

    const int BLOCK_SIZE = 4096;
    unsigned char buf[BLOCK_SIZE];

    qint64 processedSize = 0;
    qint64 fileSize = inputFile.size();
    int lastReportedProgress = -1;
    const qint64 PROGRESS_ONE = fileSize / 100;
    qint64 nextUpdate = PROGRESS_ONE;

    while(!inputFile.atEnd())
    {
        if(QThread::currentThread()->isInterruptionRequested())
        {
            qDebug() << "Обработка прервана пользователем";
            return false;
        }

        qint64 bytesRead = inputFile.read((char*)buf, BLOCK_SIZE);
        if (bytesRead == -1)
        {
            emit error("Ошибка чтения файла");
            return false;
        }
        if(QThread::currentThread()->isInterruptionRequested())
        {
            return false;
        }
        // Применяем XOR
        for (int i = 0; i < bytesRead; i++)
        {
            buf[i] ^= xorKeys[i % 8];
        }
        if(QThread::currentThread()->isInterruptionRequested())
        {
            return false;
        }

        qint64 bytesWritten = outputFile.write((const char*)buf, bytesRead);
        if (bytesWritten != bytesRead)
        {
            emit error("Ошибка записи в файл");
            return false;
        }
        processedSize += bytesRead;

        // Обновление прогресса
        if(processedSize >= nextUpdate||processedSize == fileSize)
        {
            int currentProgress = static_cast<int>((processedSize * 100)/fileSize);

            if(currentProgress != lastReportedProgress)
            {
                emit progressChanged(currentProgress);
                lastReportedProgress = currentProgress;
            }

            // Вычисляем следующий порог обновления
            nextUpdate = ((lastReportedProgress + 1) * fileSize) / 100;
        }
    }
    if(lastReportedProgress != 100)
    {
        emit progressChanged(100);
    }
    return true;
}

void ProcessingXOR::cancelProcessing()//Остановка обработки
{
    QThread::currentThread()->requestInterruption();

}

bool ProcessingXOR::checkFileMask(const QString &fileName)//Проверка соответствия маски
{
    QRegularExpression regex(fileMask, QRegularExpression::CaseInsensitiveOption);
    return regex.match(fileName).hasMatch();
}

void ProcessingXOR::setFileMask(const QString &mask)//Изменение маски файлов
{
    fileMask = mask.trimmed();
}



