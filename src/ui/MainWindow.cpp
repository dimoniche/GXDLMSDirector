#include "MainWindow.h"
#include "ByteArrayEditorDialog.h"
#include "ConformanceTestDialog.h"
#include "CosemObjectDialog.h"
#include "DevicePropertiesDialog.h"
#include "DlmsTranslatorDialog.h"
#include "HdlcAddressScannerDialog.h"
#include "MacroEditorDialog.h"
#include "PlcDiscoverDialog.h"
#include "ProfileGenericDialog.h"
#include "ui_MainWindow.h"

#include "core/CosemObjectHelper.h"
#include "core/DataConcentratorManager.h"
#include "core/ManufacturerSettings.h"
#include "core/ObjectAttributeNames.h"
#include "core/ProjectSerializer.h"
#include "core/VariantConverter.h"

#include <GXDLMSConverter.h>
#include <GXDLMSObject.h>
#include <enums.h>

#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QStandardItem>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_device(std::make_unique<GXDLMSDevice>(this))
{
    ui->setupUi(this);
    m_treeModel = new QStandardItemModel(this);
    ui->objectTree->setModel(m_treeModel);
    ui->propertyTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    ui->propertyTable->horizontalHeader()->setStretchLastSection(true);
    ui->methodsTable->horizontalHeader()->setStretchLastSection(true);
    ui->clockEditorGroup->setVisible(false);

    ManufacturerSettings::instance().load();
    m_device->setName(tr("Meter 1"));
    setupConnections();
    rebuildObjectTree();
    updateActions();
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::setupConnections()
{
    connect(ui->actionNewDevice, &QAction::triggered, this, &MainWindow::onNewDevice);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::onSaveProject);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::onSaveProjectAs);
    connect(ui->actionDeviceProperties, &QAction::triggered, this, &MainWindow::onDeviceProperties);
    connect(ui->actionEditManufacturers, &QAction::triggered, this, &MainWindow::onEditManufacturers);
    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::onConnect);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::onDisconnect);
    connect(ui->actionReadAll, &QAction::triggered, this, &MainWindow::onReadAll);
    connect(ui->actionReadObject, &QAction::triggered, this, &MainWindow::onReadObject);
    connect(ui->actionWriteObject, &QAction::triggered, this, &MainWindow::onWriteObject);
    connect(ui->actionInvokeMethod, &QAction::triggered, this, &MainWindow::onInvokeMethod);
    connect(ui->actionAddObject, &QAction::triggered, this, &MainWindow::onAddObject);
    connect(ui->actionDeleteObject, &QAction::triggered, this, &MainWindow::onDeleteObject);
    connect(ui->actionEditOctetString, &QAction::triggered, this, &MainWindow::onEditOctetString);
    connect(ui->invokeMethodButton, &QPushButton::clicked, this, &MainWindow::onInvokeMethod);
    connect(ui->clockWriteButton, &QPushButton::clicked, this, &MainWindow::onClockWrite);
    connect(ui->actionReadProfileGeneric, &QAction::triggered, this, &MainWindow::onReadProfileGeneric);
    connect(ui->actionDlmsTranslator, &QAction::triggered, this, &MainWindow::onDlmsTranslator);
    connect(ui->actionHdlcAddressScanner, &QAction::triggered, this, &MainWindow::onHdlcAddressScanner);
    connect(ui->actionMacroEditor, &QAction::triggered, this, &MainWindow::onMacroEditor);
    connect(ui->actionConformanceTests, &QAction::triggered, this, &MainWindow::onConformanceTests);
    connect(ui->actionPlcDiscover, &QAction::triggered, this, &MainWindow::onPlcDiscover);
    connect(ui->actionDataConcentrators, &QAction::triggered, this, &MainWindow::onDataConcentrators);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    connect(m_device.get(), &GXDLMSDevice::stateChanged, this, &MainWindow::onDeviceStateChanged);
    connect(m_device.get(), &GXDLMSDevice::traceMessage, this, &MainWindow::onTraceMessage);
    connect(m_device.get(), &GXDLMSDevice::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_device.get(), &GXDLMSDevice::objectRead, this, &MainWindow::onObjectRead);
    connect(m_device.get(), &GXDLMSDevice::objectWritten, this, &MainWindow::onObjectWritten);
    connect(m_device.get(), &GXDLMSDevice::methodInvoked, this, &MainWindow::onMethodInvoked);
    connect(m_device.get(), &GXDLMSDevice::errorOccurred, this, [this](const QString &msg) {
        appendTrace(tr("ERROR: %1").arg(msg));
        QMessageBox::warning(this, tr("Error"), msg);
    });
    connect(m_device.get(), &GXDLMSDevice::readAllFinished, this, [this]() {
        statusBar()->showMessage(tr("Read all completed."), 3000);
    });

    connect(ui->objectTree, &QTreeView::clicked, this, &MainWindow::onObjectTreeClicked);
    connect(ui->propertyTable, &QTableWidget::itemChanged, this, &MainWindow::onPropertyTableChanged);
    connect(ui->methodsTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateActions);
}

void MainWindow::onNewDevice()
{
    if (!maybeSave())
        return;

    if (m_device->state() & DeviceState::Connected)
        m_device->disconnectAsync();

    m_projectPath.clear();
    m_device->setName(tr("Meter 1"));
    m_device->setManufacturer(QStringLiteral("GRX"));
    m_device->setState(DeviceState::None);
    m_device->objects().Free();
    m_selectedObject = nullptr;
    setDirty(false);
    rebuildObjectTree();
    updateObjectEditors(nullptr);
    updateActions();
    appendTrace(tr("New device created."));
}

void MainWindow::onOpenProject()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                      m_projectPath.isEmpty() ? QDir::homePath() : m_projectPath,
                                                      tr("GXDLMSDirector projects (*.gxc)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!ProjectSerializer::load(path, m_device.get(), &error)) {
        QMessageBox::warning(this, tr("Open Project"), error);
        return;
    }

    m_projectPath = path;
    m_selectedObject = nullptr;
    setDirty(false);
    rebuildObjectTree();
    updateActions();
    appendTrace(tr("Project loaded: %1").arg(path));
}

void MainWindow::onSaveProject()
{
    if (m_projectPath.isEmpty()) {
        onSaveProjectAs();
        return;
    }

    QString error;
    if (!ProjectSerializer::save(m_projectPath, m_device.get(), &error)) {
        QMessageBox::warning(this, tr("Save Project"), error);
        return;
    }

    setDirty(false);
    statusBar()->showMessage(tr("Project saved."), 3000);
}

void MainWindow::onSaveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Project As"),
                                                      m_projectPath.isEmpty() ? QDir::homePath() : m_projectPath,
                                                      tr("GXDLMSDirector projects (*.gxc)"));
    if (path.isEmpty())
        return;

    m_projectPath = path.endsWith(QStringLiteral(".gxc"), Qt::CaseInsensitive) ? path : path + QStringLiteral(".gxc");
    onSaveProject();
}

void MainWindow::onDeviceProperties()
{
    DevicePropertiesDialog dlg(m_device.get(), this);
    if (dlg.exec() == QDialog::Accepted)
        setDirty(true);
}

void MainWindow::onEditManufacturers()
{
    ManufacturerSettings::instance().save();
    QDesktopServices::openUrl(QUrl::fromLocalFile(ManufacturerSettings::instance().filePath()));
    QMessageBox::information(this, tr("Manufacturers"),
                           tr("Manufacturer settings saved to:\n%1\n\nEdit the XML file and restart the application to reload.")
                               .arg(ManufacturerSettings::instance().filePath()));
}

void MainWindow::onConnect()
{
    recordMacroStep(MacroActionType::Connect);
    appendTrace(tr("Connecting to %1...").arg(m_device->name()));
    m_device->connectAsync();
}

void MainWindow::onDisconnect()
{
    recordMacroStep(MacroActionType::Disconnect);
    m_device->disconnectAsync();
}

void MainWindow::onReadAll()
{
    appendTrace(tr("Reading all objects..."));
    m_device->readAllAsync();
}

void MainWindow::onReadObject()
{
    const int attributeIndex = selectedAttributeIndex();
    if (!m_selectedObject || attributeIndex <= 0)
        return;
    recordMacroStep(MacroActionType::Get, m_selectedObject, attributeIndex);
    m_device->readObjectAsync(m_selectedObject, attributeIndex);
}

void MainWindow::onWriteObject()
{
    const int attributeIndex = selectedAttributeIndex();
    if (!m_selectedObject || attributeIndex <= 0)
        return;

    auto *valueItem = ui->propertyTable->item(attributeIndex - 1, 2);
    if (!valueItem)
        return;

    const DLMS_ACCESS_MODE access = m_selectedObject->GetAccess(attributeIndex);
    if (access != DLMS_ACCESS_MODE_WRITE && access != DLMS_ACCESS_MODE_READ_WRITE
        && access != DLMS_ACCESS_MODE_AUTHENTICATED_WRITE
        && access != DLMS_ACCESS_MODE_AUTHENTICATED_READ_WRITE) {
        QMessageBox::warning(this, tr("Write Object"), tr("Attribute %1 is not writable.").arg(attributeIndex));
        return;
    }

    appendTrace(tr("Writing attribute %1...").arg(attributeIndex));
    recordMacroStep(MacroActionType::Set, m_selectedObject, attributeIndex, valueItem->text());
    m_device->writeObjectAsync(m_selectedObject, attributeIndex, valueItem->text());
}

void MainWindow::onInvokeMethod()
{
    const int methodIndex = selectedMethodIndex();
    if (!m_selectedObject || methodIndex <= 0)
        return;

    appendTrace(tr("Invoking method %1...").arg(methodIndex));
    recordMacroStep(MacroActionType::Action, m_selectedObject, methodIndex, ui->methodParamEdit->text());
    m_device->invokeMethodAsync(m_selectedObject, methodIndex, ui->methodParamEdit->text());
}

void MainWindow::onAddObject()
{
    CosemObjectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString error;
    CGXDLMSObject *object = CosemObjectHelper::addObject(
        m_device->objects(), dlg.objectType(), dlg.logicalName(), dlg.version(), dlg.description(), &error);
    if (!object) {
        QMessageBox::warning(this, tr("Add Object"), error);
        return;
    }

    setDirty(true);
    rebuildObjectTree();
    appendTrace(tr("Added object: %1").arg(dlg.logicalName()));
}

void MainWindow::onDeleteObject()
{
    if (!m_selectedObject)
        return;

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Delete Object"), tr("Delete selected COSEM object?"),
                              QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    if (!CosemObjectHelper::removeObject(m_device->objects(), m_selectedObject)) {
        QMessageBox::warning(this, tr("Delete Object"), tr("Failed to delete object."));
        return;
    }

    m_selectedObject = nullptr;
    setDirty(true);
    rebuildObjectTree();
    updateObjectEditors(nullptr);
    appendTrace(tr("Object deleted."));
}

void MainWindow::onEditOctetString()
{
    const int attributeIndex = selectedAttributeIndex();
    if (!m_selectedObject || attributeIndex <= 0)
        return;

    auto *valueItem = ui->propertyTable->item(attributeIndex - 1, 2);
    if (!valueItem)
        return;

    ByteArrayEditorDialog dlg(valueItem->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    valueItem->setText(dlg.hexValue());
    setDirty(true);
}

void MainWindow::onClockWrite()
{
    if (!m_selectedObject || m_selectedObject->GetObjectType() != DLMS_OBJECT_TYPE_CLOCK)
        return;

    ui->propertyTable->setCurrentCell(1, 2);
    auto *valueItem = ui->propertyTable->item(1, 2);
    if (!valueItem)
        return;

    valueItem->setText(ui->clockDateTimeEdit->dateTime().toUTC().toString(Qt::ISODate));
    onWriteObject();
}

void MainWindow::onReadProfileGeneric()
{
    if (!m_selectedObject || !isProfileGenericSelected())
        return;

    ProfileGenericDialog dlg(m_device.get(), m_selectedObject, this);
    dlg.exec();
}

void MainWindow::onDlmsTranslator()
{
    DlmsTranslatorDialog dlg(this);
    dlg.exec();
}

void MainWindow::onHdlcAddressScanner()
{
    HdlcAddressScannerDialog dlg(this);
    dlg.exec();
}

void MainWindow::onMacroEditor()
{
    if (!m_macroEditor) {
        m_macroEditor = new MacroEditorDialog(m_device.get(), this);
        connect(m_macroEditor, &QObject::destroyed, this, [this]() { m_macroEditor = nullptr; });
    }
    m_macroEditor->show();
    m_macroEditor->raise();
    m_macroEditor->activateWindow();
}

void MainWindow::onConformanceTests()
{
    ConformanceTestDialog dlg(m_device.get(), this);
    dlg.exec();
}

void MainWindow::onPlcDiscover()
{
    PlcDiscoverDialog dlg(m_device.get(), this);
    if (dlg.exec() == QDialog::Accepted)
        setDirty(true);
}

void MainWindow::onDataConcentrators()
{
    if (!DataConcentratorManager::instance().hasPlugins()) {
        QMessageBox::information(this, tr("Data Concentrators"),
                                 tr("No data concentrator plugins are available.\n\n"
                                    "Data concentrator support requires vendor-specific plugins."));
        return;
    }
}

void MainWindow::recordMacroStep(MacroActionType type, CGXDLMSObject *object, int index,
                                 const QString &value, const QString &error)
{
    if (!m_macroEditor || !m_macroEditor->isRecording())
        return;

    MacroStep step;
    step.timestamp = QDateTime::currentDateTime();
    step.type = type;
    step.device = m_device->name();
    step.index = index;
    step.value = value;
    if (!error.isEmpty())
        step.expectedException = error;

    if (object) {
        step.objectType = static_cast<int>(object->GetObjectType());
        step.objectVersion = object->GetVersion();
        std::string ln;
        object->GetLogicalName(ln);
        step.logicalName = QString::fromStdString(ln);
        step.name = QString::fromUtf8(CGXDLMSConverter::ToString(object->GetObjectType()))
                    + QStringLiteral(" ") + step.logicalName + QStringLiteral(" (") + QString::number(index)
                    + QLatin1Char(')');
    } else if (type == MacroActionType::Connect) {
        step.name = tr("Connect");
    } else if (type == MacroActionType::Disconnect) {
        step.name = tr("Disconnect");
    }

    m_macroEditor->addRecordedStep(step);
}

void MainWindow::onDeviceStateChanged(DeviceStates state)
{
    updateActions();

    if (state & DeviceState::Connected) {
        statusBar()->showMessage(tr("Connected"));
        if (!m_device->negotiatedConformance().isEmpty()) {
            appendTrace(tr("Negotiated conformance: %1").arg(m_device->negotiatedConformance()));
        }
        rebuildObjectTree();
    } else if (state & DeviceState::Connecting) {
        statusBar()->showMessage(tr("Connecting..."));
    } else if (state & DeviceState::Disconnecting) {
        statusBar()->showMessage(tr("Disconnecting..."));
    } else if (state & DeviceState::Writing) {
        statusBar()->showMessage(tr("Writing..."));
    } else if (state & DeviceState::Reading) {
        statusBar()->showMessage(tr("Reading..."));
    } else {
        statusBar()->showMessage(tr("Disconnected"));
    }
}

void MainWindow::onTraceMessage(const QString &message)
{
    appendTrace(message);
}

void MainWindow::onProgressChanged(const QString &description, int current, int maximum)
{
    statusBar()->showMessage(QStringLiteral("%1 (%2/%3)").arg(description).arg(current).arg(maximum));
}

void MainWindow::onObjectRead(CGXDLMSObject *object, int attributeIndex, const QString &value)
{
    if (object == m_selectedObject)
        updateObjectEditors(object);

    std::string objName;
    object->GetLogicalName(objName);
    appendTrace(QStringLiteral("%1 attr %2 = %3")
                    .arg(QString::fromStdString(objName))
                    .arg(attributeIndex)
                    .arg(value));
}

void MainWindow::onObjectWritten(CGXDLMSObject *object, int attributeIndex, const QString &value)
{
    if (object == m_selectedObject)
        updateObjectEditors(object);

    std::string objName;
    object->GetLogicalName(objName);
    appendTrace(tr("Written %1 attr %2 = %3")
                    .arg(QString::fromStdString(objName))
                    .arg(attributeIndex)
                    .arg(value));
    statusBar()->showMessage(tr("Write completed."), 3000);
}

void MainWindow::onMethodInvoked(CGXDLMSObject *object, int methodIndex)
{
    std::string objName;
    object->GetLogicalName(objName);
    appendTrace(tr("Method %1 invoked on %2")
                    .arg(methodIndex)
                    .arg(QString::fromStdString(objName)));
    statusBar()->showMessage(tr("Method invoked."), 3000);
}

void MainWindow::onObjectTreeClicked(const QModelIndex &index)
{
    auto *item = m_treeModel->itemFromIndex(index);
    if (!item)
        return;

    auto *object = reinterpret_cast<CGXDLMSObject *>(item->data(Qt::UserRole).value<quintptr>());
    m_selectedObject = object;
    updateObjectEditors(object);
}

void MainWindow::onPropertyTableChanged(QTableWidgetItem *item)
{
    if (item && item->column() == 2)
        setDirty(true);
}

void MainWindow::updateActions()
{
    const bool connected = m_device->state() & DeviceState::Connected;
    const bool busy = m_device->state() & (DeviceState::Connecting | DeviceState::Disconnecting
                                           | DeviceState::Reading | DeviceState::Writing);
    const bool hasSelection = m_selectedObject != nullptr;

    ui->actionConnect->setEnabled(!connected && !busy);
    ui->actionDisconnect->setEnabled(connected && !busy);
    ui->actionReadAll->setEnabled(connected && !busy);
    ui->actionReadObject->setEnabled(connected && !busy && hasSelection);
    ui->actionWriteObject->setEnabled(connected && !busy && hasSelection);
    ui->actionInvokeMethod->setEnabled(connected && !busy && hasSelection && selectedMethodIndex() > 0);
    ui->actionReadProfileGeneric->setEnabled(connected && !busy && isProfileGenericSelected());
    ui->actionDeleteObject->setEnabled(hasSelection && !busy);
    ui->actionEditOctetString->setEnabled(hasSelection && isOctetStringSelected());
    ui->invokeMethodButton->setEnabled(connected && !busy && hasSelection && selectedMethodIndex() > 0);
    ui->clockWriteButton->setEnabled(connected && !busy && isClockSelected());
    ui->actionSave->setEnabled(m_dirty || !m_projectPath.isEmpty());
}

bool MainWindow::isClockSelected() const
{
    return m_selectedObject && m_selectedObject->GetObjectType() == DLMS_OBJECT_TYPE_CLOCK;
}

bool MainWindow::isOctetStringSelected() const
{
    const int index = selectedAttributeIndex();
    if (!m_selectedObject || index <= 0)
        return false;
    DLMS_DATA_TYPE type = DLMS_DATA_TYPE_NONE;
    m_selectedObject->GetDataType(index, type);
    if (type == DLMS_DATA_TYPE_NONE)
        m_selectedObject->GetUIDataType(index, type);
    return type == DLMS_DATA_TYPE_OCTET_STRING;
}

bool MainWindow::isProfileGenericSelected() const
{
    return m_selectedObject && m_selectedObject->GetObjectType() == DLMS_OBJECT_TYPE_PROFILE_GENERIC;
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("GXDLMSDirector");
    if (!m_device->name().isEmpty())
        title += QStringLiteral(" - ") + m_device->name();
    if (!m_projectPath.isEmpty())
        title += QStringLiteral(" [") + QFileInfo(m_projectPath).fileName() + QLatin1Char(']');
    if (m_dirty)
        title += QStringLiteral(" *");
    setWindowTitle(title);
}

void MainWindow::setDirty(bool dirty)
{
    m_dirty = dirty;
    updateWindowTitle();
    updateActions();
}

bool MainWindow::maybeSave()
{
    if (!m_dirty)
        return true;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Save Changes"), tr("Save changes to the project?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel)
        return false;
    if (answer == QMessageBox::Yes) {
        onSaveProject();
        return !m_dirty;
    }
    return true;
}

void MainWindow::appendTrace(const QString &message)
{
    ui->traceLog->appendPlainText(message);
}

void MainWindow::rebuildObjectTree()
{
    m_treeModel->clear();
    m_treeModel->setHorizontalHeaderLabels({tr("Objects")});

    auto *root = new QStandardItem(m_device->name());
    m_treeModel->appendRow(root);

    for (auto *obj : m_device->objects()) {
        std::string logicalName;
        obj->GetLogicalName(logicalName);
        const QString label = QString::fromUtf8(CGXDLMSConverter::ToString(obj->GetObjectType()))
                              + QStringLiteral(" ") + QString::fromStdString(logicalName);
        auto *item = new QStandardItem(label);
        item->setData(static_cast<quintptr>(reinterpret_cast<quintptr>(obj)), Qt::UserRole);
        root->appendRow(item);
    }

    ui->objectTree->expandAll();
}

void MainWindow::updateObjectEditors(CGXDLMSObject *object)
{
    ui->clockEditorGroup->setVisible(object && object->GetObjectType() == DLMS_OBJECT_TYPE_CLOCK);
    updatePropertyTable(object);
    updateMethodsTable(object);
    updateActions();
}

void MainWindow::updatePropertyTable(CGXDLMSObject *object)
{
    ui->propertyTable->blockSignals(true);
    ui->propertyTable->setRowCount(0);
    if (!object) {
        ui->propertyTable->blockSignals(false);
        return;
    }

    const int count = object->GetAttributeCount();
    ui->propertyTable->setRowCount(count);
    for (int i = 0; i < count; ++i) {
        const int index = i + 1;
        const QString attrLabel = QStringLiteral("%1: %2")
                                      .arg(index)
                                      .arg(ObjectAttributeNames::attributeName(object->GetObjectType(), index));

        auto *nameItem = new QTableWidgetItem(attrLabel);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->propertyTable->setItem(i, 0, nameItem);

        auto *typeItem = new QTableWidgetItem(VariantConverter::dataTypeLabel(object, index));
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        ui->propertyTable->setItem(i, 1, typeItem);

        std::vector<std::string> values;
        object->GetValues(values);
        auto *valueItem = new QTableWidgetItem();
        if (index - 1 < static_cast<int>(values.size()))
            valueItem->setText(QString::fromStdString(values.at(static_cast<size_t>(index - 1))));

        if (VariantConverter::isWritable(object, index))
            valueItem->setBackground(QColor(255, 255, 220));
        else
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);

        ui->propertyTable->setItem(i, 2, valueItem);
    }
    ui->propertyTable->resizeColumnsToContents();
    ui->propertyTable->blockSignals(false);
}

void MainWindow::updateMethodsTable(CGXDLMSObject *object)
{
    ui->methodsTable->blockSignals(true);
    ui->methodsTable->setRowCount(0);
    if (!object) {
        ui->methodsTable->blockSignals(false);
        return;
    }

    const int count = object->GetMethodCount();
    ui->methodsTable->setRowCount(count);
    for (int i = 0; i < count; ++i) {
        const int index = i + 1;
        const QString label = QStringLiteral("%1: %2")
                                  .arg(index)
                                  .arg(ObjectAttributeNames::methodName(object->GetObjectType(), index));
        auto *nameItem = new QTableWidgetItem(label);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->methodsTable->setItem(i, 0, nameItem);

        QString accessText;
        switch (object->GetMethodAccess(index)) {
        case DLMS_METHOD_ACCESS_MODE_ACCESS:
            accessText = tr("Access");
            break;
        case DLMS_METHOD_ACCESS_MODE_AUTHENTICATED_ACCESS:
            accessText = tr("Authenticated access");
            break;
        case DLMS_METHOD_ACCESS_MODE_NONE:
            accessText = tr("No access");
            break;
        default:
            accessText = tr("Unknown");
            break;
        }
        auto *accessItem = new QTableWidgetItem(accessText);
        accessItem->setFlags(accessItem->flags() & ~Qt::ItemIsEditable);
        ui->methodsTable->setItem(i, 1, accessItem);
    }
    ui->methodsTable->resizeColumnsToContents();
    ui->methodsTable->blockSignals(false);
}

int MainWindow::selectedMethodIndex() const
{
    const int row = ui->methodsTable->currentRow();
    return row >= 0 ? row + 1 : 0;
}

int MainWindow::selectedAttributeIndex() const
{
    const int row = ui->propertyTable->currentRow();
    return row >= 0 ? row + 1 : 0;
}
