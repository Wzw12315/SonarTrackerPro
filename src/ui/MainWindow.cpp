#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QScrollArea>
#include <QDateTime>
#include <QSplitter>
#include <cmath>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_worker(new DspWorker(this)) {
    setupUi();

    connect(m_btnSelectFiles, &QPushButton::clicked, this, &MainWindow::onSelectFilesClicked);
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_btnPauseResume, &QPushButton::clicked, this, &MainWindow::onPauseResumeClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_btnExport, &QPushButton::clicked, this, &MainWindow::onExportClicked);

    connect(m_worker, &DspWorker::frameProcessed, this, &MainWindow::onFrameProcessed, Qt::QueuedConnection);
    connect(m_worker, &DspWorker::logReady, this, &MainWindow::appendLog, Qt::QueuedConnection);
    connect(m_worker, &DspWorker::reportReady, this, &MainWindow::appendReport, Qt::QueuedConnection);
    connect(m_worker, &DspWorker::offlineResultsReady, this, &MainWindow::onOfflineResultsReady, Qt::QueuedConnection);
    connect(m_worker, &DspWorker::processingFinished, this, &MainWindow::onProcessingFinished, Qt::QueuedConnection);
}

MainWindow::~MainWindow() {
    m_worker->stop(); m_worker->wait();
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainVLayout = new QVBoxLayout(centralWidget);
    QSplitter* verticalSplitter = new QSplitter(Qt::Vertical, centralWidget);

    QWidget* topWidget = new QWidget(verticalSplitter);
    QHBoxLayout* topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* leftPanel = new QWidget(topWidget);
    leftPanel->setFixedWidth(320);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0,0,0,0);

    QGroupBox* groupButtons = new QGroupBox("控制指令区", leftPanel);
    QVBoxLayout* btnLayout = new QVBoxLayout(groupButtons);
    m_btnSelectFiles = new QPushButton("数据文件输入...", this);
    m_btnStart = new QPushButton("▶ 开始处理", this);
    m_btnPauseResume = new QPushButton("⏸ 暂停/继续", this);
    m_btnStop = new QPushButton("⏹ 终止处理", this);
    m_btnExport = new QPushButton("💾 数据结果导出", this);
    m_btnStart->setEnabled(false); m_btnPauseResume->setEnabled(false); m_btnStop->setEnabled(false);
    btnLayout->addWidget(m_btnSelectFiles); btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnPauseResume); btnLayout->addWidget(m_btnStop); btnLayout->addWidget(m_btnExport);
    leftLayout->addWidget(groupButtons);

    QGroupBox* groupParams = new QGroupBox("核心参数设置", leftPanel);
    QFormLayout* paramLayout = new QFormLayout(groupParams);
    paramLayout->addRow("采样率 (Hz):", new QLineEdit("5000", this));
    paramLayout->addRow("阵元数目:", new QLineEdit("512", this));
    paramLayout->addRow("时间步进 (s):", new QLineEdit("3.0", this));
    leftLayout->addWidget(groupParams);

    QGroupBox* groupInfo = new QGroupBox("系统状态看板", leftPanel);
    QVBoxLayout* infoLayout = new QVBoxLayout(groupInfo);
    m_lblSysInfo = new QLabel("系统启动完毕。\n等待输入数据...", this);
    m_lblSysInfo->setStyleSheet("color: #333333; font-weight: bold;");
    infoLayout->addWidget(m_lblSysInfo);
    leftLayout->addWidget(groupInfo);

    QGroupBox* groupLog = new QGroupBox("实时流水日志终端", leftPanel);
    QVBoxLayout* logLayout = new QVBoxLayout(groupLog);
    m_logConsole = new QPlainTextEdit(this); m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: Consolas;");
    logLayout->addWidget(m_logConsole);
    leftLayout->addWidget(groupLog, 1);

    topLayout->addWidget(leftPanel);

    m_mainTabWidget = new QTabWidget(topWidget);
    topLayout->addWidget(m_mainTabWidget, 1);

    // --- Tab 1 ---
    QWidget* tab1 = new QWidget();
    QHBoxLayout* tab1Layout = new QHBoxLayout(tab1);
    QSplitter* horizontalSplitter = new QSplitter(Qt::Horizontal, tab1);

    QWidget* midPanel = new QWidget(horizontalSplitter);
    QVBoxLayout* midLayout = new QVBoxLayout(midPanel);
    m_timeAzimuthPlot = new QCustomPlot(midPanel);
    m_timeAzimuthPlot->addGraph();
    m_timeAzimuthPlot->graph(0)->setLineStyle(QCPGraph::lsNone);
    m_timeAzimuthPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::black, 7));
    m_timeAzimuthPlot->plotLayout()->insertRow(0);
    m_timeAzimuthPlot->plotLayout()->addElement(0, 0, new QCPTextElement(m_timeAzimuthPlot, "宽带实时方位检测提取结果", QFont("sans", 12, QFont::Bold)));
    m_timeAzimuthPlot->xAxis->setLabel("方位角/°"); m_timeAzimuthPlot->yAxis->setLabel("物理时间/s");
    m_timeAzimuthPlot->xAxis->setRange(0, 180);
    m_timeAzimuthPlot->yAxis->setRangeReversed(true);
    midLayout->addWidget(m_timeAzimuthPlot);

    QWidget* rightPanel = new QWidget(horizontalSplitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);

    m_spatialPlot = new QCustomPlot(rightPanel);
    m_spatialPlot->setMinimumHeight(250); m_spatialPlot->setMaximumHeight(350);
    m_spatialPlot->addGraph(); m_spatialPlot->graph(0)->setName("CBF (常规波束)"); m_spatialPlot->graph(0)->setPen(QPen(Qt::gray, 2, Qt::DashLine));
    m_spatialPlot->addGraph(); m_spatialPlot->graph(1)->setName("DCV (高分辨)"); m_spatialPlot->graph(1)->setPen(QPen(Qt::blue, 2));
    m_spatialPlot->plotLayout()->insertRow(0);
    m_plotTitle = new QCPTextElement(m_spatialPlot, "宽带空间谱实时折线图", QFont("sans", 12, QFont::Bold));
    m_spatialPlot->plotLayout()->addElement(0, 0, m_plotTitle);
    m_spatialPlot->xAxis->setLabel("方位角/°"); m_spatialPlot->yAxis->setLabel("归一化功率/dB");
    m_spatialPlot->xAxis->setRange(0, 180); m_spatialPlot->yAxis->setRange(-40, 5); m_spatialPlot->legend->setVisible(true);
    rightLayout->addWidget(m_spatialPlot);

    QScrollArea* scrollArea = new QScrollArea(rightPanel);
    scrollArea->setWidgetResizable(true);
    m_targetPanelWidget = new QWidget(scrollArea);
    m_targetLayout = new QGridLayout(m_targetPanelWidget);
    m_targetLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(m_targetPanelWidget);
    rightLayout->addWidget(scrollArea, 1);

    horizontalSplitter->addWidget(midPanel);
    horizontalSplitter->addWidget(rightPanel);
    horizontalSplitter->setStretchFactor(0, 1);
    horizontalSplitter->setStretchFactor(1, 3);
    tab1Layout->addWidget(horizontalSplitter);
    m_mainTabWidget->addTab(tab1, "💻 实时特征动态刷新区");

    // --- Tab 2 (纯净 DCV 全景，去除冗余 LOFAR) ---
    QWidget* tab2 = new QWidget();
    m_tab2Layout = new QVBoxLayout(tab2);
    m_dcvWaterfallPlot = new QCustomPlot(tab2);
    m_dcvWaterfallPlot->plotLayout()->insertRow(0);
    m_dcvWaterfallPlot->plotLayout()->addElement(0, 0, new QCPTextElement(m_dcvWaterfallPlot, "高分辨反卷积(DCV) 全方位时空谱历程", QFont("sans", 14, QFont::Bold)));
    m_tab2Layout->addWidget(m_dcvWaterfallPlot);
    m_mainTabWidget->addTab(tab2, "📊 后处理: 空间方位谱全景");

    // --- Tab 3 (深度解耦分析) ---
    QWidget* tab3 = new QWidget();
    QVBoxLayout* tab3Layout = new QVBoxLayout(tab3);
    QScrollArea* lofarScroll = new QScrollArea(tab3);
    lofarScroll->setWidgetResizable(true);
    m_lofarWaterfallWidget = new QWidget(lofarScroll);
    m_lofarWaterfallLayout = new QGridLayout(m_lofarWaterfallWidget);
    m_lofarWaterfallLayout->setAlignment(Qt::AlignTop);
    lofarScroll->setWidget(m_lofarWaterfallWidget);
    tab3Layout->addWidget(lofarScroll);
    m_mainTabWidget->addTab(tab3, "📉 后处理: 深度解耦与DP线谱提取");

    verticalSplitter->addWidget(topWidget);

    QGroupBox* groupReport = new QGroupBox("综合处理评估报告终端", verticalSplitter);
    QVBoxLayout* reportLayout = new QVBoxLayout(groupReport);
    m_reportConsole = new QPlainTextEdit(this);
    m_reportConsole->setReadOnly(true);
    m_reportConsole->setStyleSheet("background-color: #2b2b2b; color: #ffaa00; font-family: Consolas; font-size: 13px;");
    reportLayout->addWidget(m_reportConsole);

    verticalSplitter->addWidget(groupReport);
    verticalSplitter->setStretchFactor(0, 4);
    verticalSplitter->setStretchFactor(1, 1);

    mainVLayout->addWidget(verticalSplitter);

    setCentralWidget(centralWidget);
    resize(1600, 1000);
    setWindowTitle("SonarTrackerPro - 宽带方位动态跟踪与解耦系统");
}

void MainWindow::onSelectFilesClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据根目录", "");
    if (dir.isEmpty()) return;
    m_currentDir = dir;
    m_lblSysInfo->setText(QString("状态: 就绪\n目录: %1").arg(dir));
    appendLog(QString("已选择目录: %1\n请点击【开始处理】...\n").arg(dir));
    m_btnStart->setEnabled(true);
}

void MainWindow::onStartClicked() {
    if (m_currentDir.isEmpty()) return;

    m_btnStart->setEnabled(false); m_btnSelectFiles->setEnabled(false);
    m_btnPauseResume->setEnabled(true); m_btnStop->setEnabled(true);
    m_mainTabWidget->setCurrentIndex(0);
    m_lblSysInfo->setText(QString("状态: 运行中\n开始时间: %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

    m_historyResults.clear();
    m_timeAzimuthPlot->graph(0)->data()->clear();
    m_reportConsole->clear();
    m_logConsole->clear();

    QLayoutItem* item;

    while ((item = m_targetLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    m_lsPlots.clear(); m_lofarPlots.clear(); m_demonPlots.clear();

    if (m_dcvWaterfallPlot) {
        m_dcvWaterfallPlot->clearPlottables();
        m_dcvWaterfallPlot->replot();
    }

    while ((item = m_lofarWaterfallLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    m_worker->setDirectory(m_currentDir);
    m_worker->start();
}

void MainWindow::onPauseResumeClicked() {
    if (m_worker->isRunning()) {
        if (m_worker->isPaused()) {
            m_worker->resume(); m_lblSysInfo->setText("状态: 运行中 (恢复)"); appendLog("\n>> 系统恢复处理...\n");
        } else {
            m_worker->pause(); m_lblSysInfo->setText("状态: 已挂起 (暂停)"); appendLog("\n>> 系统已暂停处理...\n");
        }
    }
}

void MainWindow::onStopClicked() {
    if (m_worker->isRunning()) {
        m_worker->stop(); m_lblSysInfo->setText("状态: 已手动终止"); appendLog("\n>> 接收到终止指令...\n");
        m_btnStart->setEnabled(true); m_btnSelectFiles->setEnabled(true);
        m_btnPauseResume->setEnabled(false); m_btnStop->setEnabled(false);
    }
}

void MainWindow::onExportClicked() { appendLog("\n>> 数据导出模块加载中 (待实现)...\n"); }

void MainWindow::createTargetPlots(int targetId) {
    QCustomPlot* lsPlot = new QCustomPlot(this);
    lsPlot->setMinimumHeight(200); lsPlot->addGraph(); lsPlot->graph(0)->setPen(QPen(Qt::red, 1.5));
    lsPlot->xAxis->setLabel("频率/Hz"); lsPlot->yAxis->setLabel("功率/dB");
    lsPlot->xAxis->setRange(80, 250); lsPlot->yAxis->setRange(-60, 40);
    lsPlot->plotLayout()->insertRow(0);
    lsPlot->plotLayout()->addElement(0, 0, new QCPTextElement(lsPlot, "", QFont("sans", 9, QFont::Bold)));

    QCustomPlot* lofarPlot = new QCustomPlot(this);
    lofarPlot->setMinimumHeight(200); lofarPlot->addGraph(); lofarPlot->graph(0)->setPen(QPen(Qt::blue, 1.5));
    lofarPlot->xAxis->setLabel("频率/Hz"); lofarPlot->yAxis->setLabel("功率/dB");
    lofarPlot->xAxis->setRange(80, 250); lofarPlot->yAxis->setRange(-60, 40);
    lofarPlot->plotLayout()->insertRow(0);
    lofarPlot->plotLayout()->addElement(0, 0, new QCPTextElement(lofarPlot, "", QFont("sans", 9, QFont::Bold)));

    QCustomPlot* demonPlot = new QCustomPlot(this);
    demonPlot->setMinimumHeight(200); demonPlot->addGraph(); demonPlot->graph(0)->setPen(QPen(Qt::darkGreen, 1.5));
    demonPlot->xAxis->setLabel("频率/Hz"); demonPlot->yAxis->setLabel("归一幅度");
    demonPlot->xAxis->setRange(0, 100); demonPlot->yAxis->setRange(0, 1.1);
    demonPlot->plotLayout()->insertRow(0);
    demonPlot->plotLayout()->addElement(0, 0, new QCPTextElement(demonPlot, "", QFont("sans", 9, QFont::Bold)));

    m_lsPlots.insert(targetId, lsPlot);
    m_lofarPlots.insert(targetId, lofarPlot);
    m_demonPlots.insert(targetId, demonPlot);

    int col = targetId - 1;
    m_targetLayout->addWidget(lsPlot, 0, col);
    m_targetLayout->addWidget(lofarPlot, 1, col);
    m_targetLayout->addWidget(demonPlot, 2, col);
}

void MainWindow::onFrameProcessed(const FrameResult& result) {
    m_historyResults.append(result);

    m_spatialPlot->graph(0)->setData(result.thetaAxis, result.cbfData);
    m_spatialPlot->graph(1)->setData(result.thetaAxis, result.dcvData);
    m_plotTitle->setText(QString("宽带空间谱实时折线图 (第%1帧 | 时间: %2s)").arg(result.frameIndex).arg(result.timestamp));
    m_spatialPlot->replot();

    for (double ang : result.detectedAngles) m_timeAzimuthPlot->graph(0)->addData(ang, result.timestamp);
    m_timeAzimuthPlot->yAxis->setRange(std::max(0.0, result.timestamp - 30.0), result.timestamp + 5.0);
    m_timeAzimuthPlot->replot();

    for (const TargetTrack& t : result.tracks) {
        if (!m_lofarPlots.contains(t.id)) createTargetPlots(t.id);

        QCustomPlot* lsp = m_lsPlots[t.id];
        QCustomPlot* lp = m_lofarPlots[t.id];
        QCustomPlot* dp = m_demonPlots[t.id];

        QString statusStr = t.isActive ? "[跟踪中]" : "[已熄火]";
        QColor lsColor    = t.isActive ? Qt::red : Qt::darkGray;
        QColor lofarColor = t.isActive ? Qt::blue : Qt::darkGray;
        QColor demonColor = t.isActive ? Qt::darkGreen : Qt::darkGray;
        QColor bgColor    = t.isActive ? Qt::white : QColor(240, 240, 240);
        QColor textColor  = t.isActive ? Qt::black : Qt::gray;

        lsp->setBackground(bgColor); lp->setBackground(bgColor); dp->setBackground(bgColor);

        // 【优化】：加入实时的 (方位: XX°) 信息
        QString t1 = QString("目标%1 (方位: %2°) 线谱提取 (第%3帧)")
                        .arg(t.id).arg(t.currentAngle, 0, 'f', 1).arg(result.frameIndex);
        QString t2 = QString("目标%1 (方位: %2°) 波束定向功率谱估计 %3")
                        .arg(t.id).arg(t.currentAngle, 0, 'f', 1).arg(statusStr);
        QString t3 = t.isActive ? QString("目标%1 (方位: %2°) DEMON谱提取: %3Hz")
                                    .arg(t.id).arg(t.currentAngle, 0, 'f', 1).arg(t.shaftFreq, 0, 'f', 1)
                                : QString("目标%1 (方位: %2°) DEMON谱提取: --Hz")
                                    .arg(t.id).arg(t.currentAngle, 0, 'f', 1);

        if (auto* title = qobject_cast<QCPTextElement*>(lsp->plotLayout()->element(0, 0))) { title->setText(t1); title->setTextColor(textColor); }
        if (auto* title = qobject_cast<QCPTextElement*>(lp->plotLayout()->element(0, 0))) { title->setText(t2); title->setTextColor(textColor); }
        if (auto* title = qobject_cast<QCPTextElement*>(dp->plotLayout()->element(0, 0))) { title->setText(t3); title->setTextColor(textColor); }

        if (!t.lofarSpectrum.isEmpty()) {
            QVector<double> f_lofar(t.lofarSpectrum.size());
            for(int i=0; i<f_lofar.size(); ++i) f_lofar[i] = 80.0 + i * (170.0 / f_lofar.size());

            if (!t.lineSpectrumAmp.isEmpty()) {
                lsp->graph(0)->setData(f_lofar, t.lineSpectrumAmp); lsp->graph(0)->setPen(QPen(lsColor, 1.5)); lsp->replot();
            }
            lp->graph(0)->setData(f_lofar, t.lofarSpectrum); lp->graph(0)->setPen(QPen(lofarColor, 1.5)); lp->replot();
        }
        if (!t.demonSpectrum.isEmpty()) {
            QVector<double> f_demon(t.demonSpectrum.size());
            for(int i=0; i<f_demon.size(); ++i) f_demon[i] = (i + 1) * (5000.0 / 30000.0);
            dp->graph(0)->setData(f_demon, t.demonSpectrum); dp->graph(0)->setPen(QPen(demonColor, 1.5)); dp->replot();
        }
    }
}

void MainWindow::appendLog(const QString& log) {
    m_logConsole->appendPlainText(log);
    m_logConsole->moveCursor(QTextCursor::End);
}

void MainWindow::appendReport(const QString& report) {
    m_reportConsole->appendPlainText(report);
    m_reportConsole->moveCursor(QTextCursor::End);
}

// =========================================================
// Tab 3: 自适应截取最佳观察窗的 3xN 图矩阵
// =========================================================
void MainWindow::onOfflineResultsReady(const QList<OfflineTargetResult>& results) {
    if (results.isEmpty()) return;

    int col = 0;
    for (const auto& res : results) {
        // --- 0: 原始 LOFAR (放大到最佳频带) ---
        QCustomPlot* pRaw = new QCustomPlot(m_lofarWaterfallWidget);
        pRaw->setMinimumSize(400, 250); m_lofarWaterfallLayout->addWidget(pRaw, 0, col);
        pRaw->plotLayout()->insertRow(0);
        pRaw->plotLayout()->addElement(0, 0, new QCPTextElement(pRaw, QString("目标%1 原始LOFAR谱 (自适应局部放大)").arg(res.targetId), QFont("sans", 10, QFont::Bold)));
        QCPColorMap *cmapRaw = new QCPColorMap(pRaw->xAxis, pRaw->yAxis);
        cmapRaw->data()->setSize(res.freqBins, res.timeFrames); cmapRaw->data()->setRange(QCPRange(0, 2500.0), QCPRange(res.minTime, res.maxTime));
        double rmax = -999; for(double v : res.rawLofarDb) if(v > rmax) rmax = v;
        for(int t=0; t<res.timeFrames; ++t) for(int f=0; f<res.freqBins; ++f) cmapRaw->data()->setCell(f, t, res.rawLofarDb[t * res.freqBins + f] - rmax);
        cmapRaw->setGradient(QCPColorGradient::gpJet); cmapRaw->setInterpolate(true);
        cmapRaw->setDataRange(QCPRange(-40.0, 0)); cmapRaw->setTightBoundary(true);

        pRaw->xAxis->setLabel("频率/Hz"); pRaw->yAxis->setLabel("物理时间/s");
        // 【关键优化】：使用 DspWorker 智能分析的最佳显示频段！
        pRaw->xAxis->setRange(res.displayFreqMin, res.displayFreqMax);
        pRaw->yAxis->setRange(res.minTime, res.maxTime);

        // --- 1: TPSW 均衡谱 ---
        QCustomPlot* pTpsw = new QCustomPlot(m_lofarWaterfallWidget);
        pTpsw->setMinimumSize(400, 250); m_lofarWaterfallLayout->addWidget(pTpsw, 1, col);
        pTpsw->plotLayout()->insertRow(0); pTpsw->plotLayout()->addElement(0, 0, new QCPTextElement(pTpsw, "历史LOFAR谱 (TPSW均衡)", QFont("sans", 10, QFont::Bold)));
        QCPColorMap *cmapTpsw = new QCPColorMap(pTpsw->xAxis, pTpsw->yAxis);
        cmapTpsw->data()->setSize(res.freqBins, res.timeFrames); cmapTpsw->data()->setRange(QCPRange(0, 2500.0), QCPRange(res.minTime, res.maxTime));
        double tmax = -999; for(double v : res.tpswLofarDb) if(v > tmax) tmax = v;
        for(int t=0; t<res.timeFrames; ++t) for(int f=0; f<res.freqBins; ++f) cmapTpsw->data()->setCell(f, t, res.tpswLofarDb[t * res.freqBins + f] - tmax);
        cmapTpsw->setGradient(QCPColorGradient::gpJet); cmapTpsw->setInterpolate(true);
        cmapTpsw->setDataRange(QCPRange(-15.0, 0)); cmapTpsw->setTightBoundary(true);

        pTpsw->xAxis->setLabel("频率/Hz"); pTpsw->yAxis->setLabel("物理时间/s");
        pTpsw->xAxis->setRange(res.displayFreqMin, res.displayFreqMax); // 【同步聚焦显示】
        pTpsw->yAxis->setRange(res.minTime, res.maxTime);

        // --- 2: DP 线谱轨迹图 ---
        QCustomPlot* pDp = new QCustomPlot(m_lofarWaterfallWidget);
        pDp->setMinimumSize(400, 250); m_lofarWaterfallLayout->addWidget(pDp, 2, col);
        pDp->plotLayout()->insertRow(0); pDp->plotLayout()->addElement(0, 0, new QCPTextElement(pDp, "专属线谱连续轨迹图 (DP提取)", QFont("sans", 10, QFont::Bold)));
        QCPColorMap *cmapDp = new QCPColorMap(pDp->xAxis, pDp->yAxis);
        cmapDp->data()->setSize(res.freqBins, res.timeFrames); cmapDp->data()->setRange(QCPRange(0, 2500.0), QCPRange(res.minTime, res.maxTime));
        for(int t=0; t<res.timeFrames; ++t) for(int f=0; f<res.freqBins; ++f) cmapDp->data()->setCell(f, t, res.dpCounter[t * res.freqBins + f]);
        cmapDp->setGradient(QCPColorGradient::gpJet); cmapDp->setInterpolate(false);
        cmapDp->setDataRange(QCPRange(0, 10)); cmapDp->setTightBoundary(true);

        pDp->xAxis->setLabel("频率/Hz"); pDp->yAxis->setLabel("物理时间/s");
        pDp->xAxis->setRange(res.displayFreqMin, res.displayFreqMax); // 【同步聚焦显示】
        pDp->yAxis->setRange(res.minTime, res.maxTime);

        col++;
    }
}

// =========================================================
// Tab 2: 纯净的宏大 DCV 瀑布图 (修复角度畸变问题！)
// =========================================================
void MainWindow::onProcessingFinished() {
    m_lblSysInfo->setText(QString("状态: 分析完成\n结束时间: %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    m_btnStart->setEnabled(true); m_btnSelectFiles->setEnabled(true);
    m_btnPauseResume->setEnabled(false); m_btnStop->setEnabled(false);

    if (m_historyResults.isEmpty()) return;
    int num_frames = m_historyResults.size();
    double min_time = m_historyResults.first().timestamp;
    double max_time = m_historyResults.last().timestamp;
    if (std::abs(max_time - min_time) < 0.1) max_time = min_time + 3.0;

    m_dcvWaterfallPlot->clearPlottables();
    int nx_uniform = 361; // 物理角度以0.5度为步长重采样，共361个点 (0~180)
    QCPColorMap *colorMap = new QCPColorMap(m_dcvWaterfallPlot->xAxis, m_dcvWaterfallPlot->yAxis);
    colorMap->data()->setSize(nx_uniform, num_frames);
    colorMap->data()->setRange(QCPRange(0, 180), QCPRange(min_time, max_time));

    double dcv_max = -9999.0;
    for (int t = 0; t < num_frames; ++t) {
        const auto& frame = m_historyResults[t];
        const QVector<double>& theta_arr = frame.thetaAxis;
        const QVector<double>& dcv_arr = frame.dcvData;

        for (int x = 0; x < nx_uniform; ++x) {
            double target_theta = x * 0.5; // 每格代表 0.5度物理角度
            double val = -120.0;

            // 线性插值：将非均匀的 cos(theta) 数据投影回真实的 0-180 度物理平面
            if (theta_arr.size() > 1) {
                if (target_theta <= theta_arr.first()) val = dcv_arr.first();
                else if (target_theta >= theta_arr.last()) val = dcv_arr.last();
                else {
                    auto it = std::lower_bound(theta_arr.begin(), theta_arr.end(), target_theta);
                    int idx = std::distance(theta_arr.begin(), it);
                    if (idx > 0 && idx < theta_arr.size()) {
                        double t1 = theta_arr[idx - 1], t2 = theta_arr[idx];
                        double v1 = dcv_arr[idx - 1], v2 = dcv_arr[idx];
                        if (t2 - t1 > 1e-6) val = v1 + (v2 - v1) * (target_theta - t1) / (t2 - t1);
                        else val = v1;
                    }
                }
            }
            colorMap->data()->setCell(x, t, val);
            if (val > dcv_max) dcv_max = val;
        }
    }

    colorMap->setGradient(QCPColorGradient::gpJet);
    colorMap->setInterpolate(true);
    colorMap->setDataRange(QCPRange(dcv_max - 35.0, dcv_max));
    colorMap->setTightBoundary(true);

    m_dcvWaterfallPlot->xAxis->setLabel("方位角/°");
    m_dcvWaterfallPlot->yAxis->setLabel("物理时间/s");
    m_dcvWaterfallPlot->xAxis->setRange(0, 180);
    m_dcvWaterfallPlot->yAxis->setRange(min_time, max_time);
    m_dcvWaterfallPlot->replot();
}
