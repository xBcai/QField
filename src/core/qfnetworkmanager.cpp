#include "qfnetworkmanager.h"
#include "qfnetworkreply.h"

#include "qgsnetworkaccessmanager.h"


QfNetworkReply *QfNetworkManager::get( const QNetworkRequest request )
{
  return new QfNetworkReply( QNetworkAccessManager::GetOperation, request, QByteArray() );
}


QfNetworkReply *QfNetworkManager::post( const QNetworkRequest request, const QByteArray payload )
{
  return new QfNetworkReply( QNetworkAccessManager::PostOperation, request, payload );
}


QfNetworkReply *QfNetworkManager::post( const QNetworkRequest request, QHttpMultiPart *payload )
{
  return new QfNetworkReply( QNetworkAccessManager::PostOperation, request, payload );
}


QfNetworkReply *QfNetworkManager::put( const QNetworkRequest request, const QByteArray payload )
{
  return new QfNetworkReply( QNetworkAccessManager::PutOperation, request, payload );
}


QfNetworkReply *QfNetworkManager::put( const QNetworkRequest request, QHttpMultiPart *payload )
{
  return new QfNetworkReply( QNetworkAccessManager::PutOperation, request, payload );
}


QfNetworkReply *QfNetworkManager::deleteResource( const QNetworkRequest request, QByteArray payload )
{
  return new QfNetworkReply( QNetworkAccessManager::DeleteOperation, request, payload );
}
