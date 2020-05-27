/***************************************************************************
                        layerobserver.cpp
                        -----------------
  begin                : Apr 2020
  copyright            : (C) 2020 by Ivan Ivanov
  email                : ivan@opengis.ch
***************************************************************************/

/***************************************************************************
 *                                     *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or   *
 *   (at your option) any later version.                   *
 *                                     *
 ***************************************************************************/

#include "layerobserver.h"
#include "qfieldcloudutils.h"

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsvectorlayereditbuffer.h>

#include <QDir>


LayerObserver::LayerObserver( const QgsProject *project )
  : mProject( project )
{
  mCurrentDeltaFileWrapper.reset( new DeltaFileWrapper( mProject, generateDeltaFileName( true ) ) );
  mCommittedDeltaFileWrapper.reset( new DeltaFileWrapper( mProject, generateDeltaFileName( false ) ) );

  connect( mProject, &QgsProject::homePathChanged, this, &LayerObserver::onHomePathChanged );
  connect( mProject, &QgsProject::layersAdded, this, &LayerObserver::onLayersAdded );
}


QString LayerObserver::generateDeltaFileName( bool isCurrentDeltaFile )
{
  return ( isCurrentDeltaFile )
    ? QStringLiteral( "%1/deltafile.json" )
        .arg( mProject->homePath() )
    : QStringLiteral( "%1/deltafile_commited.json" )
        .arg( mProject->homePath() );
}


bool LayerObserver::hasError() const
{
  return mCurrentDeltaFileWrapper->hasError() && mCommittedDeltaFileWrapper->hasError();
}


bool LayerObserver::commit()
{
  // First make sure the current delta file is synced to the disk storage. There might be some unsynced changes.
  if ( ! mCurrentDeltaFileWrapper->toFile() )
  {
    QgsLogger::warning( QStringLiteral( "Cannot write the current delta file: %1" ).arg( mCurrentDeltaFileWrapper->errorString() ) );
    return false;
  }

  // If the delta file is empty, there is nothing to commit, so we return success.
  if ( mCurrentDeltaFileWrapper->count() == 0 )
    return true;

  // Try to append the contents of the current delta file to the committed one. Very unlikely to break there.
  if ( ! mCommittedDeltaFileWrapper->append( mCurrentDeltaFileWrapper.get() ) )
  {
    QgsLogger::warning( QStringLiteral( "Unable to append delta file wrapper contents!" ) );
    return false;
  }

  // Make sure the committed changes are synced to the disk storage.
  if ( ! mCommittedDeltaFileWrapper->toFile() )
  {
    QgsLogger::warning( QStringLiteral( "Cannot write the committed delta file: %1" ).arg( mCommittedDeltaFileWrapper->errorString() ) );
    return false;
  }

  // Create brand new delta file for the current (uncommitted) deltas.
  mCurrentDeltaFileWrapper->reset( true );

  // Make sure the brand new current delta file is synced to the disk storage.
  if ( ! mCurrentDeltaFileWrapper->toFile() )
  {
    QgsLogger::warning( QStringLiteral( "Cannot write the current delta file: %1" ).arg( mCurrentDeltaFileWrapper->errorString() ) );
    return false;
  }

  // Successfully committed!
  return true;
}


void LayerObserver::reset( bool isHardReset ) const
{
  return mCurrentDeltaFileWrapper->reset( isHardReset );
}


void LayerObserver::onHomePathChanged()
{
  Q_ASSERT( ! mCurrentDeltaFileWrapper->isDirty() );
  Q_ASSERT( ! mCommittedDeltaFileWrapper->isDirty() );

  // we should make deltas only on cloud projects
  if ( QFieldCloudUtils::getProjectId( mProject ).isEmpty() )
    return;

  mCurrentDeltaFileWrapper.reset( new DeltaFileWrapper( mProject, generateDeltaFileName( true ) ) );
  mCommittedDeltaFileWrapper.reset( new DeltaFileWrapper( mProject, generateDeltaFileName( false ) ) );
}


void LayerObserver::onLayersAdded( const QList<QgsMapLayer *> layers )
{
  // we should keep track only of the layers on cloud projects
  if ( QFieldCloudUtils::getProjectId( mProject ).isEmpty() )
    return;

  for ( QgsMapLayer *layer : layers )
  {
    QgsVectorLayer *vl = dynamic_cast<QgsVectorLayer *>( layer );

    if ( vl && vl->dataProvider() )
    {
      // keep track of `offline` or `cloud` layers has changed, so we should sync them
      if ( QFieldCloudUtils::layerAction( vl ) == QFieldCloudProjectsModel::LayerAction::Offline )
      {
        // we just make sure that a committed `offline` layer mark the project as dirty
        // TODO use the future "afterCommitChanges" signal
        connect( vl, &QgsVectorLayer::editingStopped, this, &LayerObserver::onEditingStopped );
      }
      else if ( QFieldCloudUtils::layerAction( vl ) == QFieldCloudProjectsModel::LayerAction::Cloud )
      {
        // for `cloud` projects, we keep track of any change that has occurred
        connect( vl, &QgsVectorLayer::beforeCommitChanges, this, &LayerObserver::onBeforeCommitChanges );
        connect( vl, &QgsVectorLayer::committedFeaturesAdded, this, &LayerObserver::onCommittedFeaturesAdded );
        connect( vl, &QgsVectorLayer::committedFeaturesRemoved, this, &LayerObserver::onCommittedFeaturesRemoved );
        connect( vl, &QgsVectorLayer::committedAttributeValuesChanges, this, &LayerObserver::onCommittedAttributeValuesChanges );
        connect( vl, &QgsVectorLayer::committedGeometriesChanges, this, &LayerObserver::onCommittedGeometriesChanges );
        // TODO use the future "afterCommitChanges" signal
        connect( vl, &QgsVectorLayer::editingStopped, this, &LayerObserver::onEditingStopped );
      }
    }
  }
}


void LayerObserver::onBeforeCommitChanges()
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  QgsVectorLayerEditBuffer *eb = vl->editBuffer();

  if ( ! eb )
    return;

  const QgsFeatureIds deletedFids = eb->deletedFeatureIds();
  const QgsFeatureIds changedGeometriesFids = eb->changedGeometries().keys().toSet();
  const QgsFeatureIds changedAttributesFids = eb->changedAttributeValues().keys().toSet();
  // NOTE QgsFeatureIds underlying implementation is QSet, so no need to check if the QgsFeatureId already exists
  QgsFeatureIds changedFids;

  for ( const QgsFeatureId fid : deletedFids )
    changedFids.insert( fid );

  for ( const QgsFeatureId fid : changedGeometriesFids )
    changedFids.insert( fid );

  for ( const QgsFeatureId fid : changedAttributesFids )
    changedFids.insert( fid );

  // NOTE we read the features from the dataProvider directly as we want to access the old values.
  // If we use the layer, we get the values from the edit buffer.
  QgsFeatureIterator featuresIt = vl->dataProvider()->getFeatures( QgsFeatureRequest( changedFids ) );
  QgsChangedFeatures changedFeatures;
  QgsFeature f;

  // ? is it possible to use the iterator in a less ugly way? something like normal `for ( QgsFeature &f : it ) {}`
  while ( featuresIt.nextFeature( f ) )
    changedFeatures.insert( f.id(), f );

  // NOTE no need to keep track of added features, as they are always present in the layer after commit
  mChangedFeatures.insert( vl->id(), changedFeatures );
  mPatchedFids.insert( vl->id(), QgsFeatureIds() );
}


void LayerObserver::onCommittedFeaturesAdded( const QString &layerId, const QgsFeatureList &addedFeatures )
{
  for ( const QgsFeature &newFeature : addedFeatures )
  {
    mCurrentDeltaFileWrapper->addCreate( layerId, newFeature );
  }
}


void LayerObserver::onCommittedFeaturesRemoved( const QString &layerId, const QgsFeatureIds &deletedFeatureIds )
{
  QgsChangedFeatures changedFeatures = mChangedFeatures.value( layerId );

  for ( const QgsFeatureId &fid : deletedFeatureIds )
  {
    Q_ASSERT( changedFeatures.contains( fid ) );

    QgsFeature oldFeature = changedFeatures.take( fid );
    mCurrentDeltaFileWrapper->addDelete( layerId, oldFeature );
  }

  mChangedFeatures.insert( layerId, changedFeatures );
}


void LayerObserver::onCommittedAttributeValuesChanges( const QString &layerId, const QgsChangedAttributesMap &changedAttributesValues )
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  QgsFeatureIds patchedFids = mPatchedFids.value( layerId );
  QgsChangedFeatures changedFeatures = mChangedFeatures.value( layerId );
  const QgsFeatureIds changedAttributesValuesFids = changedAttributesValues.keys().toSet();

  for ( const QgsFeatureId &fid : changedAttributesValuesFids )
  {
    if ( patchedFids.contains( fid ) )
      continue;

    Q_ASSERT( changedFeatures.contains( fid ) );

    patchedFids.insert( fid );

    QgsFeature oldFeature = changedFeatures.take( fid );
    QgsFeature newFeature = vl->getFeature( fid );
    mCurrentDeltaFileWrapper->addPatch( layerId, oldFeature, newFeature );
  }

  mPatchedFids.insert( layerId, patchedFids );
  mChangedFeatures.insert( layerId, changedFeatures );
}


void LayerObserver::onCommittedGeometriesChanges( const QString &layerId, const QgsGeometryMap &changedGeometries )
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  QgsFeatureIds patchedFids = mPatchedFids.value( layerId );
  QgsChangedFeatures changedFeatures = mChangedFeatures.value( layerId );
  const QgsFeatureIds changedGeometriesFids = changedGeometries.keys().toSet();

  for ( const QgsFeatureId &fid : changedGeometriesFids )
  {
    if ( patchedFids.contains( fid ) )
      continue;

    Q_ASSERT( changedFeatures.contains( fid ) );

    patchedFids.insert(fid);

    QgsFeature oldFeature = changedFeatures.take( fid );
    QgsFeature newFeature = vl->getFeature( fid );

    mCurrentDeltaFileWrapper->addPatch( layerId, oldFeature, newFeature );
  }

  mPatchedFids.insert( layerId, patchedFids );
  mChangedFeatures.insert( layerId, changedFeatures );
}


void LayerObserver::onEditingStopped( )
{
  const QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  const QFieldCloudProjectsModel::LayerAction layerAction = QFieldCloudUtils::layerAction( vl );

  if ( layerAction == QFieldCloudProjectsModel::LayerAction::Offline  )
  {
    // TODO make the project dirty
    return;
  }
  else if ( layerAction != QFieldCloudProjectsModel::LayerAction::Cloud  )
  {
    mPatchedFids.take( vl->id() );
    mChangedFeatures.take( vl->id() );

    if ( ! mCurrentDeltaFileWrapper->toFile() )
    {
      // TODO somehow indicate the user that writing failed
      QgsLogger::warning( QStringLiteral( "Failed writing JSON file" ) );
    }
  }
}
