/***************************************************************************
                          featuredeltas.h
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

#include "featuredeltas.h"

#include <QFileInfo>
#include <QFile>
#include <QUuid>

#include <qgsproject.h>


const QString FeatureDeltas::FormatVersion = QStringLiteral( "1.0" );


FeatureDeltas::FeatureDeltas( const QString &fileName )
{
  mFileName = fileName;
  QFile deltasFile( mFileName );
  QString errorReason;

  // TODO fix this
  mProjectId = QStringLiteral( "<MISSING>" );

  if ( QFileInfo::exists( mFileName ) )
  {
    QJsonParseError jsonError;

    QgsLogger::debug( QStringLiteral( "Loading deltas from file %1" ).arg( mFileName ) );

    if ( ! deltasFile.open( QIODevice::ReadWrite ) )
      errorReason = QStringLiteral( "Cannot open file for read and write" );

    if ( errorReason.isEmpty() )
      mJsonRoot = QJsonDocument::fromJson( deltasFile.readAll(), &jsonError ).object();

    if ( errorReason.isEmpty() && ( jsonError.error != QJsonParseError::NoError ) )
      errorReason = jsonError.errorString();

    if ( errorReason.isEmpty() && ( ! mJsonRoot.value( "id" ).isString() || mJsonRoot.value( "id" ).toString().isEmpty() ) )
      errorReason = QStringLiteral( "Delta file is missing a valid id" );

    if ( errorReason.isEmpty() && ( ! mJsonRoot.value( "projectId" ).isString() || mJsonRoot.value( "projectId" ).toString().isEmpty() ) )
      errorReason = QStringLiteral( "Delta file is missing a valid project id" );

    if ( errorReason.isEmpty() && ! mJsonRoot.value( "deltas" ).isArray() )
      errorReason = QStringLiteral( "Delta file is missing a valid array of deltas" );

    if ( errorReason.isEmpty() && ( ! mJsonRoot.value( "version" ).isString() || mJsonRoot.value( "version" ).toString().isEmpty() ) )
      errorReason = QStringLiteral( "Delta file is missing a valid version" );

    if ( errorReason.isEmpty() && mJsonRoot.value( "version" ) != FeatureDeltas::FormatVersion )
      errorReason = QStringLiteral( "File has incompatible version" );

    if ( errorReason.isEmpty() )
    {
      for ( const QJsonValue &v : mJsonRoot.value( "deltas" ).toArray() )
      {
        // ? how far should I go in checking the validity?
        if ( ! v.isObject() )
          QgsLogger::warning( QStringLiteral( "Encountered delta element that is not an object" ) );

        mDeltas.append( v );
      }
    }
  }
  else
  {
    mJsonRoot = QJsonObject( {{"version", FeatureDeltas::FormatVersion},
                              {"id", QUuid::createUuid().toString( QUuid::WithoutBraces )},
                              // Mario thinks this is not needed for now
                              // {"clientId",clientId},
                              {"projectId", mProjectId},
                              // It seems this should go to individual deltas
                              // {"layerId",layer.id()},
                              {"deltas", mDeltas}} );

    if ( ! deltasFile.open( QIODevice::ReadWrite ) )
      errorReason = QStringLiteral( "Cannot open deltas file for read and write" );

    if ( ! toFile() )
      errorReason = QStringLiteral( "Cannot write deltas file" );
  }

  if ( ! errorReason.isEmpty() )
  {
    mErrorReason = errorReason;
    mHasError = true;
    return;
  }
}


QString FeatureDeltas::fileName() const
{
  return mFileName;
}


QString FeatureDeltas::projectId() const
{
  return mProjectId;
}


void FeatureDeltas::clear()
{
  mIsDirty = mDeltas.size() != 0;
  mDeltas = QJsonArray();
}


bool FeatureDeltas::hasError() const
{
  return mHasError;
}


bool FeatureDeltas::isDirty() const
{
  return mIsDirty;
}


int FeatureDeltas::count() const
{
  return mDeltas.size();
}


QString FeatureDeltas::errorString() const
{
  return mErrorReason;
}


QByteArray FeatureDeltas::toJson( QJsonDocument::JsonFormat jsonFormat ) const
{
  QJsonObject jsonRoot (mJsonRoot);
  jsonRoot.insert( QStringLiteral( "deltas" ), mDeltas );

  return QJsonDocument( jsonRoot ).toJson( jsonFormat );
}


QString FeatureDeltas::toString() const
{
  return QString::fromStdString( toJson().toStdString() );
}


bool FeatureDeltas::toFile()
{
  QFile deltasFile( mFileName );

  QgsLogger::debug( "Start writing deltas JSON" );

  if ( ! deltasFile.open( QIODevice::WriteOnly | QIODevice::Unbuffered ) )
  {
    mErrorReason = deltasFile.errorString();
    QgsLogger::warning( QStringLiteral( "File %1 cannot be open for writing. Reason %2" ).arg( mFileName ).arg( mErrorReason ) );

    return false;
  }

  if ( deltasFile.write( toJson() )  == -1)
  {
    mErrorReason = deltasFile.errorString();
    QgsLogger::warning( QStringLiteral( "Contents of the file %1 has not been written. Reason %2" ).arg( mFileName ).arg( mErrorReason ) );
    return false;
  }

  deltasFile.close();
  mIsDirty = false;
  QgsLogger::debug( "Finished writing deltas JSON" );

  return true;
}


QStringList FeatureDeltas::attachmentFieldNames( const QString &layerId ) const
{
  if ( mCacheAttachmentFieldNames.contains( layerId ) )
    return mCacheAttachmentFieldNames.value( layerId );

  const QgsVectorLayer *vl = static_cast<QgsVectorLayer *>( QgsProject::instance()->mapLayer( layerId ) );
  QStringList attachmentFieldNames;

  if ( ! vl )
    return attachmentFieldNames;

  const QgsFields fields = vl->fields();

  for ( const QgsField &field : fields)
  {
    const QString type = vl->editFormConfig().widgetConfig( field.name() ).value( QStringLiteral( "type" ) ).toString();

    if ( type == QStringLiteral() )
      attachmentFieldNames.append( field.name() );
  }

  // TODO why this does not work?
  // mCacheAttachmentFieldNames.insert( layerId, attachmentFieldNames );

  return attachmentFieldNames;
}


QSet<QString> FeatureDeltas::attachmentFileNames() const
{
  // NOTE represents { layerId: { featureId: { attributeName: fileName } } }
  // We store all the changes in such mapping that we can return only the last attachment file name that is associated with a feature.
  // E.g. for given feature we start with attachment A.jpg, then we update to B.jpg. Later we change our mind and we apply C.jpg. In this case we only care about C.jpg.
  QMap<QString, QMap<QString, QMap<QString, QString>>> fileNamesMap;

  for ( const QJsonValue &deltaJson: qgis::as_const( mDeltas ) )
  {
    Q_ASSERT( deltaJson.isObject() );

    QVariantMap delta = deltaJson.toObject().toVariantMap();
    const QString layerId = delta.value( QStringLiteral( "layerId" ) ).toString();
    
    Q_ASSERT( ! layerId.isEmpty() );

    const QString method = delta.value( QStringLiteral( "method" ) ).toString();
    const QString fid = delta.value( QStringLiteral( "fid" ) ).toString();
    const QStringList attachmentFieldsList = attachmentFieldNames( layerId );

    QMap<QString, QMap<QString, QString>> featureAttributeFileNames = fileNamesMap.value( layerId );
    QMap<QString, QString> attributeFileNames = featureAttributeFileNames.value( fid );

    if ( method == QStringLiteral( "delete" ) || method == QStringLiteral( "patch" ) )
    {
      const QVariantMap oldData = delta.value( QStringLiteral( "old" ) ).toMap();

      Q_ASSERT( ! oldData.isEmpty() );
    }
    else if ( method == QStringLiteral( "create" ) || method == QStringLiteral( "patch" ) )
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
          if ( ! attachmentFieldsList.contains( fieldName ) )
            continue;

          attributeFileNames.insert( fieldName, attributes.value( fieldName ).toString() );
        }
      }
    }
    else
    {
      QgsLogger::debug( QStringLiteral( "File `%1` contains unknown method `%2`" ).arg( mFileName, method ) );
      Q_ASSERT(0);
    }

    featureAttributeFileNames.insert( fid, attributeFileNames );
    fileNamesMap.insert( layerId, featureAttributeFileNames );
  }

  QSet<QString> fileNames;

  for ( const QString &layerId : fileNamesMap.keys() )
  {
    const QMap<QString, QMap<QString, QString>> featureAttributeFileNames = fileNamesMap.value( layerId );
    for ( const QString &fid : featureAttributeFileNames.keys() )
    {
      const QMap<QString, QString> attributeFileNames = featureAttributeFileNames.value( fid );
      for ( const QString &fieldName : attributeFileNames.keys() )
      {
        fileNames.insert( attributeFileNames.value( fieldName ) );
      }
    }
  }

  return fileNames;
}


QByteArray FeatureDeltas::fileChecksum( const QString &fileName, const QCryptographicHash::Algorithm hashAlgorithm ) const
{
    QFile f(fileName);

    if ( ! f.open(QFile::ReadOnly) )
      return QByteArray();

    QCryptographicHash hash(hashAlgorithm);

    if ( hash.addData( &f ) )
      return hash.result();

    return QByteArray();
}


void FeatureDeltas::addPatch( const QString &layerId, const QgsFeature &oldFeature, const QgsFeature &newFeature )
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
        const QString oldFileName = oldVal.toString();
        const QByteArray oldFileChecksum = fileChecksum( oldFileName, QCryptographicHash::Sha256 );
        const QJsonValue oldFileChecksumJson = oldFileChecksum.isEmpty() ? QJsonValue::Null : QJsonValue( QString( oldFileChecksum ) );
        const QString newFileName = newVal.toString();
        const QByteArray newFileChecksum = fileChecksum( newFileName, QCryptographicHash::Sha256 );
        const QJsonValue newFileChecksumJson = newFileChecksum.isEmpty() ? QJsonValue::Null : QJsonValue( QString( newFileChecksum ) );

        tmpOldFileChecksums.insert( oldVal.toString(), oldFileChecksumJson );
        tmpNewFileChecksums.insert( newVal.toString(), newFileChecksumJson );
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


void FeatureDeltas::addDelete( const QString &layerId, const QgsFeature &oldFeature )
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

    if ( attachmentFieldsList.contains( name ) )
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


void FeatureDeltas::addCreate( const QString &layerId, const QgsFeature &newFeature )
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


QJsonValue FeatureDeltas::geometryToJsonValue( const QgsGeometry &geom ) const
{
  if ( geom.isNull() )
    return QJsonValue::Null;

  return QJsonValue( geom.asWkt() );
}