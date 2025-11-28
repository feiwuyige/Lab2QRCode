#include "CameraWidget.h"
#include <ZXing/ReadBarcode.h>
#include <QMessageBox>
#include <QDateTime>
#include <QCloseEvent>
#include <QGroupBox>
#include <spdlog/spdlog.h>

CameraWidget::CameraWidget(QWidget* parent)
    : QWidget(parent)
    , capture(nullptr)
    , timer(nullptr)
    , videoLabel(nullptr)
    , statusLabel(nullptr)
    , mainLayout(nullptr)
    , cameraStarted(false)
{
    // 设置UI
    setWindowTitle("摄像头预览");
    setMinimumSize(800, 600);

    mainLayout = new QVBoxLayout(this);

    // 视频显示区域
    videoLabel = new QLabel(this);
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setStyleSheet("QLabel { background-color: black; }");
    videoLabel->setMinimumSize(640, 480);
    mainLayout->addWidget(videoLabel);

    // 状态标签
    statusLabel = new QLabel("准备启动摄像头...", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("QLabel { color: blue; padding: 5px; }");
    mainLayout->addWidget(statusLabel);

    // 扫码结果显示区域 TODO 暂时性
    QGroupBox* resultGroup = new QGroupBox("扫码结果", this);
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);

    resultDisplay = new QTextEdit(this);
    resultDisplay->setReadOnly(true);  // 设置为只读
    resultDisplay->setTextInteractionFlags(Qt::TextSelectableByMouse |
        Qt::TextSelectableByKeyboard);
    resultDisplay->setMaximumHeight(120);  // 限制高度
    resultDisplay->setStyleSheet(
        "QTextEdit {"
        "    background-color: #f0f0f0;"
        "    border: 1px solid #ccc;"
        "    border-radius: 5px;"
        "    padding: 5px;"
        "    font-family: 'Consolas', 'Monaco', monospace;"
        "}"
    );
    resultDisplay->setPlaceholderText("扫码结果将显示在这里...");

    resultLayout->addWidget(resultDisplay);
    mainLayout->addWidget(resultGroup);
}

CameraWidget::~CameraWidget()
{
    stopCamera();
}

void CameraWidget::startCamera()
{
    if (cameraStarted) return;

    // 初始化摄像头
    capture = new cv::VideoCapture(0);

    if (!capture->isOpened()) {
        statusLabel->setText("无法打开摄像头");
        QMessageBox::warning(this, "错误", "无法打开摄像头");
        delete capture;
        capture = nullptr;
        return;
    }

    // 设置摄像头参数
    capture->set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture->set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture->set(cv::CAP_PROP_FPS, 30);

    // 创建定时器用于更新帧 TODO 后续修改为线程方式
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CameraWidget::updateFrame);
    timer->start(33);  // 约30fps

    cameraStarted = true;
    statusLabel->setText("摄像头已启动");
}

void CameraWidget::stopCamera()
{
    if (timer && timer->isActive()) {
        timer->stop();
    }

    if (capture && capture->isOpened()) {
        capture->release();
    }

    delete timer;
    delete capture;

    timer = nullptr;
    capture = nullptr;
    cameraStarted = false;

    statusLabel->setText("摄像头已停止");
    videoLabel->clear();
}

void CameraWidget::closeEvent(QCloseEvent* event)
{
    stopCamera();
    event->accept();
}

void CameraWidget::updateFrame()
{
    if (!capture || !capture->isOpened()) return;

    cv::Mat frame;
    *capture >> frame;

    if (frame.empty()) {
        statusLabel->setText("捕获到空帧");
        return;
    }

    // 处理帧（绘制文字等）
    processFrame(frame);

    // 转换为Qt图像格式
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

    QImage qtImage(rgbFrame.data,
        rgbFrame.cols,
        rgbFrame.rows,
        rgbFrame.step,
        QImage::Format_RGB888);

    // 显示图像
    QPixmap pixmap = QPixmap::fromImage(qtImage);
    videoLabel->setPixmap(pixmap.scaled(videoLabel->width(),
        videoLabel->height(),
        Qt::KeepAspectRatio));
}

inline ZXing::ImageView ImageViewFromMat(const cv::Mat& image)
{
    using ZXing::ImageFormat;

    auto fmt = ImageFormat::None;
    switch (image.channels()) {
        case 1: fmt = ImageFormat::Lum; break;
        case 2: fmt = ImageFormat::LumA; break;
        case 3: fmt = ImageFormat::BGR; break;
        case 4: fmt = ImageFormat::BGRA; break;
        default:
            spdlog::error("不支持的图像通道数: {}", image.channels());
            break;
    }

    if (image.depth() != CV_8U || fmt == ImageFormat::None)
        return { nullptr, 0, 0, ImageFormat::None };

    return { image.data, image.cols, image.rows, fmt };
}

inline ZXing::Barcodes ReadBarcodes(const cv::Mat& image, const ZXing::ReaderOptions& options = {})
{
    return ZXing::ReadBarcodes(ImageViewFromMat(image), options);
}

inline void DrawBarcode(cv::Mat& img, ZXing::Barcode barcode)
{
    // 获取条码的四个角点位置
    const auto pos = barcode.position();

    // 将 ZXing 坐标点转换为 OpenCV 坐标点
    const auto zx2cv = [](ZXing::PointI p) { return cv::Point(p.x, p.y); };

    // 创建包含四个角点的多边形轮廓
    const auto contour = std::vector<cv::Point>{
        zx2cv(pos[0]),  // 左上角
        zx2cv(pos[1]),  // 右上角  
        zx2cv(pos[2]),  // 右下角
        zx2cv(pos[3])   // 左下角
    };
    const auto* pts = contour.data();
    const int npts = static_cast<int>(contour.size());
    // 根据四个顶点绘制条码绿色边框
    cv::polylines(img, &pts, &npts, 1, true, CV_RGB(0, 255, 0));
    // 在条码下方显示解码文本
    cv::putText(img, barcode.text(), zx2cv(pos[3]) + cv::Point(0, 20), cv::FONT_HERSHEY_DUPLEX, 0.5, CV_RGB(0, 255, 0));
}


void CameraWidget::processFrame(cv::Mat& frame) const
{
    const auto barcodes = ReadBarcodes(frame);

    // 清空之前的状态
    bool hasBarcode = false;

    for (const auto& barcode : barcodes) {
        if (barcode.isValid()) {
            DrawBarcode(frame, barcode);

            // 获取条码类型和内容
            std::string type = ZXing::ToString(barcode.format());
            std::string content = barcode.text();

            // 显示扫码结果
            displayScanResult(type, content);
            hasBarcode = true;

            // 更新状态栏
            statusLabel->setText(QString("检测到 %1 码").arg(QString::fromStdString(type)));
            statusLabel->setStyleSheet("QLabel { color: green; padding: 5px; font-weight: bold; }");
        }
    }

    // 如果没有检测到条码，恢复状态
    if (!hasBarcode) {
        statusLabel->setText("摄像头运行中...");
        statusLabel->setStyleSheet("QLabel { color: blue; padding: 5px; font-weight: bold; }");
    }
}

void CameraWidget::displayScanResult(const std::string& type, const std::string& content) const
{
    QString resultText = QString("条码类型: %1\n内容: %2\n时间: %3\n%4")
        .arg(QString::fromStdString(type))
        .arg(QString::fromStdString(content))
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
        .arg("------------------------");

    // 在现有内容前追加新结果
    QString currentText = resultDisplay->toPlainText();
    if (!currentText.isEmpty()) {
        resultText = resultText + "\n" + currentText;
    }

    resultDisplay->setPlainText(resultText);
}