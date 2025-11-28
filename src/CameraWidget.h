#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QTextEdit>
#include <opencv2/opencv.hpp>

class CameraWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraWidget(QWidget* parent = nullptr);
    ~CameraWidget();

    void startCamera();
    void stopCamera();

protected:
    void closeEvent(QCloseEvent* event) override;  // 重写关闭事件

private:
    void updateFrame();

    void processFrame(cv::Mat& frame) const;
    void displayScanResult(const std::string& type, const std::string& content) const;

    cv::VideoCapture* capture;
    QTimer* timer;
    QLabel* videoLabel;
    QLabel* statusLabel;
    QVBoxLayout* mainLayout;
    QTextEdit* resultDisplay;
    bool cameraStarted;
};

#endif // CAMERAWIDGET_H