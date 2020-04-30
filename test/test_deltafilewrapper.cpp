/***************************************************************************
                          test_deltafilewrapper.h
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
#include "deltafilewrapper.h"


class TestDeltaFileWrapper: public QObject
{
    Q_OBJECT
  private slots:
    void testErrors()
    {
        // invalid filename
        DeltaFileWrapper invalidFileNameDfw( "" );
        QCOMPARE( invalidFileNameDfw.hasError(), true );

        // valid nonexisting file
        QString fileName( std::tmpnam( nullptr ) );
        DeltaFileWrapper validNonexistingFileDfw( fileName );
        QCOMPARE( validNonexistingFileDfw.hasError(), false );
        QVERIFY( QFileInfo::exists( fileName ) );
        DeltaFileWrapper validNonexistingFileCheckDfw( fileName );
        QCOMPARE( validNonexistingFileCheckDfw.hasError(), false );
        QJsonDocument validNonexistingFileDoc = normalizeSchema( validNonexistingFileCheckDfw.toString() );
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
        QTemporaryFile tmpFile( QDir::tempPath() + QStringLiteral( "/deltafile.json" ) );
        tmpFile.open();

        // invalid JSON
        QVERIFY( tmpFile.write( R""""( asd )"""" ) );
        tmpFile.flush();
        DeltaFileWrapper invalidJsonDfw( tmpFile.fileName() );
        QCOMPARE( invalidJsonDfw.hasError(), true );
        tmpFile.resize(0);
        
        // wrong version type
        QVERIFY( tmpFile.write( R""""({"version":5,"id":"11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper wrongVersionTypeDfw( tmpFile.fileName() );
        QCOMPARE( wrongVersionTypeDfw.hasError(), true );
        tmpFile.resize(0);
        
        // empty version
        QVERIFY( tmpFile.write( R""""({"version":"","id":"11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper emptyVersionDfw( tmpFile.fileName() );
        QCOMPARE( emptyVersionDfw.hasError(), true );
        tmpFile.resize(0);

        // wrong version number
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id":"11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper wrongVersionNumberDfw( tmpFile.fileName() );
        QCOMPARE( wrongVersionNumberDfw.hasError(), true );
        tmpFile.resize(0);

        // wrong id type
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": 5,"projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper wrongIdTypeDfw( tmpFile.fileName() );
        QCOMPARE( wrongIdTypeDfw.hasError(), true );
        tmpFile.resize(0);

        // empty id
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "","projectId":"projectId","deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper emptyIdDfw( tmpFile.fileName() );
        QCOMPARE( emptyIdDfw.hasError(), true );
        tmpFile.resize(0);

        // wrong projectId type
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "11111111-1111-1111-1111-111111111111","projectId":5,"deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper wrongProjectIdTypeDfw( tmpFile.fileName() );
        QCOMPARE( wrongProjectIdTypeDfw.hasError(), true );
        tmpFile.resize(0);

        // empty projectId
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "11111111-1111-1111-1111-111111111111","projectId":"","deltas":[]})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper emptyProjectIdDfw( tmpFile.fileName() );
        QCOMPARE( emptyProjectIdDfw.hasError(), true );
        tmpFile.resize(0);

        // wrong deltas type
        QVERIFY( tmpFile.write( R""""({"version":"2.0","id": "11111111-1111-1111-1111-111111111111","projectId":"projectId","deltas":{}})"""" ) );
        tmpFile.flush();
        DeltaFileWrapper wrongDeltasTypeDfw( tmpFile.fileName() );
        QCOMPARE( wrongDeltasTypeDfw.hasError(), true );
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
        DeltaFileWrapper correctExistingDfw( tmpFile.fileName() );
        QCOMPARE( correctExistingDfw.hasError(), false );
        QJsonDocument correctExistingDoc = normalizeSchema( correctExistingDfw.toString() );
        QVERIFY( ! correctExistingDoc.isNull() );
        QCOMPARE( correctExistingDoc, QJsonDocument::fromJson( correctExistingContents.toUtf8() ) );
        tmpFile.resize(0);
    }


    void testFileName()
    {
        QString fileName( std::tmpnam( nullptr ) );
        DeltaFileWrapper dfw( fileName );
        QCOMPARE( dfw.fileName(), fileName );
    }


    void testClear()
    {
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
        dfw.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( getDeltasArray( dfw.toString() ).size(), 1 );

        dfw.clear();

        QCOMPARE( getDeltasArray( dfw.toString() ).size(), 0 );
    }


    void testToString()
    {
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
        dfw.addCreate( "dummyLayerId", QgsFeature( QgsFields() , 100 ) );
        dfw.addDelete( "dummyLayerId", QgsFeature( QgsFields() , 101 ) );
        QJsonDocument doc = normalizeSchema( dfw.toString() );

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
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
        dfw.addCreate( "dummyLayerId", QgsFeature( QgsFields() , 100 ) );
        dfw.addDelete( "dummyLayerId", QgsFeature( QgsFields() , 101 ) );
        QJsonDocument doc = normalizeSchema( QString( dfw.toJson() ) );

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
        DeltaFileWrapper dfw( fileName );

        QCOMPARE( dfw.isDirty(), false );

        dfw.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( dfw.isDirty(), true );
        QVERIFY( dfw.toFile() );
        QCOMPARE( dfw.isDirty(), false );

        dfw.clear();

        QCOMPARE( dfw.isDirty(), true );
    }


    void testCount()
    {
        QString fileName = std::tmpnam( nullptr );
        DeltaFileWrapper dfw( fileName );
        dfw.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( dfw.count(), 1 );

        dfw.addCreate( "dummyLayerId", QgsFeature() );

        QCOMPARE( dfw.count(), 2 );

        dfw.clear();

        QCOMPARE( dfw.count(), 0 );
    }


    void testToFile()
    {
        QString fileName = std::tmpnam( nullptr );
        DeltaFileWrapper dfw1( fileName );
        dfw1.addCreate( "dummyLayerId", QgsFeature() );
        DeltaFileWrapper dfw2( fileName );

        QCOMPARE( getDeltasArray( dfw1.toString() ).size(), 1);
        QCOMPARE( getDeltasArray( dfw1.toString() ).size(), getDeltasArray( dfw2.toString() ).size() + 1 );

        dfw1.toFile();
        DeltaFileWrapper dfw3( fileName );

        QCOMPARE( getDeltasArray( dfw1.toString() ).size(), 1);
        // TODO make sure that dfw1 and dfw2 are in sync
        // QCOMPARE( getDeltasArray( dfw1.toString() ).size(), getDeltasArray( dfw2.toString() ).size() );
        QCOMPARE( getDeltasArray( dfw1.toString() ).size(), getDeltasArray( dfw3.toString() ).size() );
    }


    void testAddCreate()
    {
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
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
        dfw.addCreate( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        f.setGeometry( QgsGeometry() );
        dfw.addCreate( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        f.setFields( QgsFields(), true );
        f.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        dfw.addCreate( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
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

        dfw.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

        dfw.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        newFeature.setGeometry( QgsGeometry() );
        oldFeature.setGeometry( QgsGeometry() );

        dfw.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        newFeature.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        newFeature.setAttribute( QStringLiteral( "int" ), 42 );
        newFeature.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 23.398819, 41.7672147 ) ) );

        dfw.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        oldFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        newFeature.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );

        dfw.addPatch( "dummyLayerId", oldFeature, newFeature );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( "[]" ) );
    }


    void testAddDelete()
    {
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
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
        dfw.addDelete( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        f.setGeometry( QgsGeometry() );
        dfw.addDelete( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        dfw.clear();
        f.setFields( QgsFields(), true );
        f.setGeometry( QgsGeometry( new QgsPoint( 25.9657, 43.8356 ) ) );
        dfw.addDelete( "dummyLayerId", f );

        QCOMPARE( QJsonDocument( getDeltasArray( dfw.toString() ) ), QJsonDocument::fromJson( R""""(
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
        DeltaFileWrapper dfw( QString( std::tmpnam( nullptr ) ) );
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

        dfw.addCreate( "dummyLayerId1", f1 );
        dfw.addDelete( "dummyLayerId2", f2 );
        dfw.addDelete( "dummyLayerId1", f3 );

        QJsonDocument doc = normalizeSchema( dfw.toString() );

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

        if ( o.value( QStringLiteral( "version" ) ).toString() != DeltaFileWrapper::FormatVersion )
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

QFIELDTEST_MAIN( TestDeltaFileWrapper )
#include "test_deltafilewrapper.moc"
