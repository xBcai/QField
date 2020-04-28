/***************************************************************************
                          test_featuredeltas.h
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
#include "featuredeltas.h"


class TestFeatureDeltas: public QObject
{
    Q_OBJECT
  private slots:
    void testErrors()
    {
        // invalid filename
        FeatureDeltas invalidFileNameFd( "" );
        QCOMPARE( invalidFileNameFd.hasError(), true );

        // valid nonexisting file
        QString fileName( std::tmpnam( nullptr ) );
        FeatureDeltas validNonexistingFileFd( fileName );
        QCOMPARE( validNonexistingFileFd.hasError(), false );
        QVERIFY( QFileInfo::exists( fileName ) );
        FeatureDeltas validNonexistingFileCheckFd( fileName );
        QCOMPARE( validNonexistingFileCheckFd.hasError(), false );
        QJsonDocument validNonexistingFileDoc = normalizeSchema( validNonexistingFileCheckFd.toString() );
        QVERIFY( ! validNonexistingFileDoc.isNull() );
        QCOMPARE( validNonexistingFileDoc, QJsonDocument::fromJson( R""""(
            {
                "deltas": [],
                "id": "11111111-1111-1111-1111-111111111111",
                "projectId": "projectId",
                "version": "1.0"
            }
        )"""" ) );


        // prepare temporary file
        QTemporaryFile tmpFile( QDir::tempPath() + QStringLiteral( "/feature_deltas.json" ) );
        tmpFile.open();

        // invalid JSON
        QVERIFY( tmpFile.write( R""""( asd )"""" ) );
        tmpFile.flush();
        FeatureDeltas invalidJsonFd( tmpFile.fileName() );
        QCOMPARE( invalidJsonFd.hasError(), true );
        tmpFile.resize(0);
        
        // wrong version type
        QVERIFY( tmpFile.write( R""""({"version":5,"id":"11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas wrongVersionTypeFd( tmpFile.fileName() );
        QCOMPARE( wrongVersionTypeFd.hasError(), true );
        tmpFile.resize(0);
        
        // empty version
        QVERIFY( tmpFile.write( R""""({"version":"","id":"11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas emptyVersionFd( tmpFile.fileName() );
        QCOMPARE( emptyVersionFd.hasError(), true );
        tmpFile.resize(0);

        // wrong version number
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id":"11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas wrongVersionNumberFd( tmpFile.fileName() );
        QCOMPARE( wrongVersionNumberFd.hasError(), true );
        tmpFile.resize(0);

        // wrong id type
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": 5,"projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas wrongIdTypeFd( tmpFile.fileName() );
        QCOMPARE( wrongIdTypeFd.hasError(), true );
        tmpFile.resize(0);

        // empty id
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas emptyIdFd( tmpFile.fileName() );
        QCOMPARE( emptyIdFd.hasError(), true );
        tmpFile.resize(0);

        // wrong projectId type
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "11111111-1111-1111-1111-111111111111","projectId":5,"deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas wrongProjectIdTypeFd( tmpFile.fileName() );
        QCOMPARE( wrongProjectIdTypeFd.hasError(), true );
        tmpFile.resize(0);

        // empty projectId
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "11111111-1111-1111-1111-111111111111","projectId":"","deltas":[]})"""" ) );
        tmpFile.flush();
        FeatureDeltas emptyProjectIdFd( tmpFile.fileName() );
        QCOMPARE( emptyProjectIdFd.hasError(), true );
        tmpFile.resize(0);

        // wrong deltas type
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":{}})"""" ) );
        tmpFile.flush();
        FeatureDeltas wrongDeltasTypeFd( tmpFile.fileName() );
        QCOMPARE( wrongDeltasTypeFd.hasError(), true );
        tmpFile.resize(0);

        // loads existing file
        QString correctExistingContents = QStringLiteral( R""""(
            {
                "deltas":[],
                "id":"11111111-1111-1111-1111-111111111111",
                "projectId":"projectId",
                "version":"1.0"
            }
        )"""" );
        QVERIFY( tmpFile.write( correctExistingContents.toUtf8() ) );
        tmpFile.flush();
        FeatureDeltas correctExistingFd( tmpFile.fileName() );
        QCOMPARE( correctExistingFd.hasError(), false );
        QJsonDocument correctExistingDoc = normalizeSchema( correctExistingFd.toString() );
        QVERIFY( ! correctExistingDoc.isNull() );
        QCOMPARE( correctExistingDoc, QJsonDocument::fromJson( correctExistingContents.toUtf8() ) );
        tmpFile.resize(0);
    }


    void testFileName()
    {
        QString fileName( std::tmpnam( nullptr ) );
        FeatureDeltas fd( fileName );
        QCOMPARE( fd.fileName(), fileName );
    }


    void testClear()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        fd.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( getDeltasArray( fd.toString() ).size(), 1 );

        fd.clear();

        QCOMPARE( getDeltasArray( fd.toString() ).size(), 0 );
    }


    void testToString()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        fd.addCreate( "dummyLayerId", QgsFeature( QgsFields() , 100 ) );
        fd.addDelete( "dummyLayerId", QgsFeature( QgsFields() , 101 ) );
        QJsonDocument doc = normalizeSchema( fd.toString() );

        QVERIFY( ! doc.isNull() );
        QCOMPARE( doc, QJsonDocument::fromJson( R""""(
            {
                "deltas": [
                    {
                        "fid": 100,
                        "layerId": "dummyLayerId",
                        "method": "create",
                        "new": {
                            "geometry": null
                        }
                    },
                    {
                        "fid": 101,
                        "layerId": "dummyLayerId",
                        "method": "delete",
                        "old": {
                            "geometry": null
                        }
                    }
                ],
                "id": "11111111-1111-1111-1111-111111111111",
                "projectId": "projectId",
                "version": "1.0"
            }
        )"""" ) );
    }


    void testToJson()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        fd.addCreate( "dummyLayerId", QgsFeature( QgsFields() , 100 ) );
        fd.addDelete( "dummyLayerId", QgsFeature( QgsFields() , 101 ) );
        QJsonDocument doc = normalizeSchema( QString( fd.toJson() ) );

        QVERIFY( ! doc.isNull() );
        QCOMPARE( doc, QJsonDocument::fromJson( R""""(
            {
                "deltas": [
                    {
                        "fid": 100,
                        "layerId": "dummyLayerId",
                        "method": "create",
                        "new": {
                            "geometry": null
                        }
                    },
                    {
                        "fid": 101,
                        "layerId": "dummyLayerId",
                        "method": "delete",
                        "old": {
                            "geometry": null
                        }
                    }
                ],
                "id": "11111111-1111-1111-1111-111111111111",
                "projectId": "projectId",
                "version": "1.0"
            }
        )"""" ) );
    }


    void testProjectId()
    {
      QSKIP("decide how we get the current project id");
    }


    void testIsDirty()
    {
        QString fileName = std::tmpnam( nullptr );
        FeatureDeltas fd( fileName );

        QCOMPARE( fd.isDirty(), false );

        fd.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( fd.isDirty(), true );
        QVERIFY( fd.toFile() );
        QCOMPARE( fd.isDirty(), false );

        fd.clear();

        QCOMPARE( fd.isDirty(), true );
    }


    void testCount()
    {
        QString fileName = std::tmpnam( nullptr );
        FeatureDeltas fd( fileName );
        fd.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( fd.count(), 1 );

        fd.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( fd.count(), 2 );

        fd.clear();

        QCOMPARE( fd.count(), 0 );
    }


    void testToFile()
    {
        QString fileName = std::tmpnam( nullptr );
        FeatureDeltas fd1( fileName );
        fd1.addCreate( "dummyLayerId", QgsFeature() );
        FeatureDeltas fd2( fileName );

        QCOMPARE( getDeltasArray( fd1.toString() ).size(), 1);
        QCOMPARE( getDeltasArray( fd1.toString() ).size(), getDeltasArray( fd2.toString() ).size() + 1 );

        fd1.toFile();
        FeatureDeltas fd3( fileName );

        QCOMPARE( getDeltasArray( fd1.toString() ).size(), 1);
        // TODO make sure that fd1 and fd2 are in sync
        // QCOMPARE( getDeltasArray( fd1.toString() ).size(), getDeltasArray( fd2.toString() ).size() );
        QCOMPARE( getDeltasArray( fd1.toString() ).size(), getDeltasArray( fd3.toString() ).size() );
    }


    void testAddCreate()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        QgsFields fields;
        fields.append( QgsField( "dbl", QVariant::Double, "double" ) );
        fields.append( QgsField( "int", QVariant::Int, "integer" ) );
        fields.append( QgsField( "str", QVariant::String, "text" ) );
        QgsFeature f(fields, 100);
        f.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        f.setAttribute( QStringLiteral( "int" ), 42 );
        f.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );

        // Check if creates delta of a feature with a geometry
        f.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        fd.addCreate( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "create",
                    "new": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        },
                        "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                    }
                }
            ]
        )"""" ) );


        // Check if creates delta of a feature with a NULL geometry. 
        // NOTE this is the same as calling f clearGeometry()
        fd.clear();
        f.setGeometry( QgsGeometry() );
        fd.addCreate( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "create",
                    "new": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        },
                        "geometry": null
                    }
                }
            ]
        )"""" ) );


        // Check if creates delta of a feature without attributes
        fd.clear();
        f.setFields( QgsFields(), true );
        f.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        fd.addCreate( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "create",
                    "new": {
                        "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                    }
                }
            ]
        )"""" ) );
    }


    void testAddPatch()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        QgsFields fields;
        fields.append( QgsField( "dbl", QVariant::Double, "double" ) );
        fields.append( QgsField( "int", QVariant::Int, "integer" ) );
        fields.append( QgsField( "str", QVariant::String, "text" ) );
        QgsFeature oldFeature(fields, 100);
        oldFeature.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        oldFeature.setAttribute( QStringLiteral( "int" ), 42 );
        oldFeature.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );
        QgsFeature newFeature(fields, 100);
        newFeature.setAttribute( QStringLiteral( "dbl" ), 9.81 );
        newFeature.setAttribute( QStringLiteral( "int" ), 680 );
        newFeature.setAttribute( QStringLiteral( "str" ), QStringLiteral( "pingy" ) );


        // Patch both the attributes and the geometry
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 23.398819, 41.7672147 ) ) );

        fd.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "patch",
                    "new": {
                        "attributes": {
                            "dbl": 9.81,
                            "int": 680,
                            "str": "pingy"
                        },
                        "geometry": "Point (23.39881899999999959 41.7672146999999967)"
                    },
                    "old": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        },
                        "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                    }
                }
            ]
        )"""" ) );


        // Patch attributes only
        fd.clear();
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

        fd.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "patch",
                    "new": {
                        "attributes": {
                            "dbl": 9.81,
                            "int": 680,
                            "str": "pingy"
                        }
                    },
                    "old": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        }
                    }
                }
            ]
        )"""" ) );


        // Patch feature without geometry on attributes only
        fd.clear();
        newFeature.setGeometry( QgsGeometry() );
        oldFeature.setGeometry( QgsGeometry() );

        fd.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "patch",
                    "new": {
                        "attributes": {
                            "dbl": 9.81,
                            "int": 680,
                            "str": "pingy"
                        },
                        "geometry": null
                    },
                    "old": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        },
                        "geometry": null
                    }
                }
            ]
        )"""" ) );


        // Patch geometry only
        fd.clear();
        newFeature.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        newFeature.setAttribute( QStringLiteral( "int" ), 42 );
        newFeature.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 23.398819, 41.7672147 ) ) );

        fd.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "patch",
                    "new": {
                        "geometry": "Point (23.39881899999999959 41.7672146999999967)"
                    },
                    "old": {
                        "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                    }
                }
            ]
        )"""" ) );


        // Do not patch equal features
        fd.clear();
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

        fd.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( "[]" ) );
    }


    void testAddDelete()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        QgsFields fields;
        fields.append( QgsField( "dbl", QVariant::Double, "double" ) );
        fields.append( QgsField( "int", QVariant::Int, "integer" ) );
        fields.append( QgsField( "str", QVariant::String, "text" ) );
        QgsFeature f(fields, 100);
        f.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        f.setAttribute( QStringLiteral( "int" ), 42 );
        f.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );

        // Check if creates delta of a feature with a geometry
        f.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        // ? why this is not working, as QgsPoint is QgsAbstractGeometry and there is example in the docs? https://qgis.org/api/classQgsFeature.html#a14dcfc99b476b613c21b8c35840ff388
        // f.setGeometry( QgsPoint( 25.9657, 43.8356 ) );
        fd.addDelete( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "delete",
                    "old": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        },
                        "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                    }
                }
            ]
        )"""" ) );


        // Check if creates delta of a feature with a NULL geometry. 
        // NOTE this is the same as calling f clearGeometry()
        fd.clear();
        f.setGeometry( QgsGeometry() );
        fd.addDelete( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "delete",
                    "old": {
                        "attributes": {
                            "dbl": 3.14,
                            "int": 42,
                            "str": "stringy"
                        },
                        "geometry": null
                    }
                }
            ]
        )"""" ) );


        // Check if creates delta of a feature without attributes
        fd.clear();
        f.setFields( QgsFields(), true );
        f.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        fd.addDelete( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( fd.toString() ) ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
                    "method": "delete",
                    "old": {
                        "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                    }
                }
            ]
        )"""" ) );
    }

    void testMultipleDeltaAdd()
    {
        FeatureDeltas fd( QString( std::tmpnam( nullptr ) ) );
        QgsFields fields;
        fields.append( QgsField( "dbl", QVariant::Double, "double" ) );
        fields.append( QgsField( "int", QVariant::Int, "integer" ) );
        fields.append( QgsField( "str", QVariant::String, "text" ) );
        QgsFeature f1(fields, 100);
        f1.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        f1.setAttribute( QStringLiteral( "int" ), 42 );
        f1.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );

        QgsFeature f2( QgsFields(), 101);
        f2.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

        QgsFeature f3(fields, 102);

        fd.addCreate( "dummyLayerId1", f1 );
        fd.addDelete( "dummyLayerId2", f2 );
        fd.addDelete( "dummyLayerId1", f3 );

        QJsonDocument doc = normalizeSchema( fd.toString() );

        QVERIFY( ! doc.isNull() );
        QCOMPARE( doc, QJsonDocument::fromJson( R""""(
            {
                "deltas": [
                        {
                            "fid": 100,
                        "layerId": "dummyLayerId1",
                        "method": "create",
                        "new": {
                                "attributes": {
                                    "dbl": 3.14,
                                "int": 42,
                                "str": "stringy"
                            },
                            "geometry": null
                        }
                    },
                    {
                            "fid": 101,
                        "layerId": "dummyLayerId2",
                        "method": "delete",
                        "old": {
                                "geometry": "Point (25.96569999999999823 43.83559999999999945)"
                        }
                    },
                    {
                            "fid": 102,
                        "layerId": "dummyLayerId1",
                        "method": "delete",
                        "old": {
                                "attributes": {
                                    "dbl": null,
                                "int": null,
                                "str": null
                            },
                            "geometry": null
                        }
                    }
                ],
                "id": "11111111-1111-1111-1111-111111111111",
                "projectId": "projectId",
                "version": "1.0"
            }
        )"""" ) );
    }

  private:
    /**
     * Normalized the random part of the delta file JSON schema to static values.
     * "id"         - "11111111-1111-1111-1111-111111111111"
     * "projectId"  - "projectId"
     * 
     * @param json - JSON string
     * @return QJsonDocument normalized JSON document. NULL document if the input is invalid.
     */
    QJsonDocument normalizeSchema ( const QString &json )
    {
        QJsonDocument doc = QJsonDocument::fromJson( json.toUtf8() );

        if ( doc.isNull() )
            return doc;
        
        QJsonObject o = doc.object();

        if ( o.value( QStringLiteral( "version" ) ).toString() != FeatureDeltas::FormatVersion )
            return QJsonDocument();
        if ( o.value( QStringLiteral( "projectId" ) ).toString().size() == 0 )
            return QJsonDocument();
        if ( QUuid::fromString( o.value( QStringLiteral( "id" ) ).toString() ).isNull() )
            return QJsonDocument();
        if ( ! o.value( QStringLiteral( "deltas" ) ).isArray() )
            return QJsonDocument();

        // normalize non-constant values
        o.insert( QStringLiteral( "id" ), QStringLiteral( "11111111-1111-1111-1111-111111111111" ) );
        o.insert( QStringLiteral( "projectId" ), QStringLiteral( "projectId" ) );

        return QJsonDocument( o );
    }

    QJsonArray getDeltasArray ( const QString &json )
    {
        return QJsonDocument::fromJson( json.toUtf8() )
            .object()
            .value( QStringLiteral( "deltas" ) )
            .toArray();
    }
};

QFIELDTEST_MAIN( TestFeatureDeltas )
#include "test_featuredeltas.moc"
