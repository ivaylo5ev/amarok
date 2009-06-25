/****************************************************************************************
 * Copyright (c) 2008 Ian Monroe <ian@monroe.nu>                                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Pulic License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "AmarokInfoScript.h"

#include "Amarok.h"

#include <KIconLoader>

InfoScript::InfoScript( const KUrl& scriptUrl )
    : QObject( 0 )
    , m_scriptUrl( scriptUrl )
{ }

QString
InfoScript::version() const
{
    return APP_VERSION;
}

QString
InfoScript::scriptPath() const
{
    return m_scriptUrl.directory();
}

QString
InfoScript::iconPath( const QString& name, int size ) const
{
    //if size was positive it would refer to KIconLoader::Group
    return KIconLoader::global()->iconPath( name, -size ); 
}

QString InfoScript::scriptConfigPath( const QString& name ) const
{
    return Amarok::saveLocation( "scripts/" + name );
}

#include "AmarokInfoScript.moc"
