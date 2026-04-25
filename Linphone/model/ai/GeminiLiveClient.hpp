#ifndef GEMINI_LIVE_CLIENT_H_
#define GEMINI_LIVE_CLIENT_H_

#include <QByteArray>
#include <QObject>
#include <QString>

class QWebSocket;

class GeminiLiveClient : public QObject {
	Q_OBJECT
public:
	explicit GeminiLiveClient(QObject *parent = nullptr);
	~GeminiLiveClient();

public slots:
	void connectToGemini(QString apiKey, QString model, QString voice, QString language, QString systemPrompt);
	void disconnectFromGemini();
	void sendAudioChunk(QByteArray pcmData);

signals:
	void connected();
	void disconnected(QString reason);
	void error(QString message);
	void audioResponseChunk(QByteArray pcmData);
	void textResponseChunk(QString text);
	void inputTranscription(QString text);
	void outputTranscription(QString text);
	void turnComplete(QByteArray fullAudioResponse);

private slots:
	void onWsConnected();
	void onWsDisconnected();
	void onWsTextMessage(const QString &message);
	void onWsBinaryMessage(const QByteArray &message);
	void onWsError();

private:
	QWebSocket *mWebSocket = nullptr;
	QString mModel;
	QString mVoice;
	QString mLanguage;
	QString mSystemPrompt;
	QByteArray mTurnAudioBuffer;
	bool mBidiReady = false;
	QByteArray mPendingInputPcm;
	qint64 mDbgPcmBeforeBidiReady = 0;
	qint64 mDbgPcmSentOnWire = 0;

	void sendSetupMessage();
	void sendPcmPayload(const QByteArray &pcmData);
	void appendPendingInput(const QByteArray &pcmData);
	void flushPendingInput();
};

#endif
