#pragma once
#include <QList>
#include <vector>
#include "DataTypes.h"

class TrackManager {
public:
    TrackManager();

    // 传入当前帧检测到的方位角及其索引，更新并返回所有全局航迹
    QList<TargetTrack> updateTracks(const std::vector<double>& detected_angles,
                                    const std::vector<int>& detected_locs);

    int getTotalTargetCount() const { return num_total_targets; }

private:
    double ASSOCIATION_GATE = 6.0; // 关联波门阈值 6.0 度
    int num_total_targets;

    QList<TargetTrack> m_tracks;
};
