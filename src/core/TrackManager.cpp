#include "TrackManager.h"
#include <cmath>
#include <limits>
#include <algorithm>

TrackManager::TrackManager() : num_total_targets(0) {}

QList<TargetTrack> TrackManager::updateTracks(const std::vector<double>& detected_angles,
                                              const std::vector<int>& detected_locs)
{
    int num_detected = detected_angles.size();
    std::vector<bool> detected_unassigned(num_detected, true);
    std::vector<bool> track_updated(m_tracks.size(), false);

    // 1. 尝试将新检测到的点与现有的轨迹进行关联 (最近邻匹配)
    for (int t = 0; t < m_tracks.size(); ++t) {
        double last_ang = m_tracks[t].currentAngle;

        double min_dist = std::numeric_limits<double>::max();
        int min_idx = -1;

        // 寻找波门内距离最近的检测点
        for (int d = 0; d < num_detected; ++d) {
            double dist = std::abs(detected_angles[d] - last_ang);
            if (dist < min_dist) {
                min_dist = dist;
                min_idx = d;
            }
        }

        // 如果在波门范围内，且该点尚未被分配
        if (min_idx != -1 && min_dist <= ASSOCIATION_GATE && detected_unassigned[min_idx]) {
            m_tracks[t].currentAngle = detected_angles[min_idx];
            m_tracks[t].currentLoc = detected_locs[min_idx];
            m_tracks[t].isActive = true;
            m_tracks[t].missedCount = 0;

            detected_unassigned[min_idx] = false;
            track_updated[t] = true;
        }
    }

    // 2. 对于没有更新的轨迹，标记为丢失/熄火，保持最后方位不变
    for (int t = 0; t < m_tracks.size(); ++t) {
        if (!track_updated[t]) {
            m_tracks[t].missedCount++;
            m_tracks[t].isActive = false;
            // 保持 currentAngle 和 currentLoc 不变 (对应 MATLAB 保留最后方位逻辑)
        }
    }

    // 3. 为未分配的检测点创建新的目标航迹 (新目标出生)
    for (int d = 0; d < num_detected; ++d) {
        if (detected_unassigned[d]) {
            num_total_targets++;

            TargetTrack new_track;
            new_track.id = num_total_targets;
            new_track.isActive = true;
            new_track.missedCount = 0;
            new_track.currentAngle = detected_angles[d];
            new_track.currentLoc = detected_locs[d];

            m_tracks.append(new_track);
        }
    }

    return m_tracks;
}
