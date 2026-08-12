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
#include <QListWidget>
#include <gst/gst.h>

class MainWindow : public QMainWindow {
    Q_OBJECT

    static constexpr int kChannelCount = 8; // Number of video channels

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSelectRootDirectory();
    void onSessionSelected(QListWidgetItem *item);
    void onPlaylistSelected(QListWidgetItem *item);
    void onNextPlaylist();
    void onPreviousPlaylist();
    void onStartClicked();
    void onPlayPauseClicked();
    void onSeekSliderMoved(int position);
    void updateInterface();
    void processBusMessages();

private:
    void setupUI();
    void setupGStreamer();
    void loadSessions(const QString &rootDir);
    void loadPlaylistsForSession(const QString &sessionPath);
    void loadCurrentPlaylist();
    void setWindowHandles();
    void onDecodedPadAdded(GstElement *decodebin, GstPad *pad);
    static GstPadProbeReturn logCapsEvent(GstPad *pad, GstPadProbeInfo *info, gpointer userData);

    // UI Elements
    QLineEdit *rootDirEdit;
    QPushButton *selectRootBtn;
    QListWidget *sessionList;
    QListWidget *playlistList;
    QPushButton *nextPlaylistBtn;
    QPushButton *previousPlaylistBtn;
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
    QString currentSessionPath;
};

#endif // MAINWINDOW_H