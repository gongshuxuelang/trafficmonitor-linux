#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/logger.h"

using namespace tm_linux;

class LoggerTest : public QObject {
    Q_OBJECT

private slots:
    void writesMessages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("app.log"));

        {
            Logger logger(path.toStdString());
            logger.log(QStringLiteral("hello"));
            qInfo("world");
        }

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.contains(QStringLiteral("hello")));
        QVERIFY(content.contains(QStringLiteral("world")));
    }

    void rotatesAtFiveMegabytes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("app.log"));

        {
            Logger logger(path.toStdString());
            const QString line(1024, QLatin1Char('x'));
            for (int index = 0; index < 6000; ++index) {
                logger.log(line);
            }
        }

        QVERIFY(QFile::exists(path));
        QVERIFY(QFile::exists(path + QStringLiteral(".1")));
    }

    void dropsDebugMessagesInRelease()
    {
#ifdef TM_DEBUG
        QSKIP("Debug builds keep qDebug messages");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("app.log"));

        {
            Logger logger(path.toStdString());
            qDebug("debug-only-message");
        }

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(!content.contains(QStringLiteral("debug-only-message")));
#endif
    }

    void keepsDebugMessagesInDebugBuild()
    {
#ifdef TM_DEBUG
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("app.log"));

        {
            Logger logger(path.toStdString());
            qDebug("debug-only-message");
        }

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.contains(QStringLiteral("debug-only-message")));
#else
        QSKIP("Release builds drop qDebug messages");
#endif
    }
};

QTEST_APPLESS_MAIN(LoggerTest)

#include "test_logger.moc"
