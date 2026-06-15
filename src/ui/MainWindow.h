#pragma once

#include "core/GXDLMSDevice.h"
#include "core/MacroStep.h"

#include <QMainWindow>
#include <QStandardItemModel>
#include <memory>

class MacroEditorDialog;
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
    void onNewDevice();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onDeviceProperties();
    void onEditManufacturers();
    void onConnect();
    void onDisconnect();
    void onReadAll();
    void onReadObject();
    void onWriteObject();
    void onInvokeMethod();
    void onAddObject();
    void onDeleteObject();
    void onEditOctetString();
    void onClockWrite();
    void onReadProfileGeneric();
    void onDlmsTranslator();
    void onHdlcAddressScanner();
    void onMacroEditor();
    void onConformanceTests();
    void onPlcDiscover();
    void onDataConcentrators();
    void onDeviceStateChanged(DeviceStates state);
    void onTraceMessage(const QString &message);
    void onProgressChanged(const QString &description, int current, int maximum);
    void onObjectRead(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void onObjectWritten(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void onMethodInvoked(CGXDLMSObject *object, int methodIndex);
    void onObjectTreeClicked(const QModelIndex &index);
    void onPropertyTableChanged(QTableWidgetItem *item);

private:
    void setupConnections();
    void updateActions();
    void updateWindowTitle();
    void setDirty(bool dirty);
    bool maybeSave();
    void appendTrace(const QString &message);
    void rebuildObjectTree();
    void updatePropertyTable(CGXDLMSObject *object);
    void updateMethodsTable(CGXDLMSObject *object);
    void updateObjectEditors(CGXDLMSObject *object);
    int selectedAttributeIndex() const;
    int selectedMethodIndex() const;
    bool isProfileGenericSelected() const;
    bool isClockSelected() const;
    bool isOctetStringSelected() const;
    void recordMacroStep(MacroActionType type, CGXDLMSObject *object = nullptr, int index = 0,
                         const QString &value = {}, const QString &error = {});

    Ui::MainWindow *ui;
    std::unique_ptr<GXDLMSDevice> m_device;
    QStandardItemModel *m_treeModel = nullptr;
    CGXDLMSObject *m_selectedObject = nullptr;
    QString m_projectPath;
    bool m_dirty = false;
    MacroEditorDialog *m_macroEditor = nullptr;
};
