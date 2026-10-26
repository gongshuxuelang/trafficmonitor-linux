#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "tm/logger.h"

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
};

QTEST_APPLESS_MAIN(LoggerTest)

#include "test_logger.moc"
