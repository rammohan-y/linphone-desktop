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

#ifndef CALLFORGE_HOST_CONTEXT_IMPL_HPP
#define CALLFORGE_HOST_CONTEXT_IMPL_HPP

#include "CallForgeHostContext.hpp"

#include <QSharedPointer>

class App;
class CallCore;
class CallList;

class CallHandleImpl : public CallHandle {
	Q_OBJECT

public:
	explicit CallHandleImpl(QSharedPointer<CallCore> callCore, QObject *parent = nullptr);
	~CallHandleImpl();

	bool isActive() const override;
	QString remoteName() const override;
	bool isMicrophoneMuted() const override;

	void *audioStreamPtr() override;

	void playFileToRemote(const QString &wavPath) override;
	void stopFilePlay() override;

	void setMicrophoneMuted(bool muted) override;

	int audioSampleRate() const override;

	void startMixedRecordToFile(const QString &path) override;
	void stopMixedRecord() override;

	QSharedPointer<CallCore> callCore() const;

private:
	QSharedPointer<CallCore> mCallCore;
	QMetaObject::Connection mStateConn;
	QMetaObject::Connection mPlayFinishedConn;
	int mCachedSampleRate = 0;
};

class CallForgeHostContextImpl : public CallForgeHostContext {
	Q_OBJECT

public:
	explicit CallForgeHostContextImpl(QObject *parent = nullptr);
	~CallForgeHostContextImpl();

	CallHandle *currentCallHandle() override;

	void runOnSdkThread(std::function<void()> fn) override;
	void runOnUiThread(std::function<void()> fn) override;

	QString
	configGetString(const QString &section, const QString &key, const QString &defaultValue = {}) const override;
	void configSetString(const QString &section, const QString &key, const QString &value) override;
	int configGetInt(const QString &section, const QString &key, int defaultValue = 0) const override;
	void configSetInt(const QString &section, const QString &key, int value) override;

	void registerSettingsTab(const QString &title, const QUrl &qmlUrl) override;
	void registerCallPanel(const QString &id, const QString &title, const QUrl &qmlUrl) override;
	void registerMoreOptionsEntry(const QString &title, const QString &iconSource, const QUrl &qmlUrl) override;
	void registerCallPageAction(const QString &title, const QString &iconSource, const QUrl &qmlUrl) override;

	void log(const QString &message) override;
	void logWarning(const QString &message) override;
	void logError(const QString &message) override;

	QString appVersion() const override;
	QString pluginDataDir(const QString &pluginName) const override;

private:
	void onCallListAvailable();
	void onCurrentCallChanged();

	CallHandleImpl *mCurrentCallHandle = nullptr;
	QMetaObject::Connection mAppConn;
	QMetaObject::Connection mCallListConn;
};

#endif
