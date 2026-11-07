#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPair>
#include <QString>

class QNetworkReply;

/// Small, dependency-free check against the GitHub Releases API.
///
/// The checker only performs a GET request.  It does not send telemetry,
/// identifiers or configuration data.  Results are cached in QSettings so an
/// automatic check happens at most once per day.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    struct Release {
        QString version;
        QString tagName;
        QString htmlUrl;
        QString body;
        QList<QPair<QString, QString>> assets;  // file name -> download URL

        QString assetUrl(const QString &suffix) const;
    };

    explicit UpdateChecker(QObject *parent = nullptr);

    void check(bool manual);

    static int compareVersions(const QString &lhs, const QString &rhs);
    static bool parseRelease(const QByteArray &json, Release *release,
                             QString *error = nullptr);
    static QString currentArchitecture();
    static QString releaseAssetUrl(const Release &release, const QString &suffix);
    static bool isFlatpak();
    static bool isAppImage();
    static QString updateUrl();

signals:
    void updateAvailable(const UpdateChecker::Release &release);
    void upToDate(const QString &currentVersion);
    void failed(const QString &message);

private:
    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply = nullptr;
};

Q_DECLARE_METATYPE(UpdateChecker::Release)
