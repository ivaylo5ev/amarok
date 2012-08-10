/****************************************************************************************
 * Copyright (c) 2012 Matěj Laitl <matej@laitl.cz>                                      *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "ChooseProvidersPage.h"

#include "App.h"
#include "core/meta/support/MetaConstants.h"
#include "statsyncing/models/ProvidersModel.h"

#include <KPushButton>

#include <QCheckBox>

using namespace StatSyncing;

ChooseProvidersPage::ChooseProvidersPage( QWidget *parent, Qt::WindowFlags f )
    : QWidget( parent, f )
    , m_providersModel( 0 )
{
    setupUi( this );
    KGuiItem configure = KStandardGuiItem::configure();
    configure.setText( i18n( "Configure Automatic Synchronization..." ) );
    buttonBox->addButton( configure, QDialogButtonBox::ActionRole, this, SLOT(openConfiguration()) );
    buttonBox->addButton( KGuiItem( i18n( "Next" ), "go-next" ), QDialogButtonBox::AcceptRole );
    connect( buttonBox, SIGNAL(accepted()), SIGNAL(accepted()) );
    connect( buttonBox, SIGNAL(rejected()), SIGNAL(rejected()) );
    progressBar->hide();
}

ChooseProvidersPage::~ChooseProvidersPage()
{
}

void
ChooseProvidersPage::setFields( const QList<qint64> &fields, qint64 checkedFields )
{
    QLayout *fieldsLayout = fieldsBox->layout();
    foreach( qint64 field, fields )
    {
        QString name = Meta::i18nForField( field );
        QCheckBox *checkBox = new QCheckBox( name );
        fieldsLayout->addWidget( checkBox );
        checkBox->setCheckState( ( field & checkedFields ) ? Qt::Checked : Qt::Unchecked );
        checkBox->setProperty( "field", field );
        connect( checkBox, SIGNAL(stateChanged(int)), SIGNAL(checkedFieldsChanged()) );
    }
    fieldsLayout->addItem( new QSpacerItem( 0, 0, QSizePolicy::Expanding ) );

    connect( this, SIGNAL(checkedFieldsChanged()), SLOT(updateEnabledFields()) );
    updateEnabledFields();
}

qint64
ChooseProvidersPage::checkedFields() const
{
    qint64 ret = 0;
    QLayout *fieldsLayout = fieldsBox->layout();
    for( int i = 0; i < fieldsLayout->count(); i++ )
    {
        QCheckBox *checkBox = qobject_cast<QCheckBox *>( fieldsLayout->itemAt( i )->widget() );
        if( !checkBox )
            continue;
        if( checkBox->isChecked() && checkBox->property( "field" ).canConvert<qint64>() )
            ret |= checkBox->property( "field" ).value<qint64>();
    }
    return ret;
}

void
ChooseProvidersPage::setProvidersModel( ProvidersModel *model, QItemSelectionModel *selectionModel )
{
    m_providersModel = model;
    providersView->setModel( model );
    providersView->setSelectionModel( selectionModel );

    connect( model, SIGNAL(selectedProvidersChanged()), SLOT(updateMatchedLabel()) );
    connect( model, SIGNAL(selectedProvidersChanged()), SLOT(updateEnabledFields()) );
    updateMatchedLabel();
    updateEnabledFields();
}

void
ChooseProvidersPage::disableControls()
{
    // disable checkboxes
    QLayout *fieldsLayout = fieldsBox->layout();
    for( int i = 0; i < fieldsLayout->count(); i++ )
    {
        QWidget *widget = fieldsLayout->itemAt( i )->widget();
        if( widget )
            widget->setEnabled( false );
    }

    // disable view
    providersView->setEnabled( false );

    // disable all but Cancel button
    foreach( QAbstractButton *button, buttonBox->buttons() )
    {
        if( buttonBox->buttonRole( button ) != QDialogButtonBox::RejectRole )
            button->setEnabled( false );
    }
}

void
ChooseProvidersPage::setProgressBarText( const QString &text )
{
    progressBar->show();
    progressBar->setFormat( text );
}

void
ChooseProvidersPage::setProgressBarMaximum( int maximum )
{
    progressBar->show();
    progressBar->setMaximum( maximum );
}

void
ChooseProvidersPage::progressBarIncrementProgress()
{
    progressBar->show();
    progressBar->setValue( progressBar->value() + 1 );
}

void
ChooseProvidersPage::updateMatchedLabel()
{
    qint64 fields = m_providersModel->reliableTrackMetadataIntersection();
    QString fieldNames = m_providersModel->fieldsToString( fields );
    matchLabel->setText( i18n( "Tracks matched by: %1", fieldNames ) );
}

void
ChooseProvidersPage::updateEnabledFields()
{
    if( !m_providersModel )
        return;

    qint64 writableFields = m_providersModel->writableTrackStatsDataUnion();
    QLayout *fieldsLayout = fieldsBox->layout();
    for( int i = 0; i < fieldsLayout->count(); i++ )
    {
        QWidget *checkBox = fieldsLayout->itemAt( i )->widget();
        if( !checkBox || !checkBox->property( "field" ).canConvert<qint64>() )
            continue;
        qint64 field = checkBox->property( "field" ).value<qint64>();
        bool enabled = writableFields & field;
        checkBox->setEnabled( enabled );
        QString text = i18nc( "%1 is field name such as Rating", "No selected collection "
                "supports writing %1 - it doesn't make sense to synchronize it.",
                Meta::i18nForField( field ) );
        checkBox->setToolTip( enabled ? QString() : text );
    }

    QAbstractButton *nextButton = 0;
    foreach( QAbstractButton *button, buttonBox->buttons() )
    {
        if( buttonBox->buttonRole( button ) == QDialogButtonBox::AcceptRole )
            nextButton = button;
    }
    if( nextButton )
        nextButton->setEnabled( writableFields != 0 );
}

void ChooseProvidersPage::openConfiguration()
{
    App *app = App::instance();
    if( app )
        app->slotConfigAmarok( "MetadataConfig" );
}
