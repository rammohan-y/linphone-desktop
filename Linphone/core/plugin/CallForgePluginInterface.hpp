// CallForge Plugin Interface — shared between host and plugins.
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

#ifndef CALLFORGE_PLUGIN_INTERFACE_HPP
#define CALLFORGE_PLUGIN_INTERFACE_HPP

#include <QHash>
#include <QString>
#include <QVariantList>

class CallForgeHostContext;
class QObject;

class CallForgePluginInterface {
public:
	virtual ~CallForgePluginInterface() = default;

	virtual QString pluginName() const = 0;
	virtual QString pluginVersion() const = 0;

	virtual bool initialize(CallForgeHostContext *hostContext) = 0;
	virtual void shutdown() = 0;

	virtual QVariantList settingsTabs() const {
		return {};
	}
	virtual QVariantList callPanels() const {
		return {};
	}
	virtual QVariantList callPageActions() const {
		return {};
	}
	virtual QVariantList moreOptionsEntries() const {
		return {};
	}

	virtual QHash<QString, QObject *> qmlContextObjects() const {
		return {};
	}
};

#define CallForgePluginInterface_iid "org.callforge.PluginInterface/1.0"
Q_DECLARE_INTERFACE(CallForgePluginInterface, CallForgePluginInterface_iid)

#endif
