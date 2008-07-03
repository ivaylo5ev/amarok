/* 
   Copyright (C) 2008 Daniel Winter <dw@danielwinter.de>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include "NepomukArtist.h"

#include "Meta.h"

#include <QString>


using namespace Meta;

NepomukArtist::NepomukArtist( const QString &name )
        : Artist()
        , m_name( name )
{
}

QString
NepomukArtist::name() const
{
    return m_name;
}

QString
NepomukArtist::prettyName() const
{
    return m_name;
}

QString
NepomukArtist::sortableName() const
{
    if ( m_sortName.isEmpty() && !m_name.isEmpty() )
    {
        if ( m_name.startsWith( "the ", Qt::CaseInsensitive ) )
        {
            QString begin = m_name.left( 3 );
            m_sortName = QString( "%1, %2" ).arg( m_name, begin );
            m_sortName = m_sortName.mid( 4 );
        }
        else
        {
            m_sortName = m_name;
        }
    }
    return m_sortName;
}

TrackList
NepomukArtist::tracks()
{
    return TrackList();
}

AlbumList
NepomukArtist::albums()
{
    return AlbumList();
}
