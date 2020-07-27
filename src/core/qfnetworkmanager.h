#ifndef QFNETWORKMANAGER_H
#define QFNETWORKMANAGER_H

#include <QObject>


class QNetworkRequest;
class NetworkReply;
class QHttpMultiPart;
class QgsNetworkAccessManager;


class QfNetworkManager
{

  public:

    /**
     * Makes a new GET request
     * @param request
     * @return
     */
    static NetworkReply *get( const QNetworkRequest request );


    /**
     * Makes a new POST request
     * @param request
     * @param payload
     * @return
     */
    static NetworkReply *post( const QNetworkRequest request, const QByteArray payload = QByteArray() );


    /**
     * Makes a new POST request
     * @param request
     * @param payload
     * @return
     */
    static NetworkReply *post( const QNetworkRequest request, QHttpMultiPart *payload );


    /**
     * Makes a new PUT request
     * @param request
     * @param payload
     * @return
     */
    static NetworkReply *put( const QNetworkRequest request, const QByteArray payload = QByteArray() );


    /**
     * Makes a new PUT request
     * @param request
     * @param payload
     * @return
     */
    static NetworkReply *put( const QNetworkRequest request, QHttpMultiPart *payload );


    /**
     * Makes a new DELETE request
     * @param request
     * @param payload
     * @return
     */
    static NetworkReply *deleteResource( const QNetworkRequest request, QByteArray payload = QByteArray() );

};


#endif // QFNETWORKMANAGER_H
