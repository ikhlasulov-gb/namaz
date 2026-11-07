#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

namespace {

constexpr const char *kLastCheckKey = "updates/lastCheck";
constexpr const char *kSkippedVersionKey = "updates/skippedVersion";
constexpr int kCheckIntervalHours = 24;

QList<int> numericParts(const QString &value) {
    QList<int> parts;
    const QString core = value.section(QLatin1Char('-'), 0, 0);
    for (const QString &part : core.split(QLatin1Char('.'))) {
        bool ok = false;
        const int number = part.toInt(&ok);
        parts.append(ok ? number : 0);
    }
    while (parts.size() < 3) {
        parts.append(0);
    }
    return parts;
}

QString prerelease(const QString &value) {
    const int dash = value.indexOf(QLatin1Char('-'));
    return dash < 0 ? QString() : value.mid(dash + 1);
}

}  // namespace

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    qRegisterMetaType<UpdateChecker::Release>();
}

QString UpdateChecker::updateUrl() {
    const QString override = qEnvironmentVariable("NAMAZ_UPDATE_URL");
    if (!override.isEmpty()) {
        return override;
    }
    return QStringLiteral(
        "https://api.github.com/repos/ikhlasulov-gb/namaz/releases/latest");
}

int UpdateChecker::compareVersions(const QString &lhs, const QString &rhs) {
    QString left = lhs.trimmed();
    QString right = rhs.trimmed();
    if (left.startsWith(QLatin1Char('v')) || left.startsWith(QLatin1Char('V'))) {
        left.remove(0, 1);
    }
    if (right.startsWith(QLatin1Char('v')) || right.startsWith(QLatin1Char('V'))) {
        right.remove(0, 1);
    }

    const QList<int> leftParts = numericParts(left);
    const QList<int> rightParts = numericParts(right);
    const int count = qMax(leftParts.size(), rightParts.size());
    for (int index = 0; index < count; ++index) {
        const int a = index < leftParts.size() ? leftParts.at(index) : 0;
        const int b = index < rightParts.size() ? rightParts.at(index) : 0;
        if (a != b) {
            return a < b ? -1 : 1;
        }
    }

    const QString leftPre = prerelease(left);
    const QString rightPre = prerelease(right);
    if (leftPre == rightPre) {
        return 0;
    }
    // A release without a pre-release suffix is newer than a pre-release.
    if (leftPre.isEmpty()) {
        return 1;
    }
    if (rightPre.isEmpty()) {
        return -1;
    }
    return leftPre.compare(rightPre, Qt::CaseInsensitive) < 0 ? -1 : 1;
}

QString UpdateChecker::currentArchitecture() {
    const QString architecture = QSysInfo::currentCpuArchitecture();
    if (architecture == QStringLiteral("arm64")
        || architecture == QStringLiteral("aarch64")) {
        return QStringLiteral("aarch64");
    }
    if (architecture == QStringLiteral("x86_64")
        || architecture == QStringLiteral("amd64")) {
        return QStringLiteral("x86_64");
    }
    return architecture;
}

QString UpdateChecker::Release::assetUrl(const QString &suffix) const {
    for (const auto &asset : assets) {
        if (asset.first.endsWith(suffix, Qt::CaseInsensitive)) {
            return asset.second;
        }
    }
    return QString();
}

QString UpdateChecker::releaseAssetUrl(const Release &release,
                                       const QString &suffix) {
    const QString architecture = currentArchitecture();
    for (const auto &asset : release.assets) {
        if (asset.first.endsWith(suffix, Qt::CaseInsensitive)
            && asset.first.contains(architecture, Qt::CaseInsensitive)) {
            return asset.second;
        }
    }
    return release.assetUrl(suffix);
}

bool UpdateChecker::parseRelease(const QByteArray &json, Release *release,
                                 QString *error) {
    if (!release) {
        if (error) {
            *error = QStringLiteral("no output object");
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = parseError.errorString();
        }
        return false;
    }
    if (!document.isObject()) {
        if (error) {
            *error = QStringLiteral("GitHub returned a non-object response");
        }
        return false;
    }
    const QJsonObject object = document.object();
    if (object.contains(QStringLiteral("message"))
        && !object.contains(QStringLiteral("tag_name"))) {
        if (error) {
            *error = object.value(QStringLiteral("message")).toString();
        }
        return false;
    }
    const QString tag = object.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) {
        if (error) {
            *error = QStringLiteral("release has no tag_name");
        }
        return false;
    }

    Release result;
    result.tagName = tag;
    result.version = tag;
    if (result.version.startsWith(QLatin1Char('v'))
        || result.version.startsWith(QLatin1Char('V'))) {
        result.version.remove(0, 1);
    }
    result.htmlUrl = object.value(QStringLiteral("html_url")).toString();
    result.body = object.value(QStringLiteral("body")).toString();

    const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString url =
            asset.value(QStringLiteral("browser_download_url")).toString();
        if (!name.isEmpty() && !url.isEmpty()) {
            result.assets.append({name, url});
        }
    }

    *release = result;
    return true;
}

bool UpdateChecker::isFlatpak() {
    return QFile::exists(QStringLiteral("/.flatpak-info"))
        || qEnvironmentVariableIsSet("FLATPAK_ID");
}

bool UpdateChecker::isAppImage() {
    return qEnvironmentVariableIsSet("APPIMAGE")
        || qEnvironmentVariableIsSet("APPDIR")
        || QCoreApplication::applicationFilePath().endsWith(
            QStringLiteral(".AppImage"), Qt::CaseInsensitive);
}

void UpdateChecker::check(bool manual) {
    if (m_reply) {
        return;
    }

    QSettings settings;
    if (!manual) {
        const QDateTime last =
            QDateTime::fromString(settings.value(QString::fromLatin1(kLastCheckKey))
                                      .toString(),
                                  Qt::ISODate);
        if (last.isValid()
            && last.secsTo(QDateTime::currentDateTime())
                < kCheckIntervalHours * 3600) {
            return;
        }
    }

    QNetworkRequest request{QUrl(updateUrl())};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Namaz/%1")
                          .arg(QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_manager.get(request);
    QTimer::singleShot(15000, m_reply, [this] {
        if (m_reply) {
            m_reply->abort();
        }
    });

    connect(m_reply, &QNetworkReply::finished, this, [this, manual] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply) {
            return;
        }
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();

        QSettings store;
        store.setValue(QString::fromLatin1(kLastCheckKey),
                       QDateTime::currentDateTime().toString(Qt::ISODate));

        if (networkError != QNetworkReply::NoError) {
            if (manual) {
                emit failed(networkMessage);
            }
            return;
        }

        Release release;
        QString error;
        if (!parseRelease(payload, &release, &error)) {
            if (manual) {
                emit failed(error);
            }
            return;
        }

        const QString current = QCoreApplication::applicationVersion();
        if (compareVersions(release.version, current) <= 0) {
            if (manual) {
                emit upToDate(current);
            }
            return;
        }

        if (!manual
            && store.value(QString::fromLatin1(kSkippedVersionKey)).toString()
                   == release.version) {
            return;
        }
        emit updateAvailable(release);
    });
}
