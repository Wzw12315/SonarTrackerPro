#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMap>
#include <QSet>          // 新增
#include <QGridLayout>
#include <QList>         // 新增
#include "qcustomplot.h"
#include "../core/DspWorker.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSelectFilesClicked();
    void onFrameProcessed(const FrameResult& result);
    void appendLog(const QString& log);
    void onProcessingFinished(); // 【新增】：处理完成时绘制瀑布图

private:
    void setupUi();
    void createTargetPlots(int targetId);

    QCustomPlot* m_spatialPlot;
    QPlainTextEdit* m_logConsole;
    QPushButton* m_btnSelectFiles;
    QCPTextElement* m_plotTitle;

    DspWorker* m_worker;

    QWidget* m_targetPanelWidget;
    QGridLayout* m_targetLayout;

    QMap<int, QCustomPlot*> m_lofarPlots;
    QMap<int, QCustomPlot*> m_demonPlots;

    // 【新增】：用于保存所有历史帧，在处理结束后生成离线瀑布图
    QList<FrameResult> m_historyResults;
};
