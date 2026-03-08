#pragma once
#include <QThread>
#include <QString>     // <--- 注意这里
#include "DataTypes.h"

class DspWorker : public QThread {
    Q_OBJECT
public:
    explicit DspWorker(QObject *parent = nullptr);
    ~DspWorker();

    void setDirectory(const QString& dirPath);
    void stop();

signals:
    // 每处理完一帧，发射此信号通知 UI 绘图
    void frameProcessed(const FrameResult& result);
    // 专门用于发送系统日志的信号
    void logReady(const QString& log);
    // 批量处理完成信号
    void processingFinished();

protected:
    void run() override;

private:
    QString m_directory; // <--- 【核心修复】：这里必须是 QString，不能是 QStringList
    bool m_isRunning;
};
