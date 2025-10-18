#ifndef PROCESSINGXOR_H
#define PROCESSINGXOR_H

#include <QObject>
#include <QFile>
#include <QVector>
#include <QThread>

class ProcessingXOR : public QObject
{
    Q_OBJECT
public:
    explicit ProcessingXOR(QObject *parent = nullptr);
    ~ProcessingXOR();
    void setFileMask(const QString &mask);
public slots:
    void processFile(const QString &inputPath, const QString &outputPath, const QVector<unsigned char> &xorKeys, bool deleteOriginal);
    void cancelProcessing();

signals:
    void progressChanged(int value);//Прогресс
    void finished(const QString &resultFile);
    void error(const QString &errorMessage);

private:
    bool processChunk(QFile &inputFile, QFile &outputFile, const QVector<unsigned char> &xorKeys, qint64 totalSize);
    bool checkFileMask(const QString &fileName);
    QString fileMask;

};

#endif // PROCESSINGXOR_H
