/*
 *  Copyright (c) 2007-2008 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#define DEBUG_PREFIX "CollectionManager"

#include "CollectionManager.h"

#include "Debug.h"

#include "BlockingQuery.h"
#include "Collection.h"
#include "MetaQueryMaker.h"
#include "meta/file/File.h"
#include "meta/stream/Stream.h"
#include "PluginManager.h"
#include "SqlStorage.h"

#include <QtAlgorithms>
#include <QtGlobal>
#include <QList>
#include <QMetaEnum>
#include <QMetaObject>
#include <QPair>
#include <QTimer>

#include <KService>
#include <KConfigGroup>
#include <KGlobal>
#include <KMessageBox>
#include <KRun>
#include <KSharedConfig>

#include <cstdlib>

typedef QPair<Collection*, CollectionManager::CollectionStatus> CollectionPair;

struct CollectionManager::Private
{
    QList<CollectionPair> collections;
    QList<CollectionFactory*> factories;
    SqlStorage *sqlDatabase;
    QList<Collection*> unmanagedCollections;
    QList<Collection*> managedCollections;
    QList<TrackProvider*> trackProviders;
    Collection *primaryCollection;
};

class CollectionManagerSingleton
{
    public:
        CollectionManager instance;
};

K_GLOBAL_STATIC( CollectionManagerSingleton, privateInstance )


CollectionManager *
CollectionManager::instance( )
{
    return &privateInstance->instance;
}

CollectionManager::CollectionManager()
    : QObject()
    , d( new Private )
{
    qAddPostRoutine( privateInstance.destroy );  // Ensure that the dtor gets called when QCoreApplication destructs

    init();
}

CollectionManager::~CollectionManager()
{
    DEBUG_BLOCK

    d->collections.clear();
    d->unmanagedCollections.clear();
    d->trackProviders.clear();
    qDeleteAll( d->managedCollections );
    qDeleteAll( d->factories );

    delete d;
}

void
CollectionManager::init()
{
    DEBUG_BLOCK

    d->sqlDatabase = 0;
    d->primaryCollection = 0;
    KService::List plugins = PluginManager::query( "[X-KDE-Amarok-plugintype] == 'collection'" );
    debug() << "Received [" << QString::number( plugins.count() ) << "] collection plugin offers";
//     assert( plugins.count() );
    //This code was in enginecontroller originally, it now ends up here.
    if( plugins.count() == 0 )
    {
        KRun::runCommand( "kbuildsycoca4", 0 );

        KMessageBox::error( 0, i18n(
                "<p>Amarok could not find any collection plugins. "
                "Amarok is now updating the KDE configuration database. Please wait a couple of minutes, then restart Amarok.</p>"
                "<p>If this does not help, "
                "it is likely that Amarok is installed under the wrong prefix, please fix your installation using:<pre>"
                "$ cd /path/to/amarok/source-code/<br>"
                "$ su -c \"make uninstall\"<br>"
                "$ ./configure --prefix=`kde-config --prefix` && su -c \"make install\"<br>"
                "$ kbuildsycoca4<br>"
                "$ amarok</pre>"
                "More information can be found in the README file. For further assistance join us at #amarok on irc.freenode.net.</p>" ) );
        // don't use QApplication::exit, as the eventloop may not have started yet
        std::exit( EXIT_SUCCESS );
    }

    foreach( KService::Ptr service, plugins )
    {
        Amarok::Plugin *plugin = PluginManager::createFromService( service );
        if ( plugin )
        {
            CollectionFactory* factory = dynamic_cast<CollectionFactory*>( plugin );
            if ( factory )
            {
                connect( factory, SIGNAL( newCollection( Collection* ) ), this, SLOT( slotNewCollection( Collection* ) ) );
                d->factories.append( factory );
                factory->init();
            }
            else
            {
                debug() << "Plugin has wrong factory class";
                continue;
            }
        }
    }
}

void
CollectionManager::startFullScan()
{
    foreach( const CollectionPair &pair, d->collections )
    {
        pair.first->startFullScan();
    }
}

void
CollectionManager::checkCollectionChanges()
{
    foreach( const CollectionPair &pair, d->collections )
    {
        pair.first->startIncrementalScan();
    }
}

QueryMaker*
CollectionManager::queryMaker() const
{
    QList<Collection*> colls;
    foreach( const CollectionPair &pair, d->collections )
    {
        if( pair.second & CollectionQueryable )
        {
            colls << pair.first;
        }
    }
    return new MetaQueryMaker( colls );
}

void
CollectionManager::slotNewCollection( Collection* newCollection )
{
    if( !newCollection )
    {
        debug() << "Warning, newCollection in slotNewCollection is 0";
        return;
    }
    const QMetaObject *mo = metaObject();
    const QMetaEnum me = mo->enumerator( mo->indexOfEnumerator( "CollectionStatus" ) );
    const QString &value = KGlobal::config()->group( "CollectionManager" ).readEntry( newCollection->collectionId() );
    int enumValue = me.keyToValue( value.toLocal8Bit().constData() );
    CollectionStatus status;
    enumValue == -1 ? status = CollectionEnabled : status = (CollectionStatus) enumValue;
    CollectionPair pair( newCollection, status );
    d->collections.append( pair );
    d->managedCollections.append( newCollection );
    d->trackProviders.append( newCollection );
    connect( newCollection, SIGNAL( remove() ), SLOT( slotRemoveCollection() ), Qt::QueuedConnection );
    connect( newCollection, SIGNAL( updated() ), SLOT( slotCollectionChanged() ), Qt::QueuedConnection );
    SqlStorage *sqlCollection = dynamic_cast<SqlStorage*>( newCollection );
    if( sqlCollection )
    {
        //let's cheat a bit and assume that sqlStorage and the primaryCollection are always the same
        //it is true for now anyway
        if( d->sqlDatabase )
        {
            if( d->sqlDatabase->sqlDatabasePriority() < sqlCollection->sqlDatabasePriority() )
            {
                d->sqlDatabase = sqlCollection;
                d->primaryCollection = newCollection;
            }
        }
        else
        {
            d->sqlDatabase = sqlCollection;
            d->primaryCollection = newCollection;
        }
    }
    if( status & CollectionViewable )
    {
        emit collectionAdded( newCollection );
    }
}

void
CollectionManager::slotRemoveCollection()
{
    Collection* collection = qobject_cast<Collection*>( sender() );
    if( collection )
    {
        CollectionStatus status = collectionStatus( collection->collectionId() );
        CollectionPair pair( collection, status );
        d->collections.removeAll( pair );
        d->managedCollections.removeAll( collection );
        d->trackProviders.removeAll( collection );
        SqlStorage *sqlDb = dynamic_cast<SqlStorage*>( collection );
        if( sqlDb && sqlDb == d->sqlDatabase )
        {
            SqlStorage *newSqlDatabase = 0;
            foreach( const CollectionPair &pair, d->collections )
            {
                SqlStorage *sqlDb = dynamic_cast<SqlStorage*>( pair.first );
                if( sqlDb )
                {
                    if( newSqlDatabase )
                    {
                        if( newSqlDatabase->sqlDatabasePriority() < sqlDb->sqlDatabasePriority() )
                            newSqlDatabase = sqlDb;
                    }
                    else
                        newSqlDatabase = sqlDb;
                }
            }
            d->sqlDatabase = newSqlDatabase;
        }
        emit collectionRemoved( collection->collectionId() );
        QTimer::singleShot( 0, collection, SLOT( deleteLater() ) );
    }
}

void
CollectionManager::slotCollectionChanged()
{
    Collection *collection = dynamic_cast<Collection*>( sender() );
    if( collection )
    {
        CollectionStatus status = collectionStatus( collection->collectionId() );
        if( status & CollectionViewable )
        {
            emit collectionDataChanged( collection );
        }
    }
}

QList<Collection*>
CollectionManager::viewableCollections() const
{
    QList<Collection*> result;
    foreach( const CollectionPair &pair, d->collections )
    {
        if( pair.second & CollectionViewable )
        {
            result << pair.first;
        }
    }
    return result;
}

QList<Collection*>
CollectionManager::queryableCollections() const
{
    QList<Collection*> result;
    foreach( const CollectionPair &pair, d->collections )
        if( pair.second & CollectionQueryable )
            result << pair.first;
    return result;

}

Collection*
CollectionManager::primaryCollection() const
{
    return d->primaryCollection;
}

SqlStorage*
CollectionManager::sqlStorage() const
{
    return d->sqlDatabase;
}

Meta::TrackList
CollectionManager::tracksForUrls( const KUrl::List &urls )
{
    DEBUG_BLOCK

    debug() << "adding " << urls.size() << " tracks";

    Meta::TrackList tracks;
    foreach( KUrl url, urls )
        tracks.append( trackForUrl( url ) );
    return tracks;
}

Meta::TrackPtr
CollectionManager::trackForUrl( const KUrl &url )
{
    //TODO method stub
    //check all collections
    //might be a podcast, in that case we'll have additional meta information
    //might be a lastfm track, another stream
    //or a file which is not in any collection
    if( !url.isValid() )
    {
        return Meta::TrackPtr( 0 );
    }

    foreach( TrackProvider *provider, d->trackProviders )
    {
        if( provider->possiblyContainsTrack( url ) )
        {
            Meta::TrackPtr track = provider->trackForUrl( url );
            if( track )
                return track;
        }
    }

    if( url.protocol() == "http" || url.protocol() == "mms" )
        return Meta::TrackPtr( new MetaStream::Track( url ) );

    if( url.protocol() == "file" )
        return Meta::TrackPtr( new MetaFile::Track( url ) );

    return Meta::TrackPtr( 0 );
}

Meta::ArtistList
CollectionManager::relatedArtists( Meta::ArtistPtr artist, int maxArtists )
{
    SqlStorage *sql = sqlStorage();
    QString query = QString( "SELECT suggestion FROM related_artists WHERE artist = '%1' ORDER BY %2 LIMIT %3 OFFSET 0;" )
               .arg( sql->escape( artist->name() ), sql->randomFunc(), QString::number( maxArtists ) );

    QStringList artistNames = sql->query( query );
    //TODO: figure out a way to retrieve similar artists from last.fm here
    /*if( artistNames.isEmpty() )
    {
        artistNames = Scrobbler::instance()->similarArtists( artist->name() );
    }*/
    QueryMaker *qm = queryMaker();
    foreach( const QString &artist, artistNames )
    {
        qm->addFilter( QueryMaker::valArtist, artist, true, true );
    }
    qm->setQueryType( QueryMaker::Artist );
    qm->limitMaxResultSize( maxArtists );
    BlockingQuery bq( qm );
    bq.startQuery();

    QStringList ids = bq.collectionIds();
    Meta::ArtistList result;
    QSet<QString> artistNameSet;
    foreach( const QString &collectionId, ids )
    {
        Meta::ArtistList artists = bq.artists( collectionId );
        foreach( Meta::ArtistPtr artist, artists )
        {
            if( !artistNameSet.contains( artist->name() ) )
            {
                result.append( artist );
                artistNameSet.insert( artist->name() );
                if( result.size() == maxArtists )
                    break;
            }
        }
        if( result.size() == maxArtists )
                    break;
    }
    return result;
}

void
CollectionManager::addUnmanagedCollection( Collection *newCollection, CollectionStatus defaultStatus )
{
    if( newCollection && d->unmanagedCollections.indexOf( newCollection ) == -1 )
    {
        const QMetaObject *mo = metaObject();
        const QMetaEnum me = mo->enumerator( mo->indexOfEnumerator( "CollectionStatus" ) );
        const QString &value = KGlobal::config()->group( "CollectionManager" ).readEntry( newCollection->collectionId() );
        int enumValue = me.keyToValue( value.toLocal8Bit().constData() );
        CollectionStatus status;
        enumValue == -1 ? status = defaultStatus : status = (CollectionStatus) enumValue;
        d->unmanagedCollections.append( newCollection );
        CollectionPair pair( newCollection, status );
        d->collections.append( pair );
        d->trackProviders.append( newCollection );
        if( status & CollectionViewable )
        {
            emit collectionAdded( newCollection );
        }
        emit trackProviderAdded( newCollection );
    }
}

void
CollectionManager::removeUnmanagedCollection( Collection *collection )
{
    //do not remove it from collection if it is not in unmanagedCollections
    if( collection && d->unmanagedCollections.removeAll( collection ) )
    {
        CollectionPair pair( collection, collectionStatus( collection->collectionId() ) );
        d->collections.removeAll( pair );
        d->trackProviders.removeAll( collection );
        emit collectionRemoved( collection->collectionId() );
    }
}

void
CollectionManager::setCollectionStatus( const QString &collectionId, CollectionStatus status )
{
    foreach( const CollectionPair &pair, d->collections )
    {
        if( pair.first->collectionId() == collectionId )
        {
            if( ( pair.second & CollectionViewable ) &&
               !( status & CollectionViewable ) )
            {
                emit collectionRemoved( collectionId );
            }
            else if( ( pair.second & CollectionQueryable ) &&
                    !( status & CollectionViewable ) )
            {
                emit collectionAdded( pair.first );
            }
            CollectionPair &pair2 = const_cast<CollectionPair&>( pair );
            pair2.second = status;
            const QMetaObject *mo = metaObject();
            const QMetaEnum me = mo->enumerator( mo->indexOfEnumerator( "CollectionStatus" ) );
            KGlobal::config()->group( "CollectionManager" ).writeEntry( collectionId, me.valueToKey( status ) );
            break;
        }
    }
}

CollectionManager::CollectionStatus
CollectionManager::collectionStatus( const QString &collectionId ) const
{
    foreach( const CollectionPair &pair, d->collections )
    {
        if( pair.first->collectionId() == collectionId )
        {
            return pair.second;
        }
    }
    return CollectionDisabled;
}

QHash<Collection*, CollectionManager::CollectionStatus>
CollectionManager::collections() const
{
    QHash<Collection*, CollectionManager::CollectionStatus> result;
    foreach( const CollectionPair &pair, d->collections )
    {
        result.insert( pair.first, pair.second );
    }
    return result;
}

void
CollectionManager::addTrackProvider( TrackProvider *provider )
{
    d->trackProviders.append( provider );
    emit trackProviderAdded( provider );
}

void
CollectionManager::removeTrackProvider( TrackProvider *provider )
{
    d->trackProviders.removeAll( provider );
}

#include "CollectionManager.moc"
