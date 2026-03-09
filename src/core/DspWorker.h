#pragma once
#include <QThread>
#include <QString>
#include <atomic>
#include "DataTypes.h"
#include "detect_line_spectrum_from_lofar_change.h"

class DspWorker : public QThread {
    Q_OBJECT
public:
    explicit DspWorker(QObject *parent = nullptr);
    ~DspWorker();

    void setDirectory(const QString& dirPath);
    void stop();
    void pause();
    void resume();
    bool isPaused() const { return m_isPaused; }

signals:
    void frameProcessed(const FrameResult& result);
    void logReady(const QString& log);
    void reportReady(const QString& report);

    // 【新增】：专门用于传递离线 DP 矩阵结果的信号
    void offlineResultsReady(const QList<OfflineTargetResult>& results);

    void processingFinished();

protected:
    void run() override;

private:
    QString m_directory;
    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_isPaused;
};
