// Copyright (c) 2026 Rammohan Yadavalli. All rights reserved. Proprietary.
#include "CallForgeBridge.hpp"

#include "core/App.hpp"
#include "core/bridge/CallHandleImpl.hpp"
#include "core/call/CallCore.hpp"
#include "core/call/CallList.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QtEndian>

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msaudiomixer.h>

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

// --- Arm/Disarm ---

bool CallForgeBridge::isArmed() const {
	return mArmed;
}

int CallForgeBridge::getArmedScenarioIndex() const {
	return mArmedScenarioIndex;
}

QString CallForgeBridge::getArmedScenarioName() const {
	return mArmedScenarioName;
}

void CallForgeBridge::armAICall(int scenarioIndex) {
	mArmed = true;
	mArmedScenarioIndex = scenarioIndex;
	auto scenarios = mScenarios.toVariantList();
	if (scenarioIndex >= 0 && scenarioIndex < scenarios.size())
		mArmedScenarioName = scenarios[scenarioIndex].toMap()["name"].toString();
	else mArmedScenarioName.clear();
	mAiStatus = QStringLiteral("Armed — waiting for call");
	qInfo() << "[CallForgeBridge] Armed scenario:" << mArmedScenarioIndex << mArmedScenarioName;
	emit armedChanged();
	emit aiStateChanged();

	sendMessage({{"cmd", "armAICall"}, {"scenarioIndex", scenarioIndex}});

	auto *app = App::getInstance();
	auto callList = app ? app->getCallList() : nullptr;
	if (callList) {
		if (mCallListConn) disconnect(mCallListConn);
		mCallListConn = connect(callList.get(), &CallList::currentCallChanged, this, [this]() {
			if (!mArmed && !mAiActive) return;
			ensureCallHandle();
		});
		ensureCallHandle();
	}
}

void CallForgeBridge::disarmAICall() {
	mArmed = false;
	mArmedScenarioIndex = -1;
	mArmedScenarioName.clear();
	mAiActive = false;
	mAiStatus.clear();
	qInfo() << "[CallForgeBridge] Disarmed";
	emit armedChanged();
	emit aiStateChanged();

	sendMessage({{"cmd", "disarmAICall"}});
	releaseCallHandle();
}

QString CallForgeBridge::getCallPanelQml() const {
	return mCallPanelQml;
}

// --- AI call state ---

bool CallForgeBridge::isAiActive() const {
	return mAiActive;
}

QString CallForgeBridge::getAiStatus() const {
	return mAiStatus;
}

QString CallForgeBridge::getAiTranscript() const {
	return mAiTranscript;
}

void CallForgeBridge::startAICall() {
	if (!mArmed) return;
	mAiActive = true;
	mAiStatus = QStringLiteral("Connecting to AI...");
	mAiTranscript.clear();
	qInfo() << "[CallForgeBridge] AI call started for scenario:" << mArmedScenarioName;
	emit aiStateChanged();
	emit aiTranscriptChanged();

	QJsonObject msg;
	msg["cmd"] = QStringLiteral("startAICall");
	msg["scenarioIndex"] = mArmedScenarioIndex;
	sendMessage(msg);
}

void CallForgeBridge::stopAICall() {
	mAiActive = false;
	mAiStatus = QStringLiteral("AI stopped");
	qInfo() << "[CallForgeBridge] AI call stopped";
	emit aiStateChanged();

	sendMessage({{"cmd", "stopAICall"}});
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

		mCallPanelQml = msg["callPanelQml"].toString();
		qInfo() << "[CallForgeBridge] Call panel QML:" << (mCallPanelQml.isEmpty() ? "none" : "received");

		mDaemonConnected = true;
		emit daemonConnectedChanged();
		emit settingsTabsChanged();
		emit callPanelQmlChanged();

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

	} else if (event == QStringLiteral("aiStatus")) {
		mAiStatus = msg["status"].toString();
		mAiActive = msg["active"].toBool();
		qInfo() << "[CallForgeBridge] AI status:" << mAiActive << mAiStatus;
		emit aiStateChanged();

	} else if (event == QStringLiteral("aiTranscript")) {
		mAiTranscript = msg["transcript"].toString();
		emit aiTranscriptChanged();

	} else if (event == QStringLiteral("startCapture") || event == QStringLiteral("stopCapture") ||
	           event == QStringLiteral("playToRemote") || event == QStringLiteral("setMicMute")) {
		handleDaemonEvent(event, msg);

	} else if (event == QStringLiteral("error")) {
		qWarning() << "[CallForgeBridge] Daemon error:" << msg["message"].toString();
	}
}

// --- SDK proxy: execute daemon commands on the current call ---

void CallForgeBridge::ensureCallHandle() {
	auto *app = App::getInstance();
	auto callList = app ? app->getCallList() : nullptr;
	if (!callList) return;

	auto callCore = callList->getCurrentCallCore();
	if (!callCore) return;

	if (mCallHandle && mCallHandle->callCore() == callCore) return;

	releaseCallHandle();
	mCallHandle = new CallHandleImpl(callCore, this);

	mCallStateConn = connect(mCallHandle, &CallHandleImpl::stateChanged, this, &CallForgeBridge::onCallStateForwarded);

	if (mCallHandle->isActive()) {
		int sr = mCallHandle->audioSampleRate();
		qInfo() << "[CallForgeBridge] Call already active, forwarding StreamsRunning sampleRate=" << sr;
		sendMessage({{"cmd", "callStateChanged"}, {"state", 8}, {"sampleRate", sr}});
	}
}

void CallForgeBridge::releaseCallHandle() {
	if (mCallStateConn) {
		disconnect(mCallStateConn);
		mCallStateConn = {};
	}
	if (mPlayFinishedConn) {
		disconnect(mPlayFinishedConn);
		mPlayFinishedConn = {};
	}
	if (mCallListConn) {
		disconnect(mCallListConn);
		mCallListConn = {};
	}
	if (mCallHandle) {
		mCallHandle->stopFilePlay();
		mCallHandle->stopMixedRecord();
		mCallHandle->setMicrophoneMuted(false);
		mCallHandle->deleteLater();
		mCallHandle = nullptr;
	}
}

void CallForgeBridge::onCallStateForwarded(int state) {
	int sr = 0;
	if (state == 8 && mCallHandle) sr = mCallHandle->audioSampleRate();
	qInfo() << "[CallForgeBridge] Forwarding callState=" << state << "sampleRate=" << sr;
	sendMessage({{"cmd", "callStateChanged"}, {"state", state}, {"sampleRate", sr}});

	if (state == 14 || state == 13 || state == 19) {
		qInfo() << "[CallForgeBridge] Call ended (state=" << state << "), releasing call handle";
		releaseCallHandle();
	}
}

void CallForgeBridge::handleDaemonEvent(const QString &event, const QJsonObject &msg) {
	if (!mCallHandle) {
		qWarning() << "[CallForgeBridge] No call handle for daemon event:" << event;
		return;
	}

	if (event == QStringLiteral("startCapture")) {
		QString filePath = msg["filePath"].toString();
		qInfo() << "[CallForgeBridge] startCapture:" << filePath;

		mCallHandle->startMixedRecordToFile(filePath);

		// Disable outbound_mixer output pin so only callee audio is captured
		AudioStream *stream = reinterpret_cast<AudioStream *>(mCallHandle->audioStreamPtr());
		if (stream && stream->outbound_mixer) {
			MSAudioMixerCtl mctl = {0};
			mctl.pin = 1;
			mctl.param.enabled = FALSE;
			ms_filter_call_method(stream->outbound_mixer, MS_AUDIO_MIXER_ENABLE_OUTPUT, &mctl);
			qInfo() << "[CallForgeBridge] Disabled outbound_mixer output pin 1";
		}

		sendMessage({{"cmd", "captureStarted"}, {"filePath", filePath}});

	} else if (event == QStringLiteral("stopCapture")) {
		qInfo() << "[CallForgeBridge] stopCapture";
		mCallHandle->stopMixedRecord();

	} else if (event == QStringLiteral("playToRemote")) {
		QString filePath = msg["filePath"].toString();
		qInfo() << "[CallForgeBridge] playToRemote:" << filePath;

		if (mPlayFinishedConn) disconnect(mPlayFinishedConn);
		mPlayFinishedConn = connect(mCallHandle, &CallHandleImpl::filePlayFinished, this, [this]() {
			qInfo() << "[CallForgeBridge] Playback finished, notifying daemon";
			if (mPlayFinishedConn) {
				disconnect(mPlayFinishedConn);
				mPlayFinishedConn = {};
			}
			sendMessage({{"cmd", "playbackFinished"}});
		});

		mCallHandle->playFileToRemote(filePath);

	} else if (event == QStringLiteral("setMicMute")) {
		bool muted = msg["muted"].toBool();
		qInfo() << "[CallForgeBridge] setMicMute:" << muted;

		if (muted) {
			mMicWasMuted = mCallHandle->isMicrophoneMuted();
		}
		mCallHandle->setMicrophoneMuted(muted);
		if (!muted && mMicWasMuted) {
			mCallHandle->setMicrophoneMuted(true);
		}
	}
}
