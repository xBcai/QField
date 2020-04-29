/***************************************************************************
                          test_layerobserver.h
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

#include <QtTest>

#include "qfield_testbase.h"
#include "layerobserver.h"


class TestLayerObserver: public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
      mLayer.reset( new QgsVectorLayer( QStringLiteral( "Point?crs=EPSG:3857&field=int:integer&field=str:string" ), QStringLiteral( "int" ), QStringLiteral( "memory" ) ) );
      QVERIFY( mLayer->isValid() );

      QgsFeature f1( mLayer->fields() );
      f1.setAttribute( QStringLiteral( "int" ), 1 );
      f1.setAttribute( QStringLiteral( "str" ), "string1" );
      f1.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
      QgsFeature f2( mLayer->fields() );
      f2.setAttribute( QStringLiteral( "int" ), 2 );
      f2.setAttribute( QStringLiteral( "str" ), "string2" );
      f2.setGeometry( QgsGeometry( new QgsPoint( 23.398819, 41.7672147 ) ) );
      QgsFeature f3( mLayer->fields() );
      f3.setAttribute( QStringLiteral( "int" ), 3 );
      f3.setAttribute( QStringLiteral( "str" ), "string3" );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      QVERIFY( mLayer->addFeature( f2 ) );
      QVERIFY( mLayer->addFeature( f3 ) );
      QVERIFY( mLayer->commitChanges() );

      QTemporaryDir projectDir;
      projectDir.setAutoRemove( false );

      QVERIFY2( projectDir.isValid(), "Failed to create temp dir" );
      
      QgsProject::instance()->setPresetHomePath( projectDir.path() );
      mLayerObserver.reset( new LayerObserver( QgsProject::instance() ) );
      
      QVERIFY( QgsProject::instance()->addMapLayer( mLayer.get(), false, false ) );
      QVERIFY( ! mLayerObserver->hasError() );
    }


    void init()
    {
      mLayerObserver->clear();
    }


    void cleanup()
    {
      mLayerObserver->clear();
    }


    void testProjectDeltaFiles()
    {
      QTemporaryDir dir;
      
      QVERIFY( dir.isValid() );

      QStringList fileNames;

      int limit = 5;
      int counter = 0;

      while ( counter++ < limit )
      {
        QString fileName = QStringLiteral( "%1/deltafile_%2_%3.json" ).arg( dir.path() ).arg( QDateTime::currentSecsSinceEpoch() ).arg( counter );
        fileNames.append( fileName );
        
        QVERIFY( QFile( fileName ).open( QIODevice::ReadWrite ) );
      }

      // make sure the Layer Observer is no longer dirty
      QVERIFY( mLayerObserver->commit() );

      QgsProject::instance()->setPresetHomePath( dir.path() );
      QFileInfoList deltaFilesInfo = LayerObserver::projectDeltaFiles();
      QStringList deltaFileNames;

      for ( const QFileInfo &fi : deltaFilesInfo )
        deltaFileNames.append( fi.absoluteFilePath() );


      QCOMPARE( deltaFileNames, fileNames );
    }


    void testHasError()
    {
      // ? how I can test such thing?
      QSKIP("decide how we test errors");
      QCOMPARE( mLayerObserver->hasError(), false );
      QVERIFY( QFile::exists( mLayerObserver->fileName() ) );
    }


    void testFileName()
    {
      qDebug() << mLayerObserver->fileName();
      QVERIFY( mLayerObserver->fileName().size() > 0 );
      QVERIFY( QFile::exists( mLayerObserver->fileName() ) );
    }


    void testCommit()
    {
      QString fileNameOld = mLayerObserver->fileName();

      QVERIFY( mLayerObserver->fileName().size() > 0 );
      QVERIFY( QFile::exists( mLayerObserver->fileName() ) );
      QVERIFY( mLayerObserver->commit() );
  
      QFileInfoList deltaFilesInfo = LayerObserver::projectDeltaFiles();
      
      // other tests might have created other delta files, therefore > (greater than)
      QVERIFY( deltaFilesInfo.size() >= 1 );

      QString fileNameNew = deltaFilesInfo.last().absoluteFilePath();
  
      QVERIFY( QFile::exists( fileNameNew ) );
      QVERIFY( fileNameOld != fileNameNew );
    }


    void testClear()
    {
      QgsFeature f1( mLayer->fields() );
      f1.setAttribute( QStringLiteral( "int" ), 1000 );
      f1.setAttribute( QStringLiteral( "str" ), "new_string1" );
      f1.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ).size(), 1 );

      QgsFeature f2( mLayer->fields() );
      f2.setAttribute( QStringLiteral( "int" ), 1001 );
      f2.setAttribute( QStringLiteral( "str" ), "new_string2" );
      f2.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ).size(), 2 );

      mLayerObserver->clear();
      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->commitChanges() );

      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ).size(), 0 );
    }


    void testObservesEditingStopped()
    {
      QgsFeature f1( mLayer->fields() );
      f1.setAttribute( QStringLiteral( "int" ), 1002 );
      f1.setAttribute( QStringLiteral( "str" ), "new_string" );
      f1.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      // the changes are not written on the disk yet
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ), QStringList() );
      // when we stop editing, all changes are written
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ), QStringList({"create"}) );
    }


    void testObservesAdded()
    {
      QgsFeature f1( mLayer->fields() );
      f1.setAttribute( QStringLiteral( "int" ), 1003 );
      f1.setAttribute( QStringLiteral( "str" ), "new_string" );
      f1.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ), QStringList({"create"}) );
    }


    void testObservesRemoved()
    {
      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->deleteFeature( 1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ), QStringList({"delete"}) );
    }


    void testObservesAttributeValueChanges()
    {
      QgsFeature f1 = mLayer->getFeature( 2 );
      f1.setAttribute( QStringLiteral( "str" ), f1.attribute( QStringLiteral( "str" ) ).toString() + "_new" );
      QgsFeature f2 = mLayer->getFeature( 3 );
      f2.setAttribute( QStringLiteral( "str" ), f2.attribute( QStringLiteral( "str" ) ).toString() + "_new" );
      f2.setGeometry( QgsGeometry( new QgsPoint( 88.7695313, 51.0897229 ) ) );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->updateFeature( f1 ) );
      QVERIFY( mLayer->updateFeature( f2 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ), QStringList({"patch", "patch"}) );
    }


    void testObservesGeometryChanges()
    {
      QVERIFY( mLayer->startEditing() );

      QgsFeature f1 = mLayer->getFeature( 2 );
      f1.setGeometry( QgsGeometry( new QgsPoint( 13.0545044, 47.8094654 ) ) );
      QgsFeature f2 = mLayer->getFeature( 3 );
      f2.setAttribute( QStringLiteral( "str" ), f2.attribute( QStringLiteral( "str" ) ).toString() + "_new" );
      f2.setGeometry( QgsGeometry( new QgsPoint( 13.0545044, 47.8094654 ) ) );

      QVERIFY( mLayer->updateFeature( f1 ) );
      QVERIFY( mLayer->updateFeature( f2 ) );

      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( mLayerObserver->fileName() ), QStringList({"patch", "patch"}) );
    }
    
  private:

    std::unique_ptr<QgsVectorLayer> mLayer;
    std::unique_ptr<LayerObserver> mLayerObserver;

    QStringList getDeltaOperations( QString fileName )
    {
      QStringList operations;
      QFile deltasFile( fileName );

      if ( ! deltasFile.open( QIODevice::ReadOnly ) )
        return operations;
      
      QJsonDocument doc = QJsonDocument::fromJson( deltasFile.readAll() );

      if ( doc.isNull() )
        return operations;

      for ( const QJsonValue &v : doc.object().value( "deltas" ).toArray() )
        operations.append( v.toObject().value( QStringLiteral( "method" ) ).toString() );

      return operations;
    }
};

QFIELDTEST_MAIN( TestLayerObserver )
#include "test_layerobserver.moc"
