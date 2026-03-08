#pragma once
#include <QVector>
#include <QList>
#include <QString>
#include <QMetaType>

// 定义单个目标的航迹状态 (对应 MATLAB 动态跟踪池)
struct TargetTrack {
    int id;               // 目标编号 (从 1 开始)
    bool isActive;        // 是否存活 (跟踪中为 true，丢失熄火为 false)
    int missedCount;      // 连续丢失帧数
    double currentAngle;  // 当前帧的物理方位角
    int currentLoc;       // 当前帧的方位角索引 (用于后续提取提取信号)

    QVector<double> lofarSpectrum;  // 当前帧的 LOFAR 功率谱 (dB)
    QVector<double> demonSpectrum;  // 当前帧的 DEMON 包络谱 (归一化幅度)

    std::vector<double> lineSpectra;// 提取到的高频线谱 (Hz)
    double shaftFreq;               // 提取到的低频轴频 (Hz)，如果没有则为 0.0
};

// 注册元类型
Q_DECLARE_METATYPE(TargetTrack)

// 单帧处理结果的结构体
struct FrameResult {
    int frameIndex;
        double timestamp;
        QVector<double> thetaAxis;
        QVector<double> cbfData;
        QVector<double> dcvData;
        QString logMessage;
        QList<TargetTrack> tracks;// 这一帧的所有被跟踪的目标



};

Q_DECLARE_METATYPE(FrameResult)
