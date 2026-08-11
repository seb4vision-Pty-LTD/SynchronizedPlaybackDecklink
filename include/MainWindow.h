#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include <QDir>
#include <gst/gst.h>

class MainWindow : public QMainWindow {
    Q_OBJECT

    static constexpr int kChannelCount = 8; // Number of video channels

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSelectRootDirectory();
    void onPlaylistChanged(const QString &dirName);
    void onStartClicked();
    void onPlayPauseClicked();
    void onSeekSliderMoved(int position);
    void updateInterface();
    void processBusMessages();

private:
    void setupUI();
    void setupGStreamer();
    void loadPlaylists(const QString &rootDir);
    void setWindowHandles();

    // UI Elements
    QLineEdit *rootDirEdit;
    QPushButton *selectRootBtn;
    QComboBox *playlistCombo;
    QPushButton *startBtn;
    QPushButton *playPauseBtn;
    QSlider *seekSlider;
    QLabel *timeLabel;
    QWidget *videoWidgets[kChannelCount];
    QTimer *updateTimer;
    QTimer *busTimer;

    // GStreamer Elements
    GstElement *pipeline;
    GstBus *bus;
    GstElement *fileSrcs[kChannelCount];
    gint64 duration;
    bool isPlaying;
    QString currentRootDir;
};

#endif // MAINWINDOW_H