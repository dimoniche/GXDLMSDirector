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
#include "FindObjectDialog.h"
#include "PropertyTableDelegate.h"
#include "ui_MainWindow.h"

#include "core/CosemObjectHelper.h"
#include "core/DataConcentratorManager.h"
#include "core/ManufacturerSettings.h"
#include "core/ObjectAttributeNames.h"
#include "core/GXDLMSProject.h"
#include "core/ProjectSerializer.h"
#include "core/RecentFilesManager.h"
#include "core/TraceFormatter.h"
#include "core/ValuesSerializer.h"
#include "core/VariantConverter.h"

#include <GXDLMSConverter.h>
#include <GXDLMSObject.h>
#include <enums.h>

#include <QActionGroup>
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItem>
#include <QUrl>

namespace {
enum TreeRole {
    KindRole = Qt::UserRole + 1,
    DeviceIndexRole = Qt::UserRole + 2,
};
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_project(this)
{
    ui->setupUi(this);
    m_treeModel = new QStandardItemModel(this);
    ui->objectTree->setModel(m_treeModel);
    ui->propertyTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    ui->propertyTable->horizontalHeader()->setStretchLastSection(true);
    ui->methodsTable->horizontalHeader()->setStretchLastSection(true);
    ui->clockEditorGroup->setVisible(false);
    ui->hdlcEditorGroup->setVisible(false);
    ui->disconnectControlGroup->setVisible(false);

    m_propertyDelegate = new PropertyTableDelegate(ui->propertyTable);
    ui->propertyTable->setItemDelegateForColumn(2, m_propertyDelegate);

    ManufacturerSettings::instance().load();
    RecentFilesManager::instance().load();
    setupEditorGroups();
    setupConnections();
    bindActiveDevice();
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

void MainWindow::setupEditorGroups()
{
    populateHdlcSpeedCombo();

    auto *traceGroup = new QActionGroup(this);
    traceGroup->addAction(ui->actionTraceHex);
    traceGroup->addAction(ui->actionTraceXml);
    traceGroup->addAction(ui->actionTracePdu);
    traceGroup->addAction(ui->actionTraceNone);

    auto *notificationGroup = new QActionGroup(this);
    notificationGroup->addAction(ui->actionNotificationHex);
    notificationGroup->addAction(ui->actionNotificationXml);
    notificationGroup->addAction(ui->actionNotificationPdu);
}

void MainWindow::populateHdlcSpeedCombo()
{
    if (ui->hdlcSpeedCombo->count() > 0)
        return;

    ui->hdlcSpeedCombo->addItem(QStringLiteral("300"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("600"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("1200"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("2400"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("4800"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("9600"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("19200"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("38400"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("57600"));
    ui->hdlcSpeedCombo->addItem(QStringLiteral("115200"));
}

void MainWindow::setupConnections()
{
    connect(ui->actionNewDevice, &QAction::triggered, this, &MainWindow::onNewProject);
    connect(ui->actionAddDevice, &QAction::triggered, this, &MainWindow::onAddDevice);
    connect(ui->actionCloneDevice, &QAction::triggered, this, &MainWindow::onCloneDevice);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::onSaveProject);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::onSaveProjectAs);
    connect(ui->actionSaveValues, &QAction::triggered, this, &MainWindow::onSaveValues);
    connect(ui->actionLoadValues, &QAction::triggered, this, &MainWindow::onLoadValues);
    connect(ui->actionFindObject, &QAction::triggered, this, &MainWindow::onFindObject);
    connect(ui->actionFindNextObject, &QAction::triggered, this, &MainWindow::onFindNextObject);
    connect(ui->menuRecentFiles, &QMenu::aboutToShow, this, &MainWindow::onRecentFilesAboutToShow);
    connect(&m_project, &GXDLMSProject::dirtyChanged, this, &MainWindow::onProjectDirtyChanged);
    connect(&m_project, &GXDLMSProject::currentDeviceChanged, this, [this](GXDLMSDevice *) {
        bindActiveDevice();
        updateWindowTitle();
        updateActions();
    });
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
    connect(ui->hdlcWriteButton, &QPushButton::clicked, this, &MainWindow::onHdlcWrite);
    connect(ui->remoteDisconnectButton, &QPushButton::clicked, this, &MainWindow::onRemoteDisconnect);
    connect(ui->remoteReconnectButton, &QPushButton::clicked, this, &MainWindow::onRemoteReconnect);
    connect(ui->actionCancel, &QAction::triggered, this, &MainWindow::onCancel);
    connect(ui->actionForceRead, &QAction::toggled, this, &MainWindow::onForceReadToggled);
    connect(ui->actionTraceHex, &QAction::triggered, this, &MainWindow::onTraceModeChanged);
    connect(ui->actionTraceXml, &QAction::triggered, this, &MainWindow::onTraceModeChanged);
    connect(ui->actionTracePdu, &QAction::triggered, this, &MainWindow::onTraceModeChanged);
    connect(ui->actionTraceNone, &QAction::triggered, this, &MainWindow::onTraceModeChanged);
    connect(ui->actionClearTrace, &QAction::triggered, ui->traceLog, &QPlainTextEdit::clear);
    connect(ui->actionStartNotifications, &QAction::triggered, this, &MainWindow::onStartNotifications);
    connect(ui->actionStopNotifications, &QAction::triggered, this, &MainWindow::onStopNotifications);
    connect(ui->actionNotificationHex, &QAction::triggered, this, &MainWindow::onNotificationModeChanged);
    connect(ui->actionNotificationXml, &QAction::triggered, this, &MainWindow::onNotificationModeChanged);
    connect(ui->actionNotificationPdu, &QAction::triggered, this, &MainWindow::onNotificationModeChanged);
    connect(ui->actionClearNotifications, &QAction::triggered, ui->notificationsLog, &QPlainTextEdit::clear);
    connect(ui->actionReadProfileGeneric, &QAction::triggered, this, &MainWindow::onReadProfileGeneric);
    connect(ui->actionDlmsTranslator, &QAction::triggered, this, &MainWindow::onDlmsTranslator);
    connect(ui->actionHdlcAddressScanner, &QAction::triggered, this, &MainWindow::onHdlcAddressScanner);
    connect(ui->actionMacroEditor, &QAction::triggered, this, &MainWindow::onMacroEditor);
    connect(ui->actionConformanceTests, &QAction::triggered, this, &MainWindow::onConformanceTests);
    connect(ui->actionPlcDiscover, &QAction::triggered, this, &MainWindow::onPlcDiscover);
    connect(ui->actionDataConcentrators, &QAction::triggered, this, &MainWindow::onDataConcentrators);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    connect(ui->objectTree, &QTreeView::clicked, this, &MainWindow::onObjectTreeClicked);
    connect(ui->propertyTable, &QTableWidget::itemChanged, this, &MainWindow::onPropertyTableChanged);
    connect(ui->methodsTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateActions);
}

void MainWindow::onNewProject()
{
    if (!maybeSave())
        return;

    if (GXDLMSDevice *device = activeDevice()) {
        if (device->state() & DeviceState::Connected)
            device->disconnectAsync();
    }

    m_project.resetToSingleDevice();
    m_selectedObject = nullptr;
    rebuildObjectTree();
    updateObjectEditors(nullptr);
    updateActions();
    appendTrace(tr("New project created."));
}

void MainWindow::onAddDevice()
{
    m_project.addDevice();
    setDirty(true);
    rebuildObjectTree();
    appendTrace(tr("Device added: %1").arg(activeDevice()->name()));
}

void MainWindow::onCloneDevice()
{
    const int index = m_project.currentDeviceIndex();
    GXDLMSDevice *clone = m_project.cloneDeviceAt(index);
    if (!clone) {
        QMessageBox::warning(this, tr("Clone Device"), tr("Failed to clone device."));
        return;
    }

    m_selectedObject = nullptr;
    rebuildObjectTree();
    updateObjectEditors(nullptr);
    appendTrace(tr("Cloned device: %1").arg(clone->name()));
}

void MainWindow::onOpenProject()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                      m_project.path().isEmpty() ? QDir::homePath() : m_project.path(),
                                                      tr("GXDLMSDirector projects (*.gxc)"));
    if (path.isEmpty())
        return;

    openProjectFile(path);
}

void MainWindow::openProjectFile(const QString &path)
{
    QString error;
    if (!ProjectSerializer::load(path, &m_project, &error)) {
        QMessageBox::warning(this, tr("Open Project"), error);
        RecentFilesManager::instance().remove(path);
        return;
    }

    m_selectedObject = nullptr;
    bindActiveDevice();
    rebuildObjectTree();
    updateObjectEditors(nullptr);
    updateActions();
    RecentFilesManager::instance().add(path);
    appendTrace(tr("Project loaded: %1").arg(path));
}

void MainWindow::onOpenRecentFile()
{
    if (auto *action = qobject_cast<QAction *>(sender())) {
        const QString path = action->data().toString();
        if (!path.isEmpty())
            openProjectFile(path);
    }
}

void MainWindow::onRecentFilesAboutToShow()
{
    ui->menuRecentFiles->clear();
    const QStringList files = RecentFilesManager::instance().files();
    ui->menuRecentFiles->setEnabled(!files.isEmpty());
    for (const QString &path : files) {
        auto *action = ui->menuRecentFiles->addAction(QFileInfo(path).fileName());
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, &MainWindow::onOpenRecentFile);
    }
}

void MainWindow::onSaveProject()
{
    if (m_project.path().isEmpty()) {
        onSaveProjectAs();
        return;
    }

    QString error;
    if (!ProjectSerializer::save(m_project.path(), &m_project, &error)) {
        QMessageBox::warning(this, tr("Save Project"), error);
        return;
    }

    setDirty(false);
    RecentFilesManager::instance().add(m_project.path());
    statusBar()->showMessage(tr("Project saved."), 3000);
}

void MainWindow::onSaveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Project As"),
                                                      m_project.path().isEmpty() ? QDir::homePath() : m_project.path(),
                                                      tr("GXDLMSDirector projects (*.gxc)"));
    if (path.isEmpty())
        return;

    m_project.setPath(path.endsWith(QStringLiteral(".gxc"), Qt::CaseInsensitive) ? path : path + QStringLiteral(".gxc"));
    onSaveProject();
}

void MainWindow::onSaveValues()
{
    GXDLMSDevice *device = activeDevice();
    if (!device)
        return;

    const QString path = QFileDialog::getSaveFileName(this, tr("Save Values As"), QDir::homePath(),
                                                        tr("COSEM object values (*.objects.xml);;All files (*.*)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!ValuesSerializer::saveValues(path, device, &error)) {
        QMessageBox::warning(this, tr("Save Values"), error);
        return;
    }
    statusBar()->showMessage(tr("Values saved."), 3000);
}

void MainWindow::onLoadValues()
{
    GXDLMSDevice *device = activeDevice();
    if (!device)
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Load Values"), QDir::homePath(),
                                                        tr("COSEM object values (*.objects.xml);;All files (*.*)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!ValuesSerializer::loadValues(path, device, &error)) {
        QMessageBox::warning(this, tr("Load Values"), error);
        return;
    }

    m_selectedObject = nullptr;
    setDirty(true);
    rebuildObjectTree();
    updateObjectEditors(nullptr);
    statusBar()->showMessage(tr("Values loaded."), 3000);
}

void MainWindow::onFindObject()
{
    FindObjectDialog dlg(this);
    dlg.setSearchText(m_findText);
    dlg.setLogicalName(m_findLogicalName);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_findText = dlg.searchText();
    m_findLogicalName = dlg.logicalName();
    if (m_findText.isEmpty() && m_findLogicalName.isEmpty()) {
        QMessageBox::information(this, tr("Find Object"), tr("Enter search text or logical name."));
        return;
    }

    if (findObject(true))
        ui->actionFindNextObject->setEnabled(true);
    else
        QMessageBox::information(this, tr("Find Object"), tr("No matching object found."));
}

void MainWindow::onFindNextObject()
{
    if (!findObject(false))
        QMessageBox::information(this, tr("Find Next"), tr("No more matches."));
}

void MainWindow::onDeviceProperties()
{
    GXDLMSDevice *device = activeDevice();
    if (!device)
        return;

    DevicePropertiesDialog dlg(device, this);
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
    appendTrace(tr("Connecting to %1...").arg(activeDevice()->name()));
    activeDevice()->connectAsync();
}

void MainWindow::onDisconnect()
{
    recordMacroStep(MacroActionType::Disconnect);
    activeDevice()->disconnectAsync();
}

void MainWindow::onReadAll()
{
    appendTrace(tr("Reading all objects..."));
    activeDevice()->readAllAsync();
}

void MainWindow::onReadObject()
{
    if (!m_selectedObject)
        return;

    appendTrace(tr("Reading object..."));
    activeDevice()->readSelectedObjectAsync(m_selectedObject, activeDevice()->forceRead());
}

void MainWindow::onCancel()
{
    activeDevice()->cancelOperation();
    statusBar()->showMessage(tr("Cancelling..."), 2000);
}

void MainWindow::onForceReadToggled(bool checked)
{
    if (GXDLMSDevice *device = activeDevice())
        device->setForceRead(checked);
}

void MainWindow::onTraceModeChanged()
{
    ui->actionTraceTimestamps->setEnabled(ui->actionTraceNone != sender() || !ui->actionTraceNone->isChecked());

    if (ui->actionTraceHex->isChecked())
        m_traceMode = TraceDisplayMode::Hex;
    else if (ui->actionTraceXml->isChecked())
        m_traceMode = TraceDisplayMode::Xml;
    else if (ui->actionTracePdu->isChecked())
        m_traceMode = TraceDisplayMode::Pdu;
    else
        m_traceMode = TraceDisplayMode::None;
}

void MainWindow::onNotificationModeChanged()
{
    if (ui->actionNotificationHex->isChecked())
        m_notificationMode = NotificationDisplayMode::Hex;
    else if (ui->actionNotificationXml->isChecked())
        m_notificationMode = NotificationDisplayMode::Xml;
    else
        m_notificationMode = NotificationDisplayMode::Pdu;
}

void MainWindow::onStartNotifications()
{
    if (!(activeDevice()->state() & DeviceState::Connected)) {
        QMessageBox::information(this, tr("Notifications"), tr("Connect to a device first."));
        return;
    }
    activeDevice()->setNotificationsEnabled(true);
    ui->actionStartNotifications->setEnabled(false);
    ui->actionStopNotifications->setEnabled(true);
    ui->logTabs->setCurrentWidget(ui->notificationsTab);
    appendNotification(tr("Listening for notifications..."));
}

void MainWindow::onStopNotifications()
{
    activeDevice()->setNotificationsEnabled(false);
    ui->actionStartNotifications->setEnabled(true);
    ui->actionStopNotifications->setEnabled(false);
    appendNotification(tr("Notifications stopped."));
}

void MainWindow::onHdlcWrite()
{
    if (!isHdlcSelected())
        return;

    appendTrace(tr("Writing HDLC communication speed..."));
    activeDevice()->writeObjectAsync(m_selectedObject, 2, QString::number(ui->hdlcSpeedCombo->currentIndex()));
}

void MainWindow::onRemoteDisconnect()
{
    if (!isDisconnectControlSelected())
        return;
    appendTrace(tr("Remote disconnect..."));
    activeDevice()->invokeMethodAsync(m_selectedObject, 1, {});
}

void MainWindow::onRemoteReconnect()
{
    if (!isDisconnectControlSelected())
        return;
    appendTrace(tr("Remote reconnect..."));
    activeDevice()->invokeMethodAsync(m_selectedObject, 2, {});
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
    activeDevice()->writeObjectAsync(m_selectedObject, attributeIndex, valueItem->text());
}

void MainWindow::onInvokeMethod()
{
    const int methodIndex = selectedMethodIndex();
    if (!m_selectedObject || methodIndex <= 0)
        return;

    appendTrace(tr("Invoking method %1...").arg(methodIndex));
    recordMacroStep(MacroActionType::Action, m_selectedObject, methodIndex, ui->methodParamEdit->text());
    activeDevice()->invokeMethodAsync(m_selectedObject, methodIndex, ui->methodParamEdit->text());
}

void MainWindow::onAddObject()
{
    CosemObjectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString error;
    CGXDLMSObject *object = CosemObjectHelper::addObject(
        activeDevice()->objects(), dlg.objectType(), dlg.logicalName(), dlg.version(), dlg.description(), &error);
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

    if (!CosemObjectHelper::removeObject(activeDevice()->objects(), m_selectedObject)) {
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

    ProfileGenericDialog dlg(activeDevice(), m_selectedObject, this);
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
        m_macroEditor = new MacroEditorDialog(activeDevice(), this);
        connect(m_macroEditor, &QObject::destroyed, this, [this]() { m_macroEditor = nullptr; });
    }
    m_macroEditor->show();
    m_macroEditor->raise();
    m_macroEditor->activateWindow();
}

void MainWindow::onConformanceTests()
{
    ConformanceTestDialog dlg(activeDevice(), this);
    dlg.exec();
}

void MainWindow::onPlcDiscover()
{
    PlcDiscoverDialog dlg(activeDevice(), this);
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
    step.device = activeDevice()->name();
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
        if (!activeDevice()->negotiatedConformance().isEmpty()) {
            appendTrace(tr("Negotiated conformance: %1").arg(activeDevice()->negotiatedConformance()));
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

void MainWindow::onTraceData(const QString &direction, const QByteArray &data)
{
    m_traceTimestamps = ui->actionTraceTimestamps->isChecked();
    const QString formatted =
        TraceFormatter::formatPacket(direction, data, m_traceMode, m_traceTimestamps);
    if (!formatted.isEmpty())
        appendTrace(formatted);
}

void MainWindow::onNotificationReceived(const QByteArray &data)
{
    m_notificationTimestamps = ui->actionNotificationTimestamps->isChecked();
    const QString formatted =
        TraceFormatter::formatNotification(data, m_notificationMode, m_notificationTimestamps);
    if (!formatted.isEmpty())
        appendNotification(formatted);
}

void MainWindow::onProgressChanged(const QString &description, int current, int maximum)
{
    statusBar()->showMessage(QStringLiteral("%1 (%2/%3)").arg(description).arg(current).arg(maximum));
}

void MainWindow::onObjectRead(CGXDLMSObject *object, int attributeIndex, const QString &value)
{
    if (object == m_selectedObject) {
        updateObjectEditors(object);
        if (isDisconnectControlSelected())
            updateDisconnectControlPanel(object);
    }

    std::string objName;
    object->GetLogicalName(objName);
    appendTrace(QStringLiteral("%1 attr %2 = %3")
                    .arg(QString::fromStdString(objName))
                    .arg(attributeIndex)
                    .arg(value));
}

void MainWindow::onObjectWritten(CGXDLMSObject *object, int attributeIndex, const QString &value)
{
    if (object == m_selectedObject) {
        updateObjectEditors(object);
        if (isDisconnectControlSelected())
            updateDisconnectControlPanel(object);
    }

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

    const int deviceIndex = item->data(DeviceIndexRole).toInt();
    const auto kind = static_cast<TreeItemKind>(item->data(KindRole).toInt());
    selectDevice(deviceIndex);

    if (kind == TreeItemKind::DeviceNode) {
        m_selectedObject = nullptr;
        updateObjectEditors(nullptr);
        return;
    }

    m_selectedObject = reinterpret_cast<CGXDLMSObject *>(item->data(Qt::UserRole).value<quintptr>());
    updateObjectEditors(m_selectedObject);
}

void MainWindow::onPropertyTableChanged(QTableWidgetItem *item)
{
    if (item && item->column() == 2)
        setDirty(true);
}

void MainWindow::updateActions()
{
    GXDLMSDevice *device = activeDevice();
    const bool connected = device && (device->state() & DeviceState::Connected);
    const bool busy = device && (device->state() & (DeviceState::Connecting | DeviceState::Disconnecting
                                                      | DeviceState::Reading | DeviceState::Writing));
    const bool hasSelection = m_selectedObject != nullptr;

    ui->actionConnect->setEnabled(!connected && !busy);
    ui->actionDisconnect->setEnabled(connected && !busy);
    ui->actionReadAll->setEnabled(connected && !busy);
    ui->actionReadObject->setEnabled(connected && !busy && hasSelection);
    ui->actionWriteObject->setEnabled(connected && !busy && hasSelection);
    ui->actionCancel->setEnabled(busy);
    ui->actionInvokeMethod->setEnabled(connected && !busy && hasSelection && selectedMethodIndex() > 0);
    ui->actionReadProfileGeneric->setEnabled(connected && !busy && isProfileGenericSelected());
    ui->actionDeleteObject->setEnabled(hasSelection && !busy);
    ui->actionEditOctetString->setEnabled(hasSelection && isOctetStringSelected());
    ui->invokeMethodButton->setEnabled(connected && !busy && hasSelection && selectedMethodIndex() > 0);
    ui->clockWriteButton->setEnabled(connected && !busy && isClockSelected());
    ui->hdlcWriteButton->setEnabled(connected && !busy && isHdlcSelected());
    ui->remoteDisconnectButton->setEnabled(connected && !busy && isDisconnectControlSelected());
    ui->remoteReconnectButton->setEnabled(connected && !busy && isDisconnectControlSelected());
    ui->actionSave->setEnabled(m_project.isDirty() || !m_project.path().isEmpty());
    ui->actionCloneDevice->setEnabled(m_project.deviceCount() > 0);
}

bool MainWindow::isHdlcSelected() const
{
    return m_selectedObject && m_selectedObject->GetObjectType() == DLMS_OBJECT_TYPE_IEC_HDLC_SETUP;
}

bool MainWindow::isDisconnectControlSelected() const
{
    return m_selectedObject && m_selectedObject->GetObjectType() == DLMS_OBJECT_TYPE_DISCONNECT_CONTROL;
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
    if (GXDLMSDevice *device = activeDevice()) {
        if (!device->name().isEmpty())
            title += QStringLiteral(" - ") + device->name();
        if (m_project.deviceCount() > 1)
            title += QStringLiteral(" (%1/%2)").arg(m_project.currentDeviceIndex() + 1).arg(m_project.deviceCount());
    }
    if (!m_project.path().isEmpty())
        title += QStringLiteral(" [") + QFileInfo(m_project.path()).fileName() + QLatin1Char(']');
    if (m_project.isDirty())
        title += QStringLiteral(" *");
    setWindowTitle(title);
}

void MainWindow::setDirty(bool dirty)
{
    m_project.setDirty(dirty);
}

void MainWindow::onProjectDirtyChanged(bool dirty)
{
    Q_UNUSED(dirty)
    updateWindowTitle();
    updateActions();
}

bool MainWindow::maybeSave()
{
    if (!m_project.isDirty())
        return true;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Save Changes"), tr("Save changes to the project?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel)
        return false;
    if (answer == QMessageBox::Yes) {
        onSaveProject();
        return !m_project.isDirty();
    }
    return true;
}

void MainWindow::appendTrace(const QString &message)
{
    ui->traceLog->appendPlainText(message);
}

void MainWindow::appendNotification(const QString &message)
{
    ui->notificationsLog->appendPlainText(message);
}

void MainWindow::rebuildObjectTree()
{
    m_treeModel->clear();
    m_treeModel->setHorizontalHeaderLabels({tr("Devices / Objects")});

    for (int deviceIndex = 0; deviceIndex < m_project.deviceCount(); ++deviceIndex) {
        GXDLMSDevice *device = m_project.deviceAt(deviceIndex);
        auto *deviceItem = new QStandardItem(device->name());
        deviceItem->setData(static_cast<int>(TreeItemKind::DeviceNode), KindRole);
        deviceItem->setData(deviceIndex, DeviceIndexRole);
        m_treeModel->appendRow(deviceItem);

        for (auto *obj : device->objects()) {
            std::string logicalName;
            obj->GetLogicalName(logicalName);
            const QString label = QString::fromUtf8(CGXDLMSConverter::ToString(obj->GetObjectType()))
                                  + QStringLiteral(" ") + QString::fromStdString(logicalName);
            auto *item = new QStandardItem(label);
            item->setData(static_cast<int>(TreeItemKind::ObjectNode), KindRole);
            item->setData(deviceIndex, DeviceIndexRole);
            item->setData(static_cast<quintptr>(reinterpret_cast<quintptr>(obj)), Qt::UserRole);
            deviceItem->appendRow(item);
        }
    }

    ui->objectTree->expandAll();
}

void MainWindow::updateObjectEditors(CGXDLMSObject *object)
{
    m_propertyDelegate->setObject(object);
    ui->clockEditorGroup->setVisible(object && object->GetObjectType() == DLMS_OBJECT_TYPE_CLOCK);
    ui->hdlcEditorGroup->setVisible(object && object->GetObjectType() == DLMS_OBJECT_TYPE_IEC_HDLC_SETUP);
    ui->disconnectControlGroup->setVisible(object && object->GetObjectType() == DLMS_OBJECT_TYPE_DISCONNECT_CONTROL);
    updatePropertyTable(object);
    updateMethodsTable(object);
    updateDisconnectControlPanel(object);

    if (object && object->GetObjectType() == DLMS_OBJECT_TYPE_IEC_HDLC_SETUP) {
        std::vector<std::string> values;
        object->GetValues(values);
        if (values.size() >= 2) {
            bool ok = false;
            const int enumVal = QString::fromStdString(values.at(1)).toInt(&ok);
            if (ok && enumVal >= 0 && enumVal < ui->hdlcSpeedCombo->count())
                ui->hdlcSpeedCombo->setCurrentIndex(enumVal);
            else {
                const int idx = ui->hdlcSpeedCombo->findText(QString::fromStdString(values.at(1)));
                if (idx >= 0)
                    ui->hdlcSpeedCombo->setCurrentIndex(idx);
            }
        }
    }

    updateActions();
}

void MainWindow::updateDisconnectControlPanel(CGXDLMSObject *object)
{
    if (!object || object->GetObjectType() != DLMS_OBJECT_TYPE_DISCONNECT_CONTROL) {
        ui->disconnectStateLabel->setText(tr("State: —"));
        return;
    }

    std::vector<std::string> values;
    object->GetValues(values);
    const QString outputState = values.size() >= 2 ? QString::fromStdString(values.at(1)) : QStringLiteral("—");
    const QString controlState = values.size() >= 3 ? QString::fromStdString(values.at(2)) : QStringLiteral("—");
    ui->disconnectStateLabel->setText(tr("Output: %1, Control: %2").arg(outputState, controlState));
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

GXDLMSDevice *MainWindow::activeDevice()
{
    return m_project.currentDevice();
}

const GXDLMSDevice *MainWindow::activeDevice() const
{
    return m_project.currentDevice();
}

void MainWindow::bindActiveDevice()
{
    unbindActiveDevice();
    GXDLMSDevice *device = activeDevice();
    if (!device)
        return;

    m_deviceConnections.append(connect(device, &GXDLMSDevice::stateChanged, this, &MainWindow::onDeviceStateChanged));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::traceMessage, this, &MainWindow::onTraceMessage));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::traceData, this, &MainWindow::onTraceData));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::notificationReceived, this,
                                         &MainWindow::onNotificationReceived));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::progressChanged, this, &MainWindow::onProgressChanged));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::objectRead, this, &MainWindow::onObjectRead));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::objectWritten, this, &MainWindow::onObjectWritten));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::methodInvoked, this, &MainWindow::onMethodInvoked));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::errorOccurred, this, [this](const QString &msg) {
        appendTrace(tr("ERROR: %1").arg(msg));
        QMessageBox::warning(this, tr("Error"), msg);
    }));
    m_deviceConnections.append(connect(device, &GXDLMSDevice::readAllFinished, this, [this]() {
        statusBar()->showMessage(tr("Read all completed."), 3000);
    }));
}

void MainWindow::unbindActiveDevice()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_deviceConnections))
        disconnect(connection);
    m_deviceConnections.clear();
}

void MainWindow::selectDevice(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= m_project.deviceCount())
        return;
    if (deviceIndex == m_project.currentDeviceIndex())
        return;

    GXDLMSDevice *previous = activeDevice();
    if (previous && (previous->state() & DeviceState::Connected))
        previous->disconnectAsync();

    m_project.setCurrentDeviceIndex(deviceIndex);
}

bool MainWindow::objectMatchesSearch(CGXDLMSObject *object, const QString &searchText,
                                     const QString &logicalName) const
{
    if (!object)
        return false;

    std::string ln;
    object->GetLogicalName(ln);
    const QString label = QString::fromUtf8(CGXDLMSConverter::ToString(object->GetObjectType()))
                          + QStringLiteral(" ") + QString::fromStdString(ln);

    if (!logicalName.isEmpty() && !label.contains(logicalName, Qt::CaseInsensitive)
        && !QString::fromStdString(ln).contains(logicalName, Qt::CaseInsensitive)) {
        return false;
    }

    if (!searchText.isEmpty() && !label.contains(searchText, Qt::CaseInsensitive))
        return false;

    return true;
}

bool MainWindow::findObject(bool fromStart)
{
    int startDevice = fromStart ? 0 : m_lastFoundDeviceIndex;
    int startRow = fromStart ? -1 : m_lastFoundObjectRow;

    for (int deviceIndex = startDevice; deviceIndex < m_project.deviceCount(); ++deviceIndex) {
        GXDLMSDevice *device = m_project.deviceAt(deviceIndex);
        int row = 0;
        for (auto *object : device->objects()) {
            if (deviceIndex == startDevice && row <= startRow) {
                ++row;
                continue;
            }
            if (objectMatchesSearch(object, m_findText, m_findLogicalName)) {
                m_lastFoundDeviceIndex = deviceIndex;
                m_lastFoundObjectRow = row;
                selectTreeObject(deviceIndex, object);
                return true;
            }
            ++row;
        }
        startRow = -1;
    }

    if (!fromStart) {
        m_lastFoundDeviceIndex = -1;
        m_lastFoundObjectRow = -1;
        return findObject(true);
    }

    return false;
}

void MainWindow::selectTreeObject(int deviceIndex, CGXDLMSObject *object)
{
    if (!object)
        return;

    for (int row = 0; row < m_treeModel->rowCount(); ++row) {
        auto *deviceItem = m_treeModel->item(row);
        if (!deviceItem || deviceItem->data(DeviceIndexRole).toInt() != deviceIndex)
            continue;

        for (int childRow = 0; childRow < deviceItem->rowCount(); ++childRow) {
            auto *objectItem = deviceItem->child(childRow);
            if (!objectItem)
                continue;
            auto *itemObject = reinterpret_cast<CGXDLMSObject *>(objectItem->data(Qt::UserRole).value<quintptr>());
            if (itemObject != object)
                continue;

            const QModelIndex index = m_treeModel->indexFromItem(objectItem);
            ui->objectTree->setCurrentIndex(index);
            ui->objectTree->scrollTo(index);
            onObjectTreeClicked(index);
            return;
        }
    }
}
