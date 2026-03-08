#include "MainWindow.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QScrollArea>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_worker(new DspWorker(this)) {
    setupUi();
    connect(m_worker, &DspWorker::frameProcessed, this, &MainWindow::onFrameProcessed, Qt::QueuedConnection);
    connect(m_worker, &DspWorker::logReady, this, &MainWindow::appendLog, Qt::QueuedConnection);
    connect(m_worker, &DspWorker::processingFinished, this, &MainWindow::onProcessingFinished, Qt::QueuedConnection);
    connect(m_btnSelectFiles, &QPushButton::clicked, this, &MainWindow::onSelectFilesClicked);
}

MainWindow::~MainWindow() {
    m_worker->stop();
    m_worker->wait();
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    m_btnSelectFiles = new QPushButton("选择包含所有 RAW 文件的【根目录】并开始分析", this);
    m_btnSelectFiles->setMinimumHeight(40);

    m_spatialPlot = new QCustomPlot(this);
    m_spatialPlot->setMinimumHeight(250);
    m_spatialPlot->addGraph();
    m_spatialPlot->graph(0)->setName("CBF (常规波束形成)");
    m_spatialPlot->graph(0)->setPen(QPen(Qt::gray, 2, Qt::DashLine));
    m_spatialPlot->addGraph();
    m_spatialPlot->graph(1)->setName("DCV (反卷积高分辨)");
    m_spatialPlot->graph(1)->setPen(QPen(Qt::blue, 2));
    m_spatialPlot->plotLayout()->insertRow(0);
    m_plotTitle = new QCPTextElement(m_spatialPlot, "宽带空间谱实时折线图", QFont("sans", 12, QFont::Bold));
    m_spatialPlot->plotLayout()->addElement(0, 0, m_plotTitle);
    m_spatialPlot->xAxis->setLabel("方位角/°");
    m_spatialPlot->yAxis->setLabel("归一化功率/dB");
    m_spatialPlot->xAxis->setRange(0, 180);
    m_spatialPlot->yAxis->setRange(-40, 5);
    m_spatialPlot->legend->setVisible(true);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    m_targetPanelWidget = new QWidget(scrollArea);
    m_targetLayout = new QGridLayout(m_targetPanelWidget);
    m_targetLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(m_targetPanelWidget);

    m_logConsole = new QPlainTextEdit(this);
    m_logConsole->setReadOnly(true);
    m_logConsole->setMaximumHeight(150);
    m_logConsole->setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: Consolas;");

    mainLayout->addWidget(m_btnSelectFiles);
    mainLayout->addWidget(m_spatialPlot, 1);
    mainLayout->addWidget(scrollArea, 2);
    mainLayout->addWidget(m_logConsole, 0);

    setCentralWidget(centralWidget);
    resize(1200, 900);
    setWindowTitle("宽带方位动态跟踪与解耦系统 (C++ 终极版)");
}

void MainWindow::onSelectFilesClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择包含各个目标RAW文件的【根目录】", "");
    if (dir.isEmpty()) return;

    m_logConsole->clear();
    appendLog(QString("已选择根目录: %1\n引擎准备就绪...\n").arg(dir));

    qDeleteAll(m_lofarPlots); m_lofarPlots.clear();
    qDeleteAll(m_demonPlots); m_demonPlots.clear();
    m_historyResults.clear();

    m_worker->setDirectory(dir);
    m_worker->start();
}

void MainWindow::createTargetPlots(int targetId) {
    QCustomPlot* lofarPlot = new QCustomPlot(this);
    lofarPlot->setMinimumHeight(200);
    lofarPlot->addGraph();
    lofarPlot->graph(0)->setPen(QPen(Qt::red, 1.5));
    lofarPlot->xAxis->setLabel("频率/Hz");
    lofarPlot->yAxis->setLabel("功率/dB");
    lofarPlot->xAxis->setRange(80, 250);
    lofarPlot->yAxis->setRange(-60, 40);
    lofarPlot->plotLayout()->insertRow(0);
    QCPTextElement* titleLofar = new QCPTextElement(lofarPlot, QString("目标%1 载入中...").arg(targetId), QFont("sans", 10, QFont::Bold));
    lofarPlot->plotLayout()->addElement(0, 0, titleLofar);

    QCustomPlot* demonPlot = new QCustomPlot(this);
    demonPlot->setMinimumHeight(200);
    demonPlot->addGraph();
    demonPlot->graph(0)->setPen(QPen(Qt::darkGreen, 1.5));
    demonPlot->xAxis->setLabel("频率/Hz");
    demonPlot->yAxis->setLabel("归一幅度");
    demonPlot->xAxis->setRange(0, 100);
    demonPlot->yAxis->setRange(0, 1.1);
    demonPlot->plotLayout()->insertRow(0);
    QCPTextElement* titleDemon = new QCPTextElement(demonPlot, QString("目标%1 载入中...").arg(targetId), QFont("sans", 10, QFont::Bold));
    demonPlot->plotLayout()->addElement(0, 0, titleDemon);

    m_lofarPlots.insert(targetId, lofarPlot);
    m_demonPlots.insert(targetId, demonPlot);

    int col = targetId - 1;
    m_targetLayout->addWidget(lofarPlot, 0, col);
    m_targetLayout->addWidget(demonPlot, 1, col);
}

void MainWindow::appendLog(const QString& log) {
    m_logConsole->appendPlainText(log);
    m_logConsole->moveCursor(QTextCursor::End);
}

void MainWindow::onFrameProcessed(const FrameResult& result) {
    m_historyResults.append(result);

    m_spatialPlot->graph(0)->setData(result.thetaAxis, result.cbfData);
    m_spatialPlot->graph(1)->setData(result.thetaAxis, result.dcvData);
    m_plotTitle->setText(QString("宽带空间谱实时折线图 (第%1帧 | 时间: %2s)").arg(result.frameIndex).arg(result.timestamp));
    m_spatialPlot->replot();

    for (const TargetTrack& t : result.tracks) {
        if (!m_lofarPlots.contains(t.id)) {
            createTargetPlots(t.id);
        }

        QCustomPlot* lofarPlot = m_lofarPlots[t.id];
        QCustomPlot* demonPlot = m_demonPlots[t.id];

        QString statusStr = t.isActive ? "[跟踪中]" : "[已熄火/丢失]";
        QColor lineColor  = t.isActive ? Qt::red : Qt::darkGray;
        QColor demonColor = t.isActive ? Qt::darkGreen : Qt::darkGray;
        QColor bgColor    = t.isActive ? Qt::white : QColor(240, 240, 240);
        QColor textColor  = t.isActive ? Qt::black : Qt::gray;

        lofarPlot->setBackground(QBrush(bgColor));
        demonPlot->setBackground(QBrush(bgColor));

        lofarPlot->xAxis->setTickLabelColor(textColor); lofarPlot->xAxis->setLabelColor(textColor);
        lofarPlot->yAxis->setTickLabelColor(textColor); lofarPlot->yAxis->setLabelColor(textColor);
        demonPlot->xAxis->setTickLabelColor(textColor); demonPlot->xAxis->setLabelColor(textColor);
        demonPlot->yAxis->setTickLabelColor(textColor); demonPlot->yAxis->setLabelColor(textColor);

        QString titleLofarStr = QString("目标%1 (%2°) %3 (第%4帧) LOFAR")
                .arg(t.id).arg(t.currentAngle, 0, 'f', 1).arg(statusStr).arg(result.frameIndex);
        QString titleDemonStr;
        if (t.isActive) {
            titleDemonStr = QString("目标%1 轴频: %2 Hz %3 (第%4帧)")
                    .arg(t.id).arg(t.shaftFreq, 0, 'f', 1).arg(statusStr).arg(result.frameIndex);
        } else {
            titleDemonStr = QString("目标%1 轴频: -- Hz %2 (第%3帧)")
                    .arg(t.id).arg(statusStr).arg(result.frameIndex);
        }

        if (QCPTextElement* titleLofar = qobject_cast<QCPTextElement*>(lofarPlot->plotLayout()->element(0, 0))) {
            titleLofar->setText(titleLofarStr);
            titleLofar->setTextColor(textColor);
        }
        if (QCPTextElement* titleDemon = qobject_cast<QCPTextElement*>(demonPlot->plotLayout()->element(0, 0))) {
            titleDemon->setText(titleDemonStr);
            titleDemon->setTextColor(textColor);
        }

        if (!t.lofarSpectrum.isEmpty()) {
            QVector<double> f_lofar(t.lofarSpectrum.size());
            for(int i=0; i<f_lofar.size(); ++i) f_lofar[i] = 80.0 + i * (170.0 / f_lofar.size());
            lofarPlot->graph(0)->setData(f_lofar, t.lofarSpectrum);
            lofarPlot->graph(0)->setPen(QPen(lineColor, 1.5));
            lofarPlot->replot();
        }

        if (!t.demonSpectrum.isEmpty()) {
            QVector<double> f_demon(t.demonSpectrum.size());
            for(int i=0; i<f_demon.size(); ++i) f_demon[i] = (i + 1) * (5000.0 / 30000.0);
            demonPlot->graph(0)->setData(f_demon, t.demonSpectrum);
            demonPlot->graph(0)->setPen(QPen(demonColor, 1.5));
            demonPlot->replot();
        }
    }
}

void MainWindow::onProcessingFinished() {
    if (m_historyResults.isEmpty()) return;

    // 创建独立的离线分析窗口
    QWidget* offlineWindow = new QWidget();
    offlineWindow->setAttribute(Qt::WA_DeleteOnClose);
    offlineWindow->setWindowTitle("离线后处理特征分析报表");
    offlineWindow->resize(1300, 850);
    QVBoxLayout* layout = new QVBoxLayout(offlineWindow);
    QTabWidget* tabWidget = new QTabWidget(offlineWindow);
    layout->addWidget(tabWidget);

    int num_frames = m_historyResults.size();
    double min_time = m_historyResults.first().timestamp;
    double max_time = m_historyResults.last().timestamp;
    if (std::abs(max_time - min_time) < 0.1) max_time = min_time + 3.0; // 容错处理

    // --- Tab 1: DCV 空间方位谱瀑布图 (全方位历程) ---
    QWidget* dcvWidget = new QWidget();
    QVBoxLayout* dcvLayout = new QVBoxLayout(dcvWidget);
    QCustomPlot* dcvPlot = new QCustomPlot(dcvWidget);
    dcvLayout->addWidget(dcvPlot);
    tabWidget->addTab(dcvWidget, "高分辨DCV空间方位谱历程");

    int nx = m_historyResults.first().thetaAxis.size();
    QCPColorMap *colorMap = new QCPColorMap(dcvPlot->xAxis, dcvPlot->yAxis);
    colorMap->data()->setSize(nx, num_frames);
    colorMap->data()->setRange(QCPRange(0, 180), QCPRange(min_time, max_time));

    // 动态探测全局最大值
    double dcv_global_max = -999.0;
    for (const auto& frame : m_historyResults) {
        for (double val : frame.dcvData) if(val > dcv_global_max) dcv_global_max = val;
    }

    // 填充数据：执行归一化，将能量最高值对齐到 0dB
    for (int t = 0; t < num_frames; ++t) {
        for (int x = 0; x < nx; ++x) {
            double raw_val = m_historyResults[t].dcvData[x];
            colorMap->data()->setCell(x, t, raw_val - dcv_global_max);
        }
    }

    // 视觉效果配置
    colorMap->setGradient(QCPColorGradient::gpJet); // MATLAB jet
    colorMap->setInterpolate(true);                 // 开启平滑插值
    colorMap->setDataRange(QCPRange(-35.0, 0));     // 锁定显示范围
    colorMap->setTightBoundary(true);               // 数据填满坐标边缘

    // 色标显示
    QCPColorScale *colorScale = new QCPColorScale(dcvPlot);
    dcvPlot->plotLayout()->addElement(0, 1, colorScale);
    colorMap->setColorScale(colorScale);
    colorScale->axis()->setLabel("归一化幅度 (dB)");

    // 坐标轴配置
    dcvPlot->xAxis->setLabel("方位角/°");
    dcvPlot->yAxis->setLabel("物理时间/s");
    dcvPlot->xAxis->setRange(0, 180);
    dcvPlot->yAxis->setRange(min_time, max_time);
    dcvPlot->replot();

    // --- Tab 2: 各目标专属动态 LOFAR 瀑布图 ---
    QWidget* lofarWidget = new QWidget();
    QGridLayout* lofarLayout = new QGridLayout(lofarWidget);
    tabWidget->addTab(lofarWidget, "目标专属动态 LOFAR 谱历程");

    QSet<int> targetIds;
    for (const auto& frame : m_historyResults) {
        for (const auto& track : frame.tracks) targetIds.insert(track.id);
    }

    int colCount = 0;
    for (int id : targetIds) {
        QCustomPlot* p = new QCustomPlot(lofarWidget);
        lofarLayout->addWidget(p, 0, colCount++);

        p->plotLayout()->insertRow(0);
        QCPTextElement* title = new QCPTextElement(p, QString("目标%1 LOFAR 特征历程").arg(id), QFont("sans", 11, QFont::Bold));
        p->plotLayout()->addElement(0, 0, title);

        // 确定该目标的数据维度
        int f_size = 0;
        for(const auto& fr : m_historyResults) {
            for(const auto& tr : fr.tracks) {
                if(tr.id == id && !tr.lofarSpectrum.isEmpty()) { f_size = tr.lofarSpectrum.size(); break; }
            }
            if(f_size > 0) break;
        }
        if (f_size == 0) continue;

        QCPColorMap *cmap = new QCPColorMap(p->xAxis, p->yAxis);
        cmap->data()->setSize(f_size, num_frames);
        cmap->data()->setRange(QCPRange(80, 250), QCPRange(min_time, max_time));

        // 探测该目标自身的能量极值
        double lmax = -999.0;
        for (const auto& frame : m_historyResults) {
            for (const auto& track : frame.tracks) {
                if (track.id == id) {
                    for(double v : track.lofarSpectrum) if(v > lmax) lmax = v;
                }
            }
        }

        // 填充数据
        for (int t = 0; t < num_frames; ++t) {
            const auto& frame = m_historyResults[t];
            bool found = false;
            for (const auto& track : frame.tracks) {
                if (track.id == id && !track.lofarSpectrum.isEmpty()) {
                    for (int x = 0; x < f_size; ++x) cmap->data()->setCell(x, t, track.lofarSpectrum[x] - lmax);
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int x = 0; x < f_size; ++x) cmap->data()->setCell(x, t, -120.0); // 熄火时刻全蓝
            }
        }

        cmap->setGradient(QCPColorGradient::gpJet);
        cmap->setInterpolate(true);
        cmap->setDataRange(QCPRange(-40.0, 0)); // 展现 40dB 的动态范围
        cmap->setTightBoundary(true);

        QCPColorScale *cscale = new QCPColorScale(p);
        p->plotLayout()->addElement(1, 1, cscale);
        cmap->setColorScale(cscale);

        p->xAxis->setLabel("频率/Hz");
        p->yAxis->setLabel("物理时间/s");
        p->xAxis->setRange(80, 250);
        p->yAxis->setRange(min_time, max_time);
        p->replot();
    }

    offlineWindow->show();
}
