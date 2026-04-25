#include "CaptureFilePoller.hpp"

#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTimer>

#include <QDebug>

CaptureFilePoller::CaptureFilePoller(QObject *parent) : QObject(parent) {
}

void CaptureFilePoller::ensureTimerInThisThread() {
	if (mTimer) return;
	if (QThread::currentThread() != thread()) {
		qWarning() << "[CaptureFilePoller] ensureTimer on wrong thread";
		return;
	}
	mTimer = new QTimer(this);
	mTimer->setInterval(50);
	mTimer->setTimerType(Qt::PreciseTimer);
	connect(mTimer, &QTimer::timeout, this, &CaptureFilePoller::onTimeout);
}

void CaptureFilePoller::startCapture(const QString &filePath) {
	ensureTimerInThisThread();
	stopCapture();
	mDbgLoggedFirstPcm = false;
	mDbgSessionPcmBytes = 0;
	mPath = filePath;
	mReadPos = 0;
	mWavHeaderParsed = false;
	mFile.close();
	if (!mPath.isEmpty()) mFile.setFileName(mPath);
	if (mTimer) {
		mTimer->start();
		QTimer::singleShot(0, this, &CaptureFilePoller::onTimeout);
	}
}

void CaptureFilePoller::stopCapture() {
	if (mTimer) mTimer->stop();
	if (mDbgSessionPcmBytes > 0) {
		qInfo() << "[AICall-Debug] poller_stop session_pcm_read_bytes:" << mDbgSessionPcmBytes;
	}
	mFile.close();
	mPath.clear();
	mReadPos = 0;
	mWavHeaderParsed = false;
	mDbgLoggedFirstPcm = false;
	mDbgSessionPcmBytes = 0;
}

void CaptureFilePoller::pauseCapture() {
	if (mTimer) mTimer->stop();
}

void CaptureFilePoller::resumeCapture() {
	ensureTimerInThisThread();
	if (mFile.isOpen() && mWavHeaderParsed) {
		mReadPos = mFile.size();
	} else {
		mReadPos = 0;
		mWavHeaderParsed = false;
	}
	if (mTimer) mTimer->start();
}

void CaptureFilePoller::openIfNeeded() {
	if (mFile.isOpen()) return;
	if (mPath.isEmpty() || !QFileInfo::exists(mPath)) return;
	mFile.setFileName(mPath);
	if (!mFile.open(QIODevice::ReadOnly)) {
		qWarning() << "[CaptureFilePoller] Cannot open" << mPath;
		return;
	}
	mReadPos = 0;
	mWavHeaderParsed = false;
	qInfo() << "[AICall] Capture file opened (poller thread)";
}

void CaptureFilePoller::onTimeout() {
	if (mPath.isEmpty()) return;

	if (!mFile.isOpen()) {
		if (!QFile::exists(mPath)) return;
		openIfNeeded();
		if (!mFile.isOpen()) return;
	}

	if (!mWavHeaderParsed) {
		if (mFile.size() < 44) return;
		mWavHeaderParsed = true;
		mReadPos = 44;
		mFile.seek(44);
		qInfo() << "[AICall] Skipping WAV header, reading PCM from offset 44, file size:" << mFile.size();
	}

	const qint64 available = mFile.size() - mReadPos;
	if (available <= 0) return;
	mFile.seek(mReadPos);
	const QByteArray newData = mFile.read(available);
	mReadPos += newData.size();
	if (!newData.isEmpty()) {
		mDbgSessionPcmBytes += newData.size();
		if (!mDbgLoggedFirstPcm) {
			mDbgLoggedFirstPcm = true;
			qInfo() << "[AICall-Debug] first_pcm_from_file bytes:" << newData.size() << "read_pos_after:" << mReadPos
			        << "file_size:" << mFile.size();
		}
		emit pcmReady(newData);
	}
}
