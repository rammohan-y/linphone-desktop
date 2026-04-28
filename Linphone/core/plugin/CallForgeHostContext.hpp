// CallForge Host Context — shared between host and plugins.
// Copyright (c) 2026 Rammohan Yadavalli.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this file, to deal in it without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of this file.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

#ifndef CALLFORGE_HOST_CONTEXT_HPP
#define CALLFORGE_HOST_CONTEXT_HPP

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace CallForge {
namespace CallState {
static constexpr int Idle = 0;
static constexpr int IncomingReceived = 1;
static constexpr int PushIncomingReceived = 2;
static constexpr int OutgoingInit = 3;
static constexpr int OutgoingProgress = 4;
static constexpr int OutgoingRinging = 5;
static constexpr int OutgoingEarlyMedia = 6;
static constexpr int Connected = 7;
static constexpr int StreamsRunning = 8;
static constexpr int Pausing = 9;
static constexpr int Paused = 10;
static constexpr int Resuming = 11;
static constexpr int Referred = 12;
static constexpr int Error = 13;
static constexpr int End = 14;
static constexpr int PausedByRemote = 15;
static constexpr int UpdatedByRemote = 16;
static constexpr int IncomingEarlyMedia = 17;
static constexpr int Updating = 18;
static constexpr int Released = 19;
static constexpr int EarlyUpdatedByRemote = 20;
static constexpr int EarlyUpdating = 21;
} // namespace CallState
} // namespace CallForge

class CallHandle : public QObject {
	Q_OBJECT

	Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
	Q_PROPERTY(bool microphoneMuted READ isMicrophoneMuted NOTIFY microphoneMutedChanged)
	Q_PROPERTY(QString remoteName READ remoteName NOTIFY remoteNameChanged)

public:
	explicit CallHandle(QObject *parent = nullptr) : QObject(parent) {
	}
	virtual ~CallHandle() = default;

	virtual bool isActive() const = 0;
	virtual QString remoteName() const = 0;
	virtual bool isMicrophoneMuted() const = 0;

	virtual void *audioStreamPtr() = 0;

	virtual void playFileToRemote(const QString &wavPath) = 0;
	virtual void stopFilePlay() = 0;

	virtual void setMicrophoneMuted(bool muted) = 0;

	virtual int audioSampleRate() const = 0;

	virtual void startMixedRecordToFile(const QString &path) = 0;
	virtual void stopMixedRecord() = 0;

signals:
	void activeChanged();
	void microphoneMutedChanged();
	void remoteNameChanged();
	void stateChanged(int state);
	void filePlayFinished();
};

class CallForgeHostContext : public QObject {
	Q_OBJECT

public:
	explicit CallForgeHostContext(QObject *parent = nullptr) : QObject(parent) {
	}
	virtual ~CallForgeHostContext() = default;

	virtual CallHandle *currentCallHandle() = 0;

	virtual void runOnSdkThread(std::function<void()> fn) = 0;
	virtual void runOnUiThread(std::function<void()> fn) = 0;

	virtual QString
	configGetString(const QString &section, const QString &key, const QString &defaultValue = {}) const = 0;
	virtual void configSetString(const QString &section, const QString &key, const QString &value) = 0;
	virtual int configGetInt(const QString &section, const QString &key, int defaultValue = 0) const = 0;
	virtual void configSetInt(const QString &section, const QString &key, int value) = 0;

	virtual void registerSettingsTab(const QString &title, const QUrl &qmlUrl) = 0;
	virtual void registerCallPanel(const QString &id, const QString &title, const QUrl &qmlUrl) = 0;
	virtual void registerMoreOptionsEntry(const QString &title, const QString &iconSource, const QUrl &qmlUrl) = 0;
	virtual void registerCallPageAction(const QString &title, const QString &iconSource, const QUrl &qmlUrl) = 0;

	virtual void log(const QString &message) = 0;
	virtual void logWarning(const QString &message) = 0;
	virtual void logError(const QString &message) = 0;

	virtual QString appVersion() const = 0;
	virtual QString pluginDataDir(const QString &pluginName) const = 0;

signals:
	void currentCallChanged();
	void callStateChanged(int newState);
};

#endif
