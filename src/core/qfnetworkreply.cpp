#include "qfnetworkreply.h"

#include <QTimer>


QfNetworkReply::QfNetworkReply( const QNetworkAccessManager::Operation operation, const QNetworkRequest request, const QByteArray payloadByteArray = QByteArray() ):
  mOperation( operation ),
  mRequest( request ),
  mPayloadByteArray( payloadByteArray )
{
  mIsMultiPartPayload = false;

  initiateRequest();
};


QfNetworkReply::QfNetworkReply( const QNetworkAccessManager::Operation operation, const QNetworkRequest request, QHttpMultiPart *payloadMultiPart ):
  mOperation( operation ),
  mRequest( request ),
  mPayloadMultiPart( payloadMultiPart )
{
  mIsMultiPartPayload = true;

  initiateRequest();
};


void QfNetworkReply::abort()
{
  mIsFinished = true;
  mReply->abort();
}


QNetworkReply *QfNetworkReply::reply() const
{
  if ( mIsFinished )
    return mReply;

  return nullptr;
}


void QfNetworkReply::ignoreSslErrors( QList<QSslError> errors )
{
  mExpectedSslErrors = errors;
}


bool QfNetworkReply::isFinished() const
{
  return mIsFinished;
}


void QfNetworkReply::initiateRequest()
{
  switch ( mOperation )
  {
    case QNetworkAccessManager::HeadOperation:
      mReply = mNetworkAccessManager->head( mRequest );
      break;
    case QNetworkAccessManager::GetOperation:
      mReply = mNetworkAccessManager->get( mRequest );
      break;
    case QNetworkAccessManager::PutOperation:
      if ( mIsMultiPartPayload )
        mReply = mNetworkAccessManager->put( mRequest, mPayloadMultiPart );
      else
        mReply = mNetworkAccessManager->put( mRequest, mPayloadByteArray );
      break;
    case QNetworkAccessManager::PostOperation:
      if ( mIsMultiPartPayload )
        mReply = mNetworkAccessManager->post( mRequest, mPayloadMultiPart );
      else
        mReply = mNetworkAccessManager->post( mRequest, mPayloadByteArray );
      break;
    case QNetworkAccessManager::DeleteOperation:
      mReply = mNetworkAccessManager->deleteResource( mRequest );
      break;
    case QNetworkAccessManager::CustomOperation:
      throw QStringLiteral( "Not implemented!" );
    case QNetworkAccessManager::UnknownOperation:
      throw QStringLiteral( "Not implemented!" );
  }

  mReply->ignoreSslErrors( mExpectedSslErrors );

  connect( mReply, &QNetworkReply::finished, this, &QfNetworkReply::onFinished );
  connect( mReply, &QNetworkReply::encrypted, this, &QfNetworkReply::onEncrypted );
  connect( mReply, &QNetworkReply::downloadProgress, this, &QfNetworkReply::onDownloadProgress );
  connect( mReply, &QNetworkReply::uploadProgress, this, &QfNetworkReply::onUploadProgress );
}


void QfNetworkReply::onDownloadProgress( int bytesReceived, int bytesTotal )
{
  emit uploadProgress( bytesReceived, bytesTotal );
}


void QfNetworkReply::onUploadProgress( int bytesSent, int bytesTotal )
{
  emit uploadProgress( bytesSent, bytesTotal );
}


void QfNetworkReply::onEncrypted()
{
  emit encrypted();
}


void QfNetworkReply::onFinished()
{
  bool canRetry = false;
  QNetworkReply::NetworkError error = mReply->error();

  switch ( error )
  {
    case QNetworkReply::NoError:
      mIsFinished = true;
      emit finished();
      return;
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::InternalServerError:
    case QNetworkReply::ContentReSendError:
    case QNetworkReply::ServiceUnavailableError:
      canRetry = true;
      break;
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::OperationCanceledError:
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::BackgroundRequestNotAllowedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::ContentGoneError:
    case QNetworkReply::ContentConflictError:
    case QNetworkReply::OperationNotImplementedError:
    case QNetworkReply::ProtocolUnknownError:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::UnknownProxyError:
    case QNetworkReply::UnknownContentError:
    case QNetworkReply::ProtocolFailure:
    case QNetworkReply::UnknownServerError:
      canRetry = false;
      break;
    default:
      canRetry = false;
      break;
  }

  if ( ! canRetry || mRetriesLeft == 0 )
  {
    mIsFinished = true;

    emit errorOccurred( error );
    emit finished();

    return;
  }

  emit temporaryErrorOccurred( error );

  // wait random time before the retry is sent
  QTimer::singleShot( mRNG.bounded( mMaxTimeoutBetweenRetriesMs ), this, [ = ] () {
    emit retry();

    mRetriesLeft--;

    initiateRequest();
  } );
}
