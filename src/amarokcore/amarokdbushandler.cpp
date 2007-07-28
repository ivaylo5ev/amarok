/***************************************************************************
                         amarokdbushandler.cpp  -  D-Bus Implementation
                            -------------------
   begin                : Sat Oct 11 2003
   copyright            : (C) 2003 by Stanislav Karchebny
                          (C) 2004 Christian Muehlhaeuser
                          (C) 2005 Ian Monroe
                          (C) 2005 Seb Ruiz
                          (C) 2006 Alexandre Oliveira
                          (C) 2007 Leonardo Franchi
   email                : berkus@users.sf.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "amarok.h"
#include "amarokconfig.h"
#include "amarokdbushandler.h"
#include "app.h" //transferCliArgs
#include "debug.h"
#include "collectiondb.h"
#include "collection/collectionmanager.h"
#include "collection/SqlStorage.h"
#include "devicemanager.h"
#include "enginebase.h"
#include "enginecontroller.h"
#include "equalizersetup.h"
#include "lastfm.h"
#include "MainWindow.h"
#include "mediabrowser.h"
#include "meta/meta.h"
#include "mountpointmanager.h"
#include "osd.h"
#include "playlist/PlaylistModel.h"
#include "playlist.h"
#include "playlistbrowser.h"
#include "playlistitem.h"
#include "progressslider.h"
#include "scancontroller.h"
#include "scriptmanager.h"
#include "statusbar.h"
#include "TheInstances.h"

#include <QFile>
//Added by qt3to4:
#include <Q3ValueList>
#include <QByteArray>

#include <kactioncollection.h>
#include <kstartupinfo.h>

#include <collectionadaptor.h>
#include <contextadaptor.h>
#include <mediabrowseradaptor.h>
#include <playeradaptor.h>
#include <playlistadaptor.h>
#include <playlistbrowseadaptor.h>
#include <scriptadaptor.h>


namespace Amarok
{
/////////////////////////////////////////////////////////////////////////////////////
// class DbusPlayerHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusPlayerHandler::DbusPlayerHandler()
        : QObject( kapp )
    {
     (void)new PlayerAdaptor(this);
     QDBusConnection::sessionBus().registerObject("/Player", this);
    }

    QString DbusPlayerHandler::version()
    {
        return APP_VERSION;
    }

    bool DbusPlayerHandler::dynamicModeStatus()
    {
        return Amarok::dynamicMode();
    }

    bool DbusPlayerHandler::equalizerEnabled()
    {
        if(EngineController::hasEngineProperty( "HasEqualizer" ))
            return AmarokConfig::equalizerEnabled();
        else
            return false;
    }

    bool DbusPlayerHandler::osdEnabled()
    {
        return AmarokConfig::osdEnabled();
    }

    bool DbusPlayerHandler::isPlaying()
    {
        return EngineController::engine()->state() == Engine::Playing;
    }

    bool DbusPlayerHandler::randomModeStatus()
    {
        return AmarokConfig::randomMode();
    }

    bool DbusPlayerHandler::repeatPlaylistStatus()
    {
        return Amarok::repeatPlaylist();
    }

    bool DbusPlayerHandler::repeatTrackStatus()
    {
        return Amarok::repeatTrack();
    }

    int DbusPlayerHandler::getVolume()
    {
        return EngineController::engine() ->volume();
    }

    int DbusPlayerHandler::sampleRate()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->sampleRate() : 0;
    }

    float DbusPlayerHandler::score()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        //TODO: return type mismatch. fix this
        return track ? track->score() : 0.0;
    }

    int DbusPlayerHandler::rating()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->rating() : 0;
    }

    int  DbusPlayerHandler::status()
    {
        // <0 - error, 0 - stopped, 1 - paused, 2 - playing
        switch( EngineController::engine()->state() )
        {
        case Engine::Playing:
            return 2;
        case Engine::Paused:
            return 1;
        case Engine::Empty:
        case Engine::Idle:
            return 0;
        }
        return -1;
    }

    int DbusPlayerHandler::trackCurrentTime()
    {
        return EngineController::instance()->trackPosition() / 1000;
    }

    int DbusPlayerHandler::trackCurrentTimeMs()
    {
        return EngineController::instance()->trackPosition();
    }

    int DbusPlayerHandler::trackPlayCounter()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->playCount() : 0;
    }

    int DbusPlayerHandler::trackTotalTime()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->length() : 0;
    }

    QStringList DbusPlayerHandler::labels()
    {
        //TODO: fix me
        return QStringList();
    }

    QString DbusPlayerHandler::album()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->album()->prettyName() : QString();
    }

    QString DbusPlayerHandler::artist()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->artist()->prettyName() : QString();
    }

    QString DbusPlayerHandler::bitrate()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? QString::number( track->bitrate() ) : QString();
    }

    QString DbusPlayerHandler::comment()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->comment() : QString();
    }

    QString DbusPlayerHandler::coverImage()
    {
        //TODO: fix me. oups, Meta:.Album can't actually do this yet:(
        return QString();
    }

    QString DbusPlayerHandler::currentTime()
    {
        //TODO: move utility methods somewhere else
        //return MetaBundle::prettyLength( EngineController::instance()->trackPosition() / 1000 ,true );
        return QString();
    }

    QString DbusPlayerHandler::encodedURL()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->playableUrl().url() : QString();
    }

    QString DbusPlayerHandler::engine()
    {
        return AmarokConfig::soundSystem();
    }

    QString DbusPlayerHandler::genre()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->genre()->prettyName() : QString();
    }

    QString DbusPlayerHandler::lyrics()
    {
        //TODO: implement this
        //return CollectionDB::instance()->getLyrics( EngineController::instance()->bundle().url().path() );
        return QString();
    }

    QString DbusPlayerHandler::lyricsByPath( QString path )
    {
        return CollectionDB::instance()->getLyrics( path );
    }

    QString DbusPlayerHandler::lastfmStation()
    {
       return LastFm::Controller::stationDescription(); //return QString::null if not playing
    }

    QString DbusPlayerHandler::nowPlaying()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        //TODO: i think this is not correct yet
        return track ? track->prettyName() : QString();
    }

    QString DbusPlayerHandler::path()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->playableUrl().path() : QString();
    }

    QString DbusPlayerHandler::title()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->prettyName() : QString();
    }

    QString DbusPlayerHandler::totalTime()
    {
        //TODO: implement utility method for this somewhere
        //return EngineController::instance()->bundle().prettyLength();
        return QString();
    }

    QString DbusPlayerHandler::track()
    {
        //what does this do?
        /*if ( EngineController::instance()->bundle().track() != 0 )
            return QString::number( EngineController::instance()->bundle().track() );
        else
            return QString();*/
        return QString();
    }

    QString DbusPlayerHandler::type()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        QString type = track ? track->type() : QString();
        if( type == "stream/lastfm" )
            return "LastFm Stream";
        else
            return type;
    }

    QString DbusPlayerHandler::year()
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        return track ? track->year()->prettyName() : QString();
    }

    void DbusPlayerHandler::addBookmark( uint second )
    {
        ProgressSlider::instance()->addBookmark( second );
    }

    void DbusPlayerHandler::configEqualizer()
    {
        if(EngineController::hasEngineProperty( "HasEqualizer" ))
            EqualizerSetup::instance()->show();
            EqualizerSetup::instance()->raise();
    }

    void DbusPlayerHandler::enableOSD(bool enable)
    {
        Amarok::OSD::instance()->setEnabled( enable );
        AmarokConfig::setOsdEnabled( enable );
    }

    void DbusPlayerHandler::enableRandomMode( bool /*enable*/ )
    {
#if 0
        static_cast<KSelectAction*>(Amarok::actionCollection()->action( "random_mode" ))
            ->setCurrentItem( enable ? AmarokConfig::EnumRandomMode::Tracks : AmarokConfig::EnumRandomMode::Off );
#endif
    }

    void DbusPlayerHandler::enableRepeatPlaylist( bool /*enable*/ )
    {
#if 0
        static_cast<KSelectAction*>( Amarok::actionCollection()->action( "repeat" ) )
               ->setCurrentItem( enable ? AmarokConfig::EnumRepeat::Playlist : AmarokConfig::EnumRepeat::Off );
#endif
    }

     void DbusPlayerHandler::enableRepeatTrack( bool /*enable*/)
    {
#if 0
        static_cast<KSelectAction*>( Amarok::actionCollection()->action( "repeat" ) )
               ->setCurrentItem( enable ? AmarokConfig::EnumRepeat::Track : AmarokConfig::EnumRepeat::Off );
#endif
    }

    void DbusPlayerHandler::mediaDeviceMount()
    {
        if ( MediaBrowser::instance()->currentDevice() )
            MediaBrowser::instance()->currentDevice()->connectDevice();
    }

    void DbusPlayerHandler::mediaDeviceUmount()
    {
        if ( MediaBrowser::instance()->currentDevice() )
            MediaBrowser::instance()->currentDevice()->disconnectDevice();
    }

    void DbusPlayerHandler::mute()
    {
        EngineController::instance()->mute();
    }

    void DbusPlayerHandler::next()
    {
        EngineController::instance() ->next();
    }

    void DbusPlayerHandler::pause()
    {
        EngineController::instance()->pause();
    }

    void DbusPlayerHandler::play()
    {
        EngineController::instance() ->play();
    }

    void DbusPlayerHandler::playPause()
    {
        EngineController::instance() ->playPause();
    }

    void DbusPlayerHandler::prev()
    {
        EngineController::instance() ->previous();
    }

    void DbusPlayerHandler::queueForTransfer( KUrl url )
    {
        MediaBrowser::queue()->addUrl( url );
        MediaBrowser::queue()->URLsAdded();
    }

    void DbusPlayerHandler::seek(int s)
    {
        if ( s > 0 && EngineController::engine()->state() != Engine::Empty )
            EngineController::instance()->seek( s * 1000 );
    }

    void DbusPlayerHandler::seekRelative(int s)
    {
        EngineController::instance() ->seekRelative( s * 1000 );
    }

    void DbusPlayerHandler::setEqualizer(int preamp, int band60, int band170, int band310,
        int band600, int band1k, int band3k, int band6k, int band12k, int band14k, int band16k)
    {
        if( EngineController::hasEngineProperty( "HasEqualizer" ) ) {
            bool instantiated = EqualizerSetup::isInstantiated();
            EqualizerSetup* eq = EqualizerSetup::instance();

            Q3ValueList<int> gains;
            gains << band60 << band170 << band310 << band600 << band1k
                  << band3k << band6k << band12k << band14k << band16k;

            eq->setBands( preamp, gains );
            if( !instantiated )
                delete eq;
        }
    }

    void DbusPlayerHandler::setEqualizerEnabled( bool active )
    {
        EngineController::engine()->setEqualizerEnabled( active );
        AmarokConfig::setEqualizerEnabled( active );

        if( EqualizerSetup::isInstantiated() )
            EqualizerSetup::instance()->setActive( active );
    }

    void DbusPlayerHandler::setEqualizerPreset( QString name )
    {
        if( EngineController::hasEngineProperty( "HasEqualizer" ) ) {
            bool instantiated = EqualizerSetup::isInstantiated();
            EqualizerSetup* eq = EqualizerSetup::instance();
            eq->setPreset( name );
            if ( !instantiated )
                delete eq;
        }
    }

    void DbusPlayerHandler::setLyricsByPath( const QString& url, const QString& lyrics )
    {
        CollectionDB::instance()->setLyrics( url, lyrics );
    }

    void DbusPlayerHandler::setScore( float score )
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        if( track )
            track->setScore( score );
    }

    void DbusPlayerHandler::setScoreByPath( const QString &url, float score )
    {
        CollectionDB::instance()->setSongPercentage(url, score);
    }

    void DbusPlayerHandler::setBpm( float bpm )
    {
        //Meta::Track does not provide a setBpm method
        //is it necessary?
    }

    void DbusPlayerHandler::setBpmByPath( const QString &url, float bpm )
    {
        /*MetaBundle bundle( url );
        bundle.setBpm(bpm);
        bundle.save();
        CollectionDB::instance()->updateTags( bundle.url().path(), bundle, true );*/
    }

    void DbusPlayerHandler::setRating( int rating )
    {
        Meta::TrackPtr track = EngineController::instance()->currentTrack();
        if( track )
            track->setRating( rating );
    }

    void DbusPlayerHandler::setRatingByPath( const QString &url, int rating )
    {
        CollectionDB::instance()->setSongRating(url, rating);
    }

    void DbusPlayerHandler::setVolume(int volume)
    {
        EngineController::instance()->setVolume(volume);
    }

    void DbusPlayerHandler::setVolumeRelative(int ticks)
    {
        EngineController::instance()->increaseVolume(ticks);
    }

    void DbusPlayerHandler::showBrowser( QString browser )
    {
        if ( browser == "collection" )
            MainWindow::self()->showBrowser( "CollectionBrowser" );
        if ( browser == "playlist" )
            MainWindow::self()->showBrowser( "PlaylistBrowser" );
        if ( browser == "media" )
            MainWindow::self()->showBrowser( "MediaBrowser" );
        if ( browser == "file" )
            MainWindow::self()->showBrowser( "FileBrowser" );
    }

    void DbusPlayerHandler::showOSD()
    {
        Amarok::OSD::instance()->forceToggleOSD();
    }

    void DbusPlayerHandler::stop()
    {
        EngineController::instance() ->stop();
    }

    void DbusPlayerHandler::transferDeviceFiles()
    {
        if ( MediaBrowser::instance()->currentDevice() )
            MediaBrowser::instance()->currentDevice()->transferFiles();
    }

    void DbusPlayerHandler::volumeDown()
    {
        EngineController::instance()->decreaseVolume();
    }

    void DbusPlayerHandler::volumeUp()
    {
        EngineController::instance()->increaseVolume();
    }

    void DbusPlayerHandler::transferCliArgs( QStringList args )
    {
        DEBUG_BLOCK

        //stop startup cursor animation - do not mess with this, it's carefully crafted
        //NOTE I have no idea why we need to do this, I never get startup notification from
        //the amarok binary anyway --mxcl
        debug() << "Startup ID: " << args.first() << endl;
        kapp->setStartupId( args.first().toLocal8Bit() );
#ifdef Q_WS_X11
        // currently X11 only
        KStartupInfo::appStarted();
#endif
        args.pop_front();

        const int argc = args.count() + 1;
        char **argv = new char*[argc];

        QStringList::ConstIterator it = args.constBegin();
        for( int i = 1; i < argc; ++i, ++it ) {
            argv[i] = qstrdup( (*it).toLocal8Bit() );
            debug() << "Extracted: " << argv[i] << endl;
        }

        // required, loader doesn't add it
        argv[0] = qstrdup( "amarok" );

        // re-initialize KCmdLineArgs with the new arguments
        App::initCliArgs( argc, argv );
        App::handleCliArgs();

        //FIXME are we meant to leave this around?
        //FIXME are we meant to allocate it all on the heap?
        //NOTE we allow the memory leak because I think there are
        // some very mysterious crashes due to deleting this
        //delete[] argv;
    }

/////////////////////////////////////////////////////////////////////////////////////
// class DbusPlaylistHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusPlaylistHandler::DbusPlaylistHandler()
        : QObject( kapp )
    {
         (void)new PlaylistAdaptor(this);
         QDBusConnection::sessionBus().registerObject("/PlayerList", this);
    }

    int  DbusPlaylistHandler::getActiveIndex()
    {
        return Playlist::instance()->currentTrackIndex( false );
    }

    int  DbusPlaylistHandler::getTotalTrackCount()
    {
        return Playlist::instance()->totalTrackCount();
    }

    QString DbusPlaylistHandler::saveCurrentPlaylist()
    {
        Playlist::instance()->saveXML( Playlist::defaultPlaylistPath() );
        return Playlist::defaultPlaylistPath();
    }

    void DbusPlaylistHandler::addMedia(const KUrl &url)
    {
        Playlist::instance()->appendMedia(url);
    }

    void DbusPlaylistHandler::addMediaList(const KUrl::List &urls)
    {
        The::playlistModel()->insertMedia(urls);
    }

    void DbusPlaylistHandler::clearPlaylist()
    {
        The::playlistModel()->clear();
    }

    void DbusPlaylistHandler::playByIndex(int index)
    {
        Playlist::instance()->activateByIndex( index );
    }

    void DbusPlaylistHandler::playMedia( const KUrl &url )
    {
        The::playlistModel()->insertMedia( url, Playlist::DirectPlay | Playlist::Unique);
    }

    void DbusPlaylistHandler::popupMessage( const QString& msg )
    {
        StatusBar::instance()->longMessageThreadSafe( msg );
    }

    void DbusPlaylistHandler::removeCurrentTrack()
    {
#if 0
        PlaylistItem* const item = Playlist::instance()->currentTrack();
        if ( item ) {
            if( item->isBeingRenamed() )
                item->setDeleteAfterEditing( true );
            else
            {
                Playlist::instance()->removeItem( item );
                delete item;
            }
        }
#endif
    }

    void DbusPlaylistHandler::removeByIndex( int /*index*/ )
    {
#if 0
            PlaylistItem* const item =
            static_cast<PlaylistItem*>( Playlist::instance()->itemAtIndex( index ) );

        if ( item ) {
            Playlist::instance()->removeItem( item );
            delete item;
        }
#endif
    }

    void DbusPlaylistHandler::repopulate()
    {
        Playlist::instance()->repopulate();
    }

    void DbusPlaylistHandler::saveM3u( const QString& path, bool relativePaths )
    {
        Playlist::instance()->saveM3U( path, relativePaths );
    }

    void DbusPlaylistHandler::setStopAfterCurrent( bool on )
    {
        Playlist::instance()->setStopAfterCurrent( on );
    }

    void DbusPlaylistHandler::shortStatusMessage(const QString& msg)
    {
        StatusBar::instance()->shortMessage( msg );
    }

    void DbusPlaylistHandler::shufflePlaylist()
    {
        Playlist::instance()->shuffle();
    }

    void DbusPlaylistHandler::togglePlaylist()
    {
        MainWindow::self()->showHide();
    }

    QStringList DbusPlaylistHandler::filenames()
    {
        Playlist *p_inst = Playlist::instance();
        QStringList songlist;

        if (!p_inst)
                return songlist;

        PlaylistItem *p_item = p_inst->firstChild();

        while (p_item)
        {
                songlist.append(p_item->filename());
                p_item = p_item->nextSibling();
        }

        return songlist;
    }

    QString DbusPlaylistHandler::currentTrackUniqueId()
    {
        if( Playlist::instance()->currentItem() )
            return Playlist::instance()->currentItem()->uniqueId();
        return QString();
    }

/////////////////////////////////////////////////////////////////////////////////////
// class DbusPlaylistBrowserHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusPlaylistBrowserHandler::DbusPlaylistBrowserHandler()
        :QObject( kapp )
    {
         (void)new PlaylistBrowserAdaptor(this);
         QDBusConnection::sessionBus().registerObject("/PlaylistBrowser", this);
    }

    void DbusPlaylistBrowserHandler::addPodcast( const QString &url )
    {
        PlaylistBrowser::instance()->addPodcast( url );
    }

    void DbusPlaylistBrowserHandler::scanPodcasts()
    {
        PlaylistBrowser::instance()->scanPodcasts();
    }

    void DbusPlaylistBrowserHandler::addPlaylist( const QString &url )
    {
        PlaylistBrowser::instance()->addPlaylist( url );
    }

    int DbusPlaylistBrowserHandler::loadPlaylist( const QString &playlist )
    {
        return PlaylistBrowser::instance()->loadPlaylist( playlist );
    }

/////////////////////////////////////////////////////////////////////////////////////
//  class DbusContextHandler
/////////////////////////////////////////////////////////////////////////////////////

DbusContextHandler::DbusContextHandler()
    : QObject( kapp )
{
    (void)new ContextAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/Context", this);
}
// TODO reimplement!!
void DbusContextHandler::showLyrics()
{
    ;//     LyricsItem::instance()->showLyrics( QString() );
}
void DbusContextHandler::showLyrics( const QByteArray& lyrics )
{
    ;//     LyricsItem::instance()->lyricsResult( lyrics );
}
/////////////////////////////////////////////////////////////////////////////////////
// class DbusCollectionHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusCollectionHandler::DbusCollectionHandler()
        : QObject( kapp )
    {
         (void)new CollectionAdaptor(this);
         QDBusConnection::sessionBus().registerObject("/Collection", this);
    }

    int DbusCollectionHandler::totalAlbums()
    {
        QStringList albums = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM album;" );
        QString total = albums[0];
        return total.toInt();
    }

    int DbusCollectionHandler::totalArtists()
    {
        QStringList artists = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM artist;" );
        QString total = artists[0];
        return total.toInt();
    }

    int DbusCollectionHandler::totalComposers()
    {
        QStringList composers = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM composer;" );
        QString total = composers[0];
        return total.toInt();
    }

    int DbusCollectionHandler::totalCompilations()
    {
        QStringList comps = CollectionDB::instance()->query( "SELECT COUNT( DISTINCT album ) FROM tags WHERE sampler = 1;" );
        QString total = comps[0];
        return total.toInt();
    }

    int DbusCollectionHandler::totalGenres()
    {
        QStringList genres = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM genre;" );
        QString total = genres[0];
        return total.toInt();
    }

    int DbusCollectionHandler::totalTracks()
    {
        QStringList tracks = CollectionDB::instance()->query( "SELECT COUNT( url ) FROM tags;" );
        QString total = tracks[0];
        int final = total.toInt();
        return final;
    }

    bool DbusCollectionHandler::isDirInCollection( const QString& path )
    {
        return CollectionDB::instance()->isDirInCollection( path );
    }

    bool DbusCollectionHandler::moveFile( const QString &oldURL, const QString &newURL, bool overwrite )
    {
        return CollectionDB::instance()->moveFile( oldURL, newURL, overwrite );
    }

    QStringList DbusCollectionHandler::query( const QString& sql )
    {
        return CollectionManager::instance()->sqlStorage()->query( sql );
    }

    QStringList DbusCollectionHandler::similarArtists( int artists )
    {
        //TODO: implement
        return QStringList();
    }

    void DbusCollectionHandler::migrateFile( const QString &oldURL, const QString &newURL )
    {
        CollectionDB::instance()->migrateFile( oldURL, newURL );
    }

    void DbusCollectionHandler::scanCollection()
    {
        CollectionDB::instance()->startScan();
    }

    void DbusCollectionHandler::scanCollectionChanges()
    {
        CollectionDB::instance()->scanModifiedDirs();
    }

    int DbusCollectionHandler::addLabels( const QString &url, const QStringList &labels )
    {
        CollectionDB *db = CollectionDB::instance();
        QString uid = db->getUniqueId( url );
        int count = 0;
        oldForeach( labels )
        {
            if( db->addLabel( url, *it, uid , CollectionDB::typeUser ) )
                count++;
        }
        return count;
    }

    void DbusCollectionHandler::removeLabels( const QString &url, const QStringList &oldLabels )
    {
        CollectionDB::instance()->removeLabels( url, oldLabels, CollectionDB::typeUser );
    }

    void DbusCollectionHandler::disableAutoScoring( bool disable )
    {
        CollectionDB::instance()->disableAutoScoring( disable );
    }

    int DbusCollectionHandler::deviceId( const QString &url )
    {
        return MountPointManager::instance()->getIdForUrl( url );
    }

    QString DbusCollectionHandler::relativePath( const QString &url )
    {
        int deviceid = deviceId( url );
        return MountPointManager::instance()->getRelativePath( deviceid, url );
    }

    QString DbusCollectionHandler::absolutePath( int deviceid, const QString &relativePath )
    {
        return MountPointManager::instance()->getAbsolutePath( deviceid, relativePath );
    }

/////////////////////////////////////////////////////////////////////////////////////
// class DbusScriptHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusScriptHandler::DbusScriptHandler()
        : QObject( kapp )
    {
         (void)new ScriptAdaptor(this);
         QDBusConnection::sessionBus().registerObject("/Script", this);
    }

    bool DbusScriptHandler::runScript(const QString& name)
    {
        return ScriptManager::instance()->runScript(name);
    }

    bool DbusScriptHandler::stopScript(const QString& name)
    {
        return ScriptManager::instance()->stopScript(name);
    }

    QStringList DbusScriptHandler::listRunningScripts()
    {
        return ScriptManager::instance()->listRunningScripts();
    }

    void DbusScriptHandler::addCustomMenuItem(QString submenu, QString itemTitle )
    {
        Playlist::instance()->addCustomMenuItem( submenu, itemTitle );
    }

    void DbusScriptHandler::removeCustomMenuItem(QString submenu, QString itemTitle )
    {
        Playlist::instance()->removeCustomMenuItem( submenu, itemTitle );
    }

    QString DbusScriptHandler::readConfig(const QString& key)
    {
        QString cleanKey = key;
        KConfigSkeletonItem* configItem = AmarokConfig::self()->findItem(cleanKey.remove(' '));
        if (configItem)
            return configItem->property().toString();
        else
            return QString();
    }

    QStringList DbusScriptHandler::readListConfig(const QString& key)
    {
        QString cleanKey = key;
        KConfigSkeletonItem* configItem = AmarokConfig::self()->findItem(cleanKey.remove(' '));
        QStringList stringList;
        if(configItem)
        {
            Q3ValueList<QVariant> variantList = configItem->property().toList();
            Q3ValueList<QVariant>::Iterator it = variantList.begin();
            while(it != variantList.end())
            {
                stringList << (*it).toString();
                ++it;
            }
        }
        return stringList;
    }

    QString DbusScriptHandler::proxyForUrl(const QString& url)
    {
        return Amarok::proxyForUrl( url );
    }

    QString DbusScriptHandler::proxyForProtocol(const QString& protocol)
    {
        return Amarok::proxyForProtocol( protocol );
    }

/////////////////////////////////////////////////////////////////////////////////////
// class DbusDevicesHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusDevicesHandler::DbusDevicesHandler()
        : QObject( kapp )
    {}

    void DbusDevicesHandler::mediumAdded(QString name)
    {
        DeviceManager::instance()->mediumAdded(name);
    }

    void DbusDevicesHandler::mediumRemoved(QString name)
    {
        DeviceManager::instance()->mediumRemoved(name);
    }

    void DbusDevicesHandler::mediumChanged(QString name)
    {
        DeviceManager::instance()->mediumChanged(name);
    }

    QStringList DbusDevicesHandler::showDeviceList()
    {
        return DeviceManager::instance()->getDeviceStringList();
    }

/////////////////////////////////////////////////////////////////////////////////////
// class DbusDevicesHandler
/////////////////////////////////////////////////////////////////////////////////////

    DbusMediaBrowserHandler::DbusMediaBrowserHandler()
        : QObject( kapp )
    {
         (void)new MediaBrowserAdaptor(this);
         QDBusConnection::sessionBus().registerObject("/MediaBrowser", this);
    }

    void DbusMediaBrowserHandler::deviceConnect()
    {
        if ( MediaBrowser::instance()->currentDevice() )
            MediaBrowser::instance()->currentDevice()->connectDevice();
    }

    void DbusMediaBrowserHandler::deviceDisconnect()
    {
        if ( MediaBrowser::instance()->currentDevice() )
            MediaBrowser::instance()->currentDevice()->disconnectDevice();
    }

    QStringList DbusMediaBrowserHandler::deviceList()
    {
        return MediaBrowser::instance()->deviceNames();
    }

    void DbusMediaBrowserHandler::deviceSwitch( QString name )
    {
        MediaBrowser::instance()->deviceSwitch( name );
    }

    void DbusMediaBrowserHandler::queue( KUrl url )
    {
        MediaBrowser::queue()->addUrl( url );
        MediaBrowser::queue()->URLsAdded();
    }

    void DbusMediaBrowserHandler::queueList( KUrl::List urls )
    {
        MediaBrowser::queue()->addUrls( urls );
    }

    void DbusMediaBrowserHandler::transfer()
    {
        if ( MediaBrowser::instance()->currentDevice() )
            MediaBrowser::instance()->currentDevice()->transferFiles();
    }

    void DbusMediaBrowserHandler::transcodingFinished( QString src, QString dest )
    {
        MediaBrowser::instance()->transcodingFinished( src, dest );
    }

} //namespace Amarok

#include "amarokdbushandler.moc"
