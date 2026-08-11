#include <QApplication>
#include "MainWindow.h"
#include <gst/gst.h>

int main(int argc, char *argv[]) {
    // Disable the d3d12 H.264 decoder path which frequently fails negotiation on interlaced 1080i streams.
    g_setenv("GST_PLUGIN_FEATURE_RANK", "d3d12h264dec:0", TRUE);

    // Initialize GStreamer before Qt
    gst_init(&argc, &argv);
    
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}