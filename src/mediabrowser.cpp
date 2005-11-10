// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// See COPYING file for licensing information


#define DEBUG_PREFIX "MediaBrowser"

#include <config.h>

#include "amarokconfig.h"
#include "browserToolBar.h"
#include "clicklineedit.h"
#include "colorgenerator.h"
#include "debug.h"
#include "k3bexporter.h"
#include "metabundle.h"
#include "playlist.h"      //appendMedia()
#include "statusbar.h"
#include "mediabrowser.h"
#include "amarok.h"

#ifdef HAVE_LIBGPOD
#include "gpodmediadevice/gpodmediadevice.h"
#endif

#include <qdatetime.h>
#include <qgroupbox.h>
#include <qimage.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qpainter.h>
#include <qregexp.h>
#include <qsimplerichtext.h>
#include <qtimer.h>
#include <qtooltip.h>       //QToolTip::add()
#include <qfileinfo.h>
#include <qdir.h>

#include <kapplication.h> //kapp
#include <kdirlister.h>
#include <kfiledialog.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kmountpoint.h>
#include <kpopupmenu.h>
#include <kprogress.h>
#include <kpushbutton.h>
#include <krun.h>
#include <kstandarddirs.h> //locate file
#include <ktabbar.h>
#include <ktempfile.h>
#include <ktoolbarbutton.h> //ctor
#include <kurldrag.h>       //dragObject()


MediaDevice *MediaDevice::s_instance = 0;

QPixmap *MediaItem::s_pixArtist = NULL;
QPixmap *MediaItem::s_pixAlbum = NULL;
QPixmap *MediaItem::s_pixFile = NULL;
QPixmap *MediaItem::s_pixTrack = NULL;
QPixmap *MediaItem::s_pixPodcast = NULL;
QPixmap *MediaItem::s_pixPlaylist = NULL;
QPixmap *MediaItem::s_pixInvisible = NULL;
QPixmap *MediaItem::s_pixStale = NULL;
QPixmap *MediaItem::s_pixOrphaned = NULL;

bool MediaBrowser::isAvailable() //static
{
    // perhaps the user should configure if he wants to use a media device?
#ifdef HAVE_LIBGPOD
    return true;
#else
    return false;
#endif
}

class DummyMediaDevice : public MediaDevice
{
    public:
    DummyMediaDevice( MediaDeviceView *view, MediaDeviceList *list ) : MediaDevice( view, list ) {}
    virtual ~DummyMediaDevice() {}
    virtual bool isConnected() { return false; }
    virtual void addToPlaylist(MediaItem*, MediaItem*, QPtrList<MediaItem>) {}
    virtual MediaItem* newPlaylist(const QString&, MediaItem*, QPtrList<MediaItem>) { return NULL; }
    virtual MediaItem* trackExists(const MetaBundle&) { return NULL; }
    virtual void lockDevice(bool) {}
    virtual void unlockDevice() {} 
    virtual bool openDevice(bool) { return false; }
    virtual bool closeDevice() { return false; }
    virtual void synchronizeDevice() {}
    virtual MediaItem* addTrackToDevice(const QString&, const MetaBundle&, bool) { return NULL; }
    virtual bool deleteItemFromDevice(MediaItem*, bool) { return false; }
    virtual QString createPathname(const MetaBundle&) { return QString(""); }
};


MediaBrowser::MediaBrowser( const char *name )
        : QVBox( 0, name )
        , m_timer( new QTimer( this ) )
{
    KIconLoader iconLoader;
    MediaItem::s_pixTrack = new QPixmap(iconLoader.loadIcon( "player_playlist_2", KIcon::Toolbar, KIcon::SizeSmall ));
    MediaItem::s_pixFile = new QPixmap(iconLoader.loadIcon( "sound", KIcon::Toolbar, KIcon::SizeSmall ) );
    MediaItem::s_pixPodcast = new QPixmap(iconLoader.loadIcon( "favorites", KIcon::Toolbar, KIcon::SizeSmall ) );
    MediaItem::s_pixPlaylist = new QPixmap(iconLoader.loadIcon( "player_playlist_2", KIcon::Toolbar, KIcon::SizeSmall ) );
    // history
    // favorites
    // collection
    // folder
    // folder_red
    // player_playlist_2
    // cancel
    // sound
    MediaItem::s_pixArtist = new QPixmap(iconLoader.loadIcon( "personal", KIcon::Toolbar, KIcon::SizeSmall ) );
    MediaItem::s_pixAlbum = new QPixmap(iconLoader.loadIcon( "cdrom_unmount", KIcon::Toolbar, KIcon::SizeSmall ) );
    MediaItem::s_pixInvisible = new QPixmap(iconLoader.loadIcon( "cancel", KIcon::Toolbar, KIcon::SizeSmall ) );
    MediaItem::s_pixStale = new QPixmap(iconLoader.loadIcon( "cancel", KIcon::Toolbar, KIcon::SizeSmall ) );
    MediaItem::s_pixOrphaned = new QPixmap(iconLoader.loadIcon( "cancel", KIcon::Toolbar, KIcon::SizeSmall ) );

    setSpacing( 4 );

    { //<Search LineEdit>
        KToolBar* searchToolBar = new Browser::ToolBar( this );
        KToolBarButton *button = new KToolBarButton( "locationbar_erase", 0, searchToolBar );
        m_searchEdit = new ClickLineEdit( i18n( "Filter here..." ), searchToolBar );

        searchToolBar->setStretchableWidget( m_searchEdit );
        m_searchEdit->setFrame( QFrame::Sunken );

        connect( button, SIGNAL( clicked() ), m_searchEdit, SLOT( clear() ) );

        QToolTip::add( button, i18n( "Clear filter" ) );
        QToolTip::add( m_searchEdit, i18n( "Enter space-separated terms to filter collection" ) ); //TODO text is wrong
    } //</Search LineEdit>

    m_view = new MediaDeviceView( this );

    connect( m_timer, SIGNAL( timeout() ), SLOT( slotSetFilter() ) );
    connect( m_searchEdit, SIGNAL( textChanged( const QString& ) ), SLOT( slotSetFilterTimeout() ) );
    connect( m_searchEdit, SIGNAL( returnPressed() ), SLOT( slotSetFilter() ) );

    setFocusProxy( m_view ); //default object to get focus
}

void
MediaBrowser::slotSetFilterTimeout() //SLOT
{
    m_timer->start( 280, true ); //stops the timer for us first
}

void
MediaBrowser::slotSetFilter() //SLOT
{
    m_timer->stop();

    m_view->setFilter( m_searchEdit->text() );
}

MediaBrowser::~MediaBrowser()
{
    delete m_view;
}


MediaItem::MediaItem( QListView* parent )
: KListViewItem( parent )
{ 
    init();
}

MediaItem::MediaItem( QListViewItem* parent )
: KListViewItem( parent )
{ 
    init();
}

MediaItem::MediaItem( QListView* parent, QListViewItem* after )
: KListViewItem( parent, after )
{
    init();
}

MediaItem::MediaItem( QListViewItem* parent, QListViewItem* after )
: KListViewItem( parent, after )
{ 
    init();
}

void
MediaItem::init()
{
    m_bundle=NULL;
    m_order=0;
    m_type=UNKNOWN;
    m_playlistName=QString::null;
    setExpandable( false );
    setDragEnabled( true );
    setDropEnabled( true );
}

void MediaItem::setUrl( const QString& url )
{
    m_url.setPath( url );
}

const MetaBundle *
MediaItem::bundle() const
{ 
    if(m_bundle == NULL)
        m_bundle = new MetaBundle( url() );
    return m_bundle;
}

MetaBundle *
MediaItem::bundle()
{ 
    if(m_bundle == NULL) 
        m_bundle = new MetaBundle( url() );
    return m_bundle;
}

void
MediaItem::setType( Type type )
{ 
    m_type=type; 

    setDragEnabled(true);
    setDropEnabled(false);

    switch(m_type)
    {
        case UNKNOWN:
            break;
        case INVISIBLE:
        case TRACK:
        case PODCASTITEM:
            setPixmap(0, *s_pixFile);
            break;
        case PLAYLISTITEM:
            setPixmap(0, *s_pixTrack);
            setDropEnabled(true);
            break;
        case ARTIST:
            setPixmap(0, *s_pixArtist);
            break;
        case ALBUM:
            setPixmap(0, *s_pixAlbum);
            break;
        case PODCASTSROOT:
        case PODCASTCHANNEL:
            setPixmap(0, *s_pixPodcast);
            break;
        case PLAYLISTSROOT:
        case PLAYLIST:
            setPixmap(0, *s_pixPlaylist);
            setDropEnabled(true);
            break;
        case INVISIBLEROOT:
            setPixmap(0, *s_pixInvisible);
            break;
        case STALEROOT:
        case STALE:
            setPixmap(0, *s_pixStale);
            break;
        case ORPHANEDROOT:
        case ORPHANED:
            setPixmap(0, *s_pixOrphaned);
            break;
    }
}

MediaItem *
MediaItem::lastChild() const
{
    QListViewItem *last = NULL;
    for( QListViewItem *it = firstChild();
            it;
            it = it->nextSibling() )
    {
        last = it;
    }

    return dynamic_cast<MediaItem *>(last);
}

bool
MediaItem::isLeaveItem() const
{
    switch(type())
    {
        case UNKNOWN:
            return false;

        case INVISIBLE:
        case TRACK:
        case PODCASTITEM:
        case PLAYLISTITEM:
        case STALE:
        case ORPHANED:
            return true;

        case ARTIST:
        case ALBUM:
        case PODCASTSROOT:
        case PODCASTCHANNEL:
        case PLAYLISTSROOT:
        case PLAYLIST:
        case INVISIBLEROOT:
        case STALEROOT:
        case ORPHANEDROOT:
            return false;
    }

    return false;
}

MediaItem *
MediaItem::findItem( const QString &key ) const
{
    for(MediaItem *it = dynamic_cast<MediaItem *>(firstChild());
            it;
            it = dynamic_cast<MediaItem *>(it->nextSibling()))
    {
        if(key == it->text(0))
            return it;
    }
    return NULL;
}

int
MediaItem::compare(QListViewItem *i, int col, bool ascending) const
{
    MediaItem *item = dynamic_cast<MediaItem *>(i);
    if(item && col==0 && item->m_order != m_order)
        return ascending ? m_order-item->m_order : item->m_order-m_order;

    return KListViewItem::compare(i, col, ascending);
}


MediaDeviceList::MediaDeviceList( MediaDeviceView* parent )
    : KListView( parent )
    , m_parent( parent )
{
    setSelectionMode( QListView::Extended );
    setItemsMovable( false );
    setShowSortIndicator( true );
    setFullWidth( true );
    setRootIsDecorated( true );
    setDragEnabled( true );
    setDropVisualizer( true );    //the visualizer (a line marker) is drawn when dragging over tracks
    setDropHighlighter( true );    //and the highligther (a focus rect) is drawn when dragging over playlists
    setDropVisualizerWidth( 3 );
    setAcceptDrops( true );

    addColumn( i18n( "Artist" ) );

    connect( this, SIGNAL( rightButtonPressed( QListViewItem*, const QPoint&, int ) ),
             this,   SLOT( rmbPressed( QListViewItem*, const QPoint&, int ) ) );

    connect( this, SIGNAL( itemRenamed( QListViewItem* ) ),
             this,   SLOT( renameItem( QListViewItem* ) ) );
}

void
MediaDeviceList::renameItem( QListViewItem *item )
{
    if(m_parent && m_parent->m_device)
        m_parent->m_device->renameItem( item );
}


MediaDeviceList::~MediaDeviceList()
{}


void
MediaDeviceList::startDrag()
{
    KURL::List urls = nodeBuildDragList( 0 );
    debug() << urls.first().path() << endl;
    KURLDrag* d = new KURLDrag( urls, this );
    d->dragCopy();
}


KURL::List
MediaDeviceList::nodeBuildDragList( MediaItem* item, bool onlySelected )
{
    KURL::List items;
    MediaItem* fi;

    if ( !item )
    {
        fi = (MediaItem*)firstChild();
    }
    else
        fi = item;

    while ( fi )
    {
        if( fi->isVisible() )
        {
            if ( fi->isSelected() || !onlySelected )
            {
                if( fi->isLeaveItem() )
                {
                    items += fi->url().path();
                }
                else
                {
                    if(fi->childCount() )
                        items += nodeBuildDragList( (MediaItem*)fi->firstChild(), false );
                }
            }
            else
            {
                if ( fi->childCount() )
                    items += nodeBuildDragList( (MediaItem*)fi->firstChild(), true );
            }
        }

        fi = (MediaItem*)fi->nextSibling();
    }

    return items;
}

int
MediaDeviceList::getSelectedLeaves(MediaItem *parent, QPtrList<MediaItem> *list, bool onlySelected, bool onlyPlayed )
{
    int numFiles = 0;
    if(list==NULL)
        list = new QPtrList<MediaItem>;

    MediaItem *it;
    if(parent==NULL)
        it = dynamic_cast<MediaItem *>(firstChild());
    else
        it = dynamic_cast<MediaItem *>(parent->firstChild());

    for( ; it; it = dynamic_cast<MediaItem*>(it->nextSibling()))
    {
        if( it->isVisible() )
        {
            if(it->childCount())
            {
                numFiles += getSelectedLeaves(it, list, onlySelected && !it->isSelected(), onlyPlayed);
            }
            else if(it->isSelected() || !onlySelected)
            {
                if(it->type() == MediaItem::TRACK
                        || it->type() == MediaItem::PODCASTITEM
                        || it->type() == MediaItem::INVISIBLE
                        || it->type() == MediaItem::ORPHANED)
                {
                    if(onlyPlayed)
                    {
                        if(it->played() > 0)
                            numFiles++;
                    }
                    else
                    {
                        numFiles++;
                    }
                }
                if(it->isLeaveItem() && (!onlyPlayed || it->played()>0))
                    list->append( it );
            }
        }
    }

    return numFiles;
}


void
MediaDeviceList::contentsDragEnterEvent( QDragEnterEvent *e )
{
    debug() << "Items dropping?" << endl;
    e->accept( e->source() == viewport() || KURLDrag::canDecode( e ) );
}


void
MediaDeviceList::contentsDropEvent( QDropEvent *e )
{
    if(e->source() == viewport() || e->source() == this)
    {
        const QPoint p = contentsToViewport( e->pos() );
        MediaItem *item = dynamic_cast<MediaItem *>(itemAt( p ));
        QPtrList<MediaItem> items;
        if(item->type()==MediaItem::PLAYLIST)
        {
            MediaItem *list = item;
            MediaItem *after = NULL;
            for(MediaItem *it = dynamic_cast<MediaItem *>(item->firstChild());
                    it;
                    it = dynamic_cast<MediaItem *>(it->nextSibling()))
                after = it;

            getSelectedLeaves(NULL, &items);
            m_parent->m_device->addToPlaylist(list, after, items);
        }
        else if(item->type()==MediaItem::PLAYLISTITEM)
        {
            MediaItem *list = dynamic_cast<MediaItem *>(item->parent());
            MediaItem *after = NULL;
            for(MediaItem *it = dynamic_cast<MediaItem*>(item->parent()->firstChild());
                    it;
                    it = dynamic_cast<MediaItem *>(it->nextSibling()))
            {
                if(it == item)
                    break;
                after = it;
            }

            getSelectedLeaves(NULL, &items);
            m_parent->m_device->addToPlaylist(list, after, items);
        }
        else if(item->type()==MediaItem::PLAYLISTSROOT)
        {
            QPtrList<MediaItem> items;
            getSelectedLeaves(NULL, &items);
            QString base(i18n("New Playlist"));
            QString name = base;
            int i=1;
            while(item->findItem(name))
            {
                QString num;
                num.setNum(i);
                name = base + " " + num;
                i++;
            }
            MediaItem *pl = m_parent->m_device->newPlaylist(name, item, items);
            ensureItemVisible(pl);
            m_renameFrom = pl->text(0);
            rename(pl, 0);
        }
    }
    else
    {

        KURL::List list;
        if ( KURLDrag::decode( e, list ) )
        {
            KURL::List::Iterator it = list.begin();
            for ( ; it != list.end(); ++it )
            {
                MediaDevice::instance()->addURL( *it );
            }
        }
    }
}


void
MediaDeviceList::contentsDragMoveEvent( QDragMoveEvent *e )
{
//    const QPoint p = contentsToViewport( e->pos() );
//    QListViewItem *item = itemAt( p );
    e->accept( e->source() == viewport()
            || e->source() == this
            || (e->source() != m_parent
                && e->source() != m_parent->m_device->m_transferList
                && e->source() != m_parent->m_device->m_transferList->viewport()
                && KURLDrag::canDecode( e )) );

}


void
MediaDeviceList::viewportPaintEvent( QPaintEvent *e )
{
    KListView::viewportPaintEvent( e );

    // Superimpose bubble help:

    if ( !m_parent->m_connectButton->isOn() )
    {
        QPainter p( viewport() );

        QSimpleRichText t( i18n(
                "<div align=center>"
                  "<h3>MediaDevice Browser</h3>"
                  "Click the Connect button to access your mounted media device. "
                  "Drag and drop files to enqueue them for transfer."
                "</div>" ), QApplication::font() );

        t.setWidth( width() - 50 );

        const uint w = t.width() + 20;
        const uint h = t.height() + 20;

        p.setBrush( colorGroup().background() );
        p.drawRoundRect( 15, 15, w, h, (8*200)/w, (8*200)/h );
        t.draw( &p, 20, 20, QRect(), colorGroup() );
    }
}


void
MediaDeviceList::rmbPressed( QListViewItem* qitem, const QPoint& point, int ) //SLOT
{
    MediaItem *item = dynamic_cast<MediaItem *>(qitem);
    if ( item )
    {
        KURL::List urls = nodeBuildDragList( 0 );
        KPopupMenu menu( this );

        enum Actions { APPEND, LOAD, QUEUE,
            BURN_ARTIST, BURN_ALBUM, BURN_DATACD, BURN_AUDIOCD,
            RENAME, MAKE_PLAYLIST, ADD_TO_PLAYLIST,
            ADD, DELETE_PLAYED, DELETE,
            FIRST_PLAYLIST};

        menu.insertItem( SmallIconSet( "player_playlist_2" ), i18n( "&Load" ), LOAD );
        menu.insertItem( SmallIconSet( "1downarrow" ), i18n( "&Append to Playlist" ), APPEND );
        menu.insertItem( SmallIconSet( "2rightarrow" ), i18n( "&Queue Tracks" ), QUEUE );
        menu.insertSeparator();

        switch ( item->depth() )
        {
        case 0:
            menu.insertItem( SmallIconSet( "cdrom_unmount" ), i18n( "Burn All Tracks by This Artist" ), BURN_ARTIST );
            menu.setItemEnabled( BURN_ARTIST, K3bExporter::isAvailable() );
            break;

        case 1:
            menu.insertItem( SmallIconSet( "cdrom_unmount" ), i18n( "Burn This Album" ), BURN_ALBUM );
            menu.setItemEnabled( BURN_ALBUM, K3bExporter::isAvailable() );
            break;

        case 2:
            menu.insertItem( SmallIconSet( "cdrom_unmount" ), i18n( "Burn to CD as Data" ), BURN_DATACD );
            menu.setItemEnabled( BURN_DATACD, K3bExporter::isAvailable() );
            menu.insertItem( SmallIconSet( "cdaudio_unmount" ), i18n( "Burn to CD as Audio" ), BURN_AUDIOCD );
            menu.setItemEnabled( BURN_AUDIOCD, K3bExporter::isAvailable() );
            break;
        }

        menu.insertSeparator();

        KPopupMenu *playlistsMenu = NULL;
        switch( item->type() )
        {
        case MediaItem::ARTIST:
        case MediaItem::ALBUM:
        case MediaItem::TRACK:
        case MediaItem::PODCASTCHANNEL:
        case MediaItem::PODCASTSROOT:
        case MediaItem::PODCASTITEM:
            if(m_parent->m_device->m_playlistItem)
            {
                menu.insertItem( SmallIconSet( "player_playlist_2" ), i18n( "Make Media Device Playlist" ), MAKE_PLAYLIST );

                playlistsMenu = new KPopupMenu(&menu);
                int i=0;
                for(MediaItem *it = dynamic_cast<MediaItem *>(m_parent->m_device->m_playlistItem->firstChild());
                        it;
                        it = dynamic_cast<MediaItem *>(it->nextSibling()))
                {
                    playlistsMenu->insertItem( SmallIconSet( "player_playlist_2" ), it->text(0), FIRST_PLAYLIST+i );
                    i++;
                }
                menu.insertItem( SmallIconSet( "player_playlist_2" ), i18n("Add to Playlist"), playlistsMenu, ADD_TO_PLAYLIST );
                menu.setItemEnabled( ADD_TO_PLAYLIST, m_parent->m_device->m_playlistItem->childCount()>0 );
                menu.insertSeparator();
            }
            break;

        case MediaItem::ORPHANED:
        case MediaItem::ORPHANEDROOT:
            menu.insertItem( SmallIconSet( "editrename" ), i18n( "Add to Database" ), ADD );
            break;

        case MediaItem::PLAYLIST:
            menu.insertItem( SmallIconSet( "editrename" ), i18n( "Rename" ), RENAME );
            break;

        default:
            break;
        }

        if( item->type() == MediaItem::PODCASTSROOT || item->type() == MediaItem::PODCASTCHANNEL )
        {
            menu.insertItem( SmallIconSet( "editdelete" ), i18n( "Delete Podcasts Already Played" ), DELETE_PLAYED );
        }
        menu.insertItem( SmallIconSet( "editdelete" ), i18n( "Delete" ), DELETE );

        int id =  menu.exec( point );
        switch( id )
        {
            case LOAD:
                Playlist::instance()->insertMedia( urls, Playlist::Replace );
                break;
            case APPEND:
                Playlist::instance()->insertMedia( urls, Playlist::Append );
                break;
            case QUEUE:
                Playlist::instance()->insertMedia( urls, Playlist::Queue );
                break;
            case BURN_ARTIST:
                K3bExporter::instance()->exportArtist( item->text(0) );
                break;
            case BURN_ALBUM:
                K3bExporter::instance()->exportAlbum( item->text(0) );
                break;
            case BURN_DATACD:
                K3bExporter::instance()->exportTracks( urls, K3bExporter::DataCD );
                break;
            case BURN_AUDIOCD:
                K3bExporter::instance()->exportTracks( urls, K3bExporter::AudioCD );
                break;
            case RENAME:
                m_renameFrom = item->text(0);
                rename(item, 0);
                break;
            case MAKE_PLAYLIST:
                {
                    QPtrList<MediaItem> items;
                    getSelectedLeaves(NULL, &items);
                    QString base(i18n("New Playlist"));
                    QString name = base;
                    int i=1;
                    while(m_parent->m_device->m_playlistItem->findItem(name))
                    {
                        QString num;
                        num.setNum(i);
                        name = base + " " + num;
                        i++;
                    }
                    MediaItem *pl = m_parent->m_device->newPlaylist(name, m_parent->m_device->m_playlistItem, items);
                    ensureItemVisible(pl);
                    m_renameFrom = pl->text(0);
                    rename(pl, 0);
                }
                break;
            case ADD:
                if(item->type() == MediaItem::ORPHANEDROOT)
                {
                    MediaItem *next = NULL;
                    for(MediaItem *it = dynamic_cast<MediaItem *>(item->firstChild());
                            it;
                            it = next)
                    {
                        next = dynamic_cast<MediaItem *>(it->nextSibling());
                        item->takeItem(it);
                        m_parent->m_device->addTrackToDevice(it->url().path(), *it->bundle(), false);
                        delete it;
                    }
                }
                else
                {
                    for(selectedItems().first();
                            selectedItems().current();
                            selectedItems().next())
                    {
                        MediaItem *it = dynamic_cast<MediaItem *>(selectedItems().current());
                        if(it->type() == MediaItem::ORPHANED)
                        {
                            it->parent()->takeItem(it);
                            m_parent->m_device->addTrackToDevice(it->url().path(), *it->bundle(), false);
                            delete it;
                        }
                    }
                }
                break;
            case DELETE_PLAYED:
                {
                    MediaItem *podcasts = NULL;
                    if(item->type() == MediaItem::PODCASTCHANNEL)
                        podcasts = dynamic_cast<MediaItem *>(item->parent());
                    else
                        podcasts = item;
                    m_parent->m_device->deleteFromDevice( podcasts, true );
                }
                break;
            case DELETE:
                m_parent->m_device->deleteFromDevice( NULL );
                break;
            default:
                if(id >= FIRST_PLAYLIST)
                {
                    QString name = playlistsMenu->text(id);
                    if(name != QString::null)
                    {
                        MediaItem *list = m_parent->m_device->m_playlistItem->findItem(name);
                        if(list)
                        {
                            MediaItem *after = NULL;
                            for(MediaItem *it = dynamic_cast<MediaItem *>(list->firstChild());
                                    it;
                                    it = dynamic_cast<MediaItem *>(it->nextSibling()))
                                after = it;
                            QPtrList<MediaItem> items;
                            getSelectedLeaves(NULL, &items);
                            m_parent->m_device->addToPlaylist(list, after, items);
                        }
                    }
                }
                break;
        }
    }
}

MediaDeviceView::MediaDeviceView( MediaBrowser* parent )
    : QVBox( parent )
    , m_stats( NULL )
    , m_device( NULL )
    , m_deviceList( new MediaDeviceList( this ) )
    , m_parent( parent )
{
#ifdef HAVE_LIBGPOD
    m_device = new GpodMediaDevice( this, m_deviceList );
#else
    m_device = new DummyMediaDevice( this, m_deviceList );
#endif
    m_progress = new KProgress( this );

    QHBox* hb = new QHBox( this );
    hb->setSpacing( 1 );
    m_connectButton  = new KPushButton( SmallIconSet( "usbpendrive_mount" ), i18n( "Connect"), hb );
    m_transferButton = new KPushButton( SmallIconSet( "rebuild" ), i18n( "Transfer" ), hb );
    m_playlistButton = new KPushButton( KGuiItem( QString::null, "player_playlist_2" ), hb );
    m_playlistButton->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );
    m_playlistButton->setToggleButton( true );
    m_configButton   = new KPushButton( KGuiItem( QString::null, "configure" ), hb );
    m_configButton->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred ); // too big!

    m_stats = new QLabel( i18n( "1 track in queue", "%n tracks in queue", m_device->m_transferList->childCount() ), this );

    QToolTip::add( m_connectButton,  i18n( "Connect media device" ) );
    QToolTip::add( m_transferButton, i18n( "Transfer tracks to media device" ) );
    QToolTip::add( m_playlistButton, i18n( "Append transferred items to playlist \"New amaroK additions\"" ) );
    QToolTip::add( m_configButton,   i18n( "Configure media device" ) );

    m_connectButton->setToggleButton( true );
    m_connectButton->setOn( true );
    m_device->connectDevice( true );
    m_connectButton->setOn( m_device->isConnected() ||  m_deviceList->childCount() != 0 );
    m_transferButton->setDisabled( true );

    m_progress->setFixedHeight( m_transferButton->sizeHint().height() );
    m_progress->hide();

    connect( m_connectButton,  SIGNAL( clicked() ), MediaDevice::instance(), SLOT( connectDevice() ) );
    connect( m_transferButton, SIGNAL( clicked() ), MediaDevice::instance(), SLOT( transferFiles() ) );
    connect( m_configButton,   SIGNAL( clicked() ), MediaDevice::instance(), SLOT( config() ) );
    connect( m_device->m_transferList, SIGNAL( rightButtonPressed( QListViewItem*, const QPoint&, int ) ),
             this,   SLOT( slotShowContextMenu( QListViewItem*, const QPoint&, int ) ) );

    m_device->loadTransferList( amaroK::saveLocation() + "transferlist.xml" );
}

void
MediaDeviceView::slotShowContextMenu( QListViewItem* item, const QPoint& point, int )
{
    if ( item )
    {
        KPopupMenu menu( this );

        enum Actions { REMOVE_SELECTED, CLEAR_ALL };

        menu.insertItem( SmallIconSet( "edittrash" ), i18n( "&Remove From Queue" ), REMOVE_SELECTED );
        menu.insertItem( SmallIconSet( "view_remove" ), i18n( "&Clear Queue" ), CLEAR_ALL );

        switch( menu.exec( point ) )
        {
            case REMOVE_SELECTED:
                m_device->removeSelected();
                break;
            case CLEAR_ALL:
                m_device->clearItems();
                break;
        }
    }
}


MediaDeviceView::~MediaDeviceView()
{
    m_device->saveTransferList( amaroK::saveLocation() + "transferlist.xml" );
    m_device->closeDevice();

    delete m_deviceList;
    delete m_device;
}

bool
MediaDeviceView::setFilter( const QString &filter, MediaItem *parent )
{
    MediaItem *it;
    if(parent==NULL)
    {
        it = dynamic_cast<MediaItem *>(m_deviceList->firstChild());
    }
    else
    {
        it = dynamic_cast<MediaItem *>(parent->firstChild());
    }

    bool childrenVisible = false;
    for( ; it; it = dynamic_cast<MediaItem *>(it->nextSibling()))
    {
        bool visible = true;
        if(it->isLeaveItem())
        {
            visible = match(it, filter);
        }
        else
        {
            if(it->type()==MediaItem::PLAYLISTSROOT || it->type()==MediaItem::PLAYLIST)
            {
                visible = true;
                setFilter(filter, it);
            }
            else
                visible = setFilter(filter, it);
        }
        it->setVisible( visible );
        if(visible)
            childrenVisible = true;
    }

    return childrenVisible;
}

bool
MediaDeviceView::match( const MediaItem *it, const QString &filter )
{
    if(filter.isNull() || filter.isEmpty())
        return true;

    if(it->text(0).lower().contains(filter.lower()))
        return true;

    QListViewItem *p = it->parent();
    if(p && p->text(0).lower().contains(filter.lower()))
        return true;

    if(p)
    {
        p = p->parent();
        if(p && p->text(0).lower().contains(filter.lower()))
            return true;
    }

    return false;
}


MediaDevice::MediaDevice( MediaDeviceView* parent, MediaDeviceList *listview )
    : m_parent( parent )
    , m_listview( listview )
    , m_transferList( new MediaDeviceTransferList( parent ) )
    , m_playlistItem( NULL )
    , m_podcastItem( NULL )
    , m_invisibleItem( NULL )
    , m_staleItem( NULL )
    , m_orphanedItem( NULL )
{
    s_instance = this;

    sysProc = new KShellProcess(); Q_CHECK_PTR(sysProc);

    m_mntpnt = AmarokConfig::mountPoint();
    m_mntcmd = AmarokConfig::mountCommand();
    m_umntcmd = AmarokConfig::umountCommand();
    m_autoDeletePodcasts = AmarokConfig::autoDeletePodcasts();
}

MediaDevice::~MediaDevice()
{
    delete m_transferList;
}

void
MediaDevice::updateRootItems()
{
    if(m_podcastItem)
        m_podcastItem->setVisible(m_podcastItem->childCount() > 0);
    if(m_invisibleItem)
        m_invisibleItem->setVisible(m_invisibleItem->childCount() > 0);
    if(m_staleItem)
        m_staleItem->setVisible(m_staleItem->childCount() > 0);
    if(m_orphanedItem)
        m_orphanedItem->setVisible(m_orphanedItem->childCount() > 0);
}

void
MediaDevice::addURL( const KURL& url, MetaBundle *bundle, bool isPodcast, const QString &playlistName )
{
    if(!bundle)
        bundle = new MetaBundle( url );
    if ( !playlistName.isNull() || (!trackExists( *bundle ) && ( m_transferList->findPath( url.path() ) == NULL )) )
    {
        MediaItem* item = new MediaItem( m_transferList, m_transferList->lastItem() );
        item->setExpandable( false );
        item->setDropEnabled( true );
        item->setUrl( url.path() );
        item->m_bundle = bundle;
        item->m_playlistName = playlistName;
        if(isPodcast)
            item->m_type = MediaItem::PODCASTITEM;

        QString text = item->bundle()->prettyTitle();
        if(item->type() == MediaItem::PODCASTITEM)
        {
            text += " (" + i18n("Podcast") + ")";
        }
        item->setText( 0, text);

        m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount() ) );
        m_parent->m_transferButton->setEnabled( m_parent->m_device->isConnected() || m_parent->m_deviceList->childCount() != 0 );
        m_parent->m_progress->setTotalSteps( m_parent->m_progress->totalSteps() + 1 );
    } else
        amaroK::StatusBar::instance()->longMessage( i18n( "Track already exists on media device: " + url.path().local8Bit() ), KDE::StatusBar::Sorry );
}

void
MediaDevice::addURLs( const KURL::List urls, const QString &playlistName )
{
        KURL::List::ConstIterator it = urls.begin();
        for ( ; it != urls.end(); ++it )
            addURL( *it, NULL, false, playlistName );
}

void
MediaDevice::clearItems()
{
    m_transferList->clear();
    if(m_parent && m_parent->m_stats)
        m_parent->m_stats->setText( i18n( "0 tracks in queue" ) );
    if(m_parent && m_parent->m_transferButton)
        m_parent->m_transferButton->setEnabled( false );
}

void
MediaDevice::removeSelected()
{
    QPtrList<QListViewItem>  selected = m_transferList->selectedItems();

    for( QListViewItem *item = selected.first(); item; item = selected.next() )
    {
        m_transferList->takeItem( item );
        delete item;
    }
    m_parent->m_transferButton->setEnabled( m_transferList->childCount() != 0 );
    m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount() ) );
}

void
MediaDevice::config()
{
    KDialogBase dialog( m_parent, 0, false );
    kapp->setTopWidget( &dialog );
    dialog.setCaption( kapp->makeStdCaption( i18n("Configure Media Device") ) );
    dialog.showButtonApply( false );
    QVBox *box = dialog.makeVBoxMainWidget();

    QLabel *mntpntLabel = new QLabel( box );
    mntpntLabel->setText( i18n( "&Mount point:" ) );
    QLineEdit *mntpnt = new QLineEdit( m_mntpnt, box );
    mntpntLabel->setBuddy( mntpnt );
    QToolTip::add( mntpnt, i18n( "Set the mount point of your device here, when empty autodetection is tried." ) );

    QLabel *mntLabel = new QLabel( box );
    mntLabel->setText( i18n( "&Mount command:" ) );
    QLineEdit *mntcmd = new QLineEdit( m_mntcmd, box );
    mntLabel->setBuddy( mntcmd );
    QToolTip::add( mntcmd, i18n( "Set the command to mount your device here, empty commands are not executed." ) );

    QLabel *umntLabel = new QLabel( box );
    umntLabel->setText( i18n( "&Unmount command:" ) );
    QLineEdit *umntcmd = new QLineEdit( m_umntcmd, box );
    umntLabel->setBuddy( umntcmd );
    QToolTip::add( umntcmd, i18n( "Set the command to unmount your device here, empty commands are not executed." ) );

    QHBox *hbox = new QHBox( box );
    QCheckBox *autoDeletePodcasts = new QCheckBox( hbox );
    QLabel *autoDeleteLabel = new QLabel( hbox );
    autoDeleteLabel->setBuddy( autoDeletePodcasts );
    autoDeleteLabel->setText( i18n( "Automatically delete podcasts" ) );
    QToolTip::add( autoDeletePodcasts, i18n( "Automatically delete podcast shows already played on connect" ) );

    if ( dialog.exec() != QDialog::Rejected )
    {
        setMountPoint( mntpnt->text() );
        setMountCommand( mntcmd->text() );
        setUmountCommand( umntcmd->text() );
        setAutoDeletePodcasts( autoDeletePodcasts->isChecked() );
    }
}

void MediaDevice::setMountPoint(const QString & mntpnt)
{
    AmarokConfig::setMountPoint( mntpnt );
    m_mntpnt = mntpnt;          //Update mount point
}

void MediaDevice::setMountCommand(const QString & mnt)
{
    AmarokConfig::setMountCommand( mnt );
    m_mntcmd = mnt;             //Update for mount()
}

void MediaDevice::setUmountCommand(const QString & umnt)
{
    AmarokConfig::setUmountCommand( umnt );
    m_umntcmd = umnt;        //Update for umount()
}

void MediaDevice::setAutoDeletePodcasts( bool value )
{
    AmarokConfig::setAutoDeletePodcasts( value );
    m_autoDeletePodcasts = value; //Update
}

int MediaDevice::mount()
{
    debug() << "mounting" << endl;
    QString cmdS=m_mntcmd;

    debug() << "attempting mount with command: [" << cmdS << "]" << endl;
    int e=sysCall(cmdS);
    debug() << "mount-cmd: e=" << e << endl;
    return e;
}

int MediaDevice::umount()
{
    debug() << "umounting" << endl;
    QString cmdS=m_umntcmd;

    debug() << "attempting umount with command: [" << cmdS << "]" << endl;
    int e=sysCall(cmdS);
    debug() << "umount-cmd: e=" << e << endl;

    return e;
}

int MediaDevice::sysCall(const QString & command)
{
    if ( sysProc->isRunning() )  return -1;

        sysProc->clearArguments();
        (*sysProc) << command;
        if (!sysProc->start( KProcess::Block, KProcess::AllOutput ))
            kdFatal() << i18n("could not execute %1").arg(command.local8Bit().data()) << endl;

    return (sysProc->exitStatus());
}

void
MediaDevice::connectDevice( bool silent )
{
    if ( m_parent->m_connectButton->isOn() )
    {
        if ( !m_mntcmd.isEmpty() )
        {
            mount();
        }

        openDevice( silent );

        if( isConnected() || m_parent->m_deviceList->childCount() != 0 )
        {
            m_parent->m_connectButton->setOn( true );
            if ( m_transferList->childCount() != 0 )
            {
                m_parent->m_transferButton->setEnabled( true );
                m_parent->m_stats->setText( i18n( "Checking device for duplicate files." ) );
                KURL::List urls;
                MediaItem *next = NULL;
                for( MediaItem *cur = dynamic_cast<MediaItem *>(m_transferList->firstChild());
                        cur != NULL;
                        cur = next)
                {
                    next = dynamic_cast<MediaItem *>(cur->nextSibling());
                    if ( trackExists( *cur->bundle() ) )
                    {
                        delete cur;
                    }
                }
            }

            // delete podcasts already played
            if(m_podcastItem)
            {
                QPtrList<MediaItem> list;
                //NOTE we assume that currentItem is the main target
                int numFiles  = m_parent->m_deviceList->getSelectedLeaves(m_podcastItem, &list, false /* not only selected */, true /* only played */ );

                if(numFiles > 0)
                {
                    m_parent->m_stats->setText( i18n( "1 track to be deleted", "%n tracks to be deleted", numFiles ) );

                    m_parent->m_progress->setProgress( 0 );
                    m_parent->m_progress->setTotalSteps( numFiles );
                    m_parent->m_progress->show();

                    lockDevice(true);

                    deleteItemFromDevice(m_podcastItem, true);

                    synchronizeDevice();
                    unlockDevice();

                    QTimer::singleShot( 1500, m_parent->m_progress, SLOT(hide()) );
                    m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount() ) );
                }
            }

            updateRootItems();
        }
        else
        {
            m_parent->m_connectButton->setOn( false );
            if(!silent)
            {
                KMessageBox::error( m_parent->m_parent,
                        i18n( "Could not find device, please mount it and try again." ),
                        i18n( "Media Device Browser" ) );
            }
        }
    }
    else
    {
        if ( m_transferList->childCount() != 0 &&  isConnected() )
        {
            KGuiItem transfer = KGuiItem(i18n("&Transfer"),"rebuild");
            KGuiItem disconnect = KGuiItem(i18n("Disconnect immediately"),"rebuild");
            int button = KMessageBox::warningYesNo( m_parent->m_parent,
                    i18n( "There are tracks queued for transfer."
                        " Would you like to transfer them before disconnecting?"),
                    i18n( "Media Device Browser" ),
                    transfer, disconnect);

            if ( button == KMessageBox::Yes )
            {
                transferFiles();
            }
        }

        m_parent->m_transferButton->setEnabled( false );

        closeDevice();
        QString text = i18n( "Your device is now in sync, please unmount it and disconnect now." );

        if ( !m_umntcmd.isEmpty() )
        {
            if(umount()==0)
                text=i18n( "Your device is now in sync, you can disconnect now." );
        }

        m_parent->m_connectButton->setOn( false );
        KMessageBox::information( m_parent->m_parent, text, i18n( "Media Device Browser" ) );
    }
}


void
MediaDevice::fileTransferred()  //SLOT
{
    m_wait = false;
    m_parent->m_progress->setProgress( m_parent->m_progress->progress() + 1 );
    // the track just transferred has not yet been removed from the queue
    m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount()-1 ) );
}

void
MediaDevice::transferFiles()
{
    m_parent->m_transferButton->setEnabled( false );

    m_parent->m_progress->setProgress( 0 );
    m_parent->m_progress->setTotalSteps( m_transferList->childCount() );
    m_parent->m_progress->show();

    // ok, let's copy the stuff to the device
    lockDevice( true );

    MediaItem *playlist = NULL;
    if(m_playlistItem && m_parent->m_playlistButton->isOn())
    {
        QString name = i18n("New amaroK additions");
        playlist = m_playlistItem->findItem( name );
        if(!playlist)
        {
            QPtrList<MediaItem> items;
            playlist = newPlaylist(name, m_playlistItem, items);
        }
    }

    MediaItem *after = NULL; // item after which to insert into playlist
    // iterate through items
    for( MediaItem *cur =  dynamic_cast<MediaItem *>(m_transferList->firstChild());
            cur != NULL;
            cur =  dynamic_cast<MediaItem *>(m_transferList->firstChild()) )
    {
        debug() << "Transfering: " << cur->url().path() << endl;

        MetaBundle *bundle = cur->bundle();
        if(!bundle)
        {
            cur->m_bundle = new MetaBundle( cur->url() );
            bundle = cur->m_bundle;
        }

        MediaItem *item = trackExists( *bundle );
        if(!item)
        {
            QString trackpath = createPathname(*bundle);

            // check if path exists and make it if needed
            QFileInfo finfo( trackpath );
            QDir dir = finfo.dir();
            while ( !dir.exists() )
            {
                QString path = dir.absPath();
                QDir parentdir;
                QDir create;
                do
                {
                    create.setPath(path);
                    path = path.section("/", 0, path.contains('/')-1);
                    parentdir.setPath(path);
                }
                while( !path.isEmpty() && !(path==m_mntpnt) && !parentdir.exists() );
                debug() << "trying to create \"" << path << "\"" << endl;
                if(!create.mkdir( create.absPath() ))
                {
                    break;
                }
            }

            if ( !dir.exists() )
            {
                KMessageBox::error( m_parent->m_parent,
                        i18n("Could not create directory for file") + trackpath,
                        i18n( "Media Device Browser" ) );
                delete bundle;
                break;
            }

            m_wait = true;

            KIO::CopyJob *job = KIO::copy( cur->url(), KURL( trackpath ), false );
            connect( job, SIGNAL( copyingDone( KIO::Job *, const KURL &, const KURL &, bool, bool ) ),
                    this,  SLOT( fileTransferred() ) );

            while ( m_wait )
            {
                usleep(10000);
                kapp->processEvents( 100 );
            }


            KURL url;
            url.setPath(trackpath);
            MetaBundle bundle2(url);
            if(!bundle2.isValidMedia())
            {
                // probably s.th. went wrong
                debug() << "Reading tags failed! File not added!" << endl;
                QFile::remove( trackpath );
            }
            else
            {
                item = addTrackToDevice(trackpath, *bundle, cur->type() == MediaItem::PODCASTITEM);
            }
        }

        if(item)
        {
            if(playlist)
            {
                QPtrList<MediaItem> items;
                items.append(item);
                addToPlaylist(playlist, after, items);
                after = item;
            }

            if(m_playlistItem && cur->m_playlistName!=QString::null)
            {
                MediaItem *pl = m_playlistItem->findItem( cur->m_playlistName );
                if(!pl)
                {
                    QPtrList<MediaItem> items;
                    pl = newPlaylist(cur->m_playlistName, m_playlistItem, items);
                }
                if(pl)
                {
                    QPtrList<MediaItem> items;
                    items.append(item);
                    addToPlaylist(pl, pl->lastChild(), items);
                }
            }

            m_transferList->takeItem( cur );
            delete cur->m_bundle;
            delete cur;
            cur = NULL;
        }
    }
    synchronizeDevice();
    unlockDevice();
    fileTransferFinished();

    m_parent->m_transferButton->setEnabled( m_transferList->childCount()>0 );
}


void
MediaDevice::fileTransferFinished()  //SLOT
{
    m_transferList->clear();

    m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount() ) );
    m_parent->m_progress->hide();
    m_parent->m_transferButton->setDisabled( true );
}

void
MediaDevice::deleteFile( const KURL &url )
{
    debug() << "deleting " << url.prettyURL() << endl;
    KIO::file_delete( url, false );
    m_parent->m_progress->setProgress( m_parent->m_progress->progress() + 1 );
}

void
MediaDevice::deleteFromDevice(MediaItem *item, bool onlyPlayed, bool recursing)
{
    MediaItem* fi = item;

    if ( !recursing )
    {
        QPtrList<MediaItem> list;
        //NOTE we assume that currentItem is the main target
        int numFiles  = m_parent->m_deviceList->getSelectedLeaves(item, &list, true /* only selected */, onlyPlayed);

        m_parent->m_stats->setText( i18n( "1 track to be deleted", "%n tracks to be deleted", numFiles ) );
        if(numFiles > 0)
        {
            int button = KMessageBox::warningContinueCancel( m_parent,
                    i18n( "<p>You have selected 1 track to be <b>irreversibly</b> deleted.",
                        "<p>You have selected %n tracks to be <b>irreversibly</b> deleted.",
                        numFiles
                        ),
                    QString::null,
                    KGuiItem(i18n("&Delete"),"editdelete") );

            if ( button != KMessageBox::Continue )
            {
                m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount() ) );
                return;
            }

            m_parent->m_progress->setProgress( 0 );
            m_parent->m_progress->setTotalSteps( numFiles );
            m_parent->m_progress->show();

        }
        // don't return if numFiles==0: playlist items might be to delete

        lockDevice(true);
        if(fi==NULL)
            fi = (MediaItem*)m_parent->m_deviceList->firstChild();
    }

    while ( fi )
    {
        MediaItem *next = (MediaItem*)fi->nextSibling();

        if ( fi->isSelected() )
        {
            deleteItemFromDevice(fi, onlyPlayed);
        }
        else
        {
            if ( fi->childCount() )
                deleteFromDevice( (MediaItem*)fi->firstChild(), onlyPlayed, true );
        }

        fi = next;
    }

    if(!recursing)
    {
        synchronizeDevice();
        unlockDevice();

        QTimer::singleShot( 1500, m_parent->m_progress, SLOT(hide()) );
        m_parent->m_stats->setText( i18n( "1 track in queue", "%n tracks in queue", m_transferList->childCount() ) );
    }
}

void
MediaDevice::saveTransferList( const QString &path )
{
    QFile file( path );

    if( !file.open( IO_WriteOnly ) ) return;

    QDomDocument newdoc;
    QDomElement transferlist = newdoc.createElement( "playlist" );
    transferlist.setAttribute( "product", "amaroK" );
    transferlist.setAttribute( "version", APP_VERSION );
    newdoc.appendChild( transferlist );

    for( const MediaItem *item = static_cast<MediaItem *>( m_transferList->firstChild() );
            item;
            item = static_cast<MediaItem *>( item->nextSibling() ) )
    {
        QDomElement i = newdoc.createElement("item");
        i.setAttribute("url", item->url().url());

        if(item->bundle())
        {
            QDomElement attr = newdoc.createElement( "Title" );
            QDomText t = newdoc.createTextNode( item->bundle()->title() );
            attr.appendChild( t );
            i.appendChild( attr );

            attr = newdoc.createElement( "Artist" );
            t = newdoc.createTextNode( item->bundle()->artist() );
            attr.appendChild( t );
            i.appendChild( attr );

            attr = newdoc.createElement( "Album" );
            t = newdoc.createTextNode( item->bundle()->album() );
            attr.appendChild( t );
            i.appendChild( attr );

            attr = newdoc.createElement( "Year" );
            t = newdoc.createTextNode( QString::number( item->bundle()->year() ) );
            attr.appendChild( t );
            i.appendChild( attr );

            attr = newdoc.createElement( "Comment" );
            t = newdoc.createTextNode( item->bundle()->comment() );
            attr.appendChild( t );
            i.appendChild( attr );

            attr = newdoc.createElement( "Genre" );
            t = newdoc.createTextNode( item->bundle()->genre() );
            attr.appendChild( t );
            i.appendChild( attr );

            attr = newdoc.createElement( "Track" );
            t = newdoc.createTextNode( QString::number( item->bundle()->track() ) );
            attr.appendChild( t );
            i.appendChild( attr );
        }

        if(item->type() == MediaItem::PODCASTITEM)
        {
            i.setAttribute( "podcast", "1" );
        }

        transferlist.appendChild( i );
    }

    QTextStream stream( &file );
    stream.setEncoding( QTextStream::UnicodeUTF8 );
    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    stream << newdoc.toString();
}


void
MediaDevice::loadTransferList( const QString& filename )
{
    QFile file( filename );
    if( !file.open( IO_ReadOnly ) ) {
        debug() << "failed to restore media device transfer list" << endl;
        return;
    }

    clearItems();

    QTextStream stream( &file );
    stream.setEncoding( QTextStream::UnicodeUTF8 );

    QDomDocument d;
    QString er;
    int l, c;
    if( !d.setContent( stream.read(), &er, &l, &c ) ) { // return error values
        amaroK::StatusBar::instance()->longMessageThreadSafe( i18n(
                //TODO add a link to the path to the playlist
                "The XML in the transferlist was invalid. Please report this as a bug to the amaroK "
                "developers. Thank you." ), KDE::StatusBar::Error );
        error() << "[TRANSFERLISTLOADER]: Error loading xml file: " << filename << "(" << er << ")"
                << " at line " << l << ", column " << c << endl;
        return;
    }

    QValueList<QDomNode> nodes;
    const QString ITEM( "item" ); //so we don't construct this QString all the time
    for( QDomNode n = d.namedItem( "playlist" ).firstChild(); !n.isNull(); n = n.nextSibling() )
    {
        if( n.nodeName() != ITEM ) continue;

        QDomElement elem = n.toElement();
        if( !elem.isNull() )
            nodes += n;

        if( !elem.hasAttribute( "url" ) )
        {
            continue;
        }
        KURL url(elem.attribute("url"));

        bool isPodcast = false;
        if(elem.hasAttribute( "podcast" ))
        {
            isPodcast = true;
        }

        MetaBundle *bundle = new MetaBundle( url );
        for(QDomNode node = elem.firstChild();
                !node.isNull();
                node = node.nextSibling())
        {
            if(node.firstChild().isNull())
                continue;

            if(node.nodeName() == "Title" )
                bundle->setTitle(node.firstChild().toText().nodeValue());
            else if(node.nodeName() == "Artist" )
                bundle->setArtist(node.firstChild().toText().nodeValue());
            else if(node.nodeName() == "Album" )
                bundle->setAlbum(node.firstChild().toText().nodeValue());
            else if(node.nodeName() == "Year" )
                bundle->setYear(node.firstChild().toText().nodeValue().toUInt());
            else if(node.nodeName() == "Genre" )
                bundle->setGenre(node.firstChild().toText().nodeValue());
            else if(node.nodeName() == "Comment" )
                bundle->setComment(node.firstChild().toText().nodeValue());
        }

        addURL( url, bundle, isPodcast );
    }
}


MediaDeviceTransferList::MediaDeviceTransferList(MediaDeviceView *parent)
    : KListView( parent ), m_parent( parent )
{
    setFixedHeight( 200 );
    setSelectionMode( QListView::Extended );
    setItemsMovable( true );
    setDragEnabled( true );
    setShowSortIndicator( false );
    setSorting( -1 );
    setFullWidth( true );
    setRootIsDecorated( false );
    setDropVisualizer( true );     //the visualizer (a line marker) is drawn when dragging over tracks
    setDropHighlighter( true );    //and the highligther (a focus rect) is drawn when dragging over playlists
    setDropVisualizerWidth( 3 );
    setAcceptDrops( true );
    addColumn( i18n( "Track" ) );
}

void
MediaDeviceTransferList::dragEnterEvent( QDragEnterEvent *e )
{
    KListView::dragEnterEvent( e );

    debug() << "Items dropping to list?" << endl;
    e->accept( e->source() != viewport()
            && e->source() != m_parent
            && e->source() != m_parent->m_deviceList
            && e->source() != m_parent->m_deviceList->viewport()
            && KURLDrag::canDecode( e ) );
}


void
MediaDeviceTransferList::dropEvent( QDropEvent *e )
{
    KListView::dropEvent( e );

    KURL::List list;
    if ( KURLDrag::decode( e, list ) )
    {
        KURL::List::Iterator it = list.begin();
        for ( ; it != list.end(); ++it )
        {
            MediaDevice::instance()->addURL( *it );
        }
    }
}

void
MediaDeviceTransferList::contentsDragEnterEvent( QDragEnterEvent *e )
{
    KListView::contentsDragEnterEvent( e );

    debug() << "Items dropping to list?" << endl;
    e->accept( e->source() != viewport()
            && e->source() != m_parent
            && e->source() != m_parent->m_deviceList
            && e->source() != m_parent->m_deviceList->viewport()
            && KURLDrag::canDecode( e ) );
}


void
MediaDeviceTransferList::contentsDropEvent( QDropEvent *e )
{
    KListView::contentsDropEvent( e );

    KURL::List list;
    if ( KURLDrag::decode( e, list ) )
    {
        KURL::List::Iterator it = list.begin();
        for ( ; it != list.end(); ++it )
        {
            MediaDevice::instance()->addURL( *it );
        }
    }
}

void
MediaDeviceTransferList::contentsDragMoveEvent( QDragMoveEvent *e )
{
    KListView::contentsDragMoveEvent( e );

    //    const QPoint p = contentsToViewport( e->pos() );
    //    QListViewItem *item = itemAt( p );
    e->accept( e->source() != viewport()
            && e->source() != m_parent
            && e->source() != m_parent->m_deviceList
            && e->source() != m_parent->m_deviceList->viewport()
            && KURLDrag::canDecode( e ) );

#if 0
    const QPoint p = contentsToViewport( e->pos() );
    MediaItem *item = dynamic_cast<MediaItem *>( itemAt( p ) );
    if( item )
        if(p.y() - itemRect( item ).top() < (item->height()/2))
            item = dynamic_cast<MediaItem *>(item->itemAbove());
#endif
}

MediaItem*
MediaDeviceTransferList::findPath( QString path )
{
    for( QListViewItem *item = firstChild();
            item;
            item = item->nextSibling())
    {
        if(static_cast<MediaItem *>(item)->url().path() == path)
            return static_cast<MediaItem *>(item);
    }

    return NULL;
}



#include "mediabrowser.moc"
