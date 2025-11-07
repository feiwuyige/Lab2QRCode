#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QPixmap>
#include <QMessageBox>
#include <QFont>
#include <opencv2/opencv.hpp>
#include <ZXing/BarcodeFormat.h>
#include <ZXing/BitMatrix.h>
#include <ZXing/MultiFormatWriter.h>
#include <ZXing/TextUtfEncoding.h>
#include <SimpleBase64.h>
#include "BarcodeWidget.h"

QImage BarcodeWidget::MatToQImage(const cv::Mat& mat)
{
    if (mat.type() == CV_8UC1)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
    }
    return QImage();
}

BarcodeWidget::BarcodeWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Binary to QRCode Generator");
    setMinimumSize(500, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QFont font;
    font.setPointSize(11);
    setFont(font);

    // 文件路径 + 按钮
    auto* fileLayout = new QHBoxLayout();
    filePathEdit = new QLineEdit(this);
    filePathEdit->setPlaceholderText("选择一个二进制或文本文件...");
    filePathEdit->setFont(QFont("Arial", 15));  // 设置加粗字体

    QPushButton* browseButton = new QPushButton("浏览", this);
    browseButton->setFixedWidth(100);
    browseButton->setFont(QFont("Arial", 15));  // 设置加粗字体
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(browseButton);
    mainLayout->addLayout(fileLayout);

    // 生成与保存按钮
    auto* buttonLayout = new QHBoxLayout();
    generateButton = new QPushButton("生成 QRCode", this);
    saveButton = new QPushButton("保存图片", this);
    generateButton->setFixedHeight(40);
    saveButton->setFixedHeight(40);
    generateButton->setFont(QFont("Arial", 15));
    saveButton->setFont(QFont("Arial", 15));
    saveButton->setEnabled(false);
    buttonLayout->addWidget(generateButton);
    buttonLayout->addWidget(saveButton);
    mainLayout->addLayout(buttonLayout);

    // 图片展示
    barcodeLabel = new QLabel(this);
    barcodeLabel->setAlignment(Qt::AlignCenter);
    barcodeLabel->setMinimumHeight(320);
    barcodeLabel->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid #ccc; }");
    mainLayout->addWidget(barcodeLabel);

    // 信号槽
    connect(browseButton, &QPushButton::clicked, this, &BarcodeWidget::onBrowseFile);
    connect(generateButton, &QPushButton::clicked, this, &BarcodeWidget::onGenerateClicked);
    connect(saveButton, &QPushButton::clicked, this, &BarcodeWidget::onSaveClicked);
}

void BarcodeWidget::onBrowseFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Select File", "", "RFA Files (*.rfa);;All Files (*)");
    if (!fileName.isEmpty())
        filePathEdit->setText(fileName);
}

void BarcodeWidget::onGenerateClicked()
{
    QString filePath = filePathEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择一个文件.");
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "错误", "不能打开文件.");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    try {
        std::string text = SimpleBase64::encode(reinterpret_cast<const std::uint8_t*>(data.constData()), data.size());

        ZXing::MultiFormatWriter writer(ZXing::BarcodeFormat::QRCode);
        writer.setMargin(1);

        auto bitMatrix = writer.encode(text, 300, 300);
        int width = bitMatrix.width();
        int height = bitMatrix.height();
        cv::Mat img(height, width, CV_8UC1);

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                img.at<uint8_t>(y, x) = bitMatrix.get(x, y) ? 0 : 255;

        lastImage = MatToQImage(img);
        barcodeLabel->setPixmap(QPixmap::fromImage(lastImage).scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        saveButton->setEnabled(true);
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to generate QRCode:\n%1").arg(e.what()));
    }
}

void BarcodeWidget::onSaveClicked()
{
    if (lastImage.isNull()) return;
    const QString saveFile = filePathEdit->text();
    const QFileInfo fileInfo(saveFile);
    const QString fileNameWithoutExtension = fileInfo.baseName();

    const QString fileName = QFileDialog::getSaveFileName(this, "Save QRCode", fileNameWithoutExtension + ".png", "PNG Images (*.png)");
    if (!fileName.isEmpty()) {
        if (lastImage.save(fileName))
            QMessageBox::information(this, "保存", QString("图片保存成功 %1").arg(fileName));
        else
            QMessageBox::warning(this, "错误", "无法保存图片.");
    }
}
