/***************************************************************************
                          deltafilewrapper.h
                             -------------------
  begin                : Apr 2020
  copyright            : (C) 2020 by Ivan Ivanov
  email                : ivan@opengis.ch
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "deltafilewrapper.h"

#include <QFileInfo>
#include <QFile>
#include <QUuid>

#include <qgsproject.h>


const QString DeltaFileWrapper::FormatVersion = QStringLiteral( "1.0" );
QMap<QString, QStringList> DeltaFileWrapper::sCacheAttachmentFieldNames;
QSet<QString> DeltaFileWrapper::sFileLocks;


DeltaFileWrapper::DeltaFileWrapper( const QString &fileName )
{
  QFileInfo fileInfo = QFileInfo( fileName );

  // we need to resolve all symbolic links are relative paths, so we produce a unique file path to the file. 
  // Because the file may not exist yet, we cannot use QFileInfo::canonicalFilePath() as it returns an empty string if it fails to resolve.
  // However, we assume that the parent directory exists.
  mFileName = fileInfo.canonicalFilePath().isEmpty() ? fileInfo.absoluteFilePath() : fileInfo.canonicalFilePath();
  mErrorType = DeltaFileWrapper::NoError;

  if ( mErrorType == DeltaFileWrapper::NoError && sFileLocks.contains( mFileName ) )
    mErrorType = DeltaFileWrapper::LockError;

  if ( mErrorType == DeltaFileWrapper::NoError )
    mCloudProjectId = QgsProject::instance()->readEntry( QStringLiteral( "qfieldcloud" ), QStringLiteral( "projectId" ) );

  if ( mErrorType == DeltaFileWrapper::NoError && mCloudProjectId.isEmpty() )
    mErrorType = DeltaFileWrapper::NotCloudProjectError;

  QFile deltaFile( mFileName );

  if ( mErrorType == DeltaFileWrapper::NoError && QFileInfo::exists( mFileName ) )
  {
    QJsonParseError jsonError;

    QgsLogger::debug( QStringLiteral( "Loading deltas from %1" ).arg( mFileName ) );

    if ( mErrorType == DeltaFileWrapper::NoError && ! deltaFile.open( QIODevice::ReadWrite ) )
    {
      mErrorType = DeltaFileWrapper::IOError;
      mErrorDetails = deltaFile.errorString();
    }

    if ( mErrorType == DeltaFileWrapper::NoError )
      mJsonRoot = QJsonDocument::fromJson( deltaFile.readAll(), &jsonError ).object();

    if ( mErrorType == DeltaFileWrapper::NoError && ( jsonError.error != QJsonParseError::NoError ) )
    {
      mErrorType = DeltaFileWrapper::JsonParseError;
      mErrorDetails = jsonError.errorString();
    }

    if ( mErrorType == DeltaFileWrapper::NoError && ( ! mJsonRoot.value( "id" ).isString() || mJsonRoot.value( "id" ).toString().isEmpty() ) )
      mErrorType = DeltaFileWrapper::JsonFormatIdError;

    if ( mErrorType == DeltaFileWrapper::NoError && ( ! mJsonRoot.value( "projectId" ).isString() || mJsonRoot.value( "projectId" ).toString().isEmpty() ) )
      mErrorType = DeltaFileWrapper::JsonFormatProjectIdError;

    if ( mErrorType == DeltaFileWrapper::NoError && ! mJsonRoot.value( "deltas" ).isArray() )
      mErrorType = DeltaFileWrapper::JsonFormatDeltasError;

    if ( mErrorType == DeltaFileWrapper::NoError && ( ! mJsonRoot.value( "version" ).isString() || mJsonRoot.value( "version" ).toString().isEmpty() ) )
      mErrorType = DeltaFileWrapper::JsonFormatVersionError;

    if ( mErrorType == DeltaFileWrapper::NoError && mJsonRoot.value( "version" ) != DeltaFileWrapper::FormatVersion )
      mErrorType = DeltaFileWrapper::JsonIncompatibleVersionError;

    if ( mErrorType == DeltaFileWrapper::NoError )
    {
      for ( const QJsonValue &v : mJsonRoot.value( "deltas" ).toArray() )
      {
        if ( ! v.isObject() )
        {
          mErrorType == DeltaFileWrapper::JsonFormatDeltaItemError;
          continue;
        }

        // TODO validate delta item properties

        mDeltas.append( v );
      }
    }
  }
  else if ( mErrorType == DeltaFileWrapper::NoError )
  {
    mJsonRoot = QJsonObject( {{"version", DeltaFileWrapper::FormatVersion},
                              {"id", QUuid::createUuid().toString( QUuid::WithoutBraces )},
                              {"projectId", mCloudProjectId},
                              {"deltas", mDeltas}} );

    if ( ! deltaFile.open( QIODevice::ReadWrite ) )
    {
      mErrorType = DeltaFileWrapper::IOError;
      mErrorDetails = deltaFile.errorString();
    }

    // toFile() modifies mErrorType and mErrorDetails, that's why we ignore the boolean return
    toFile();
  }
  else
  {
    return;
  }

  sFileLocks.insert( mFileName );
}


DeltaFileWrapper::~DeltaFileWrapper()
{
  sFileLocks.remove( mFileName );
}


QString DeltaFileWrapper::id() const
{
  return mJsonRoot.value( QStringLiteral( "id" ) ).toString();
}


QString DeltaFileWrapper::fileName() const
{
  return mFileName;
}


QString DeltaFileWrapper::projectId() const
{
  return mCloudProjectId;
}


void DeltaFileWrapper::reset( bool isHardReset )
{
  if ( ! mIsDirty && mDeltas.size() == 0)
    return;

  mIsDirty = true;
  mDeltas = QJsonArray();

  if ( ! isHardReset )
    return;
  
  mJsonRoot.insert( QStringLiteral( "id" ), QUuid::createUuid().toString( QUuid::WithoutBraces ) );
}


bool DeltaFileWrapper::hasError() const
{
  return mErrorType != DeltaFileWrapper::NoError;
}


bool DeltaFileWrapper::isDirty() const
{
  return mIsDirty;
}


int DeltaFileWrapper::count() const
{
  return mDeltas.size();
}


QJsonArray DeltaFileWrapper::deltas() const
{
  return mDeltas;
}


DeltaFileWrapper::ErrorTypes DeltaFileWrapper::errorType() const
{
  return mErrorType;
}


QString DeltaFileWrapper::errorString() const
{
  const QHash<DeltaFileWrapper::ErrorTypes, QString> errorMessages({
    {DeltaFileWrapper::NoError, QStringLiteral( "" )},
    {DeltaFileWrapper::LockError, QStringLiteral( "Delta file is already opened" )},
    {DeltaFileWrapper::NotCloudProjectError, QStringLiteral( "The current project is not a cloud project" ) },
    {DeltaFileWrapper::IOError, QStringLiteral( "Cannot open file for read and write" ) },
    {DeltaFileWrapper::JsonParseError, QStringLiteral( "Unable to parse JSON" ) },
    {DeltaFileWrapper::JsonFormatIdError, QStringLiteral( "Delta file is missing a valid id" ) },
    {DeltaFileWrapper::JsonFormatProjectIdError, QStringLiteral( "Delta file is missing a valid project id" ) },
    {DeltaFileWrapper::JsonFormatVersionError, QStringLiteral( "Delta file is missing a valid version" ) },
    {DeltaFileWrapper::JsonFormatDeltasError, QStringLiteral( "Delta file is missing a valid deltas" ) },
    {DeltaFileWrapper::JsonFormatDeltaItemError, QStringLiteral( "Delta file is missing a valid delta item" ) },
    {DeltaFileWrapper::JsonIncompatibleVersionError, QStringLiteral( "Delta file has incompatible version" ) }
  });

  Q_ASSERT( errorMessages.contains( mErrorType ) );

  return QStringLiteral( "%1\n%2" ).arg( errorMessages.value( mErrorType ), mErrorDetails );
}


QByteArray DeltaFileWrapper::toJson( QJsonDocument::JsonFormat jsonFormat ) const
{
  QJsonObject jsonRoot (mJsonRoot);
  jsonRoot.insert( QStringLiteral( "deltas" ), mDeltas );

  return QJsonDocument( jsonRoot ).toJson( jsonFormat );
}


QString DeltaFileWrapper::toString() const
{
  return QString::fromStdString( toJson().toStdString() );
}


bool DeltaFileWrapper::toFile()
{
  QFile deltaFile( mFileName );

  // QgsLogger::debug( "Start writing deltas JSON" );

  if ( ! deltaFile.open( QIODevice::WriteOnly | QIODevice::Unbuffered ) )
  {
    mErrorType = DeltaFileWrapper::IOError;
    mErrorDetails = deltaFile.errorString();
    QgsLogger::warning( QStringLiteral( "File %1 cannot be open for writing. Reason: %2" ).arg( mFileName ).arg( mErrorDetails ) );

    return false;
  }

  if ( deltaFile.write( toJson() )  == -1)
  {
    mErrorType = DeltaFileWrapper::IOError;
    mErrorDetails = deltaFile.errorString();
    QgsLogger::warning( QStringLiteral( "Contents of the file %1 has not been written. Reason %2" ).arg( mFileName ).arg( mErrorDetails ) );
    return false;
  }

  deltaFile.close();
  mIsDirty = false;
  // QgsLogger::debug( "Finished writing deltas JSON" );

  return true;
}


bool DeltaFileWrapper::append( const DeltaFileWrapper *deltaFileWrapper )
{
  if ( ! deltaFileWrapper )
    return false;
    
  if ( deltaFileWrapper->hasError() )
    return false;

  mDeltas.append( deltaFileWrapper->deltas() );

  return true;
}


QStringList DeltaFileWrapper::attachmentFieldNames( const QString &layerId )
{
  if ( sCacheAttachmentFieldNames.contains( layerId ) )
    return sCacheAttachmentFieldNames.value( layerId );

  const QgsVectorLayer *vl = static_cast<QgsVectorLayer *>( QgsProject::instance()->mapLayer( layerId ) );
  QStringList attachmentFieldNames;

  if ( ! vl )
    return attachmentFieldNames;

  const QgsFields fields = vl->fields();

  for ( const QgsField &field : fields)
  {
    if ( field.editorWidgetSetup().type() == QStringLiteral( "ExternalResource" ) )
      attachmentFieldNames.append( field.name() );
  }

  sCacheAttachmentFieldNames.insert( layerId, attachmentFieldNames );

  return attachmentFieldNames;
}


QMap<QString, QString> DeltaFileWrapper::attachmentFileNames() const
{
  // NOTE represents { layerId: { featureId: { attributeName: fileName } } }
  // We store all the changes in such mapping that we can return only the last attachment file name that is associated with a feature.
  // E.g. for given feature we start with attachment A.jpg, then we update to B.jpg. Later we change our mind and we apply C.jpg. In this case we only care about C.jpg.
  QMap<QString, QString> fileNames;
  QMap<QString, QString> fileChecksums;

  for ( const QJsonValue &deltaJson: qgis::as_const( mDeltas ) )
  {
    QVariantMap delta = deltaJson.toObject().toVariantMap();
    const QString layerId = delta.value( QStringLiteral( "layerId" ) ).toString();
    const QString method = delta.value( QStringLiteral( "method" ) ).toString();
    const QString fid = delta.value( QStringLiteral( "fid" ) ).toString();
    const QStringList attachmentFieldNamesList = attachmentFieldNames( layerId );

    if ( method == QStringLiteral( "delete" ) || method == QStringLiteral( "patch" ) )
    {
      const QVariantMap oldData = delta.value( QStringLiteral( "old" ) ).toMap();

      Q_ASSERT( ! oldData.isEmpty() );
    }
    
    if ( method == QStringLiteral( "create" ) || method == QStringLiteral( "patch" ) )
    {
      const QVariantMap newData = delta.value( QStringLiteral( "new" ) ).toMap();

      Q_ASSERT( ! newData.isEmpty() );

      if ( newData.contains( QStringLiteral( "files_sha256" ) ) )
      {
        const QVariantMap filesChecksum = newData.value( QStringLiteral( "files_sha256" ) ).toMap();

        Q_ASSERT( ! filesChecksum.isEmpty() );

        const QVariantMap attributes = newData.value( QStringLiteral( "attributes" ) ).toMap();

        for ( const QString &fieldName : attributes.keys() )
        {
          if ( ! attachmentFieldNamesList.contains( fieldName ) )
            continue;

          const QString fileName = attributes.value( fieldName ).toString();

          Q_ASSERT( filesChecksum.contains( fileName ) );

          const QString key = QStringLiteral( "%1//%2//%3" ).arg( layerId, fid, fieldName );
          const QString fileChecksum = filesChecksum.value( fileName ).toString();

          fileNames.insert( key, fileName );
          fileChecksums.insert( fileName, fileChecksum );
        }
      }
    }
    else
    {
      QgsLogger::debug( QStringLiteral( "File `%1` contains unknown method `%2`" ).arg( mFileName, method ) );
      Q_ASSERT(0);
    }
  }

  QMap<QString, QString> fileNameChecksum;

  for ( const QString &fileName : fileNames.values() )
  {
    fileNameChecksum.insert( fileName, fileChecksums.value( fileName ) );
  }

  return fileNameChecksum;
}


QByteArray DeltaFileWrapper::fileChecksum( const QString &fileName, const QCryptographicHash::Algorithm hashAlgorithm )
{
    QFile f(fileName);

    if ( ! f.open(QFile::ReadOnly) )
      return QByteArray();

    QCryptographicHash hash(hashAlgorithm);

    if ( hash.addData( &f ) )
      return hash.result();

    return QByteArray();
}


void DeltaFileWrapper::addPatch( const QString &layerId, const QgsFeature &oldFeature, const QgsFeature &newFeature )
{
  QJsonObject delta( {
    {"fid", oldFeature.id()},
    {"layerId", layerId},
    {"method", "patch"}
  } );
  const QStringList attachmentFieldsList = attachmentFieldNames( layerId );
  const QgsGeometry oldGeom = oldFeature.geometry();
  const QgsGeometry newGeom = newFeature.geometry();
  const QgsAttributes oldAttrs = oldFeature.attributes();
  const QgsAttributes newAttrs = newFeature.attributes();
  QJsonObject oldData;
  QJsonObject newData;
  bool areFeaturesEqual = false;

  if ( ! oldGeom.equals( newGeom ) )
  {
    oldData.insert( QStringLiteral( "geometry" ), geometryToJsonValue( oldGeom ) );
    newData.insert( QStringLiteral( "geometry" ), geometryToJsonValue( newGeom ) );
    areFeaturesEqual = true;
  }

  // TODO types should be checked too, however QgsFields::operator== is checking instances, not values
  Q_ASSERT( oldFeature.fields().names() == newFeature.fields().names() );

  QgsFields fields = newFeature.fields();
  QJsonObject tmpOldAttrs;
  QJsonObject tmpNewAttrs;
  QJsonObject tmpOldFileChecksums;
  QJsonObject tmpNewFileChecksums;

  for ( int idx = 0; idx < fields.count(); ++idx )
  {
    const QVariant oldVal = oldAttrs.at( idx );
    const QVariant newVal = newAttrs.at( idx );

    if ( newVal != oldVal )
    {
      const QString name = fields.at( idx ).name();
      tmpOldAttrs.insert( name, QJsonValue::fromVariant( oldVal ) );
      tmpNewAttrs.insert( name, QJsonValue::fromVariant( newVal ) );
      areFeaturesEqual = true;

      if ( attachmentFieldsList.contains( name ) )
      {
        const QString homeDir = QgsProject::instance()->homePath();
        const QString oldFileName = oldVal.toString();
        const QByteArray oldFileChecksum = fileChecksum( QStringLiteral( "%1/%2" ).arg( homeDir, oldFileName ), QCryptographicHash::Sha256 );
        const QJsonValue oldFileChecksumJson = oldFileChecksum.isEmpty() ? QJsonValue::Null : QJsonValue( QString( oldFileChecksum.toHex() ) );
        const QString newFileName = newVal.toString();
        const QByteArray newFileChecksum = fileChecksum( QStringLiteral( "%1/%2" ).arg( homeDir, newFileName ), QCryptographicHash::Sha256 );
        const QJsonValue newFileChecksumJson = newFileChecksum.isEmpty() ? QJsonValue::Null : QJsonValue( QString( newFileChecksum.toHex() ) );

        tmpOldFileChecksums.insert( oldFileName, oldFileChecksumJson );
        tmpNewFileChecksums.insert( newFileName, newFileChecksumJson );
      }
    }
  }

  // if features are completely equal, there is no need to change the JSON
  if ( ! areFeaturesEqual )
    return;

  if ( tmpOldAttrs.length() > 0 || tmpNewAttrs.length() > 0 )
  {
    oldData.insert( QStringLiteral( "attributes" ), tmpOldAttrs );
    newData.insert( QStringLiteral( "attributes" ), tmpNewAttrs );

    if ( tmpOldFileChecksums.length() > 0 || tmpNewFileChecksums.length() > 0 )
    {
      oldData.insert( QStringLiteral( "files_sha256" ), tmpOldFileChecksums );
      newData.insert( QStringLiteral( "files_sha256" ), tmpNewFileChecksums );
    }
  }
  else
  {
    Q_ASSERT( tmpOldFileChecksums.isEmpty() );
    Q_ASSERT( tmpNewFileChecksums.isEmpty() );
  }

  delta.insert( QStringLiteral( "old" ), oldData );
  delta.insert( QStringLiteral( "new" ), newData );

  mDeltas.append( delta );
  mIsDirty = true;
}


void DeltaFileWrapper::addDelete( const QString &layerId, const QgsFeature &oldFeature )
{
  QJsonObject delta( {{"fid", oldFeature.id()},
                      {"layerId", layerId},
                      {"method", "delete"}} );
  const QStringList attachmentFieldsList = attachmentFieldNames( layerId );
  const QgsAttributes oldAttrs = oldFeature.attributes();
  QJsonObject oldData( {{"geometry", geometryToJsonValue( oldFeature.geometry() )}});
  QJsonObject tmpOldAttrs;
  QJsonObject tmpOldFileChecksums;

  for ( int idx = 0; idx < oldAttrs.count(); ++idx )
  {
    const QVariant oldVal = oldAttrs.at( idx );
    const QString name = oldFeature.fields().at( idx ).name();
    tmpOldAttrs.insert( name, QJsonValue::fromVariant( oldVal ) );

    if ( attachmentFieldsList.contains( name ) && ! oldVal.isNull() )
    {
      const QString oldFileName = oldVal.toString();
      const QByteArray oldFileChecksum = fileChecksum( oldFileName, QCryptographicHash::Sha256 );
      const QJsonValue oldFileChecksumJson = oldFileChecksum.isEmpty() ? QJsonValue::Null : QJsonValue( QString( oldFileChecksum ) );

      tmpOldFileChecksums.insert( oldVal.toString(), oldFileChecksumJson );
    }
  }

  if ( tmpOldAttrs.length() > 0 )
  {
    oldData.insert( QStringLiteral( "attributes" ), tmpOldAttrs );

    if ( tmpOldFileChecksums.length() > 0 )
    {
      oldData.insert( QStringLiteral( "files_sha256" ), tmpOldFileChecksums );
    }
  }
  else
  {
    Q_ASSERT( tmpOldFileChecksums.isEmpty() );
  }
  
  delta.insert( QStringLiteral( "old" ), oldData );

  mDeltas.append( delta );
  mIsDirty = true;
}


void DeltaFileWrapper::addCreate( const QString &layerId, const QgsFeature &newFeature )
{
  QJsonObject delta( {{"fid", newFeature.id()},
                      {"layerId", layerId},
                      {"method", "create"}} );
  const QStringList attachmentFieldsList = attachmentFieldNames( layerId );
  const QgsAttributes newAttrs = newFeature.attributes();
  QJsonObject newData( {{"geometry", geometryToJsonValue( newFeature.geometry() )}});
  QJsonObject tmpNewAttrs;
  QJsonObject tmpNewFileChecksums;

  for ( int idx = 0; idx < newAttrs.count(); ++idx )
  {
    const QVariant newVal = newAttrs.at( idx );
    const QString name = newFeature.fields().at( idx ).name();
    tmpNewAttrs.insert( name, QJsonValue::fromVariant( newVal ) );

    if ( attachmentFieldsList.contains( name ) )
    {
      const QString newFileName = newVal.toString();
      const QByteArray newFileChecksum = fileChecksum( newFileName, QCryptographicHash::Sha256 );
      const QJsonValue newFileChecksumJson = newFileChecksum.isEmpty() ? QJsonValue::Null : QJsonValue( QString( newFileChecksum ) );

      tmpNewFileChecksums.insert( newVal.toString(), newFileChecksumJson );
    }
  }

 if ( tmpNewAttrs.length() > 0 )
  {
    newData.insert( QStringLiteral( "attributes" ), tmpNewAttrs );

    if ( tmpNewFileChecksums.length() > 0 )
    {
      newData.insert( QStringLiteral( "files_sha256" ), tmpNewFileChecksums );
    }
  }
  else
  {
    Q_ASSERT( tmpNewFileChecksums.isEmpty() );
  }
  
  delta.insert( QStringLiteral( "new" ), newData );

  mDeltas.append( delta );
  mIsDirty = true;
}


QJsonValue DeltaFileWrapper::geometryToJsonValue( const QgsGeometry &geom ) const
{
  if ( geom.isNull() )
    return QJsonValue::Null;

  return QJsonValue( geom.asWkt() );
}