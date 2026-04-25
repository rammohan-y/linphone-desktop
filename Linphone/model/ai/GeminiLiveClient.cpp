#include "GeminiLiveClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>

GeminiLiveClient::GeminiLiveClient(QObject *parent) : QObject(parent) {
}

GeminiLiveClient::~GeminiLiveClient() {
	disconnectFromGemini();
}

void GeminiLiveClient::connectToGemini(
    QString apiKey, QString model, QString voice, QString language, QString systemPrompt) {
	// Perf/debug kill-switch: completely disable WebSocket opens to isolate call/UI lag from Gemini.
	// Enable by launching with: LINPHONE_AI_DISABLE_GEMINI=1
	if (qEnvironmentVariableIsSet("LINPHONE_AI_DISABLE_GEMINI")) {
		qWarning() << "[GeminiLive] Disabled by env LINPHONE_AI_DISABLE_GEMINI=1 (not opening WebSocket)";
		mModel = model;
		mVoice = voice;
		mLanguage = language;
		mSystemPrompt = systemPrompt;
		mTurnAudioBuffer.clear();
		mBidiReady = false;
		mPendingInputPcm.clear();
		mDbgPcmBeforeBidiReady = 0;
		mDbgPcmSentOnWire = 0;
		emit error("Gemini disabled by LINPHONE_AI_DISABLE_GEMINI=1");
		emit disconnected("Gemini disabled by env");
		return;
	}

	mModel = model;
	mVoice = voice;
	mLanguage = language;
	mSystemPrompt = systemPrompt;
	mTurnAudioBuffer.clear();
	mBidiReady = false;
	mPendingInputPcm.clear();
	mDbgPcmBeforeBidiReady = 0;
	mDbgPcmSentOnWire = 0;

	if (mWebSocket) {
		mWebSocket->close();
		delete mWebSocket;
	}

	mWebSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

	connect(mWebSocket, &QWebSocket::connected, this, &GeminiLiveClient::onWsConnected);
	connect(mWebSocket, &QWebSocket::disconnected, this, &GeminiLiveClient::onWsDisconnected);
	connect(mWebSocket, &QWebSocket::textMessageReceived, this, &GeminiLiveClient::onWsTextMessage);
	connect(mWebSocket, &QWebSocket::binaryMessageReceived, this, &GeminiLiveClient::onWsBinaryMessage);
	connect(mWebSocket, &QWebSocket::errorOccurred, this, &GeminiLiveClient::onWsError);

	QString url = QString("wss://generativelanguage.googleapis.com/ws/"
	                      "google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent?key=%1")
	                  .arg(apiKey);

	qInfo() << "[GeminiLive] Connecting to" << url.left(120) + "...";
	mWebSocket->open(QUrl(url));
}

void GeminiLiveClient::disconnectFromGemini() {
	if (mDbgPcmSentOnWire > 0 || !mPendingInputPcm.isEmpty() || mDbgPcmBeforeBidiReady > 0) {
		qInfo() << "[GeminiLive-Debug] disconnect pcm_sent_on_wire_bytes:" << mDbgPcmSentOnWire
		        << "pending_discarded_bytes:" << mPendingInputPcm.size()
		        << "received_before_bidi_ready_bytes:" << mDbgPcmBeforeBidiReady;
	}
	mTurnAudioBuffer.clear();
	mBidiReady = false;
	mPendingInputPcm.clear();
	if (mWebSocket) {
		mWebSocket->disconnect(this);
		mWebSocket->abort();
		delete mWebSocket;
		mWebSocket = nullptr;
	}
}

void GeminiLiveClient::appendPendingInput(const QByteArray &pcmData) {
	static const int kMaxPending = 2 * 1024 * 1024;
	mPendingInputPcm.append(pcmData);
	if (mPendingInputPcm.size() > kMaxPending) {
		qWarning() << "[GeminiLive] Input PCM pending buffer overflow, trimming oldest" << mPendingInputPcm.size();
		mPendingInputPcm.remove(0, mPendingInputPcm.size() - kMaxPending);
	}
}

void GeminiLiveClient::sendPcmPayload(const QByteArray &pcmData) {
	if (pcmData.isEmpty()) return;
	if (!mWebSocket || mWebSocket->state() != QAbstractSocket::ConnectedState) {
		appendPendingInput(pcmData);
		return;
	}

	QString base64 = QString::fromLatin1(pcmData.toBase64());

	QJsonObject audio;
	audio["mimeType"] = "audio/pcm;rate=16000";
	audio["data"] = base64;

	QJsonObject realtimeInput;
	realtimeInput["audio"] = audio;

	QJsonObject msg;
	msg["realtimeInput"] = realtimeInput;

	mWebSocket->sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
	mDbgPcmSentOnWire += pcmData.size();
}

void GeminiLiveClient::flushPendingInput() {
	while (!mPendingInputPcm.isEmpty() && mBidiReady && mWebSocket &&
	       mWebSocket->state() == QAbstractSocket::ConnectedState) {
		const int chunk = qMin(32 * 1024, mPendingInputPcm.size());
		QByteArray part = mPendingInputPcm.left(chunk);
		mPendingInputPcm.remove(0, chunk);
		sendPcmPayload(part);
	}
}

void GeminiLiveClient::sendAudioChunk(QByteArray pcmData) {
	if (pcmData.isEmpty()) return;
	if (!mBidiReady) {
		mDbgPcmBeforeBidiReady += pcmData.size();
		appendPendingInput(pcmData);
		return;
	}
	if (!mWebSocket || mWebSocket->state() != QAbstractSocket::ConnectedState) {
		appendPendingInput(pcmData);
		return;
	}
	sendPcmPayload(pcmData);
}

void GeminiLiveClient::onWsConnected() {
	qInfo() << "[GeminiLive] WebSocket connected, sending setup message";
	sendSetupMessage();
}

void GeminiLiveClient::onWsDisconnected() {
	QString reason = mWebSocket ? mWebSocket->closeReason() : "unknown";
	int code = mWebSocket ? mWebSocket->closeCode() : -1;
	qInfo() << "[GeminiLive] WebSocket disconnected, code:" << code << "reason:" << reason;
	emit disconnected(reason.isEmpty() ? QString("Disconnected (code %1)").arg(code) : reason);
}

void GeminiLiveClient::onWsTextMessage(const QString &message) {
	QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
	if (!doc.isObject()) return;

	QJsonObject root = doc.object();

	if (root.contains("setupComplete")) {
		qInfo() << "[GeminiLive] Setup complete, ready for audio";
		qInfo() << "[GeminiLive-Debug] setupComplete pending_pcm_bytes:" << mPendingInputPcm.size()
		        << "arrived_before_bidi_ready_bytes:" << mDbgPcmBeforeBidiReady;
		mBidiReady = true;
		flushPendingInput();
		qInfo() << "[GeminiLive-Debug] after_flush pending_remaining_bytes:" << mPendingInputPcm.size()
		        << "pcm_sent_on_wire_total_bytes:" << mDbgPcmSentOnWire;
		emit connected();
		return;
	}

	QJsonObject serverContent = root["serverContent"].toObject();
	if (!serverContent.isEmpty()) {
		bool isTurnComplete = serverContent["turnComplete"].toBool(false);
		bool isGenComplete = serverContent["generationComplete"].toBool(false);

		if (serverContent.contains("inputTranscription")) {
			QString text = serverContent["inputTranscription"].toObject()["text"].toString();
			if (!text.isEmpty()) emit inputTranscription(text);
		}

		if (serverContent.contains("outputTranscription")) {
			QString text = serverContent["outputTranscription"].toObject()["text"].toString();
			if (!text.isEmpty()) emit outputTranscription(text);
		}

		QJsonObject modelTurn = serverContent["modelTurn"].toObject();
		if (!modelTurn.isEmpty()) {
			QJsonArray parts = modelTurn["parts"].toArray();
			for (const QJsonValue &partVal : parts) {
				QJsonObject part = partVal.toObject();

				if (part.contains("text")) {
					QString text = part["text"].toString();
					emit textResponseChunk(text);
				}

				if (part.contains("inlineData")) {
					QJsonObject inlineData = part["inlineData"].toObject();
					QByteArray audioBytes = QByteArray::fromBase64(inlineData["data"].toString().toLatin1());
					mTurnAudioBuffer.append(audioBytes);
					emit audioResponseChunk(audioBytes);
				}
			}
		}

		if (isTurnComplete || isGenComplete) {
			qInfo() << "[GeminiLive] Turn complete, audio buffer size:" << mTurnAudioBuffer.size();
			QByteArray completeTurnAudio = mTurnAudioBuffer;
			mTurnAudioBuffer.clear();
			emit turnComplete(completeTurnAudio);
		}
	}
}

void GeminiLiveClient::onWsBinaryMessage(const QByteArray &message) {
	onWsTextMessage(QString::fromUtf8(message));
}

void GeminiLiveClient::onWsError() {
	QString errMsg = mWebSocket ? mWebSocket->errorString() : "Unknown error";
	qWarning() << "[GeminiLive] WebSocket error:" << errMsg;
	emit error(errMsg);
}

void GeminiLiveClient::sendSetupMessage() {
	QJsonObject setup;
	setup["model"] = "models/" + mModel;

	QJsonArray modalities;
	modalities.append("AUDIO");

	QJsonObject generationConfig;
	generationConfig["responseModalities"] = modalities;
	generationConfig["temperature"] = 0.8;

	if (!mVoice.isEmpty()) {
		QJsonObject voiceConfig;
		voiceConfig["voiceName"] = mVoice;

		QJsonObject prebuiltVoice;
		prebuiltVoice["prebuiltVoiceConfig"] = voiceConfig;

		QJsonObject speechConfig;
		speechConfig["voiceConfig"] = prebuiltVoice;

		generationConfig["speechConfig"] = speechConfig;
	}

	setup["generationConfig"] = generationConfig;

	if (!mSystemPrompt.isEmpty()) {
		QJsonObject textPart;
		textPart["text"] = mSystemPrompt;

		QJsonArray parts;
		parts.append(textPart);

		QJsonObject systemInstruction;
		systemInstruction["parts"] = parts;
		setup["systemInstruction"] = systemInstruction;
	}

	QJsonObject msg;
	msg["setup"] = setup;

	QString json = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
	qInfo() << "[GeminiLive] Setup sent for model:" << mModel;
	mWebSocket->sendTextMessage(json);
}
