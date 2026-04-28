// Copyright (c) 2026 Rammohan Yadavalli. All rights reserved. Proprietary.
#ifndef CALLFORGE_BRIDGE_HPP
#define CALLFORGE_BRIDGE_HPP

#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>

class CallForgeBridge : public QObject {
	Q_OBJECT

	Q_PROPERTY(bool daemonConnected READ isDaemonConnected NOTIFY daemonConnectedChanged)
	Q_PROPERTY(QVariantList settingsTabs READ getSettingsTabs NOTIFY settingsTabsChanged)

public:
	explicit CallForgeBridge(QObject *parent = nullptr);
	~CallForgeBridge() override;

	bool isDaemonConnected() const;
	QVariantList getSettingsTabs() const;

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

signals:
	void daemonConnectedChanged();
	void settingsTabsChanged();
	void aiAgentsChanged();
	void aiScenariosChanged();
	void aiAgentTestResult(bool success, QString message);

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
};

#endif
