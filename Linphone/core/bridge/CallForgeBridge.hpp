// Copyright (c) 2026 Rammohan Yadavalli. All rights reserved. Proprietary.
#ifndef CALLFORGE_BRIDGE_HPP
#define CALLFORGE_BRIDGE_HPP

#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>

class CallHandleImpl;

class CallForgeBridge : public QObject {
	Q_OBJECT

	Q_PROPERTY(bool daemonConnected READ isDaemonConnected NOTIFY daemonConnectedChanged)
	Q_PROPERTY(QVariantList settingsTabs READ getSettingsTabs NOTIFY settingsTabsChanged)
	Q_PROPERTY(bool armed READ isArmed NOTIFY armedChanged)
	Q_PROPERTY(int armedScenarioIndex READ getArmedScenarioIndex NOTIFY armedChanged)
	Q_PROPERTY(QString armedScenarioName READ getArmedScenarioName NOTIFY armedChanged)
	Q_PROPERTY(QString callPanelQml READ getCallPanelQml NOTIFY callPanelQmlChanged)
	Q_PROPERTY(bool aiActive READ isAiActive NOTIFY aiStateChanged)
	Q_PROPERTY(QString aiStatus READ getAiStatus NOTIFY aiStateChanged)
	Q_PROPERTY(QString aiTranscript READ getAiTranscript NOTIFY aiTranscriptChanged)

public:
	explicit CallForgeBridge(QObject *parent = nullptr);
	~CallForgeBridge() override;

	bool isDaemonConnected() const;
	QVariantList getSettingsTabs() const;
	bool isArmed() const;
	int getArmedScenarioIndex() const;
	QString getArmedScenarioName() const;
	QString getCallPanelQml() const;
	bool isAiActive() const;
	QString getAiStatus() const;
	QString getAiTranscript() const;

	Q_INVOKABLE QVariantList getAiAgents() const;
	Q_INVOKABLE void addAiAgent(const QVariantMap &agent);
	Q_INVOKABLE void updateAiAgent(int index, const QVariantMap &agent);
	Q_INVOKABLE void removeAiAgent(int index);
	Q_INVOKABLE void testAiAgent(const QVariantMap &agent);
	Q_INVOKABLE QStringList getAgentNames() const;

	Q_INVOKABLE QVariantList getAiScenarios() const;
	Q_INVOKABLE void addAiScenario(const QVariantMap &scenario);
	Q_INVOKABLE void updateAiScenario(int index, const QVariantMap &scenario);
	Q_INVOKABLE void removeAiScenario(int index);

	Q_INVOKABLE void armAICall(int scenarioIndex);
	Q_INVOKABLE void disarmAICall();
	Q_INVOKABLE void startAICall();
	Q_INVOKABLE void stopAICall();

signals:
	void daemonConnectedChanged();
	void settingsTabsChanged();
	void aiAgentsChanged();
	void aiScenariosChanged();
	void aiAgentTestResult(bool success, QString message);
	void armedChanged();
	void callPanelQmlChanged();
	void aiStateChanged();
	void aiTranscriptChanged();

private slots:
	void onSocketConnected();
	void onSocketDisconnected();
	void onSocketError(QLocalSocket::LocalSocketError error);
	void onReadyRead();

private:
	void launchDaemon();
	void connectToDaemon();
	bool sendMessage(const QJsonObject &msg);
	void handleMessage(const QJsonObject &msg);

	QProcess *mDaemonProcess = nullptr;
	QLocalSocket *mSocket = nullptr;
	QByteArray mReadBuffer;
	bool mDaemonConnected = false;

	QJsonArray mAgents;
	QJsonArray mScenarios;
	QStringList mAgentNames;
	QVariantList mSettingsTabs;
	QString mCallPanelQml;

	bool mArmed = false;
	int mArmedScenarioIndex = -1;
	QString mArmedScenarioName;

	bool mAiActive = false;
	QString mAiStatus;
	QString mAiTranscript;

	CallHandleImpl *mCallHandle = nullptr;
	QMetaObject::Connection mCallStateConn;
	QMetaObject::Connection mCallEndConn;
	QMetaObject::Connection mPlayFinishedConn;
	QMetaObject::Connection mCallListConn;
	bool mMicWasMuted = false;

	void ensureCallHandle();
	void releaseCallHandle();
	void onCallStateForwarded(int state);
	void handleDaemonEvent(const QString &event, const QJsonObject &msg);
};

#endif
