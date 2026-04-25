#include "AICallController.hpp"
#include "core/App.hpp"
#include "core/ai/CaptureFilePoller.hpp"
#include "core/call/CallCore.hpp"
#include "core/call/CallList.hpp"
#include "core/path/Paths.hpp"
#include "model/ai/GeminiLiveClient.hpp"
#include "model/call/CallModel.hpp"
#include "model/core/CoreModel.hpp"
#include "tool/LinphoneEnums.hpp"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>

#include <utility>

#include <fstream>
#include <malloc.h>
#include <string>

#include <linphone/types.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msaudiomixer.h>

extern "C" {
LINPHONE_PUBLIC MediaStream *linphone_call_get_stream(LinphoneCall *call, LinphoneStreamType type);
}

static inline bool aiEnvOn(const char *name) {
	return qEnvironmentVariableIsSet(name) && qEnvironmentVariableIntValue(name) != 0;
}

static void logResourceUsage(const QString &label) {
	long rssKb = 0;
	int threadCount = 0;
	int fdCount = 0;

	std::ifstream statm("/proc/self/statm");
	if (statm.is_open()) {
		long pages = 0;
		statm >> pages >> pages;
		rssKb = pages * 4;
	}

	std::ifstream status("/proc/self/status");
	if (status.is_open()) {
		std::string line;
		while (std::getline(status, line)) {
			if (line.find("Threads:") == 0) {
				threadCount = std::stoi(line.substr(8));
				break;
			}
		}
	}

	QDir fdDir("/proc/self/fd");
	fdCount = fdDir.entryList(QDir::NoDotAndDotDot).size();

	qInfo() << "[AICall-Resource]" << label << "RSS:" << rssKb << "KB  threads:" << threadCount << "FDs:" << fdCount
	        << "QThread::idealCount:" << QThread::idealThreadCount();
}

AICallController::AICallController(QObject *parent) : QObject(parent) {
	mCaptureThread = new QThread(this);
	mCapturePoller = new CaptureFilePoller();
	mCapturePoller->moveToThread(mCaptureThread);
	mCaptureThread->start();

	QDir tmp("/tmp");
	for (const QString &name : tmp.entryList({"linphone_ai_*"}, QDir::Dirs)) {
		QDir("/tmp/" + name).removeRecursively();
	}
}

AICallController::~AICallController() {
	mDestructing = true;
	cleanup();
	if (mCapturePoller) {
		QMetaObject::invokeMethod(mCapturePoller, "stopCapture", Qt::BlockingQueuedConnection);
	}
	if (mCaptureThread) {
		mCaptureThread->quit();
		mCaptureThread->wait(2000);
		delete mCapturePoller;
		mCapturePoller = nullptr;
	}
}

bool AICallController::isActive() const {
	return mActive;
}

bool AICallController::isArmed() const {
	return mArmedScenarioIndex >= 0;
}

bool AICallController::isGeminiReady() const {
	return mGeminiReady;
}

int AICallController::getArmedScenarioIndex() const {
	return mArmedScenarioIndex;
}

void AICallController::armAICall(int scenarioIndex) {
	if (mActive) {
		qWarning() << "[AICall] Already active, cannot arm";
		return;
	}
	if (aiEnvOn("LINPHONE_AI_NOOP")) {
		qWarning() << "[AICall] LINPHONE_AI_NOOP=1 (AI call does nothing)";
		mArmedScenarioIndex = scenarioIndex;
		mGeminiReady = false;
		emit armedChanged();
		emit geminiReadyChanged();
		setStatus("AI disabled by LINPHONE_AI_NOOP=1");
		return;
	}
	if (mArmedScenarioIndex >= 0) disarmAICall();

	auto settings = App::getInstance()->getSettings();
	if (!settings) {
		setStatus("Error: Settings not available");
		return;
	}

	QVariantList scenarios = settings->getAiScenarios();
	if (scenarioIndex < 0 || scenarioIndex >= scenarios.size()) {
		setStatus("Error: Invalid scenario index");
		return;
	}

	QVariantMap scenario = scenarios[scenarioIndex].toMap();
	int agentIndex = scenario["agentIndex"].toInt();
	QString systemPrompt = scenario["systemPrompt"].toString();

	QVariantList agents = settings->getAiAgents();
	if (agentIndex < 0 || agentIndex >= agents.size()) {
		setStatus("Error: Invalid agent index in scenario");
		return;
	}

	QVariantMap agent = agents[agentIndex].toMap();
	QString apiKey = agent["apiKey"].toString();
	QString model = agent["model"].toString();
	QString voice = agent["voice"].toString();
	QString language = agent["language"].toString();

	if (apiKey.isEmpty() || model.isEmpty()) {
		setStatus("Error: Agent API key or model is empty");
		return;
	}

	mArmedScenarioIndex = scenarioIndex;
	mGeminiReady = false;
	mTranscript.clear();
	emit armedChanged();
	emit geminiReadyChanged();
	emit transcriptChanged();

	logResourceUsage("before-arm");
	setStatus("Connecting to Gemini…");

	// Pre-connect before dial so bidi is up early; do not miss callee audio at media start.
	setupGeminiClient([this, apiKey, model, voice, language, systemPrompt]() {
		emit requestConnect(apiKey, model, voice, language, systemPrompt);
	});
}

void AICallController::disarmAICall() {
	if (mArmedScenarioIndex < 0 && !mGeminiReady) return;
	mArmedScenarioIndex = -1;
	mGeminiReady = false;
	if (mCallStateConnection) {
		disconnect(mCallStateConnection);
		mCallStateConnection = {};
	}
	// Block until Gemini/WS is fully down so the next arm cannot race async teardown (2nd-call lag).
	cleanupGeminiBlocking();
	setStatus("");
	emit armedChanged();
	emit geminiReadyChanged();
	qInfo() << "[AICall] Disarmed";
}

void AICallController::setupGeminiClient(std::function<void()> afterThreadStarted) {
	auto finishSetup = [this, after = std::move(afterThreadStarted)]() {
		mGeminiThread = new QThread(this);
		mGeminiClient = new GeminiLiveClient();
		mGeminiClient->moveToThread(mGeminiThread);

		connect(mGeminiClient, &GeminiLiveClient::connected, this, &AICallController::onGeminiReadyForCall,
		        Qt::QueuedConnection);
		connect(mGeminiClient, &GeminiLiveClient::disconnected, this, &AICallController::onGeminiDisconnected,
		        Qt::QueuedConnection);
		connect(mGeminiClient, &GeminiLiveClient::error, this, &AICallController::onGeminiError, Qt::QueuedConnection);
		connect(mGeminiClient, &GeminiLiveClient::textResponseChunk, this, &AICallController::onGeminiTextResponse,
		        Qt::QueuedConnection);
		connect(
		    mGeminiClient, &GeminiLiveClient::inputTranscription, this,
		    [this](QString text) { appendTranscript("Caller", text); }, Qt::QueuedConnection);
		connect(
		    mGeminiClient, &GeminiLiveClient::outputTranscription, this,
		    [this](QString text) { appendTranscript("AI", text); }, Qt::QueuedConnection);
		connect(mGeminiClient, &GeminiLiveClient::audioResponseChunk, this, &AICallController::onGeminiAudioChunk,
		        Qt::QueuedConnection);
		connect(mGeminiClient, &GeminiLiveClient::turnComplete, this, &AICallController::onGeminiTurnComplete,
		        Qt::QueuedConnection);

		connect(this, &AICallController::requestConnect, mGeminiClient, &GeminiLiveClient::connectToGemini,
		        Qt::QueuedConnection);
		connect(this, &AICallController::requestDisconnect, mGeminiClient, &GeminiLiveClient::disconnectFromGemini,
		        Qt::QueuedConnection);

		mGeminiThread->start();
		logResourceUsage("gemini-thread-started");
		if (after) after();
	};

	if (!mGeminiThread && !mGeminiClient) {
		finishSetup();
		return;
	}
	cleanupGemini(std::move(finishSetup));
}

void AICallController::cleanupGeminiBlocking() {
	if (!mGeminiThread) return;

	if (mGeminiClient) {
		mGeminiClient->disconnect(this);
		this->disconnect(mGeminiClient);
		QMetaObject::invokeMethod(mGeminiClient, &GeminiLiveClient::disconnectFromGemini, Qt::BlockingQueuedConnection);
	}

	mGeminiThread->quit();
	mGeminiThread->wait(3000);

	delete mGeminiClient;
	mGeminiClient = nullptr;
	delete mGeminiThread;
	mGeminiThread = nullptr;
	logResourceUsage("gemini-thread-destroyed");
}

void AICallController::cleanupGemini(std::function<void()> afterTeardown) {
	if (mDestructing) {
		cleanupGeminiBlocking();
		if (afterTeardown) afterTeardown();
		return;
	}
	if (!mGeminiThread && !mGeminiClient) {
		if (afterTeardown) afterTeardown();
		return;
	}

	GeminiLiveClient *client = mGeminiClient;
	QThread *thread = mGeminiThread;
	mGeminiClient = nullptr;
	mGeminiThread = nullptr;

	if (client) {
		client->disconnect(this);
		this->disconnect(client);
	}

	QThread *joiner = QThread::create([this, client, thread, after = std::move(afterTeardown)]() {
		if (client) {
			QMetaObject::invokeMethod(client, "disconnectFromGemini", Qt::BlockingQueuedConnection);
		}
		thread->quit();
		thread->wait(3000);
		QMetaObject::invokeMethod(
		    this,
		    [this, client, thread, after]() {
			    delete client;
			    delete thread;
			    logResourceUsage("gemini-thread-destroyed");
			    if (after) after();
		    },
		    Qt::QueuedConnection);
	});
	connect(joiner, &QThread::finished, joiner, &QThread::deleteLater);
	joiner->start();
}

void AICallController::onGeminiReadyForCall() {
	if (mArmedScenarioIndex < 0) return;
	mGeminiReady = true;
	emit geminiReadyChanged();
	setStatus("Gemini ready — dial a number to start AI call");
	qInfo() << "[AICall] Gemini connected and ready, waiting for call";

	auto callList = App::getInstance()->getCallList();
	if (!callList) return;

	if (mCallStateConnection) disconnect(mCallStateConnection);

	mCallStateConnection = connect(callList.get(), &CallList::currentCallChanged, this, [this]() {
		if (mActive || mArmedScenarioIndex < 0) return;
		auto callList = App::getInstance()->getCallList();
		if (!callList) return;
		auto callCore = callList->getCurrentCallCore();
		if (!callCore) return;

		disconnect(mCallCoreStateConnection);
		if (callCore->getState() == LinphoneEnums::CallState::StreamsRunning) {
			onCallStateChanged(LinphoneEnums::CallState::StreamsRunning);
			return;
		}
		mCallCoreStateConnection =
		    connect(callCore.get(), &CallCore::stateChanged, this, &AICallController::onCallStateChanged);
	});
}

void AICallController::onCallStateChanged(LinphoneEnums::CallState state) {
	qInfo() << "[AICall] Call state changed:" << static_cast<int>(state) << "armed:" << mArmedScenarioIndex
	        << "geminiReady:" << mGeminiReady << "active:" << mActive;
	if (mActive || mArmedScenarioIndex < 0 || !mGeminiReady) return;
	if (state == LinphoneEnums::CallState::StreamsRunning) {
		qInfo() << "[AICall] Call connected — starting audio capture";
		disconnect(mCallCoreStateConnection);
		mCallCoreStateConnection = {};
		if (mCallStateConnection) {
			disconnect(mCallStateConnection);
			mCallStateConnection = {};
		}
		startAudioCapture();
	}
}

void AICallController::startAudioCapture() {
	if (mActive) return;
	auto callList = App::getInstance()->getCallList();
	if (!callList) return;
	auto callCore = callList->getCurrentCallCore();
	if (!callCore) return;

	// Feature flags: allow disabling AI features one-by-one to isolate 2nd-call degradation.
	if (aiEnvOn("LINPHONE_AI_DISABLE_CAPTURE")) {
		qWarning() << "[AICall] LINPHONE_AI_DISABLE_CAPTURE=1 (not starting mixed-record or poller)";
		setStatus("AI active (capture disabled) — monitoring only");
		mActive = true;
		QMetaObject::invokeMethod(
		    this,
		    [this]() {
			    if (!aiEnvOn("LINPHONE_AI_DISABLE_UI")) emit activeChanged();
			    emit armedChanged();
		    },
		    Qt::QueuedConnection);
		return;
	}

	mCallEndConnection = connect(callCore.get(), &CallCore::stateChanged, this, [this](LinphoneEnums::CallState s) {
		if (s == LinphoneEnums::CallState::End || s == LinphoneEnums::CallState::Error ||
		    s == LinphoneEnums::CallState::Released) {
			qInfo() << "[AICall] Call ended (state" << static_cast<int>(s) << "), auto-stopping AI agent";
			stopAICall();
		}
	});

	mCallStartTime = QDateTime::currentMSecsSinceEpoch();
	mResponseFileCounter = 0;
	mArmedScenarioIndex = -1;

	mTempDir = "/tmp/linphone_ai_" + QString::number(QDateTime::currentMSecsSinceEpoch());
	QDir().mkpath(mTempDir);
	mCaptureFilePath = mTempDir + "/ai_capture.wav";
	qInfo() << "[AICall] Temp dir:" << mTempDir;

	logResourceUsage("capture-start");

	auto callModel = callCore->getModel();
	auto captureFilePath = mCaptureFilePath.toStdString();

	QElapsedTimer mixedRecordTimer;
	mixedRecordTimer.start();

	// Start capture *before* notifying QML (activeChanged). Otherwise
	// rightPanel.replace(aiAgentPanel) runs synchronously and blocks the main
	// thread, mixing doesn't run yet, and the start of the callee's speech
	// is not recorded — worse on each subsequent call as the UI gets heavier.
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [callModel, captureFilePath]() {
		    auto call = callModel->getMonitor();
		    if (!call) {
			    qWarning() << "[AICall] No call monitor in SDK thread";
			    return;
		    }
		    LinphoneCall *cCall = call->cPtr();
		    AudioStream *stream =
		        reinterpret_cast<AudioStream *>(linphone_call_get_stream(cCall, LinphoneStreamTypeAudio));
		    if (!stream) {
			    qWarning() << "[AICall] No audio stream on call";
			    return;
		    }
		    qInfo() << "[AICall] AudioStream sample_rate:" << stream->sample_rate << "nchannels:" << stream->nchannels
		            << "features:" << stream->features;
		    int ret = audio_stream_set_mixed_record_file(stream, captureFilePath.c_str());
		    qInfo() << "[AICall] set_mixed_record_file returned:" << ret;
		    if (ret == 0) {
			    ret = audio_stream_mixed_record_start(stream);
			    qInfo() << "[AICall] mixed_record_start returned:" << ret
			            << "file:" << QString::fromStdString(captureFilePath);
			    // Disable outbound_mixer output to the recorder so only callee audio
			    // (recv_tee) is captured. Without this, the call player's audio flows
			    // through outbound_mixer into the capture file, causing Gemini to hear
			    // its own responses and role-switch.
			    if (ret == 0 && stream->outbound_mixer) {
				    MSAudioMixerCtl mctl = {0};
				    mctl.pin = 1;
				    mctl.param.enabled = FALSE;
				    ms_filter_call_method(stream->outbound_mixer, MS_AUDIO_MIXER_ENABLE_OUTPUT, &mctl);
				    qInfo() << "[AICall] Disabled outbound_mixer output pin 1 (record-only callee audio)";
			    }
		    } else {
			    qWarning() << "[AICall] Failed to set record file, features:" << stream->features;
		    }
	    },
	    Qt::BlockingQueuedConnection);

	mDidStartMixedRecord = true;
	qInfo() << "[AICall-Debug] mixed_record_blocking_ms:" << mixedRecordTimer.elapsed()
	        << "thread:" << reinterpret_cast<quintptr>(QThread::currentThread());

	if (mPcmToGemini) {
		QObject::disconnect(mPcmToGemini);
		mPcmToGemini = {};
	}
	if (!aiEnvOn("LINPHONE_AI_DISABLE_POLLING") && mCapturePoller && mGeminiClient) {
		mPcmToGemini = connect(mCapturePoller, &CaptureFilePoller::pcmReady, mGeminiClient,
		                       &GeminiLiveClient::sendAudioChunk, Qt::QueuedConnection);
	}
	if (aiEnvOn("LINPHONE_AI_DISABLE_POLLING")) {
		qWarning() << "[AICall] LINPHONE_AI_DISABLE_POLLING=1 (mixed-record running, but no poller reads)";
	} else {
		QMetaObject::invokeMethod(mCapturePoller, "startCapture", Qt::QueuedConnection,
		                          Q_ARG(QString, mCaptureFilePath));
	}
	setStatus("AI Agent active — listening...");

	mActive = true;
	// Defer: QML must not run heavy replace() in the same stack as this slot.
	QMetaObject::invokeMethod(
	    this,
	    [this]() {
		    if (!aiEnvOn("LINPHONE_AI_DISABLE_UI")) emit activeChanged();
		    emit armedChanged();
	    },
	    Qt::QueuedConnection);
}

QString AICallController::getTranscript() const {
	return mTranscript;
}

QString AICallController::getStatus() const {
	return mStatus;
}

void AICallController::setStatus(const QString &status) {
	if (mStatus != status) {
		mStatus = status;
		qInfo() << "[AICall]" << status;
		emit statusChanged();
	}
}

void AICallController::appendTranscript(const QString &speaker, const QString &text) {
	qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - mCallStartTime;
	int secs = static_cast<int>(elapsed / 1000);
	QString timestamp = QString("%1:%2").arg(secs / 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0'));
	mTranscript += QString("[%1] %2: %3\n").arg(timestamp, speaker, text);
	emit transcriptChanged();
}

void AICallController::startAICall(int scenarioIndex) {
	if (mActive) {
		qWarning() << "[AICall] Already active, ignoring startAICall";
		return;
	}

	auto callList = App::getInstance()->getCallList();
	if (!callList || !callList->getCurrentCallCore()) {
		setStatus("Error: No active call");
		return;
	}

	auto settings = App::getInstance()->getSettings();
	if (!settings) {
		setStatus("Error: Settings not available");
		return;
	}

	QVariantList scenarios = settings->getAiScenarios();
	if (scenarioIndex < 0 || scenarioIndex >= scenarios.size()) {
		setStatus("Error: Invalid scenario index");
		return;
	}

	QVariantMap scenario = scenarios[scenarioIndex].toMap();
	int agentIndex = scenario["agentIndex"].toInt();
	QString systemPrompt = scenario["systemPrompt"].toString();

	QVariantList agents = settings->getAiAgents();
	if (agentIndex < 0 || agentIndex >= agents.size()) {
		setStatus("Error: Invalid agent index in scenario");
		return;
	}

	QVariantMap agent = agents[agentIndex].toMap();
	QString apiKey = agent["apiKey"].toString();
	QString model = agent["model"].toString();
	QString voice = agent["voice"].toString();
	QString language = agent["language"].toString();

	if (apiKey.isEmpty() || model.isEmpty()) {
		setStatus("Error: Agent API key or model is empty");
		return;
	}

	logResourceUsage("before-startAICall");
	mTranscript.clear();
	emit transcriptChanged();
	setStatus("Connecting to Gemini...");

	setupGeminiClient([this, apiKey, model, voice, language, systemPrompt]() {
		if (!mGeminiClient) return;
		disconnect(mGeminiClient, &GeminiLiveClient::connected, this, &AICallController::onGeminiReadyForCall);
		connect(mGeminiClient, &GeminiLiveClient::connected, this, &AICallController::onGeminiConnected,
		        Qt::QueuedConnection);
		emit requestConnect(apiKey, model, voice, language, systemPrompt);
	});
}

void AICallController::stopAICall() {
	if (!mActive) return;
	mActive = false;
	setStatus("Stopping AI call...");
	cleanup();
	emit activeChanged();
	setStatus("Stopped");
}

void AICallController::cleanup() {
	if (mPcmToGemini) {
		QObject::disconnect(mPcmToGemini);
		mPcmToGemini = {};
	}
	if (mCapturePoller) {
		QMetaObject::invokeMethod(mCapturePoller, "stopCapture", Qt::BlockingQueuedConnection);
	}
	mStreaming = false;
	mStreamChunkBuffer.clear();

	if (mRemotePlayEofConnection) {
		disconnect(mRemotePlayEofConnection);
		mRemotePlayEofConnection = {};
	}
	if (mCallEndConnection) {
		disconnect(mCallEndConnection);
		mCallEndConnection = {};
	}
	if (mCallCoreStateConnection) {
		disconnect(mCallCoreStateConnection);
		mCallCoreStateConnection = {};
	}
	if (mCallStateConnection) {
		disconnect(mCallStateConnection);
		mCallStateConnection = {};
	}

	auto callList = App::getInstance()->getCallList();
	if (callList) {
		auto callCore = callList->getCurrentCallCore();
		if (callCore) {
			emit callCore->lStopFilePlay();

			auto callModel = callCore->getModel();
			QMetaObject::invokeMethod(
			    CoreModel::getInstance().get(),
			    [callModel]() {
				    auto call = callModel->getMonitor();
				    if (!call) return;

				    LinphoneCall *cCall = call->cPtr();
				    AudioStream *stream =
				        reinterpret_cast<AudioStream *>(linphone_call_get_stream(cCall, LinphoneStreamTypeAudio));
				    if (stream) {
					    audio_stream_mixed_record_stop(stream);
					    qInfo() << "[AICall] mixed_record_stop called";
				    }
			    },
			    Qt::BlockingQueuedConnection);
		}
	}

	if (!aiEnvOn("LINPHONE_AI_DISABLE_MIC_MUTE")) setMicMute(false);

	// Synchronous: next call / re-arm must not overlap with a half-torn-down WS thread (causes 2nd+ call lag).
	cleanupGeminiBlocking();

	if (!mTempDir.isEmpty()) {
		QDir(mTempDir).removeRecursively();
		mTempDir.clear();
	}

	mStreamChunkBuffer.squeeze();
	mTranscript.squeeze();

	malloc_trim(0);
	logResourceUsage("after-cleanup");
}

void AICallController::onGeminiConnected() {
	startAudioCapture();
}

void AICallController::onGeminiDisconnected(QString reason) {
	// Do not gate on mGeminiClient: async teardown nulls it before this queued signal runs.
	if (mArmedScenarioIndex >= 0) {
		setStatus("Gemini disconnected: " + reason);
		mArmedScenarioIndex = -1;
		mGeminiReady = false;
		emit armedChanged();
		emit geminiReadyChanged();
	} else if (mActive) {
		setStatus("Disconnected from Gemini: " + reason);
		if (mCapturePoller) {
			QMetaObject::invokeMethod(mCapturePoller, "stopCapture", Qt::QueuedConnection);
		}
	}
}

void AICallController::onGeminiError(QString message) {
	setStatus("Error: " + message);
	if (mArmedScenarioIndex >= 0) {
		mArmedScenarioIndex = -1;
		mGeminiReady = false;
		emit armedChanged();
		emit geminiReadyChanged();
	}
}

void AICallController::onGeminiTextResponse(QString text) {
	appendTranscript("AI", text);
}

void AICallController::onGeminiAudioChunk(QByteArray pcmData) {
	if (!mActive || pcmData.isEmpty()) return;

	if (!mStreaming) {
		mStreaming = true;
		setMicMute(true);
	}

	QByteArray pcm16k = resample24kTo16k(pcmData);
	mStreamChunkBuffer.append(pcm16k);
}

void AICallController::onGeminiTurnComplete(QByteArray fullAudioResponse) {
	qInfo() << "[AICall] onGeminiTurnComplete, active:" << mActive << "buf:" << mStreamChunkBuffer.size();
	if (!mActive) {
		return;
	}

	mStreaming = false;

	if (mStreamChunkBuffer.isEmpty()) {
		setMicMute(false);
		return;
	}

	QString wavPath = writeResponseWav(mStreamChunkBuffer);
	mStreamChunkBuffer.clear();
	if (wavPath.isEmpty()) {
		setMicMute(false);
		return;
	}

	auto callList = App::getInstance()->getCallList();
	if (!callList) return;
	auto callCore = callList->getCurrentCallCore();
	if (!callCore) return;

	if (mRemotePlayEofConnection) {
		disconnect(mRemotePlayEofConnection);
		mRemotePlayEofConnection = {};
	}

	mRemotePlayEofConnection =
	    connect(callCore.get(), &CallCore::filePlayFinished, this, &AICallController::resumeAfterPlayback);

	qInfo() << "[AICall] playResponseToRemote via lPlayFile:" << wavPath;
	emit callCore->lPlayFile(wavPath);
}

void AICallController::resumeAfterPlayback() {
	qInfo() << "[AICall] Remote playback finished (filePlayFinished), resuming";
	if (mRemotePlayEofConnection) {
		disconnect(mRemotePlayEofConnection);
		mRemotePlayEofConnection = {};
	}
	if (mActive) {
		setMicMute(false);
	}
}

void AICallController::setMicMute(bool mute) {
	if (aiEnvOn("LINPHONE_AI_DISABLE_MIC_MUTE")) {
		qInfo() << "[AICall] LINPHONE_AI_DISABLE_MIC_MUTE=1 (skipping mic mute request)";
		return;
	}
	qInfo() << "[AICall] setMicMute:" << mute;
	auto callList = App::getInstance()->getCallList();
	if (!callList) return;
	auto callCore = callList->getCurrentCallCore();
	if (!callCore) return;
	auto callModel = callCore->getModel();

	if (mute) {
		mMicWasMuted = callCore->getMicrophoneMuted();
	}

	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(), [callModel, mute]() { callModel->setMicrophoneMuted(mute); },
	    Qt::QueuedConnection);

	if (!mute && mMicWasMuted) {
		QMetaObject::invokeMethod(
		    CoreModel::getInstance().get(), [callModel]() { callModel->setMicrophoneMuted(true); },
		    Qt::QueuedConnection);
	}
}

QByteArray AICallController::resample24kTo16k(const QByteArray &pcm24k) {
	int srcFrames = pcm24k.size() / 2;
	double ratio = 24000.0 / 16000.0;
	int dstFrames = static_cast<int>(srcFrames / ratio);
	if (dstFrames == 0) return {};

	const int16_t *src = reinterpret_cast<const int16_t *>(pcm24k.constData());
	QByteArray pcm16k(dstFrames * 2, Qt::Uninitialized);
	int16_t *dst = reinterpret_cast<int16_t *>(pcm16k.data());

	for (int i = 0; i < dstFrames; ++i) {
		double srcPos = i * ratio;
		int idx = static_cast<int>(srcPos);
		double frac = srcPos - idx;
		if (idx + 1 < srcFrames) {
			dst[i] = static_cast<int16_t>(src[idx] * (1.0 - frac) + src[idx + 1] * frac);
		} else {
			dst[i] = src[idx < srcFrames ? idx : srcFrames - 1];
		}
	}
	return pcm16k;
}

QString AICallController::writeResponseWav(const QByteArray &pcm16k) {
	if (pcm16k.isEmpty()) return {};

	QString path = QString("%1/response_%2.wav").arg(mTempDir).arg(mResponseFileCounter++);
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "[AICall] Cannot write response file:" << path;
		return {};
	}

	writeWavHeader(file, 16000, 1, pcm16k.size());
	file.write(pcm16k);
	file.close();
	return path;
}

void AICallController::writeWavHeader(QFile &file, int sampleRate, int channels, int dataSize) {
	int byteRate = sampleRate * channels * 2;
	int blockAlign = channels * 2;
	int fileSize = 36 + dataSize;

	auto writeU16 = [&](uint16_t v) {
		char buf[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
		file.write(buf, 2);
	};
	auto writeU32 = [&](uint32_t v) {
		char buf[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
		               static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
		file.write(buf, 4);
	};

	file.write("RIFF", 4);
	writeU32(fileSize);
	file.write("WAVE", 4);
	file.write("fmt ", 4);
	writeU32(16);
	writeU16(1);
	writeU16(static_cast<uint16_t>(channels));
	writeU32(static_cast<uint32_t>(sampleRate));
	writeU32(static_cast<uint32_t>(byteRate));
	writeU16(static_cast<uint16_t>(blockAlign));
	writeU16(16);
	file.write("data", 4);
	writeU32(static_cast<uint32_t>(dataSize));
}
