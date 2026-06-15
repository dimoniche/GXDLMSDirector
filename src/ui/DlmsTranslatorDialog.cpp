#include "DlmsTranslatorDialog.h"
#include "ui_DlmsTranslatorDialog.h"

#include <GXBytebuffer.h>
#include <GXDLMSConverter.h>
#include <GXDLMSTranslator.h>
#include <GXHelpers.h>
#include <enums.h>
#include <errorcodes.h>

#include <QMessageBox>
#include <QRegularExpression>

namespace {

QString stripHex(const QString &input)
{
    QString hex = input;
    return hex.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
}

} // namespace

DlmsTranslatorDialog::DlmsTranslatorDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DlmsTranslatorDialog)
{
    ui->setupUi(this);

    ui->interfaceCombo->addItem(tr("HDLC (default)"), DLMS_INTERFACE_TYPE_HDLC);
    ui->interfaceCombo->addItem(QStringLiteral("Wrapper"), DLMS_INTERFACE_TYPE_WRAPPER);

    connect(ui->translateButton, &QPushButton::clicked, this, &DlmsTranslatorDialog::onTranslate);
    connect(ui->clearButton, &QPushButton::clicked, this, &DlmsTranslatorDialog::onClear);
}

DlmsTranslatorDialog::~DlmsTranslatorDialog()
{
    delete ui;
}

void DlmsTranslatorDialog::onTranslate()
{
    const QString hex = stripHex(ui->inputEdit->toPlainText());
    if (hex.isEmpty() || hex.size() % 2 != 0) {
        QMessageBox::warning(this, tr("DLMS Translator"),
                             tr("Enter an even number of hex digits."));
        return;
    }

    std::string hexStr = hex.toStdString();
    CGXByteBuffer buffer;
    GXHelpers::HexToBytes(hexStr, buffer);

    const auto outputType = ui->outputCombo->currentIndex() == 0
                                ? DLMS_TRANSLATOR_OUTPUT_TYPE_SIMPLE_XML
                                : DLMS_TRANSLATOR_OUTPUT_TYPE_STANDARD_XML;
    CGXDLMSTranslator translator(outputType);

    std::string xml;
    int ret = translator.PduToXml(buffer, xml);
    if (ret != DLMS_ERROR_CODE_OK)
        ret = translator.DataToXml(buffer, xml);

    if (ret != DLMS_ERROR_CODE_OK) {
        QMessageBox::warning(this, tr("DLMS Translator"),
                             tr("Translation failed: %1")
                                 .arg(QString::fromUtf8(CGXDLMSConverter::GetErrorMessage(ret))));
        return;
    }

    ui->outputEdit->setPlainText(QString::fromStdString(xml));
}

void DlmsTranslatorDialog::onClear()
{
    ui->inputEdit->clear();
    ui->outputEdit->clear();
}
