#pragma once
#include <QVector>
#include <QList>
#include <QString>
#include <QMetaType>
#include <vector>

// 定义单个目标的实时航迹状态
struct TargetTrack {
    int id;
    bool isActive;
    int missedCount;
    double currentAngle;
    int currentLoc;

    QVector<double> lofarSpectrum;
    QVector<double> demonSpectrum;
    QVector<double> lineSpectrumAmp;

    // 用于保存完整的线性功率谱 (0~fs/2)，供离线DP算法使用
    QVector<double> lofarFullLinear;

    std::vector<double> lineSpectra;
    double shaftFreq;
};
Q_DECLARE_METATYPE(TargetTrack)

// 单帧实时处理结果
struct FrameResult {
    int frameIndex;
    double timestamp;
    QVector<double> thetaAxis;
    QVector<double> cbfData;
    QVector<double> dcvData;
    QVector<double> detectedAngles;
    QString logMessage;
    QList<TargetTrack> tracks;
};
Q_DECLARE_METATYPE(FrameResult)

// 单目标离线 DP 算法完整特征包
struct OfflineTargetResult {
    int targetId;
    double startAngle;
    int timeFrames;
    int freqBins;
    double minTime;
    double maxTime;

    // 【新增】：根据历史线谱动态算出的最佳显示频段
    double displayFreqMin;
    double displayFreqMax;

    QVector<double> rawLofarDb;
    QVector<double> tpswLofarDb;
    QVector<double> dpCounter;
};
Q_DECLARE_METATYPE(QList<OfflineTargetResult>)
