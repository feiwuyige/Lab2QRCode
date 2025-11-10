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
#include <ZXing/ReadBarcode.h>
#include <ZXing/DecodeHints.h>
#include <SimpleBase64.h>
#include "BarcodeWidget.h"

QImage BarcodeWidget::MatToQImage(const cv::Mat& mat)
{
    if (mat.type() == CV_8UC1)
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    return {};
}

BarcodeWidget::BarcodeWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Binary to QRCode Generator");
    setMinimumSize(500, 500);
    setFixedSize(500, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QFont font;
    font.setPointSize(11);
    setFont(font);

    // 文件路径 + 按钮
    auto* fileLayout = new QHBoxLayout();
    filePathEdit = new QLineEdit(this);
    filePathEdit->setPlaceholderText("选择一个文件或图片");
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
    decodeToChemFile = new QPushButton("解码");
    saveButton = new QPushButton("保存", this);
    generateButton->setFixedHeight(40);
    saveButton->setFixedHeight(40);
    decodeToChemFile->setFixedHeight(40);
    generateButton->setFont(QFont("Arial", 15));
    decodeToChemFile->setFont(QFont("Arial", 15));
    saveButton->setFont(QFont("Arial", 15));

    generateButton->setEnabled(false);
    decodeToChemFile->setEnabled(false);
    saveButton->setEnabled(false);

    buttonLayout->addWidget(generateButton);
    buttonLayout->addWidget(decodeToChemFile);
    buttonLayout->addWidget(saveButton);
    mainLayout->addLayout(buttonLayout);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(320);
    scrollArea->setStyleSheet("QScrollArea { background-color: #f0f0f0; border: 1px solid #ccc; }");

    // 图片以及解码文本展示
    barcodeLabel = new QLabel();
    barcodeLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);  // 文本左上对齐
    barcodeLabel->setWordWrap(true);  // 允许文本换行
    barcodeLabel->setStyleSheet("QLabel { background-color: #fafafa; font-family: Consolas; font-size: 12pt; padding: 5px; }");

    // 设置QLabel的尺寸策略，使其可以扩展
    barcodeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 将QLabel设置为滚动区域的内容
    scrollArea->setWidget(barcodeLabel);

    mainLayout->addWidget(scrollArea);

    connect(browseButton, &QPushButton::clicked, this, &BarcodeWidget::onBrowseFile);
    connect(generateButton, &QPushButton::clicked, this, &BarcodeWidget::onGenerateClicked);
    connect(decodeToChemFile, &QPushButton::clicked, this, &BarcodeWidget::onDecodeToChemFileClicked);
    connect(saveButton, &QPushButton::clicked, this, &BarcodeWidget::onSaveClicked);
    connect(filePathEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        updateButtonStates(text);
    });
}

void BarcodeWidget::updateButtonStates(const QString& filePath)
{
    if (filePath.isEmpty()) {
        // 文件路径为空，禁用所有功能按钮
        generateButton->setEnabled(false);
        decodeToChemFile->setEnabled(false);
        saveButton->setEnabled(false);
        return;
    }

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    QStringList imageFormats = { "png", "jpg", "jpeg", "bmp", "gif", "tiff", "webp" };

    if (imageFormats.contains(suffix)) {
        generateButton->setEnabled(false);
        decodeToChemFile->setEnabled(true);
        saveButton->setEnabled(false);  // 解码后才启用保存

        // 设置工具提示
        generateButton->setToolTip("请选择任意文件来生成QR码");
        decodeToChemFile->setToolTip("可以解码PNG图片中的QR码");

    }
    else {
        generateButton->setEnabled(true);
        decodeToChemFile->setEnabled(false);
        saveButton->setEnabled(false);  // 生成后才启用保存

        generateButton->setToolTip("可以从任意文件生成QR码");
        decodeToChemFile->setToolTip("请选择PNG图片来解码QR码");

    }
}

void BarcodeWidget::onBrowseFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Select File", "", "Supported Files (*.rfa *.png);;All Files (*)");
    if (!fileName.isEmpty()) {
        filePathEdit->setText(fileName);
        barcodeLabel->clear();

        updateButtonStates(fileName);
    }
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
        barcodeLabel->clear();
        barcodeLabel->setAlignment(Qt::AlignCenter); // 重新设置居中
        barcodeLabel->setPixmap(QPixmap::fromImage(lastImage).scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        saveButton->setEnabled(true);
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to generate QRCode:\n%1").arg(e.what()));
    }
}

void BarcodeWidget::onDecodeToChemFileClicked()
{
    QString filePath = filePathEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择一个PNG图片文件.");
        return;
    }

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    QStringList imageFormats = { "png" };

    if (!imageFormats.contains(suffix)) {
        QMessageBox::warning(this, "警告", "选择的文件不是PNG图片格式。\n请选择300x300像素的PNG格式图片");
        return;
    }

    try {
        cv::Mat img = cv::imread(filePath.toStdString(), cv::IMREAD_COLOR);
        if (img.empty()) {
            QMessageBox::critical(this, "错误", "无法加载图片文件。");
            return;
        }

        // 转为灰度
        cv::Mat grayImg;
        cv::cvtColor(img, grayImg, cv::COLOR_BGR2GRAY);

        ZXing::ImageView imageView(grayImg.data, grayImg.cols, grayImg.rows, ZXing::ImageFormat::Lum);
        auto result = ZXing::ReadBarcode(imageView);

        if (!result.isValid()) {
            QMessageBox::warning(this, "警告", "无法识别QR码或QR码格式不正确。");
            return;
        }

        // Base64 解码
        std::string encodedText = result.text();
        auto decodedData = SimpleBase64::decode(encodedText);

        // ✅ 显示部分内容到界面
        QString preview;
        if (decodedData.size() > 1024)
            preview = QString::fromLatin1(reinterpret_cast<const char*>(decodedData.data()), 1024) + "\n... (内容已截断)";
        else
            preview = QString::fromLatin1(reinterpret_cast<const char*>(decodedData.data()), static_cast<int>(decodedData.size()));

        barcodeLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        barcodeLabel->setWordWrap(false);
        barcodeLabel->setText(preview);
        saveButton->setEnabled(true);

        // 保存解码后的数据到数据成员，供保存按钮使用
        lastDecodedData = QByteArray(reinterpret_cast<const char*>(decodedData.data()), static_cast<int>(decodedData.size()));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("解码失败:\n%1").arg(e.what()));
    }
}

void BarcodeWidget::onSaveClicked()
{
    QString filePath = filePathEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有可保存的内容。");
        return;
    }

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "png") {
        // 如果是PNG图片，保存解码后的文件
        if (lastDecodedData.isEmpty()) {
            QMessageBox::warning(this, "警告", "没有解码数据可保存。");
            return;
        }

        QString defaultName = fileInfo.completeBaseName() + ".rfa";
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件",
            fileInfo.dir().filePath(defaultName),
            "RFA Files (*.rfa)");

        if (!savePath.isEmpty()) {
            QFile outputFile(savePath);
            if (outputFile.open(QIODevice::WriteOnly)) {
                outputFile.write(lastDecodedData);
                outputFile.close();
                QMessageBox::information(this, "成功", QString("文件已保存至:\n%1").arg(savePath));
            }
            else {
                QMessageBox::critical(this, "错误", "无法保存文件。");
            }
        }
    }
    else {
        // 如果是其他文件（普通文件），保存图片
        if (lastImage.isNull()) {
            QMessageBox::warning(this, "警告", "没有图片可保存。");
            return;
        }

        QString fileNameWithoutExtension = fileInfo.baseName();
        QString fileName = QFileDialog::getSaveFileName(this, "保存图片",
            fileNameWithoutExtension + ".png", "PNG Images (*.png)");

        if (!fileName.isEmpty()) {
            if (lastImage.save(fileName))
                QMessageBox::information(this, "保存", QString("图片保存成功 %1").arg(fileName));
            else
                QMessageBox::warning(this, "错误", "无法保存图片.");
        }
    }
}