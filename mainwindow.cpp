#include "mainwindow.h"

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , xorThread(nullptr)
    , processingXor(nullptr)
    , fileReceiver(nullptr)
    , isProcessing(false)
    , isSingleProcessing(false)
{
    //Интерфейс
    createGUI();
    //Изменение прогресс бара
    connect(this, &MainWindow::progressChanged, &fileProgressBar, &QProgressBar::setValue);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::sendFileRequest);

    //Работа кнопки Старт/Стоп
    connect(&startChanger, &QPushButton::clicked, this, [=]()
    {
        if(stopRequested)
        {
            return;
        }
        if(!(savePath.text().isEmpty()))
        {
            if(!(QDir(savePath.text()).exists()))
            {
                return;
            }
        }
        if(workTimer.isChecked())
        {
            if(!(timer->isActive()))
            {
                startChanger.setText("Стоп");
                startChanger.setEnabled(false);
                enebledOrDisabledGUI(false);
                {
                    QMutexLocker locker(&stopMutex);
                    stopRequested = false;
                }


                switch (unitsMeasurenentTimer.currentIndex())
                {
                case 0:
                    timer->start(setWorkTimer.text().toInt());
                    break;
                case 1:
                    timer->start(setWorkTimer.text().toInt() * 1000);
                    break;
                case 2:
                    timer->start(setWorkTimer.text().toInt() * 60000);
                    break;
                default:
                    qDebug()<< "Таймер не запущен";
                }
                startChanger.setEnabled(true);
            }
            else
            {
                startChanger.setText("Остановка...");
                startChanger.setEnabled(false);
                timer->stop();
                //if (processingXor && xorThread->isRunning())
                //{
                    //QMetaObject::invokeMethod(processingXor, "cancelProcessing", Qt::QueuedConnection);
                QTimer::singleShot(0, this, [this]()
                {
                    startChanger.setText("Старт");
                    startChanger.setEnabled(true);
                    enebledOrDisabledGUI(true);
                });
                //}

            }
        }
        else
        {
            if(!isSingleProcessing)
            {
                isSingleProcessing = true;
                startChanger.setText("Стоп");
                enebledOrDisabledGUI(false);

                {
                    QMutexLocker locker(&stopMutex);
                    stopRequested = false;
                }
                sendFileRequest();
            }
            else
            {
                startChanger.setText("Остановка...");
                startChanger.setEnabled(false);
                //if (processingXor && xorThread->isRunning())
                //{
                    //QMetaObject::invokeMethod(processingXor, "cancelProcessing", Qt::QueuedConnection);
                QTimer::singleShot(100, this, [this]()
                {
                    //setupXOR();
                    stopAllOperations();
                    startChanger.setText("Старт");
                    startChanger.setEnabled(true);
                    enebledOrDisabledGUI(true);
                });
                //}

            }
        }
    });

    setupFileReceiver();
    setupXOR();

}

MainWindow::~MainWindow()
{
    if(xorThread && xorThread->isRunning())
    {
        xorThread->quit();
        xorThread->wait(5000);
    }
}

void MainWindow::createGUI()//Интерфейс
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    QFont font("Segoe UI", 10);
    setFont(font);

    //Настройка
    QGroupBox *group = new QGroupBox("Настройка", this);
    group->setFont(font);
    QVBoxLayout *settingsLayout = new QVBoxLayout(group);

    //Маска файлов
    QHBoxLayout *maskLayout = new QHBoxLayout();
    labelMaskFile.setText("Маска входных файлов:");
    labelMaskFile.setFont(font);
    maskLayout->addWidget(&labelMaskFile);
    maskLayout->addWidget(&maskFile, 1);
    settingsLayout->addLayout(maskLayout);

    //Путь сохранения
    QHBoxLayout *saveLayout = new QHBoxLayout();
    labelSavePath.setText("Путь для сохранения:");
    labelSavePath.setFont(font);
    saveLayout->addWidget(&labelSavePath);
    saveLayout->addWidget(&savePath, 1);
    openFileManager.setText("Обзор...");
    openFileManager.setFont(font);
    saveLayout->addWidget(&openFileManager);
    settingsLayout->addLayout(saveLayout);

    connect(&openFileManager, &QPushButton::clicked, this, [=]()//Вручную проставить путь к папке
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Выбор папки", QDir::homePath());
        if (!dir.isEmpty())
        {
            savePath.setText(dir);
        }
    });

    //Удаление исходного файла
    QHBoxLayout *deleteLayout = new QHBoxLayout();
    labelFileDelete.setText("Удалять исходный файл после обработки:");
    labelFileDelete.setFont(font);
    deleteLayout->addWidget(&labelFileDelete);
    deleteLayout->addWidget(&fileDelete);
    deleteLayout->addStretch(1);
    settingsLayout->addLayout(deleteLayout);

    //Действие при совпадении имени
    QHBoxLayout *modeLayout = new QHBoxLayout();
    labelSaveMode.setText("При совпадении имени файла:");
    labelSaveMode.setFont(font);
    modeLayout->addWidget(&labelSaveMode);
    modeLayout->addWidget(&saveMode);
    saveMode.addItems({"Перезаписать", "Модифицировать"});
    saveMode.setFont(font);
    settingsLayout->addLayout(modeLayout);

    //Работа по таймеру
    QHBoxLayout *timerLayout = new QHBoxLayout();
    labelWorkTimer.setText("Работа по таймеру:");
    labelWorkTimer.setFont(font);

    timerLayout->addWidget(&labelWorkTimer);
    timerLayout->addWidget(&workTimer);
    timerLayout->addStretch(0);
    timerLayout->addWidget(&setWorkTimer);
    timerLayout->addWidget(&unitsMeasurenentTimer);

    setWorkTimer.setText("1000");
    setWorkTimer.setFont(font);
    unitsMeasurenentTimer.addItems({"мс", "с", "м"});
    unitsMeasurenentTimer.setFont(font);
    unitsMeasurenentTimer.hide();
    setWorkTimer.hide();

    setWorkTimer.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(&workTimer, &QCheckBox::checkStateChanged, this, [=]()//Работа по таймеру
    {
        bool enabled = workTimer.isChecked();
        setWorkTimer.setVisible(enabled);
        unitsMeasurenentTimer.setVisible(enabled);

        if(enabled)
        {
            timerLayout->setStretch(2, 0);
            timerLayout->setStretch(3, 1);
        }
        else
        {
            timerLayout->setStretch(2, 1);
            timerLayout->setStretch(3, 0);
        }
    });
    settingsLayout->addLayout(timerLayout);

    // xor-ключ
    QHBoxLayout *xorLayout = new QHBoxLayout();
    labelXOR.setText("8-байтный ключ XOR:");
    labelXOR.setFont(font);
    xorLayout->addWidget(&labelXOR);
    xorLayout->addWidget(&textXOR, 1);
    textXOR.setText("0x12AB34CD");
    textXOR.setFont(font);
    settingsLayout->addLayout(xorLayout);

    mainLayout->addWidget(group);

    // Кнопка
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    startChanger.setText("Старт");
    startChanger.setMinimumHeight(34);
    startChanger.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(&startChanger);
    buttonsLayout->addStretch();
    mainLayout->addLayout(buttonsLayout);

    //ПрогрессБар
    QHBoxLayout *progressBarLayout = new QHBoxLayout();
    fileProgressBar.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBarLayout->addWidget(&fileProgressBar);
    mainLayout->addLayout(progressBarLayout);
    fileProgressBar.setRange(0, 100);

    //информация о файлах
    QGroupBox *fileListGroup = new QGroupBox("Обработанные файлы", this);
    fileListGroup->setFont(font);
    QVBoxLayout *fileListLayout = new QVBoxLayout(fileListGroup);

    fileListWidget = new QListWidget(this);
    fileListWidget->setFont(font);
    fileListWidget->setAlternatingRowColors(true);
    fileListLayout->addWidget(fileListWidget);

    mainLayout->addWidget(fileListGroup, 1);

    setMinimumSize(400, 500);

    setStyleSheet(R"(
    QGroupBox
    {
        font-weight: bold;
        border: 1px solid grey;
        border-radius: 6px;
        margin-top: 8px;
        padding: 10px;
    }
    QGroupBox::title
    {
        subcontrol-origin: margin;
        subcontrol-position: top left;
        padding: 0 6px;
        color: black;
    }
    QPushButton
    {
        background-color: #0078D7;
        color: white;
        border-radius: 6px;
        padding: 6px 12px;
    }
    QPushButton:hover
    {
        background-color: #2A52BE;
    }
    QPushButton:disabled
    {
        background-color: grey;
    }
    QPushButton:pressed
    {
        background-color: grey;
    }
    QProgressBar
    {
        background-color: grey;
    }
    )");
}

void MainWindow::setupFileReceiver()
{
    fileReceiver = new ReceivingFileTCP(this);

    //Параметры обработки
    connect(&maskFile, &QLineEdit::textChanged, fileReceiver, &ReceivingFileTCP::setFileMask);
    connect(&savePath, &QLineEdit::textChanged, fileReceiver, &ReceivingFileTCP::setSavePath);
    connect(&saveMode, &QComboBox::currentIndexChanged, fileReceiver, &ReceivingFileTCP::setSaveMode);

    //Сигналы от сетевых сокетов
    connect(fileReceiver, &ReceivingFileTCP::fileReceived, this, &MainWindow::onFileReceived);
    connect(fileReceiver, &ReceivingFileTCP::progressChanged, this, &MainWindow::progressChanged);
    connect(fileReceiver, &ReceivingFileTCP::errorOccurred, this, &MainWindow::onReceiverError);

    //Начальные значения
    fileReceiver->setFileMask(maskFile.text());
    fileReceiver->setSavePath(savePath.text());
    fileReceiver->setSaveMode(saveMode.currentIndex());

    // Подключение к серверу
    fileReceiver->connectToServer("127.0.0.1", 2323);
}

void MainWindow::onFileReceived(const QString &filePath)
{
    qDebug() << "Файл получен:" << filePath;

    if(!processingXor||!xorThread||!xorThread->isRunning())
    {
        qDebug() << "XOR процессор не готов, переинициализируем";
        setupXOR();
    }

    QString inputFile = filePath;
    QFileInfo fileInfo(inputFile);
    QString outputFile = fileInfo.path() + "/xor_" + fileInfo.fileName();

    startXORProcessing(inputFile, outputFile);
}

void MainWindow::onReceiverError(const QString &errorMessage)
{
    qDebug() << "Ошибка приемника файлов:" << errorMessage;
    // Можно добавить отображение ошибки в интерфейсе
}

void MainWindow::sendFileRequest()
{
    if (fileReceiver)
    {
        fileReceiver->sendFileRequest();
    }
}

void MainWindow::addProcessedFile(const QString &fileInfo)//Список обработанных файлов
{
    fileListWidget->insertItem(0, fileInfo);
    //Не больше 100
    if(fileListWidget->count() > 100)
    {
        delete fileListWidget->item(fileListWidget->count() - 1);
    }

    fileListWidget->scrollToTop();
}

void MainWindow::enebledOrDisabledGUI(bool state)//Включение/Отключение интерфейса
{
    maskFile.setEnabled(state);
    savePath.setEnabled(state);
    openFileManager.setEnabled(state);
    fileDelete.setEnabled(state);
    saveMode.setEnabled(state);
    workTimer.setEnabled(state);
    setWorkTimer.setEnabled(state);
    unitsMeasurenentTimer.setEnabled(state);
    textXOR.setEnabled(state);
}

void MainWindow::setupXOR()
{
    if(xorThread&&xorThread->isRunning())
    {
        if(processingXor)
        {
            QMetaObject::invokeMethod(processingXor, "cancelProcessing", Qt::QueuedConnection);
        }

        xorThread->quit();
        if(!xorThread->wait(3000))
        {
            qDebug() << "Принудительно останавливаем";
        }
        delete xorThread;
        processingXor = nullptr;
        xorThread = nullptr;
    }
    if(processingXor)
    {
        processingXor->deleteLater();
        processingXor = nullptr;
    }
    if(xorThread)
    {
        xorThread->deleteLater();
        xorThread = nullptr;
    }
    xorThread = new QThread(this);
    processingXor = new ProcessingXOR();
    processingXor->moveToThread(xorThread);
    //Изменение прогрессбара
    connect(processingXor, &ProcessingXOR::progressChanged, this,[this](int value)
    {
        int totalProgress = 50 + value / 2;
        emit progressChanged(totalProgress);
    });
    //Закончить обработку
    connect(processingXor, &ProcessingXOR::finished,this, &MainWindow::onXORFinished, Qt::QueuedConnection);
    //Ошибка обработки
    connect(processingXor, &ProcessingXOR::error,this, &MainWindow::onXORError, Qt::QueuedConnection);
    //Завершение потока
     connect(xorThread, &QThread::finished, processingXor, &QObject::deleteLater);
    xorThread->start();
}

void MainWindow::startXORProcessing(const QString &inputPath, const QString &outputPath)
{
    QMutexLocker locker(&processingMutex);
    if (isProcessing)
    {
        qDebug() << "Обработка уже выполняется, пропускаем запрос";
        return;
    }
    if(!processingXor || !xorThread || !xorThread->isRunning())
    {
        qDebug() << "XOR процессор не инициализирован, переинициализируем";
        setupXOR();
        QThread::msleep(50);
    }

    if(!processingXor || !xorThread || !xorThread->isRunning())
    {
        qDebug() << "XOR процессор не инициализирован";
        return;
    }
    isProcessing = true;

    // Получаем XOR ключи
    QVector<unsigned char> xorKeys(8, 0);
    QString keyText = textXOR.text();
    if (keyText.isEmpty())
    {
        qDebug() << "Ключ-XOR пуст, используются нули";
    }
    else
    {
        bool ok;
        quint64 keyValue = keyText.toULongLong(&ok, 0);

        if(ok)
        {
            for(int i = 0; i < 8; ++i)
            {
                xorKeys[7 - i] = (keyValue >> (i * 8)) & 0xFF;
            }
        }
        else
        {
            //используем байты строки
            QByteArray keyBytes = keyText.toUtf8();
            int bytesToCopy = qMin(8, keyBytes.size());
            for(int i = 0; i < bytesToCopy; ++i)
            {
                xorKeys[i] = keyBytes[i];
            }
            if (keyBytes.size() < 8)
            {
                qDebug() << "Строка короче 8 байт, остальные байты заполнены нулями";
            }
        }
    }

    bool deleteOriginal = fileDelete.isChecked();

    // Запускаем обработку в отдельном потоке
    QMetaObject::invokeMethod(processingXor, "processFile", Qt::QueuedConnection,Q_ARG(QString, inputPath),Q_ARG(QString, outputPath),Q_ARG(QVector<unsigned char>, xorKeys),Q_ARG(bool, deleteOriginal));
}

void MainWindow::onXORFinished(const QString &resultFile)
{
    qDebug() << "XOR обработка завершена:" << resultFile;
    addProcessedFile(resultFile);
    emit progressChanged(100);
    isProcessing = false;

    if(!workTimer.isChecked() && isSingleProcessing)
    {
        isSingleProcessing = false;
        startChanger.setText("Старт");
        enebledOrDisabledGUI(true);
    }
    if(workTimer.isChecked() && timer->isActive())
    {
        QTimer::singleShot(100, this, &MainWindow::sendFileRequest);
    }
}

void MainWindow::onXORError(const QString &errorMessage)
{
    qDebug() << "Ошибка XOR обработки:" << errorMessage;
    emit progressChanged(100);
    isProcessing = false;

    if(!workTimer.isChecked() && isSingleProcessing)
    {
        isSingleProcessing = false;
        startChanger.setText("Старт");
        enebledOrDisabledGUI(true);
    }
    if(workTimer.isChecked() && timer->isActive())
    {
        QTimer::singleShot(100, this, &MainWindow::sendFileRequest);
    }
}

void MainWindow::stopAllOperations()
{
    QMutexLocker locker(&stopMutex);
    stopRequested = true;

    // Останавливаем таймер
    if(timer && timer->isActive())
    {
        timer->stop();
    }
    // Прерываем обработку
    if(processingXor && xorThread && xorThread->isRunning())
    {
        QMetaObject::invokeMethod(processingXor, "cancelProcessing", Qt::BlockingQueuedConnection);
    }
    if(fileReceiver)
    {
        fileReceiver->cancelCurrentOperation();
    }
    // Ждем завершения потока с таймаутом
    if(xorThread && xorThread->isRunning())
    {
        xorThread->quit();
        if(!xorThread->wait(2000))
        {
            xorThread->terminate();
            xorThread->wait();
        }
    }

    isProcessing = false;
    isSingleProcessing = false;
    stopRequested = false;
    startChanger.setText("Старт");
    enebledOrDisabledGUI(true);
}
