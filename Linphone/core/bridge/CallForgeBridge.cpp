// Copyright (c) 2026 Rammohan Yadavalli. All rights reserved. Proprietary.
#include "CallForgeBridge.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QtEndian>

static constexpr uint8_t FRAME_TYPE_JSON = 0x01;
static constexpr int FRAME_HEADER_SIZE = 5; // 1 byte type + 4 bytes length
static const QString SOCKET_PATH = QStringLiteral("/tmp/callforge.sock");

CallForgeBridge::CallForgeBridge(QObject *parent) : QObject(parent) {
	mSocket = new QLocalSocket(this);
	connect(mSocket, &QLocalSocket::connected, this, &CallForgeBridge::onSocketConnected);
	connect(mSocket, &QLocalSocket::disconnected, this, &CallForgeBridge::onSocketDisconnected);
	connect(mSocket, &QLocalSocket::errorOccurred, this, &CallForgeBridge::onSocketError);
	connect(mSocket, &QLocalSocket::readyRead, this, &CallForgeBridge::onReadyRead);

	launchDaemon();
}

CallForgeBridge::~CallForgeBridge() {
	if (mDaemonConnected) {
		QJsonObject shutdown;
		shutdown["cmd"] = QStringLiteral("shutdown");
		sendMessage(shutdown);
	}
	if (mSocket) {
		mSocket->disconnectFromServer();
	}
	if (mDaemonProcess) {
		mDaemonProcess->terminate();
		mDaemonProcess->waitForFinished(3000);
	}
}

bool CallForgeBridge::isDaemonConnected() const {
	return mDaemonConnected;
}

QVariantList CallForgeBridge::getSettingsTabs() const {
	return mSettingsTabs;
}

// --- Agent CRUD ---

QVariantList CallForgeBridge::getAiAgents() const {
	return mAgents.toVariantList();
}

void CallForgeBridge::addAiAgent(const QVariantMap &agent) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("addAgent");
	msg["agent"] = QJsonObject::fromVariantMap(agent);
	sendMessage(msg);
}

void CallForgeBridge::updateAiAgent(int index, const QVariantMap &agent) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("updateAgent");
	msg["index"] = index;
	msg["agent"] = QJsonObject::fromVariantMap(agent);
	sendMessage(msg);
}

void CallForgeBridge::removeAiAgent(int index) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("removeAgent");
	msg["index"] = index;
	sendMessage(msg);
}

void CallForgeBridge::testAiAgent(const QVariantMap &agent) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("testAgent");
	msg["agent"] = QJsonObject::fromVariantMap(agent);
	sendMessage(msg);
}

QStringList CallForgeBridge::getAgentNames() const {
	return mAgentNames;
}

// --- Scenario CRUD ---

QVariantList CallForgeBridge::getAiScenarios() const {
	return mScenarios.toVariantList();
}

void CallForgeBridge::addAiScenario(const QVariantMap &scenario) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("addScenario");
	msg["scenario"] = QJsonObject::fromVariantMap(scenario);
	sendMessage(msg);
}

void CallForgeBridge::updateAiScenario(int index, const QVariantMap &scenario) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("updateScenario");
	msg["index"] = index;
	msg["scenario"] = QJsonObject::fromVariantMap(scenario);
	sendMessage(msg);
}

void CallForgeBridge::removeAiScenario(int index) {
	QJsonObject msg;
	msg["cmd"] = QStringLiteral("removeScenario");
	msg["index"] = index;
	sendMessage(msg);
}

// --- Daemon lifecycle ---

void CallForgeBridge::launchDaemon() {
	QString daemonPath = QCoreApplication::applicationDirPath() + QStringLiteral("/callforge-ai-daemon");
	if (!QFile::exists(daemonPath)) {
		qWarning() << "[CallForgeBridge] Daemon not found at" << daemonPath << "— running without AI features";
		return;
	}

	mDaemonProcess = new QProcess(this);
	mDaemonProcess->setProcessChannelMode(QProcess::ForwardedChannels);
	connect(mDaemonProcess, &QProcess::started, this, [this]() {
		qInfo() << "[CallForgeBridge] Daemon process started";
		QTimer::singleShot(300, this, &CallForgeBridge::connectToDaemon);
	});
	connect(mDaemonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
	        [this](int exitCode, QProcess::ExitStatus status) {
		        qWarning() << "[CallForgeBridge] Daemon exited, code:" << exitCode << "status:" << status;
		        if (mDaemonConnected) {
			        mDaemonConnected = false;
			        emit daemonConnectedChanged();
		        }
	        });

	qInfo() << "[CallForgeBridge] Launching daemon:" << daemonPath;
	mDaemonProcess->start(daemonPath, {"--socket", SOCKET_PATH});
}

void CallForgeBridge::connectToDaemon() {
	qInfo() << "[CallForgeBridge] Connecting to daemon at" << SOCKET_PATH;
	mSocket->connectToServer(SOCKET_PATH);
}

void CallForgeBridge::onSocketConnected() {
	qInfo() << "[CallForgeBridge] Socket connected, sending hello";

	QJsonObject hello;
	hello["cmd"] = QStringLiteral("hello");
	hello["hostVersion"] = QStringLiteral("6.2.0");
	sendMessage(hello);
}

void CallForgeBridge::onSocketDisconnected() {
	qWarning() << "[CallForgeBridge] Socket disconnected";
	if (mDaemonConnected) {
		mDaemonConnected = false;
		emit daemonConnectedChanged();
	}
}

void CallForgeBridge::onSocketError(QLocalSocket::LocalSocketError error) {
	qWarning() << "[CallForgeBridge] Socket error:" << error << mSocket->errorString();
}

void CallForgeBridge::onReadyRead() {
	mReadBuffer.append(mSocket->readAll());

	while (mReadBuffer.size() >= FRAME_HEADER_SIZE) {
		uint8_t frameType = static_cast<uint8_t>(mReadBuffer.at(0));
		uint32_t len = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar *>(mReadBuffer.constData() + 1));

		if (static_cast<size_t>(mReadBuffer.size()) < FRAME_HEADER_SIZE + len) break;

		QByteArray payload = mReadBuffer.mid(FRAME_HEADER_SIZE, len);
		mReadBuffer.remove(0, FRAME_HEADER_SIZE + len);

		if (frameType == FRAME_TYPE_JSON) {
			QJsonParseError parseErr;
			QJsonDocument doc = QJsonDocument::fromJson(payload, &parseErr);
			if (doc.isNull()) {
				qWarning() << "[CallForgeBridge] JSON parse error:" << parseErr.errorString();
				continue;
			}
			handleMessage(doc.object());
		}
	}
}

bool CallForgeBridge::sendMessage(const QJsonObject &msg) {
	if (!mSocket || mSocket->state() != QLocalSocket::ConnectedState) return false;

	QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
	uint8_t frameType = FRAME_TYPE_JSON;
	uint32_t netLen = qToBigEndian<uint32_t>(static_cast<uint32_t>(payload.size()));

	mSocket->write(reinterpret_cast<const char *>(&frameType), 1);
	mSocket->write(reinterpret_cast<const char *>(&netLen), 4);
	mSocket->write(payload);
	mSocket->flush();
	return true;
}

void CallForgeBridge::handleMessage(const QJsonObject &msg) {
	QString event = msg["event"].toString();

	if (event == QStringLiteral("ready")) {
		QString daemonVersion = msg["daemonVersion"].toString();
		int protocolVersion = msg["protocolVersion"].toInt();
		qInfo() << "[CallForgeBridge] Daemon ready — version:" << daemonVersion << "protocol:" << protocolVersion;

		// Parse settings tabs (QML strings from daemon)
		QJsonArray tabs = msg["settingsTabs"].toArray();
		mSettingsTabs.clear();
		for (const auto &tab : tabs) {
			QVariantMap tabMap;
			tabMap["title"] = tab.toObject()["title"].toString();
			tabMap["qml"] = tab.toObject()["qml"].toString();
			mSettingsTabs.append(tabMap);
		}
		qInfo() << "[CallForgeBridge] Received" << mSettingsTabs.size() << "settings tabs from daemon";

		mDaemonConnected = true;
		emit daemonConnectedChanged();
		emit settingsTabsChanged();

		// Request initial data
		sendMessage({{"cmd", "getAgents"}});
		sendMessage({{"cmd", "getScenarios"}});

	} else if (event == QStringLiteral("agents")) {
		mAgents = msg["agents"].toArray();
		mAgentNames.clear();
		for (const auto &name : msg["agentNames"].toArray())
			mAgentNames.append(name.toString());
		qInfo() << "[CallForgeBridge] Agents updated:" << mAgents.size();
		emit aiAgentsChanged();

	} else if (event == QStringLiteral("scenarios")) {
		mScenarios = msg["scenarios"].toArray();
		qInfo() << "[CallForgeBridge] Scenarios updated:" << mScenarios.size();
		emit aiScenariosChanged();

	} else if (event == QStringLiteral("agentTestResult")) {
		bool success = msg["success"].toBool();
		QString message = msg["message"].toString();
		qInfo() << "[CallForgeBridge] Agent test:" << success << message;
		emit aiAgentTestResult(success, message);

	} else if (event == QStringLiteral("error")) {
		qWarning() << "[CallForgeBridge] Daemon error:" << msg["message"].toString();
	}
}
