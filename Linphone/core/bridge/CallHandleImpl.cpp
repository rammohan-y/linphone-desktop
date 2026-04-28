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

#include "CallHandleImpl.hpp"
#include "core/call/CallCore.hpp"
#include "model/call/CallModel.hpp"
#include "model/core/CoreModel.hpp"

#include <linphone++/linphone.hh>
#include <linphone/types.h>
#include <mediastreamer2/mediastream.h>

extern "C" {
LINPHONE_PUBLIC MediaStream *linphone_call_get_stream(LinphoneCall *call, LinphoneStreamType type);
}

CallHandleImpl::CallHandleImpl(QSharedPointer<CallCore> callCore, QObject *parent)
    : QObject(parent), mCallCore(callCore) {
	if (mCallCore) {
		mStateConn = connect(mCallCore.get(), &CallCore::stateChanged, this,
		                     [this](LinphoneEnums::CallState s) { emit stateChanged(static_cast<int>(s)); });
		mPlayFinishedConn =
		    connect(mCallCore.get(), &CallCore::filePlayFinished, this, &CallHandleImpl::filePlayFinished);
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
