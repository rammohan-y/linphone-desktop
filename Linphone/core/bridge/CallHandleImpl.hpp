/*
 * Copyright (c) 2026 CallForge Contributors.
 *
 * This file is part of CallForge (based on linphone-desktop).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CALL_HANDLE_IMPL_HPP
#define CALL_HANDLE_IMPL_HPP

#include <QObject>
#include <QSharedPointer>

class CallCore;

class CallHandleImpl : public QObject {
	Q_OBJECT

public:
	explicit CallHandleImpl(QSharedPointer<CallCore> callCore, QObject *parent = nullptr);
	~CallHandleImpl();

	bool isActive() const;
	QString remoteName() const;
	bool isMicrophoneMuted() const;

	void *audioStreamPtr();

	void playFileToRemote(const QString &wavPath);
	void stopFilePlay();

	void setMicrophoneMuted(bool muted);

	int audioSampleRate() const;

	void startMixedRecordToFile(const QString &path);
	void stopMixedRecord();

	QSharedPointer<CallCore> callCore() const;

signals:
	void stateChanged(int state);
	void filePlayFinished();

private:
	QSharedPointer<CallCore> mCallCore;
	QMetaObject::Connection mStateConn;
	QMetaObject::Connection mPlayFinishedConn;
	int mCachedSampleRate = 0;
};

#endif
