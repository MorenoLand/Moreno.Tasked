#include "extensions_model.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>
#include <iterator>

namespace {
QString settingsId(QString id) { id.replace('/', '_'); id.replace('\\', '_'); return id; }
QString settingsPath(const QString &id, const QString &key) { return QStringLiteral("extensions/") + settingsId(id) + QStringLiteral("/settings/") + key; }
QVariant normalizeSetting(const QVariantMap &setting, QVariant value)
{
    const auto type = setting.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("bool")) return value.toBool();
    if (type == QStringLiteral("number")) {
        auto number = value.toDouble();
        if (setting.contains(QStringLiteral("min"))) number = (std::max)(number, setting.value(QStringLiteral("min")).toDouble());
        if (setting.contains(QStringLiteral("max"))) number = (std::min)(number, setting.value(QStringLiteral("max")).toDouble());
        return number;
    }
    if (type == QStringLiteral("enum")) {
        const auto options = setting.value(QStringLiteral("options")).toStringList();
        const auto selected = value.toString();
        return options.contains(selected) ? selected : (options.isEmpty() ? QString() : options.first());
    }
    return value.toString();
}
QVariantMap makeSetting(const QString &id, const QString &key, const QJsonObject &object)
{
    QVariantMap setting;
    const auto type = object.value(QStringLiteral("type")).toString(QStringLiteral("string"));
    setting.insert(QStringLiteral("key"), key);
    setting.insert(QStringLiteral("label"), object.value(QStringLiteral("label")).toString(key));
    setting.insert(QStringLiteral("type"), type);
    if (object.contains(QStringLiteral("min"))) setting.insert(QStringLiteral("min"), object.value(QStringLiteral("min")).toVariant());
    if (object.contains(QStringLiteral("max"))) setting.insert(QStringLiteral("max"), object.value(QStringLiteral("max")).toVariant());
    if (object.contains(QStringLiteral("step"))) setting.insert(QStringLiteral("step"), object.value(QStringLiteral("step")).toVariant());
    if (type == QStringLiteral("enum")) {
        QStringList options;
        for (const auto &option : object.value(QStringLiteral("options")).toArray()) options.append(option.toString());
        setting.insert(QStringLiteral("options"), options);
    }
    QVariant fallback = object.value(QStringLiteral("default")).toVariant();
    if (!object.contains(QStringLiteral("default"))) {
        if (type == QStringLiteral("bool")) fallback = false;
        else if (type == QStringLiteral("number")) fallback = 0.0;
        else if (type == QStringLiteral("enum")) fallback = setting.value(QStringLiteral("options")).toStringList().value(0);
    }
    setting.insert(QStringLiteral("value"), normalizeSetting(setting, QSettings().value(settingsPath(id, key), fallback)));
    return setting;
}
QVariantList makeSettings(const QString &id, const QJsonObject &manifest)
{
    QVariantList result;
    const auto schema = manifest.value(QStringLiteral("settings"));
    if (schema.isArray()) {
        for (const auto &value : schema.toArray()) {
            const auto object = value.toObject();
            const auto key = object.value(QStringLiteral("key")).toString();
            if (!key.isEmpty()) result.append(makeSetting(id, key, object));
        }
    } else if (schema.isObject()) {
        const auto object = schema.toObject();
        for (const auto &key : object.keys()) result.append(makeSetting(id, key, object.value(key).toObject()));
    }
    return result;
}
}

ExtensionModel::ExtensionModel(QObject *parent) : QAbstractListModel(parent) { refresh(); }
int ExtensionModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : items.size(); }
QVariant ExtensionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items.size()) return {};
    const auto &item = items.at(index.row());
    if (role == IdRole) return item.id;
    if (role == NameRole) return item.name;
    if (role == VersionRole) return item.version;
    if (role == DescriptionRole) return item.description;
    if (role == PathRole) return item.path;
    if (role == EnabledRole) return item.enabled;
    if (role == SettingsRole) return item.settings;
    return {};
}
QHash<int, QByteArray> ExtensionModel::roleNames() const { return {{IdRole, "id"}, {NameRole, "name"}, {VersionRole, "version"}, {DescriptionRole, "description"}, {PathRole, "path"}, {EnabledRole, "enabled"}, {SettingsRole, "settings"}}; }
QVector<ExtensionModel::Item> ExtensionModel::scan() const
{
    QVector<Item> result;
    const QDir root(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("extensions")));
    if (!root.exists()) return result;
    for (const auto &directory : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const auto manifestPath = QDir(directory.filePath()).filePath(QStringLiteral("manifest.json"));
        QFile file(manifestPath);
        QJsonObject manifest;
        if (file.open(QIODevice::ReadOnly)) manifest = QJsonDocument::fromJson(file.readAll()).object();
        Item item;
        item.id = manifest.value(QStringLiteral("id")).toString(directory.fileName());
        item.name = manifest.value(QStringLiteral("name")).toString(directory.fileName());
        item.version = manifest.value(QStringLiteral("version")).toString();
        item.description = manifest.value(QStringLiteral("description")).toString();
        item.path = directory.filePath();
        item.enabled = QSettings().value(QStringLiteral("extensions/") + settingsId(item.id) + QStringLiteral("/enabled"), manifest.value(QStringLiteral("enabledByDefault")).toBool(true)).toBool();
        item.settings = makeSettings(item.id, manifest);
        result.append(item);
    }
    return result;
}
void ExtensionModel::refresh()
{
    beginResetModel();
    items = scan();
    endResetModel();
}
void ExtensionModel::setEnabled(const QString &id, bool enabled)
{
    const auto found = std::find_if(items.begin(), items.end(), [&id](const Item &item) { return item.id == id; });
    if (found == items.end() || found->enabled == enabled) return;
    found->enabled = enabled;
    QSettings().setValue(QStringLiteral("extensions/") + settingsId(id) + QStringLiteral("/enabled"), enabled);
    const auto index = this->index(static_cast<int>(std::distance(items.begin(), found)), 0);
    emit dataChanged(index, index, {EnabledRole});
}
void ExtensionModel::setSetting(const QString &id, const QString &key, const QVariant &value)
{
    const auto found = std::find_if(items.begin(), items.end(), [&id](const Item &item) { return item.id == id; });
    if (found == items.end()) return;
    for (auto &entry : found->settings) {
        auto setting = entry.toMap();
        if (setting.value(QStringLiteral("key")).toString() != key) continue;
        const auto normalized = normalizeSetting(setting, value);
        if (setting.value(QStringLiteral("value")) == normalized) return;
        setting.insert(QStringLiteral("value"), normalized);
        entry = setting;
        QSettings().setValue(settingsPath(id, key), normalized);
        const auto index = this->index(static_cast<int>(std::distance(items.begin(), found)), 0);
        emit dataChanged(index, index, {SettingsRole});
        return;
    }
}
