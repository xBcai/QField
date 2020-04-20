#include <QtTest>
#include <QTemporaryFile>
#include <QFileInfo>

#include "qfield_testbase.h"
#include "featuredeltas.h"


#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>

class TestFeatureDeltas: public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
        
    }

    void testHasError()
    {
        
    }

    void testToString()
    {
        
    }

    void testToJson()
    {
        
    }

    void testAddCreate()
    {
        
    }

    void testAddPatch()
    {

    }

/**
{
    "deltas": [
        {
            "fid": 100,
            "layerId": "dummyLayerId",
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
    "id": "019f9cfa-e127-4cba-a06f-4498e1c4dacb",
    "projectId": "",
    "timestamp": "2020-04-20T22:35:21.171Z",
    "version": "1.0"
}
**/
// qDebug() << fd.toString();

    void testAddDelete()
    {
        FeatureDeltas fd( QFileInfo( QTemporaryFile() ).absoluteFilePath() );
        QgsFields fields;
        fields.append( QgsField( "dbl", QVariant::Double, "double" ) );
        fields.append( QgsField( "int", QVariant::Int, "integer" ) );
        fields.append( QgsField( "str", QVariant::String, "text" ) );
        QgsFeature f(fields, 100);
        f.setAttribute( QStringLiteral( "dbl" ), 3.14 );
        f.setAttribute( QStringLiteral( "int" ), 42 );
        f.setAttribute( QStringLiteral( "str" ), QStringLiteral( "stringy" ) );

        fd.addDelete( "dummyLayerId", f );

        QJsonArray deltas = QJsonDocument::fromJson( fd.toJson() )
            .object()
            .value( QStringLiteral( "deltas" ) )
            .toArray();

        QCOMPARE( QJsonDocument( deltas ), QJsonDocument::fromJson( R""""(
            [
                {
                    "fid": 100,
                    "layerId": "dummyLayerId",
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
    }

    void cleanupTestCase()
    {

    }


};

QFIELDTEST_MAIN( TestFeatureDeltas )
#include "test_featuredeltas.moc"
