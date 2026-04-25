#ifndef AI_CALL_CONTROLLER_H_
#define AI_CALL_CONTROLLER_H_

#include "tool/LinphoneEnums.hpp"

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QSharedPointer>
#include <QThread>

#include <functional>

class CallCore;
class CaptureFilePoller;
class GeminiLiveClient;

class AICallController : public QObject {
	Q_OBJECT

	Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
	Q_PROPERTY(bool armed READ isArmed NOTIFY armedChanged)
	Q_PROPERTY(bool geminiReady READ isGeminiReady NOTIFY geminiReadyChanged)
	Q_PROPERTY(int armedScenarioIndex READ getArmedScenarioIndex NOTIFY armedChanged)
	Q_PROPERTY(QString transcript READ getTranscript NOTIFY transcriptChanged)
	Q_PROPERTY(QString status READ getStatus NOTIFY statusChanged)

public:
	explicit AICallController(QObject *parent = nullptr);
	~AICallController();

	Q_INVOKABLE void startAICall(int scenarioIndex);
	Q_INVOKABLE void stopAICall();
	Q_INVOKABLE void armAICall(int scenarioIndex);
	Q_INVOKABLE void disarmAICall();

	bool isActive() const;
	bool isArmed() const;
	bool isGeminiReady() const;
	int getArmedScenarioIndex() const;
	QString getTranscript() const;
	QString getStatus() const;

signals:
	void activeChanged();
	void armedChanged();
	void geminiReadyChanged();
	void transcriptChanged();
	void statusChanged();

	void requestConnect(QString apiKey, QString model, QString voice, QString language, QString systemPrompt);
	void requestDisconnect();

private slots:
	void onGeminiConnected();
	void onGeminiDisconnected(QString reason);
	void onGeminiError(QString message);
	void onGeminiTextResponse(QString text);
	void onGeminiAudioChunk(QByteArray pcmData);
	void onGeminiTurnComplete(QByteArray fullAudioResponse);

private:
	void setStatus(const QString &status);
	void appendTranscript(const QString &speaker, const QString &text);
	QByteArray resample24kTo16k(const QByteArray &pcm24k);
	QByteArray downsample48kTo16k(const QByteArray &pcm48k);
	QString writeResponseWav(const QByteArray &pcm16k);
	void writeWavHeader(QFile &file, int sampleRate, int channels, int dataSize);
	void cleanup();
	void cleanupGemini(std::function<void()> afterTeardown = {});
	void cleanupGeminiBlocking();
	void setupGeminiClient(std::function<void()> afterThreadStarted = {});
	void startAudioCapture();
	void setMicMute(bool mute);
	void onCallStateChanged(LinphoneEnums::CallState state);
	void onGeminiReadyForCall();
	void resumeAfterPlayback();

	int mArmedScenarioIndex = -1;
	bool mGeminiReady = false;
	QMetaObject::Connection mCallStateConnection;
	QMetaObject::Connection mCallCoreStateConnection;

	QThread *mCaptureThread = nullptr;
	CaptureFilePoller *mCapturePoller = nullptr;
	QMetaObject::Connection mPcmToGemini;

	QThread *mGeminiThread = nullptr;
	GeminiLiveClient *mGeminiClient = nullptr;

	QString mCaptureFilePath;

	int mResponseFileCounter = 0;
	QString mTempDir;
	QByteArray mStreamChunkBuffer;
	bool mStreaming = false;
	bool mMicWasMuted = false;
	bool mDidStartMixedRecord = false;
	bool mLocalPlayDone = false;
	bool mRemotePlayDone = false;
	QMetaObject::Connection mRemotePlayEofConnection;

	QString mTranscript;
	QString mStatus;
	bool mActive = false;
	bool mDestructing = false;

	QSharedPointer<CallCore> mCallCore;
	qint64 mCallStartTime = 0;
	QMetaObject::Connection mCallEndConnection;
};

#endif
