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

#include "CallForgePluginLoader.hpp"
#include "CallForgeHostContext.hpp"
#include "CallForgePluginInterface.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QPluginLoader>
#include <QQmlContext>
#include <QQmlEngine>

CallForgePluginLoader *CallForgePluginLoader::sInstance = nullptr;

CallForgePluginLoader::CallForgePluginLoader(QObject *parent) : QObject(parent) {
	sInstance = this;
}

CallForgePluginLoader::~CallForgePluginLoader() {
	unloadAll();
	if (sInstance == this) sInstance = nullptr;
}

CallForgePluginLoader *CallForgePluginLoader::getInstance() {
	return sInstance;
}

QString CallForgePluginLoader::pluginSearchPath() const {
	return QCoreApplication::applicationDirPath() + "/plugins";
}

void CallForgePluginLoader::discoverAndLoad(CallForgeHostContext *hostContext) {
	mHostContext = hostContext;

	QString path = pluginSearchPath();
	QDir dir(path);
	qInfo() << "[PluginLoader] Scanning for plugins in:" << path;

	if (!dir.exists()) {
		qInfo() << "[PluginLoader] Plugin directory does not exist — no plugins loaded.";
		return;
	}

	for (const QString &fileName : dir.entryList(QDir::Files)) {
		if (!QLibrary::isLibrary(fileName)) continue;

		QString filePath = dir.absoluteFilePath(fileName);
		auto *loader = new QPluginLoader(filePath, this);

		if (!loader->load()) {
			qWarning() << "[PluginLoader] Failed to load" << fileName << ":" << loader->errorString();
			delete loader;
			continue;
		}

		QObject *instance = loader->instance();
		auto *plugin = qobject_cast<CallForgePluginInterface *>(instance);

		if (!plugin) {
			qWarning() << "[PluginLoader]" << fileName << "is not a CallForgePluginInterface";
			loader->unload();
			delete loader;
			continue;
		}

		qInfo() << "[PluginLoader] Found plugin:" << plugin->pluginName() << "v" << plugin->pluginVersion();

		if (!plugin->initialize(hostContext)) {
			qWarning() << "[PluginLoader] Plugin" << plugin->pluginName() << "failed to initialize";
			loader->unload();
			delete loader;
			continue;
		}

		LoadedPlugin lp;
		lp.loader = loader;
		lp.interface = plugin;
		lp.name = plugin->pluginName();
		lp.version = plugin->pluginVersion();
		mPlugins.append(lp);

		mSettingsTabs.append(plugin->settingsTabs());
		mCallPanels.append(plugin->callPanels());
		mMoreOptionsEntries.append(plugin->moreOptionsEntries());
		mCallPageActions.append(plugin->callPageActions());

		qInfo() << "[PluginLoader] Loaded:" << lp.name << "v" << lp.version;
	}

	qInfo() << "[PluginLoader] Total plugins loaded:" << mPlugins.size() << "settings tabs:" << mSettingsTabs.size()
	        << "call panels:" << mCallPanels.size() << "more-options:" << mMoreOptionsEntries.size()
	        << "call-page actions:" << mCallPageActions.size();

	emit pluginsChanged();
}

void CallForgePluginLoader::registerQmlContextObjects(QQmlEngine *engine) {
	if (!engine) return;
	QQmlContext *rootContext = engine->rootContext();
	for (const auto &lp : mPlugins) {
		if (!lp.interface) continue;
		auto objects = lp.interface->qmlContextObjects();
		for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
			qInfo() << "[PluginLoader] Registering QML context property:" << it.key() << "from plugin:" << lp.name;
			rootContext->setContextProperty(it.key(), it.value());
		}
	}
}

void CallForgePluginLoader::unloadAll() {
	for (auto &lp : mPlugins) {
		if (lp.interface) {
			lp.interface->shutdown();
		}
		if (lp.loader) {
			lp.loader->unload();
			delete lp.loader;
		}
	}
	mPlugins.clear();
	mSettingsTabs.clear();
	mCallPanels.clear();
	mMoreOptionsEntries.clear();
	mCallPageActions.clear();
	emit pluginsChanged();
}

QVariantList CallForgePluginLoader::getPluginSettingsTabs() const {
	return mSettingsTabs;
}

QVariantList CallForgePluginLoader::getPluginCallPanels() const {
	return mCallPanels;
}

QVariantList CallForgePluginLoader::getPluginMoreOptionsEntries() const {
	return mMoreOptionsEntries;
}

QVariantList CallForgePluginLoader::getPluginCallPageActions() const {
	return mCallPageActions;
}

int CallForgePluginLoader::getPluginCount() const {
	return mPlugins.size();
}
