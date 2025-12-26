#include "checker_thread.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QtConcurrent>

CheckerThread::CheckerThread(QObject *parent)
    : QThread(parent)
    , m_concurrent(1000)
    , m_timeout(10)
    , m_shouldStop(0)
{
}

CheckerThread::~CheckerThread()
{
    stop();
    wait();
}

void CheckerThread::setApiKeys(const QStringList &keys)
{
    m_apiKeys = keys;
}

void CheckerThread::setEndpoint(const QString &endpoint)
{
    m_endpoint = endpoint;
}

void CheckerThread::setMethod(const QString &method)
{
    m_method = method;
}

void CheckerThread::setHeaders(const QString &headers)
{
    m_headers = headers;
}

void CheckerThread::setRequestBody(const QString &body)
{
    m_requestBody = body;
}

void CheckerThread::setConcurrent(int concurrent)
{
    m_concurrent = concurrent;
}

void CheckerThread::setTimeout(int timeout)
{
    m_timeout = timeout;
}

void CheckerThread::stop()
{
    m_shouldStop.storeRelaxed(1);
}

QVector<ApiCheckResult> CheckerThread::getResults() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_resultsMutex));
    return m_results;
}

QStringList CheckerThread::parseHeaders(const QString &headersStr) const
{
    QStringList headers;
    QStringList parts = headersStr.split(';', Qt::SkipEmptyParts);

    for (const QString &part : parts) {
        QString header = part.trimmed();
        if (!header.isEmpty()) {
            headers.append(header);
        }
    }

    return headers;
}

void CheckerThread::run()
{
    m_shouldStop.storeRelaxed(0);
    m_results.clear();

    checkApiKeys();

    emit finished();
}

void CheckerThread::checkApiKeys()
{
    QNetworkAccessManager networkManager;
    QSemaphore semaphore(m_concurrent);
    QAtomicInt checkedCount(0);
    QAtomicInt validCount(0);
    QAtomicInt invalidCount(0);
    QAtomicInt errorCount(0);

    QStringList parsedHeaders = parseHeaders(m_headers);

    for (const QString &key : m_apiKeys) {
        if (m_shouldStop.loadRelaxed()) {
            break;
        }

        semaphore.acquire();

        QtConcurrent::run([&]() {
            if (m_shouldStop.loadRelaxed()) {
                semaphore.release();
                return;
            }

            ApiCheckResult result = checkSingleKey(key);

            {
                QMutexLocker locker(&m_resultsMutex);
                m_results.append(result);
            }

            int current = checkedCount.fetchAndAddRelaxed(1) + 1;

            if (result.isValid()) {
                validCount.fetchAndAddRelaxed(1);
            } else if (result.isInvalid()) {
                invalidCount.fetchAndAddRelaxed(1);
            } else {
                errorCount.fetchAndAddRelaxed(1);
            }

            emit progress(current, validCount.loadRelaxed(), invalidCount.loadRelaxed(), errorCount.loadRelaxed());
            semaphore.release();
        });
    }

    while (checkedCount.loadRelaxed() < m_apiKeys.size() && !m_shouldStop.loadRelaxed()) {
        QThread::msleep(10);
    }
}

ApiCheckResult CheckerThread::checkSingleKey(const QString &key)
{
    ApiCheckResult result;
    result.key = key;
    result.checkedAt = QDateTime::currentDateTime();

    QElapsedTimer timer;
    timer.start();

    QNetworkAccessManager manager;
    QEventLoop loop;
    QTimer timeoutTimer;

    QNetworkRequest request(QUrl(m_endpoint));

    QString authHeader = "Bearer " + key;
    request.setRawHeader("Authorization", authHeader.toUtf8());

    QStringList headers = parseHeaders(m_headers);
    for (const QString &header : headers) {
        int colonPos = header.indexOf(':');
        if (colonPos > 0) {
            QString headerName = header.left(colonPos).trimmed();
            QString headerValue = header.mid(colonPos + 1).trimmed();
            request.setRawHeader(headerName.toUtf8(), headerValue.toUtf8());
        }
    }

    QNetworkReply *reply = nullptr;

    if (m_method == "GET") {
        reply = manager.get(request);
    } else if (m_method == "POST") {
        reply = manager.post(request, m_requestBody.toUtf8());
    } else if (m_method == "PUT") {
        reply = manager.put(request, m_requestBody.toUtf8());
    } else if (m_method == "DELETE") {
        reply = manager.deleteResource(request);
    } else if (m_method == "PATCH") {
        QByteArray data = m_requestBody.toUtf8();
        reply = manager.sendCustomRequest(request, "PATCH", data);
    }

    if (!reply) {
        result.status = "error";
        result.message = "创建请求失败";
        result.responseTime = timer.elapsed();
        return result;
    }

    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(m_timeout * 1000);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, reply, &QNetworkReply::abort);

    loop.exec();

    result.responseTime = timer.elapsed();

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();

        if (reply->error() == QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            switch (statusCode) {
                case 200:
                case 201:
                case 204:
                    result.status = "valid";
                    result.message = "有效";
                    break;
                case 401:
                    result.status = "invalid";
                    result.message = "认证失败";
                    break;
                case 403:
                    result.status = "invalid";
                    result.message = "访问被拒绝";
                    break;
                case 404:
                    result.status = "invalid";
                    result.message = "资源不存在";
                    break;
                case 429:
                    result.status = "error";
                    result.message = "请求过多，稍后重试";
                    break;
                default:
                    if (statusCode >= 500) {
                        result.status = "error";
                        result.message = QString("服务器错误 %1").arg(statusCode);
                    } else {
                        result.status = "invalid";
                        result.message = QString("HTTP %1").arg(statusCode);
                    }
                    break;
            }
        } else {
            result.status = "error";
            result.message = reply->errorString();
        }
    } else {
        result.status = "error";
        result.message = QString("请求超时 (%1秒)").arg(m_timeout);
    }

    reply->deleteLater();
    return result;
}
