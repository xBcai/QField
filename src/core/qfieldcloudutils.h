#ifndef QFIELDCLOUDUTILS_H
#define QFIELDCLOUDUTILS_H

class QString;

class QFieldCloudUtils
{
  public:
  
  static const QString localCloudDirectory();
  static const QString localProjectFilePath( const QString &projectId );
};

#endif // QFIELDCLOUDUTILS_H
