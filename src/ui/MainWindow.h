#pragma once

#include "core/GXDLMSDevice.h"
#include "core/GXDLMSProject.h"
#include "core/MacroStep.h"
#include "core/TraceFormatter.h"

#include <QMainWindow>
#include <QMetaObject>
#include <QStandardItemModel>
#include <memory>

class MacroEditorDialog;
class PropertyTableDelegate;
class QTableWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewProject();
    void onAddDevice();
    void onCloneDevice();
    void onOpenProject();
    void onOpenRecentFile();
    void onSaveProject();
    void onSaveProjectAs();
    void onSaveValues();
    void onLoadValues();
    void onFindObject();
    void onFindNextObject();
    void onDeviceProperties();
    void onEditManufacturers();
    void onConnect();
    void onDisconnect();
    void onReadAll();
    void onReadSelected();
    void onReadObject();
    void onWriteObject();
    void onInvokeMethod();
    void onAddObject();
    void onDeleteObject();
    void onEditOctetString();
    void onClockWrite();
    void onHdlcWrite();
    void onRemoteDisconnect();
    void onRemoteReconnect();
    void onCancel();
    void onForceReadToggled(bool checked);
    void onTraceModeChanged();
    void onNotificationModeChanged();
    void onStartNotifications();
    void onStopNotifications();
    void onReadProfileGeneric();
    void onDlmsTranslator();
    void onHdlcAddressScanner();
    void onMacroEditor();
    void onConformanceTests();
    void onPlcDiscover();
    void onDataConcentrators();
    void onDeviceStateChanged(DeviceStates state);
    void onTraceMessage(const QString &message);
    void onTraceData(const QString &direction, const QByteArray &data);
    void onNotificationReceived(const QByteArray &data);
    void onProgressChanged(const QString &description, int current, int maximum);
    void onObjectRead(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void onObjectWritten(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void onMethodInvoked(CGXDLMSObject *object, int methodIndex);
    void onObjectTreeClicked(const QModelIndex &index);
    void onObjectListClicked(const QModelIndex &index);
    void onGroupByTypeToggled(bool checked);
    void showObjectTreeContextMenu(const QPoint &pos);
    void showObjectListContextMenu(const QPoint &pos);
    void onPropertyTableChanged(QTableWidgetItem *item);
    void onRecentFilesAboutToShow();
    void onProjectDirtyChanged(bool dirty);

private:
    enum class TreeItemKind { DeviceNode = 1, ObjectNode = 2, TypeGroupNode = 3 };

    GXDLMSDevice *activeDevice();
    const GXDLMSDevice *activeDevice() const;
    void setupConnections();
    void setupEditorGroups();
    void bindActiveDevice();
    void unbindActiveDevice();
    void populateRecentFilesMenu();
    void openProjectFile(const QString &path);
    void selectDevice(int deviceIndex);
    void updateActions();
    void updateWindowTitle();
    void setDirty(bool dirty);
    bool maybeSave();
    void appendTrace(const QString &message);
    void appendNotification(const QString &message);
    void rebuildObjectTree();
    void rebuildObjectList();
    void rebuildNavigationViews();
    void syncListSelection(int deviceIndex, CGXDLMSObject *object);
    void restoreNavigationSelection();
    void updatePropertyTable(CGXDLMSObject *object);
    void refreshObjectValueDisplays(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void updateMethodsTable(CGXDLMSObject *object);
    void updateObjectEditors(CGXDLMSObject *object);
    void updateDisconnectControlPanel(CGXDLMSObject *object);
    void populateHdlcSpeedCombo();
    bool findObject(bool fromStart);
    bool objectMatchesSearch(CGXDLMSObject *object, const QString &searchText, const QString &logicalName) const;
    void selectTreeObject(int deviceIndex, CGXDLMSObject *object);
    bool gatherReadTargets(int &deviceIndex, QVector<CGXDLMSObject *> &objects) const;
    bool hasReadableSelection() const;
    void startReadForObjects(int deviceIndex, const QVector<CGXDLMSObject *> &objects);
    int selectedAttributeIndex() const;
    int selectedMethodIndex() const;
    bool isProfileGenericSelected() const;
    bool isClockSelected() const;
    bool isHdlcSelected() const;
    bool isDisconnectControlSelected() const;
    bool isOctetStringSelected() const;
    void recordMacroStep(MacroActionType type, CGXDLMSObject *object = nullptr, int index = 0,
                         const QString &value = {}, const QString &error = {});

    Ui::MainWindow *ui;
    GXDLMSProject m_project;
    QList<QMetaObject::Connection> m_deviceConnections;
    QStandardItemModel *m_treeModel = nullptr;
    QStandardItemModel *m_listModel = nullptr;
    PropertyTableDelegate *m_propertyDelegate = nullptr;
    CGXDLMSObject *m_selectedObject = nullptr;
    MacroEditorDialog *m_macroEditor = nullptr;
    TraceDisplayMode m_traceMode = TraceDisplayMode::Hex;
    NotificationDisplayMode m_notificationMode = NotificationDisplayMode::Hex;
    bool m_traceTimestamps = true;
    bool m_notificationTimestamps = true;
    QString m_findText;
    QString m_findLogicalName;
    int m_lastFoundDeviceIndex = -1;
    int m_lastFoundObjectRow = -1;
    bool m_groupByType = true;
    bool m_syncingSelection = false;
};
