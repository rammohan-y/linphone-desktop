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

#include "CallForgeHostContextImpl.hpp"
#include "core/App.hpp"
#include "core/call/CallCore.hpp"
#include "core/call/CallList.hpp"
#include "model/call/CallModel.hpp"
#include "model/core/CoreModel.hpp"
#include "tool/Constants.hpp"

#include <QDir>
#include <QStandardPaths>

#include <linphone++/linphone.hh>
#include <linphone/types.h>
#include <mediastreamer2/mediastream.h>

extern "C" {
LINPHONE_PUBLIC MediaStream *linphone_call_get_stream(LinphoneCall *call, LinphoneStreamType type);
}

// --- CallHandleImpl ---

CallHandleImpl::CallHandleImpl(QSharedPointer<CallCore> callCore, QObject *parent)
    : CallHandle(parent), mCallCore(callCore) {
	if (mCallCore) {
		mStateConn = connect(mCallCore.get(), &CallCore::stateChanged, this, [this](LinphoneEnums::CallState s) {
			emit stateChanged(static_cast<int>(s));
			if (s == LinphoneEnums::CallState::End || s == LinphoneEnums::CallState::Error ||
			    s == LinphoneEnums::CallState::Released) {
				emit activeChanged();
			}
		});
		mPlayFinishedConn = connect(mCallCore.get(), &CallCore::filePlayFinished, this, &CallHandle::filePlayFinished);
	}
}

CallHandleImpl::~CallHandleImpl() {
	if (mStateConn) disconnect(mStateConn);
	if (mPlayFinishedConn) disconnect(mPlayFinishedConn);
}

bool CallHandleImpl::isActive() const {
	if (!mCallCore) return false;
	auto s = mCallCore->getState();
	return s == LinphoneEnums::CallState::StreamsRunning || s == LinphoneEnums::CallState::Connected ||
	       s == LinphoneEnums::CallState::Paused || s == LinphoneEnums::CallState::PausedByRemote;
}

QString CallHandleImpl::remoteName() const {
	return mCallCore ? mCallCore->property("remoteName").toString() : QString();
}

bool CallHandleImpl::isMicrophoneMuted() const {
	return mCallCore ? mCallCore->getMicrophoneMuted() : false;
}

void *CallHandleImpl::audioStreamPtr() {
	if (!mCallCore) return nullptr;
	void *result = nullptr;
	auto callModel = mCallCore->getModel();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [callModel, &result]() {
		    auto call = callModel->getMonitor();
		    if (!call) return;
		    LinphoneCall *cCall = call->cPtr();
		    result = reinterpret_cast<void *>(linphone_call_get_stream(cCall, LinphoneStreamTypeAudio));
	    },
	    Qt::BlockingQueuedConnection);
	return result;
}

void CallHandleImpl::playFileToRemote(const QString &wavPath) {
	if (mCallCore) emit mCallCore->lPlayFile(wavPath);
}

void CallHandleImpl::stopFilePlay() {
	if (mCallCore) emit mCallCore->lStopFilePlay();
}

void CallHandleImpl::setMicrophoneMuted(bool muted) {
	if (!mCallCore) return;
	auto callModel = mCallCore->getModel();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(), [callModel, muted]() { callModel->setMicrophoneMuted(muted); },
	    Qt::QueuedConnection);
}

int CallHandleImpl::audioSampleRate() const {
	if (mCachedSampleRate > 0) return mCachedSampleRate;
	if (!mCallCore) return 0;

	int rate = 0;
	auto callModel = mCallCore->getModel();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [callModel, &rate]() {
		    auto call = callModel->getMonitor();
		    if (!call) return;
		    LinphoneCall *cCall = call->cPtr();
		    AudioStream *stream =
		        reinterpret_cast<AudioStream *>(linphone_call_get_stream(cCall, LinphoneStreamTypeAudio));
		    if (stream) rate = stream->sample_rate;
	    },
	    Qt::BlockingQueuedConnection);

	const_cast<CallHandleImpl *>(this)->mCachedSampleRate = rate;
	return rate;
}

void CallHandleImpl::startMixedRecordToFile(const QString &path) {
	if (!mCallCore) return;
	auto callModel = mCallCore->getModel();
	std::string pathStr = path.toStdString();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [callModel, pathStr]() {
		    auto call = callModel->getMonitor();
		    if (!call) return;
		    LinphoneCall *cCall = call->cPtr();
		    AudioStream *stream =
		        reinterpret_cast<AudioStream *>(linphone_call_get_stream(cCall, LinphoneStreamTypeAudio));
		    if (!stream) return;
		    int ret = audio_stream_set_mixed_record_file(stream, pathStr.c_str());
		    if (ret == 0) audio_stream_mixed_record_start(stream);
	    },
	    Qt::BlockingQueuedConnection);
}

void CallHandleImpl::stopMixedRecord() {
	if (!mCallCore) return;
	auto callModel = mCallCore->getModel();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [callModel]() {
		    auto call = callModel->getMonitor();
		    if (!call) return;
		    LinphoneCall *cCall = call->cPtr();
		    AudioStream *stream =
		        reinterpret_cast<AudioStream *>(linphone_call_get_stream(cCall, LinphoneStreamTypeAudio));
		    if (stream) audio_stream_mixed_record_stop(stream);
	    },
	    Qt::BlockingQueuedConnection);
}

QSharedPointer<CallCore> CallHandleImpl::callCore() const {
	return mCallCore;
}

// --- CallForgeHostContextImpl ---

CallForgeHostContextImpl::CallForgeHostContextImpl(QObject *parent) : CallForgeHostContext(parent) {
	auto *app = App::getInstance();
	if (app->getCallList()) {
		qInfo() << "[PluginHost] CallList already exists at init, connecting now";
		onCallListAvailable();
	} else {
		qInfo() << "[PluginHost] CallList not yet created, waiting for App::callsChanged";
		mAppConn = connect(app, &App::callsChanged, this, &CallForgeHostContextImpl::onCallListAvailable);
	}
}

CallForgeHostContextImpl::~CallForgeHostContextImpl() {
	if (mAppConn) disconnect(mAppConn);
	if (mCallListConn) disconnect(mCallListConn);
	delete mCurrentCallHandle;
}

CallHandle *CallForgeHostContextImpl::currentCallHandle() {
	auto callList = App::getInstance()->getCallList();
	if (!callList) return nullptr;

	auto callCore = callList->getCurrentCallCore();
	if (!callCore) {
		delete mCurrentCallHandle;
		mCurrentCallHandle = nullptr;
		return nullptr;
	}

	if (mCurrentCallHandle && mCurrentCallHandle->callCore() == callCore) {
		return mCurrentCallHandle;
	}

	delete mCurrentCallHandle;
	mCurrentCallHandle = new CallHandleImpl(callCore, this);
	return mCurrentCallHandle;
}

void CallForgeHostContextImpl::onCallListAvailable() {
	qInfo() << "[PluginHost] onCallListAvailable() called";
	if (mAppConn) {
		disconnect(mAppConn);
		mAppConn = {};
	}
	auto callList = App::getInstance()->getCallList();
	if (!callList) {
		qWarning() << "[PluginHost] onCallListAvailable but getCallList() is null!";
		return;
	}
	if (mCallListConn) {
		qInfo() << "[PluginHost] Already connected to CallList, skipping";
		return;
	}
	mCallListConn =
	    connect(callList.get(), &CallList::currentCallChanged, this, &CallForgeHostContextImpl::onCurrentCallChanged);
	qInfo() << "[PluginHost] Connected to CallList::currentCallChanged signal, connection valid:"
	        << bool(mCallListConn);
}

void CallForgeHostContextImpl::onCurrentCallChanged() {
	auto callList = App::getInstance()->getCallList();
	auto callCore = callList ? callList->getCurrentCallCore() : nullptr;
	qInfo() << "[PluginHost] onCurrentCallChanged() — callCore exists:" << bool(callCore);
	delete mCurrentCallHandle;
	mCurrentCallHandle = nullptr;
	emit currentCallChanged();
	qInfo() << "[PluginHost] emitted currentCallChanged signal";
}

void CallForgeHostContextImpl::runOnSdkThread(std::function<void()> fn) {
	QMetaObject::invokeMethod(CoreModel::getInstance().get(), std::move(fn), Qt::QueuedConnection);
}

void CallForgeHostContextImpl::runOnUiThread(std::function<void()> fn) {
	QMetaObject::invokeMethod(App::getInstance(), std::move(fn), Qt::QueuedConnection);
}

QString CallForgeHostContextImpl::configGetString(const QString &section,
                                                  const QString &key,
                                                  const QString &defaultValue) const {
	QString result = defaultValue;
	std::string sec = section.toStdString();
	std::string k = key.toStdString();
	std::string def = defaultValue.toStdString();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [sec, k, def, &result]() {
		    auto core = CoreModel::getInstance()->getCore();
		    if (!core) return;
		    auto config = core->getConfig();
		    if (!config) return;
		    result = QString::fromStdString(config->getString(sec, k, def));
	    },
	    Qt::BlockingQueuedConnection);
	return result;
}

void CallForgeHostContextImpl::configSetString(const QString &section, const QString &key, const QString &value) {
	std::string sec = section.toStdString();
	std::string k = key.toStdString();
	std::string v = value.toStdString();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [sec, k, v]() {
		    auto core = CoreModel::getInstance()->getCore();
		    if (!core) return;
		    auto config = core->getConfig();
		    if (!config) return;
		    config->setString(sec, k, v);
	    },
	    Qt::BlockingQueuedConnection);
}

int CallForgeHostContextImpl::configGetInt(const QString &section, const QString &key, int defaultValue) const {
	int result = defaultValue;
	std::string sec = section.toStdString();
	std::string k = key.toStdString();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [sec, k, defaultValue, &result]() {
		    auto core = CoreModel::getInstance()->getCore();
		    if (!core) return;
		    auto config = core->getConfig();
		    if (!config) return;
		    result = config->getInt(sec, k, defaultValue);
	    },
	    Qt::BlockingQueuedConnection);
	return result;
}

void CallForgeHostContextImpl::configSetInt(const QString &section, const QString &key, int value) {
	std::string sec = section.toStdString();
	std::string k = key.toStdString();
	QMetaObject::invokeMethod(
	    CoreModel::getInstance().get(),
	    [sec, k, value]() {
		    auto core = CoreModel::getInstance()->getCore();
		    if (!core) return;
		    auto config = core->getConfig();
		    if (!config) return;
		    config->setInt(sec, k, value);
	    },
	    Qt::BlockingQueuedConnection);
}

void CallForgeHostContextImpl::registerSettingsTab(const QString &title, const QUrl &qmlUrl) {
	qInfo() << "[PluginHost] registerSettingsTab:" << title << qmlUrl;
}

void CallForgeHostContextImpl::registerCallPanel(const QString &id, const QString &title, const QUrl &qmlUrl) {
	qInfo() << "[PluginHost] registerCallPanel:" << id << title << qmlUrl;
}

void CallForgeHostContextImpl::registerMoreOptionsEntry(const QString &title,
                                                        const QString &iconSource,
                                                        const QUrl &qmlUrl) {
	qInfo() << "[PluginHost] registerMoreOptionsEntry:" << title << iconSource << qmlUrl;
}

void CallForgeHostContextImpl::registerCallPageAction(const QString &title,
                                                      const QString &iconSource,
                                                      const QUrl &qmlUrl) {
	qInfo() << "[PluginHost] registerCallPageAction:" << title << iconSource << qmlUrl;
}

void CallForgeHostContextImpl::log(const QString &message) {
	qInfo() << "[Plugin]" << message;
}

void CallForgeHostContextImpl::logWarning(const QString &message) {
	qWarning() << "[Plugin]" << message;
}

void CallForgeHostContextImpl::logError(const QString &message) {
	qCritical() << "[Plugin]" << message;
}

QString CallForgeHostContextImpl::appVersion() const {
	return QCoreApplication::applicationVersion();
}

QString CallForgeHostContextImpl::pluginDataDir(const QString &pluginName) const {
	QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins/" + pluginName;
	QDir().mkpath(dir);
	return dir;
}
