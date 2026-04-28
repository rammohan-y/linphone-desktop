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

#ifndef CALLFORGE_PLUGIN_LOADER_HPP
#define CALLFORGE_PLUGIN_LOADER_HPP

#include <QObject>
#include <QPluginLoader>
#include <QVariantList>
#include <QVariantMap>

class CallForgeHostContext;
class CallForgePluginInterface;
class QQmlEngine;

struct LoadedPlugin {
	QPluginLoader *loader = nullptr;
	CallForgePluginInterface *interface = nullptr;
	QString name;
	QString version;
};

class CallForgePluginLoader : public QObject {
	Q_OBJECT

	Q_PROPERTY(QVariantList pluginSettingsTabs READ getPluginSettingsTabs NOTIFY pluginsChanged)
	Q_PROPERTY(QVariantList pluginCallPanels READ getPluginCallPanels NOTIFY pluginsChanged)
	Q_PROPERTY(QVariantList pluginMoreOptionsEntries READ getPluginMoreOptionsEntries NOTIFY pluginsChanged)
	Q_PROPERTY(QVariantList pluginCallPageActions READ getPluginCallPageActions NOTIFY pluginsChanged)
	Q_PROPERTY(int pluginCount READ getPluginCount NOTIFY pluginsChanged)

public:
	explicit CallForgePluginLoader(QObject *parent = nullptr);
	~CallForgePluginLoader();

	void discoverAndLoad(CallForgeHostContext *hostContext);
	void registerQmlContextObjects(QQmlEngine *engine);
	void unloadAll();

	QVariantList getPluginSettingsTabs() const;
	QVariantList getPluginCallPanels() const;
	QVariantList getPluginMoreOptionsEntries() const;
	QVariantList getPluginCallPageActions() const;
	int getPluginCount() const;

	static CallForgePluginLoader *getInstance();

signals:
	void pluginsChanged();

private:
	QString pluginSearchPath() const;

	QList<LoadedPlugin> mPlugins;
	CallForgeHostContext *mHostContext = nullptr;

	QVariantList mSettingsTabs;
	QVariantList mCallPanels;
	QVariantList mMoreOptionsEntries;
	QVariantList mCallPageActions;

	static CallForgePluginLoader *sInstance;
};

#endif
