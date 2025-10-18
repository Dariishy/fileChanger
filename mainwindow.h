#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QPushButton>
#include <QFile>
#include <QLayout>
#include <QListWidget>
#include <QProgressBar>
#include "processingxor.h"
#include "receivingfiletcp.h"
#include <QThread>
#include <QMutex>

class MainWindow : public QMainWindow
{
private:
    Q_OBJECT

    QLineEdit maskFile, savePath, setWorkTimer, textXOR;
    QLabel labelMaskFile, labelSavePath, labelWorkTimer, labelFileDelete, labelSaveMode, labelXOR;
    QCheckBox workTimer, fileDelete;
    QComboBox saveMode, unitsMeasurenentTimer;
    QPushButton openFileManager, startChanger;
    QListWidget *fileListWidget;
    QProgressBar fileProgressBar;
    QVBoxLayout *mainLayout;

    QThread *xorThread;
    ProcessingXOR *processingXor;
    ReceivingFileTCP *fileReceiver;
    QTimer *timer;
    bool isProcessing;
    bool isSingleProcessing;
    QMutex processingMutex;
    std::atomic<bool> stopRequested{false};
    QMutex stopMutex;

    void createGUI();
    void enebledOrDisabledGUI(bool state);
    void startXORProcessing(const QString &inputPath, const QString &outputPath);
    void setupXOR();
    void setupFileReceiver();
    void stopAllOperations();

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    //void readyRead();
    void onFileReceived(const QString &filePath);
    void onXORFinished(const QString &resultFile);
    void onXORError(const QString &errorMessage);
    void onReceiverError(const QString &errorMessage);
    void sendFileRequest();
    void addProcessedFile(const QString &fileInfo);

signals:
    void progressChanged(int value);
};
#endif // MAINWINDOW_H
