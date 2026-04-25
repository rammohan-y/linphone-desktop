#ifndef CAPTURE_FILE_POLLER_H_
#define CAPTURE_FILE_POLLER_H_

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QTimer>

// Reads a growing PCM WAV in a dedicated thread (QTimer) so the GUI thread is
// never starved by QML (e.g. rightPanel.replace) while callees are speaking.
class CaptureFilePoller : public QObject {
	Q_OBJECT
public:
	explicit CaptureFilePoller(QObject *parent = nullptr);

public slots:
	void startCapture(const QString &filePath);
	void stopCapture();
	/// Stops the poll timer but keeps the file open (while Gemini is playing TTS).
	void pauseCapture();
	/// Seeks to EOF and restarts the poll (after TTS to avoid echoing old file data).
	void resumeCapture();

signals:
	void pcmReady(const QByteArray &data);

private slots:
	void onTimeout();

private:
	void ensureTimerInThisThread();
	void openIfNeeded();

	QPointer<QTimer> mTimer;
	QString mPath;
	QFile mFile;
	qint64 mReadPos = 0;
	bool mWavHeaderParsed = false;
	bool mDbgLoggedFirstPcm = false;
	qint64 mDbgSessionPcmBytes = 0;
};

#endif
