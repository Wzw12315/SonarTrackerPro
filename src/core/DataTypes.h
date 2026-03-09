#pragma once
#include <QVector>
#include <QList>
#include <QString>
#include <QMetaType>
#include <vector>

// 全局信号处理配置参数
struct DspConfig {
    // 阵列与声学环境
    double fs = 5000.0;
    int M = 512;
    double d = 1.2;
    double c = 1500.0;
    double r_scan = 9000.0;
    double timeStep = 3.0;

    // 频带与谱估计
    double lofarMin = 80.0;
    double lofarMax = 250.0;
    double demonMin = 350.0;
    double demonMax = 2000.0;
    int nfftR = 15000;
    int nfftWin = 30000;

    // 【新增】：实时 LOFAR 线谱提取参数
    int lofarBgMedWindow = 150;      // 中值滤波平滑窗宽
    double lofarSnrThreshMult = 2.5; // SNR 阈值乘数
    int lofarPeakMinDist = 30;       // 寻峰最小间距 (点数)

    // DEMON 包络滤波
    int firOrder = 64;
    double firCutoff = 0.1;

    // TPSW 与 DP 寻优
    double tpswG = 45.0;
    double tpswE = 2.0;
    int dpL = 5;
    double dpAlpha = 1.5;
    double dpBeta = 1.0;
    double dpGamma = 0.1;
};
Q_DECLARE_METATYPE(DspConfig)

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

    double displayFreqMin;
    double displayFreqMax;

    QVector<double> rawLofarDb;
    QVector<double> tpswLofarDb;
    QVector<double> dpCounter;
};
Q_DECLARE_METATYPE(QList<OfflineTargetResult>)
