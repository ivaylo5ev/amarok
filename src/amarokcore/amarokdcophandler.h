/***************************************************************************
                          amarokdcophandler.h  -  DCOP Implementation
                             -------------------
    begin                : Sat Oct 11 2003
    copyright            : (C) 2003 by Stanislav Karchebny
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

#ifndef AMAROK_DCOP_HANDLER_H
#define AMAROK_DCOP_HANDLER_H

#include <qobject.h>
#include "amarokdcopiface.h"

namespace amaroK
{

class DcopPlayerHandler : public QObject, virtual public AmarokPlayerInterface
{
      Q_OBJECT

   public:
      DcopPlayerHandler();

   public:
      virtual void play();
      virtual void playPause();
      virtual void stop();
      virtual void next();
      virtual void prev();
      virtual void pause();
      virtual void seek( int s );
      virtual void seekRelative( int s );
      virtual void enableRandomMode( bool enable );
      virtual void enableRepeatPlaylist( bool enable );
      virtual void enableRepeatTrack( bool enable );
      virtual void enableDynamicMode( bool enable );
      virtual int  trackTotalTime();
      virtual int  trackCurrentTime();
      virtual int  trackPlayCounter();
      virtual QString nowPlaying();
      virtual QString artist();
      virtual QString title();
      virtual QString track();
      virtual QString album();
      virtual QString totalTime();
      virtual QString currentTime();
      virtual QString genre();
      virtual QString year();
      virtual QString comment();
      virtual QString bitrate();
      virtual int sampleRate();
      virtual QString encodedURL();
      virtual QString path();
      virtual QString coverImage();
      virtual int score ();
      virtual void setScore( int score );
      virtual void setScoreByPath( const QString &url, int score );
      virtual bool isPlaying();
      virtual int  status();
      virtual bool repeatTrackStatus();
      virtual bool repeatPlaylistStatus();
      virtual bool randomModeStatus();
      virtual bool dynamicModeStatus();
      virtual void setVolume( int );
      virtual int  getVolume();
      virtual void volumeUp();
      virtual void volumeDown();
      virtual void mute();
      virtual void setEqualizerEnabled( bool active );
      virtual void configEqualizer();
      virtual bool equalizerEnabled();
      virtual void enableOSD( bool enable );
      virtual void showOSD();
      virtual QString setContextStyle(const QString&);
      virtual void setEqualizer(int preamp, int band60, int band170, int band310, int band600, int band1k, int band3k, int band6k, int band12k, int band14k, int band16k);

    private:
      virtual void transferCliArgs( QStringList args );
};


class DcopPlaylistHandler : public QObject, virtual public AmarokPlaylistInterface
{
        Q_OBJECT

    public:
        DcopPlaylistHandler();

   public:
      virtual void    addMedia(const KURL &);
      virtual void    addMediaList(const KURL::List &);
      virtual void    clearPlaylist();
      virtual void    shufflePlaylist();
      virtual QString saveCurrentPlaylist();
      virtual void    saveM3u(const QString& path, bool relativePaths);
      virtual void    removeCurrentTrack();
      virtual void    playByIndex(int);
      virtual int     getActiveIndex();
      virtual int     getTotalTrackCount();
      virtual void    setStopAfterCurrent(bool);
      virtual void    togglePlaylist();
      virtual void    playMedia(const KURL &);
      virtual void    shortStatusMessage(const QString&);
      virtual void    popupMessage(const QString&);
};


class DcopCollectionHandler : public QObject, virtual public AmarokCollectionInterface
{
   Q_OBJECT

   public:
       DcopCollectionHandler();

   public /* DCOP */ slots:
      virtual QStringList query(const QString& sql);
      virtual void scanCollection();
      virtual void scanCollectionChanges();
};


class DcopScriptHandler : public QObject, virtual public AmarokScriptInterface
{
   Q_OBJECT

   public:
       DcopScriptHandler();

   public /* DCOP */ slots:
      virtual bool runScript(const QString&);
      virtual bool stopScript(const QString&);
      virtual QStringList listRunningScripts();
      virtual void addCustomMenuItem(QString submenu, QString itemTitle );
      virtual void removeCustomMenuItem(QString submenu, QString itemTitle );
};


} // namespace amaroK

#endif
