/***************************************************************************
                        test_layerobserver.h
                        --------------------
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
      mLayerObserver->reset();
    }


    void cleanup()
    {
      mLayerObserver->reset();
    }


    void testGenerateDeltaFileName()
    {
      QVERIFY( LayerObserver::generateDeltaFileName( true ) != LayerObserver::generateDeltaFileName( false ) );
    }


    void testHasError()
    {
      // ? how I can test such thing?
      QSKIP("decide how we test errors");
      QCOMPARE( mLayerObserver->hasError(), false );
      QVERIFY( QFile::exists( LayerObserver::generateDeltaFileName( true ) ) );
    }


    void testCommit()
    {
      QString currentDeltaFileName = LayerObserver::generateDeltaFileName( true );
      QString committedDeltaFileName = LayerObserver::generateDeltaFileName( false );

      QVERIFY( QFile::exists( currentDeltaFileName ) );
      QVERIFY( QFile::exists( committedDeltaFileName ) );

      DeltaFileWrapper currentDeltaFile( currentDeltaFileName );
      DeltaFileWrapper committedDeltaFile( committedDeltaFileName );
      QString oldIdCurrentDeltaFile = currentDeltaFile.id();
      QString oldIdCommittedDeltaFile = committedDeltaFile.id();

      QgsFeature f1( mLayer->fields() );
      f1.setAttribute( QStringLiteral( "int" ), 1000 );
      f1.setAttribute( QStringLiteral( "str" ), "new_string1" );
      f1.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );


      QVERIFY( ! currentDeltaFile.hasError() );
      QVERIFY( ! committedDeltaFile.hasError() );
      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( currentDeltaFile.count(), 1 );
      QCOMPARE( committedDeltaFile.count(), 0 );
      QVERIFY( mLayerObserver->commit() );
      QCOMPARE( currentDeltaFile.count(), 0 );
      QCOMPARE( committedDeltaFile.count(), 1 );

      // Sometimes in test cases I need to have more than instance of DeltaFileWrapper of the same file. What is the best approach in this situation?
      // 1) Keep some kind of singleton based on the filename, so using the same instance everywhere.
      // 2) Add file system observer to keep the instances updated.
      DeltaFileWrapper currentDeltaFile2( currentDeltaFileName );
      DeltaFileWrapper committedDeltaFile2( committedDeltaFileName );

      QVERIFY( ! currentDeltaFile2.hasError() );
      QVERIFY( ! committedDeltaFile2.hasError() );
      QVERIFY( oldIdCurrentDeltaFile != currentDeltaFile2.id() );
      QVERIFY( oldIdCommittedDeltaFile == committedDeltaFile2.id() );
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
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ).size(), 1 );

      QgsFeature f2( mLayer->fields() );
      f2.setAttribute( QStringLiteral( "int" ), 1001 );
      f2.setAttribute( QStringLiteral( "str" ), "new_string2" );
      f2.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->addFeature( f1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ).size(), 2 );

      mLayerObserver->reset();
      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->commitChanges() );

      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ).size(), 0 );
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
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ), QStringList() );
      // when we stop editing, all changes are written
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ), QStringList({"create"}) );
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
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ), QStringList({"create"}) );
    }


    void testObservesRemoved()
    {
      QVERIFY( mLayer->startEditing() );
      QVERIFY( mLayer->deleteFeature( 1 ) );
      QVERIFY( mLayer->commitChanges() );
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ), QStringList({"delete"}) );
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
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ), QStringList({"patch", "patch"}) );
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
      QCOMPARE( getDeltaOperations( LayerObserver::generateDeltaFileName( true ) ), QStringList({"patch", "patch"}) );
    }
    
  private:

    std::unique_ptr<QgsVectorLayer> mLayer;
    std::unique_ptr<LayerObserver> mLayerObserver;

    QStringList getDeltaOperations( QString fileName )
    {
      QStringList operations;
      QFile deltaFile( fileName );

      if ( ! deltaFile.open( QIODevice::ReadOnly ) )
        return operations;
      
      QJsonDocument doc = QJsonDocument::fromJson( deltaFile.readAll() );

      if ( doc.isNull() )
        return operations;

      for ( const QJsonValue &v : doc.object().value( "deltas" ).toArray() )
        operations.append( v.toObject().value( QStringLiteral( "method" ) ).toString() );

      return operations;
    }
};

QFIELDTEST_MAIN( TestLayerObserver )
#include "test_layerobserver.moc"
