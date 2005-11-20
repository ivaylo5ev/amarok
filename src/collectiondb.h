// (c) 2004 Mark Kretschmann <markey@web.de>
// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// (c) 2004 Sami Nieminen <sami.nieminen@iki.fi>
// (c) 2005 Ian Monroe <ian@monroe.nu>
// (c) 2005 Jeff Mitchell <kde-dev@emailgoeshere.com>
// See COPYING file for licensing information.

#ifndef AMAROK_COLLECTIONDB_H
#define AMAROK_COLLECTIONDB_H

#include "engineobserver.h"
#include <kurl.h>
#include <qdir.h>            //stack allocated
#include <qdatetime.h>
#include <qimage.h>
#include <qobject.h>         //baseclass
#include <qptrqueue.h>       //baseclass
#include <qsemaphore.h>      //stack allocated
#include <qstringlist.h>     //stack allocated
#include <qptrvector.h>
#include <qthread.h>
#include <qmutex.h>

class DbConnection;
class CoverFetcher;
class MetaBundle;
class Scrobbler;


class DbConfig
{};


class SqliteConfig : public DbConfig
{
    public:
        SqliteConfig( const QString& /* dbfile */ );

        const QString dbFile() const { return m_dbfile; }

    private:
        QString m_dbfile;
};


class MySqlConfig : public DbConfig
{
    public:
        MySqlConfig(
            const QString& /* host */,
            const int /* port */,
            const QString& /* database */,
            const QString& /* username */,
            const QString& /* password */);

        const QString host() const { return m_host; }
        const int port() const { return m_port; }
        const QString database() const { return m_database; }
        const QString username() const { return m_username; }
        const QString password() const { return m_password; }

    private:
        QString m_host;
        int m_port;
        QString m_database;
        QString m_username;
        QString m_password;
};


class PostgresqlConfig : public DbConfig
{
    public:
        PostgresqlConfig(
            const QString& /* conninfo*/);

        const QString conninfo() const { return m_conninfo; }

    private:
      QString m_conninfo;
};


class DbConnection
{
    public:
        enum DbConnectionType { sqlite = 0, mysql = 1, postgresql = 2 };

        DbConnection( DbConfig* /* config */ );
        virtual ~DbConnection() = 0;

        virtual QStringList query( const QString& /* statement */ ) = 0;
        virtual int insert( const QString& /* statement */, const QString& /* table */ ) = 0;
        const bool isInitialized() const { return m_initialized; }
        virtual bool isConnected() const = 0;
        virtual const QString lastError() const { return "None"; }
    protected:
        bool m_initialized;
        DbConfig *m_config;
};


typedef struct sqlite3 sqlite3;
typedef struct sqlite3_context sqlite3_context;
typedef struct Mem sqlite3_value;

class SqliteConnection : public DbConnection
{
    public:
        SqliteConnection( SqliteConfig* /* config */ );
       ~SqliteConnection();

        QStringList query( const QString& /* statement */ );
        int insert( const QString& /* statement */, const QString& /* table */ );
        bool isConnected()const { return true; }
    private:
        static void sqlite_rand(sqlite3_context *context, int /*argc*/, sqlite3_value ** /*argv*/);
        static void sqlite_power(sqlite3_context *context, int argc, sqlite3_value **argv);

        sqlite3* m_db;
};


#ifdef USE_MYSQL
typedef struct st_mysql MYSQL;

class MySqlConnection : public DbConnection
{
    public:
        MySqlConnection( MySqlConfig* /* config */ );
       ~MySqlConnection();

        QStringList query( const QString& /* statement */ );
        int insert( const QString& /* statement */, const QString& /* table */ );
        bool isConnected()const { return m_connected; }
        const QString lastError() const { return m_error; }
    private:
        void setMysqlError();
        MYSQL* m_db;
        bool m_connected;
        QString m_error;
};
#endif


#ifdef USE_POSTGRESQL
typedef struct pg_conn PGconn;

class PostgresqlConnection : public DbConnection
{
    public:
        PostgresqlConnection( PostgresqlConfig* /* config */ );
       ~PostgresqlConnection();

        QStringList query( const QString& /* statement */ );
        int insert( const QString& /* statement */, const QString& /* table */ );
        bool isConnected()const { return m_connected; }
        const QString lastError() const { return m_error; }
    private:
        void setPostgresqlError();
        PGconn* m_db;
        bool m_connected;
        QString m_error;
};
#endif

class CollectionDB : public QObject, public EngineObserver
{
    Q_OBJECT

    friend class SimilarArtistsInsertionJob;
    friend class ScanController;

    signals:
        void scanStarted();
        void scanDone( bool changed );
        void databaseEngineChanged();

        void scoreChanged( const QString &url, int score );
        void coverChanged( const QString &artist, const QString &album ); //whenever a cover changes
        void coverFetched( const QString &artist, const QString &album ); //only when fetching from amazon
        void coverRemoved( const QString &artist, const QString &album );
        void coverFetcherError( const QString &error );

        void similarArtistsFetched( const QString &artist );
        void tagsChanged( const MetaBundle &bundle );

    public:
        CollectionDB();
        ~CollectionDB();

        static CollectionDB *instance();

        const QString escapeString(QString string ) 
            {
                return
                #ifdef USE_MYSQL
                    // We have to escape "\" for mysql, but can't do so for sqlite
                    (m_dbConnType == DbConnection::mysql)
                        ? string.replace("\\", "\\\\").replace( '\'', "''" ) :
                #endif
                    string.replace( '\'', "''" );
            }
        const QString boolT() { if (getDbConnectionType() == DbConnection::postgresql) return "'t'"; else return "1"; }
        const QString boolF() { if (getDbConnectionType() == DbConnection::postgresql) return "'f'"; else return "0"; }
        const QString textColumnType() { if ( getDbConnectionType() == DbConnection::postgresql ) return "TEXT"; else return "VARCHAR(255)"; }
        const QString textColumnType(int length){ if ( getDbConnectionType() == DbConnection::postgresql ) return "TEXT"; else return QString("VARCHAR(%1)").arg(length); }
        // We might consider using LONGTEXT type, as some lyrics could be VERY long..???
        const QString longTextColumnType() { if ( getDbConnectionType() == DbConnection::postgresql ) return "TEXT"; else return "TEXT"; }
        const QString randomFunc() { if ( getDbConnectionType() == DbConnection::postgresql ) return "random()"; else return "RAND()"; }

        int getType() { return getDbConnectionType(); }

        //sql helper methods
        QStringList query( const QString& statement );
        int insert( const QString& statement, const QString& table );

        //table management methods
        bool isEmpty();
        bool isValid();
        QString adminValue( QString noption );
        void setAdminValue( QString noption, QString value );
        void createTables( const bool temporary = false);
        void dropTables( const bool temporary = false);
        void clearTables( const bool temporary = false);
        void moveTempTables(  );

        uint artistID( QString value, bool autocreate = true, const bool temporary = false, const bool updateSpelling = false );
        uint albumID( QString value, bool autocreate = true, const bool temporary = false, const bool updateSpelling = false );
        uint genreID( QString value, bool autocreate = true, const bool temporary = false, const bool updateSpelling = false );
        uint yearID( QString value, bool autocreate = true, const bool temporary = false, const bool updateSpelling = false );

        bool isDirInCollection( QString path );
        bool isFileInCollection( const QString &url );
        void removeDirFromCollection( QString path );
        void removeSongsInDir( QString path );
        void removeSongs( const KURL::List& urls );
        void updateDirStats( QString path, const long datetime, const bool temporary = false );

        //song methods
        bool addSong( MetaBundle* bundle, const bool incremental = false );

        /**
         * The @p bundle parameter's url() will be looked up in the Collection
         * @param bundle this will be filled in with tags for you
         * @return true if in the collection
         */
        bool bundleForUrl( MetaBundle* bundle );
        QValueList<MetaBundle> bundlesByUrls( const KURL::List& urls );
        void addAudioproperties( const MetaBundle& bundle );

        void updateTags( const QString &url, const MetaBundle &bundle, const bool updateView = true);
        void updateURL( const QString &url, const bool updateView = true );

        //statistics methods
        int addSongPercentage( const QString &url, int percentage );
        int getSongPercentage( const QString &url );
        void setSongPercentage( const QString &url , int percentage );
        int getPlayCount( const QString &url );
        QDateTime getFirstPlay( const QString &url );
        QDateTime getLastPlay( const QString &url );
        void migrateFile( const QString &oldURL, const QString &newURL );
        bool moveFile( const QString &src, const QString &dest, bool overwrtie );

        //artist methods
        QStringList similarArtists( const QString &artist, uint count );

        //album methods
        void checkCompilations( const QString &path, const bool temporary = false );
        void setCompilation( const QString &album, const bool enabled, const bool updateView = true );
        QString albumSongCount( const QString &artist_id, const QString &album_id );
        bool albumIsCompilation( const QString &album_id );

        //list methods
        QStringList artistList( bool withUnknowns = true, bool withCompilations = true );
        QStringList albumList( bool withUnknowns = true, bool withCompilations = true );
        QStringList genreList( bool withUnknowns = true, bool withCompilations = true );
        QStringList yearList( bool withUnknowns = true, bool withCompilations = true );

        QStringList albumListOfArtist( const QString &artist, bool withUnknown = true, bool withCompilations = true );
        QStringList artistAlbumList( bool withUnknown = true, bool withCompilations = true );

        QStringList albumTracks( const QString &artist_id, const QString &album_id, const bool isValue = false );

        //cover management methods
        /** Returns the image from a given URL, network-transparently.
         * You must run KIO::NetAccess::removeTempFile( tmpFile ) when you are finished using the image;
         **/
        static QImage fetchImage(const KURL& url, QString &tmpFile);
        /** Saves images located on the user's filesystem */
        bool setAlbumImage( const QString& artist, const QString& album, const KURL& url );
        /** Saves images obtained from CoverFetcher */
        bool setAlbumImage( const QString& artist, const QString& album, QImage img, const QString& amazonUrl = QString::null, const QString& asin = QString::null );

        QString findImageByMetabundle( MetaBundle trackInformation, const uint = 1 );
        QString findImageByArtistAlbum( const QString &artist, const QString &album, const uint width = 1 );
        QString albumImage( MetaBundle trackInformation, const uint width = 1 );
        QString albumImage( const uint artist_id, const uint album_id, const uint width = 1 );
        QString albumImage( const QString &artist, const QString &album, const uint width = 1 );

        bool removeAlbumImage( const uint artist_id, const uint album_id );
        bool removeAlbumImage( const QString &artist, const QString &album );

        //local cover methods
        void addImageToAlbum( const QString& image, QValueList< QPair<QString, QString> > info, const bool temporary );
        QString getImageForAlbum( const QString& artist, const QString& album, uint width = 0 );
        QString notAvailCover( int width = 0 );

        void applySettings();

        void setLyrics( const QString& url, const QString& lyrics );
        void setHTMLLyrics( const QString &url, QString lyrics );
        QString getLyrics( const QString& url );
        QString getHTMLLyrics( const QString &url );

        void newAmazonReloadDate( const QString& asin, const QString& locale, const QString& md5sum );
        QStringList staleImages();

        const DbConnection::DbConnectionType getDbConnectionType() const { return m_dbConnType; }
        bool isConnected();
        void releasePreviousConnection(QThread *currThread);

    protected:
        QCString md5sum( const QString& artist, const QString& album, const QString& file = QString::null );
        void engineTrackEnded( int finalPosition, int trackLength );
        /** Manages regular folder monitoring scan */
        void timerEvent( QTimerEvent* e );

    public slots:
        void fetchCover( QWidget* parent, const QString& artist, const QString& album, bool noedit );
        void scanMonitor();
        void startScan();
        void stopScan();

    private slots:
        void dirDirty( const QString& path );
        void coverFetcherResult( CoverFetcher* );
        void similarArtistsFetched( const QString& artist, const QStringList& suggestions );

    private:
        //bump DATABASE_VERSION whenever changes to the table structure are made.
        // This erases tags, album, artist, genre, year, images, directory and related_artists tables.
        static const int DATABASE_VERSION = 21;
        // Persistent Tables hold data that is somehow valuable to the user, and can't be erased when rescaning.
        // When bumping this, write code to convert the data!
        static const int DATABASE_PERSISTENT_TABLES_VERSION = 3;
        // Bumping this erases stats table. If you ever need to, write code to convert the data!
        static const int DATABASE_STATS_VERSION = 3;

        static const int MONITOR_INTERVAL = 60; //sec
        static const bool DEBUG = false;

        void initialize();
        void destroy();
        DbConnection* getMyConnection();

        //general management methods
        void createStatsTable();
        void dropStatsTable();
        void createPersistentTables();
        void dropPersistentTables();
        void scanModifiedDirs();

        QCString makeWidthKey( uint width );
        QString artistValue( uint id );
        QString albumValue( uint id );
        QString genreValue( uint id );
        QString yearValue( uint id );

        uint IDFromValue( QString name, QString value, bool autocreate = true, const bool temporary = false,
                          const bool updateSpelling = false );

        QString valueFromID( QString table, uint id );

        void destroyConnections();

        //member variables
        QString m_amazonLicense;
        QString m_cacheArtist;
        uint m_cacheArtistID;
        QString m_cacheAlbum;
        uint m_cacheAlbumID;
        static QMutex *connectionMutex;

        bool m_monitor;
        QDir m_cacheDir;
        QDir m_coverDir;
        QImage m_noCover;

        static QMap<QThread *, DbConnection *> *threadConnections;
        DbConnection::DbConnectionType m_dbConnType;
        DbConfig *m_dbConfig;
};


class QueryBuilder
{
    public:
        //attributes:
        enum qBuilderTables  { tabAlbum = 1, tabArtist = 2, tabGenre = 4, tabYear = 8, tabSong = 32,
                               tabStats = 64, tabLyrics = 128, tabDummy = 0 };
        enum qBuilderOptions { optNoCompilations = 1, optOnlyCompilations = 2, optRemoveDuplicates = 4,
                               optRandomize = 8 };
        enum qBuilderValues  { valID = 1, valName = 2, valURL = 4, valTitle = 8, valTrack = 16, valScore = 32,
                               valComment = 64, valBitrate = 128, valLength = 256, valSamplerate = 512,
                               valPlayCounter = 1024, valCreateDate = 2048, valAccessDate = 4096,
                               valPercentage = 8192, valArtistID = 16384, valAlbumID = 32768,
                               valYearID = 65536, valGenreID = 131072, valDirectory = 262144,
                               valLyrics = 524288, valDummy = 0 };
        enum qBuilderFunctions  { funcCount = 1, funcMax = 2, funcMin = 4, funcAvg = 8, funcSum = 16 };

        enum qBuilderFilter  { modeNormal = 0, modeLess = 1, modeGreater = 2 };

        QueryBuilder();

        void addReturnValue( int table, int value );
        void addReturnFunctionValue( int function, int table, int value);
        uint countReturnValues();

        void beginOR(); //filters will be ORed instead of ANDed
        void endOR();   //don't forget to end it!

        void setGoogleFilter( int defaultTables, QString query );

        void addURLFilters( const QStringList& filter );

        void addFilter( int tables, const QString& filter);
        void addFilter( int tables, int value, const QString& filter, int mode = modeNormal );
        void addFilters( int tables, const QStringList& filter );
        void excludeFilter( int tables, const QString& filter );
        void excludeFilter( int tables, int value, const QString& filter, int mode = modeNormal );

        void addMatch( int tables, const QString& match );
        void addMatch( int tables, int value, const QString& match );
        void addMatches( int tables, const QStringList& match );
        void excludeMatch( int tables, const QString& match );

        void exclusiveFilter( int tableMatching, int tableNotMatching, int value );

        void setOptions( int options );
        void sortBy( int table, int value, bool descending = false );
        void sortByFunction( int function, int table, int value, bool descending = false );
        void groupBy( int table, int value );
        void setLimit( int startPos, int length );

        void initSQLDrag();
        void buildQuery();
        QString getQuery();
        QString query() { buildQuery(); return m_query; };
        void clear();

        QStringList run(  );

    private:
        QString tableName( int table );
        QString valueName( int value );
        QString functionName( int value );

        void linkTables( int tables );

        bool m_OR;
        QString ANDslashOR() const;

        QString m_query;
        QString m_values;
        QString m_tables;
        QString m_join;
        QString m_where;
        QString m_sort;
        QString m_group;
        QString m_limit;

        int m_linkTables;
        uint m_returnValues;
};

inline void QueryBuilder::beginOR()
{
    m_OR = true;
    m_where += "AND ( " + CollectionDB::instance()->boolF() + " ";
}
inline void QueryBuilder::endOR()
{
    m_OR = false;
    m_where += " ) ";
}
inline QString QueryBuilder::ANDslashOR() const { return m_OR ? "OR" : "AND"; }


#endif /* AMAROK_COLLECTIONDB_H */
