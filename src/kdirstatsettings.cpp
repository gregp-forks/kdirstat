/*
 *   File name: kdirstatsettings.cpp
 *   Summary:	Settings dialog for KDirStat
 *   License:	GPL - See file COPYING for details.
 *   Author:	Stefan Hundhammer <sh@suse.de>
 *
 *   Updated:	2008-11-27
 */


#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <q3buttongroup.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qslider.h>
#include <q3vbox.h>
#include <q3hgroupbox.h>
#include <q3vgroupbox.h>
#include <qspinbox.h>
#include <q3listview.h>
#include <qinputdialog.h>
#include <qpushbutton.h>
#include <q3popupmenu.h>
#include <qmessagebox.h>
#include <Q3VGroupBox>
#include <Q3HGroupBox>

#include <kcolorbutton.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kapplication.h>
#include <ktoolinvocation.h>
#include <kvbox.h>

#include "kdirtreeview.h"
#include "ktreemapview.h"
#include "kdirstatsettings.h"
#include "kexcluderules.h"


using namespace KDirStat;


KSettingsDialog::KSettingsDialog( k4dirstat *mainWin )
    : KPageDialog(mainWin)
    , _mainWin( mainWin )
{
    /**
     * This may seem like overkill, but I didn't find any other way to get
     * geometry management right with KDialogBase, yet maintain a modular and
     * object-oriented design:
     *
     * Each individual settings page is added with 'addVBoxPage()' to get some
     * initial geometry management. Only then can some generic widget be added
     * into this - and I WANT my settings pages to be generic widgets. I want
     * them to be self-sufficient - no monolithic mess of widget creation in my
     * code, intermixed with all kinds of layout objects.
     *
     * The ordinary KDialogBase::addPage() just creates a QFrame which is too
     * dumb for any kind of geometry management - it cannot even handle one
     * single child right. So, let's have KDialogBase create something more
     * intelligent: A QVBox (which is derived from QFrame anyway). This QVBox
     * gets only one child - the KSettingsPage. This KSettingsPage handles its
     * own layout.
     **/

    setFaceType(Tabbed);
    setModal(false);
    setCaption(i18n( "Settings"));
    setButtons(KDialog::Ok | KDialog::Apply |
               KDialog::Default | KDialog::Cancel | KDialog::Help);

    KVBox * page = new KVBox();
    KPageWidgetItem *item;

    item = addPage(page, i18n( "&Cleanups" ) );
    _cleanupsPageIndex = item;
    new KCleanupPage( this, page, _mainWin );

    page = new KVBox();
    _treeColorsPageIndex = addPage(page, i18n( "&Tree Colors" ));
    new KTreeColorsPage( this, page, _mainWin );

    page = new KVBox();
    _treemapPageIndex = addPage(page, i18n( "Tree&map" ) );
    new KTreemapPage( this, page, _mainWin );

    page = new KVBox();
    _generalSettingsPageIndex = addPage(page, i18n( "&General" ) );
    new KGeneralSettingsPage( this, page, _mainWin );

    // resize( sizeHint() );
}


KSettingsDialog::~KSettingsDialog()
{
    // NOP
}


void
KSettingsDialog::show()
{
    emit aboutToShow();
    KPageDialog::show();
}


void
KSettingsDialog::slotDefault()
{
    if ( KMessageBox::warningContinueCancel( this,
					     i18n( "Really revert all settings to their default values?\n"
						   "You will lose all changes you ever made!" ),
					     i18n( "Please Confirm" ),			// caption
                                             KGuiItem(i18n( "&Really Revert to Defaults" ))	// continueButton
					     ) == KMessageBox::Continue )
    {
	emit defaultClicked();
	emit applyClicked();
    }
}


void
KSettingsDialog::slotHelp()
{
    QString helpTopic = "";

    if	    ( currentPage() == _cleanupsPageIndex	)	helpTopic = "configuring_cleanups";
    else if ( currentPage() == _treeColorsPageIndex )	helpTopic = "tree_colors";
    else if ( currentPage() == _treemapPageIndex	)	helpTopic = "treemap_settings";
    else if ( currentPage() == _generalSettingsPageIndex)	helpTopic = "general_settings";

    // kdDebug() << "Help topic: " << helpTopic << endl;
    KToolInvocation::invokeHelp( helpTopic );
}


/*--------------------------------------------------------------------------*/


KSettingsPage::KSettingsPage( KSettingsDialog * dialog,
			      QWidget *		parent )
    : QWidget( parent )
{
    connect( dialog,	SIGNAL( aboutToShow	( void ) ),
	     this,	SLOT  ( setup		( void ) ) );

    connect( dialog,	SIGNAL( okClicked	( void ) ),
	     this,	SLOT  ( apply		( void ) ) );

    connect( dialog,	SIGNAL( applyClicked	( void ) ),
	     this,	SLOT  ( apply		( void ) ) );

    connect( dialog,	SIGNAL( defaultClicked	( void ) ),
	     this,	SLOT  ( revertToDefaults( void ) ) );
}


KSettingsPage::~KSettingsPage()
{
    // NOP
}


/*--------------------------------------------------------------------------*/


KTreeColorsPage::KTreeColorsPage( KSettingsDialog *	dialog,
				  QWidget *		parent,
                                  k4dirstat *		mainWin )
    : KSettingsPage( dialog, parent )
    , _mainWin( mainWin )
    , _treeView( mainWin->treeView() )
    , _maxButtons( KDirStatSettingsMaxColorButton )
{
    // Outer layout box

    QHBoxLayout * outerBox = new QHBoxLayout( this,
					      0,	// border
					      dialog->spacingHint() );


    // Inner layout box with a column of color buttons

    QGridLayout *grid = new QGridLayout( _maxButtons,		// rows
					 _maxButtons + 1,	// cols
					 dialog->spacingHint() );
    outerBox->addLayout( grid, 1 );
    grid->setColStretch( 0, 0 );	// label column - dont' stretch

    for ( int i=1; i < _maxButtons; i++ )
    {
	grid->setColStretch( i, 1 );	// all other columns stretch as you like
    }

    for ( int i=0; i < _maxButtons; i++ )
    {
	QString labelText;

	labelText=i18n( "Tree Level %1" ).arg(i+1);
	_colorLabel[i] = new QLabel( labelText, this );
	grid->addWidget( _colorLabel [i], i, 0 );

	_colorButton[i] = new KColorButton( this );
	_colorButton[i]->setMinimumSize( QSize( 80, 10 ) );
	grid->addMultiCellWidget( _colorButton [i], i, i, i+1, _maxButtons );
	grid->setRowStretch( i, 1 );
    }


    // Vertical slider

    _slider = new QSlider( 1,			// minValue
			   _maxButtons,		// maxValue
			   1,			// pageStep
			   1,			// value
                           Qt::Vertical,
			   this );
    outerBox->addWidget( _slider, 0 );
    outerBox->activate();

    connect( _slider,	SIGNAL( valueChanged( int ) ),
	     this,	SLOT  ( enableColors( int ) ) );
}


KTreeColorsPage::~KTreeColorsPage()
{
    // NOP
}


void
KTreeColorsPage::apply()
{
    _treeView->setUsedFillColors( _slider->value() );

    for ( int i=0; i < _maxButtons; i++ )
    {
	_treeView->setFillColor( i, _colorButton [i]->color() );
    }

    _treeView->triggerUpdate();
}


void
KTreeColorsPage::revertToDefaults()
{
    _treeView->setDefaultFillColors();
    setup();
}


void
KTreeColorsPage::setup()
{
    for ( int i=0; i < _maxButtons; i++ )
    {
	_colorButton [i]->setColor( _treeView->rawFillColor(i) );
    }

    _slider->setValue( _treeView->usedFillColors() );
    enableColors( _treeView->usedFillColors() );
}


void
KTreeColorsPage::enableColors( int maxColors )
{
    for ( int i=0; i < _maxButtons; i++ )
    {
	_colorButton [i]->setEnabled( i < maxColors );
	_colorLabel  [i]->setEnabled( i < maxColors );
    }
}


/*--------------------------------------------------------------------------*/



KCleanupPage::KCleanupPage( KSettingsDialog *	dialog,
			    QWidget *		parent,
                            k4dirstat *	mainWin )
    : KSettingsPage( dialog, parent )
    , _mainWin( mainWin )
    , _currentCleanup( 0 )
{
    // Copy the main window's cleanup collection.

    _workCleanupCollection = *mainWin->cleanupCollection();

    // Create layout and widgets.

    QHBoxLayout * layout = new QHBoxLayout( this,
					    0,				// border
					    dialog->spacingHint() );	// spacing
    _listBox	= new KCleanupListBox( this );
    _props	= new KCleanupPropertiesPage( this, mainWin );


    // Connect list box signals to reflect changes in the list
    // selection in the cleanup properties page - whenever the user
    // clicks on a different cleanup in the list, the properties page
    // will display that cleanup's values.

    connect( _listBox, SIGNAL( selectCleanup( KCleanup * ) ),
	     this,     SLOT  ( changeCleanup( KCleanup * ) ) );


    // Fill list box so it can determine a reasonable startup geometry - that
    // doesn't work if it happens only later.

    setup();

    // Now that _listBox will (hopefully) have determined a reasonable
    // default geometry, add the widgets to the layout.

    layout->addWidget( _listBox, 0 );
    layout->addWidget( _props  , 1 );
    layout->activate();
}


KCleanupPage::~KCleanupPage()
{
    // NOP
}


void
KCleanupPage::changeCleanup( KCleanup * newCleanup )
{
    if ( _currentCleanup && newCleanup != _currentCleanup )
    {
	storeProps( _currentCleanup );
    }

    _currentCleanup = newCleanup;
    _props->setFields( _currentCleanup );
}


void
KCleanupPage::apply()
{
    exportCleanups();
}


void
KCleanupPage::revertToDefaults()
{
    _mainWin->revertCleanupsToDefaults();
    setup();
}


void
KCleanupPage::setup()
{
    importCleanups();

    // Fill the list box.

    _listBox->clear();
    KCleanupList cleanupList = _workCleanupCollection.cleanupList();
    KCleanupListIterator it( cleanupList );

    while ( *it )
    {
	_listBox->insert( *it );
	++it;
    }


    // (Re-) Initialize list box.

    // _listBox->resize( _listBox->sizeHint() );
    _listBox->setSelected( 0, true );
}


void
KCleanupPage::importCleanups()
{
    // Copy the main window's cleanup collecton to _workCleanupCollection.

    _workCleanupCollection = * _mainWin->cleanupCollection();


    // Pointers to the old collection contents are now invalid!

    _currentCleanup = 0;
}


void
KCleanupPage::exportCleanups()
{
    // Retrieve any pending changes from the properties page and store
    // them in the current cleanup.

    storeProps( _currentCleanup );


    // Copy the _workCleanupCollection to the main window's cleanup
    // collection.

    * _mainWin->cleanupCollection() = _workCleanupCollection;
}


void
KCleanupPage::storeProps( KCleanup * cleanup )
{
    if ( cleanup )
    {
	// Retrieve the current fields contents and store them in the current
	// cleanup.

	*cleanup = _props->fields();

	// Update the list box accordingly - the cleanup's title may have
	// changed, too!

	_listBox->updateTitle( cleanup );
    }
}

/*--------------------------------------------------------------------------*/


KCleanupListBox::KCleanupListBox( QWidget *parent )
   : Q3ListBox( parent )
{
    _selection = 0;

    connect( this,
             SIGNAL( selectionChanged( Q3ListBoxItem *) ),
             SLOT  ( selectCleanup   ( Q3ListBoxItem *) ) );
}


QSize
KCleanupListBox::sizeHint() const
{
    // FIXME: Is this still needed with Qt 2.x?

    if ( count() < 1 )
    {
	// As long as the list is empty, sizeHint() would default to
	// (0,0) which is ALWAYS just a pain in the ass. We'd rather
	// have an absolutely random value than this.
	return QSize( 100, 100 );
    }
    else
    {
	// Calculate the list contents and take 3D frames (2*2 pixels)
	// into account.
	return QSize ( maxItemWidth() + 5,
		       count() * itemHeight( 0 ) + 4 );
    }
}


void
KCleanupListBox::insert( KCleanup * cleanup )
{
    // Create a new listbox item - this will insert itself (!) automatically.
    // It took me half an afternoon to figure _this_ out. Not too intuitive
    // when there is an insertItem() method, too, eh?

    new KCleanupListBoxItem( this, cleanup );
}


void
KCleanupListBox::selectCleanup( Q3ListBoxItem * listBoxItem )
{
    KCleanupListBoxItem * item = (KCleanupListBoxItem *) listBoxItem;

    _selection = item->cleanup();
    emit selectCleanup( _selection );
}


void
KCleanupListBox::updateTitle( KCleanup * cleanup )
{
    KCleanupListBoxItem * item = (KCleanupListBoxItem *) firstItem();

    while ( item )
    {
	if ( ! cleanup || item->cleanup() == cleanup )
	    item->updateTitle();

	item = (KCleanupListBoxItem *) item->next();
    }
}


/*--------------------------------------------------------------------------*/


KCleanupListBoxItem::KCleanupListBoxItem( KCleanupListBox *	listBox,
					  KCleanup *		cleanup )
    : Q3ListBoxText( listBox )
    , _cleanup( cleanup )
{
    Q_CHECK_PTR( cleanup );
    setText( cleanup->cleanTitle() );
}


void
KCleanupListBoxItem::updateTitle()
{
    setText( _cleanup->cleanTitle() );
}


/*--------------------------------------------------------------------------*/


KCleanupPropertiesPage::KCleanupPropertiesPage( QWidget *	parent,
                                                k4dirstat *	mainWin )
   : QWidget( parent )
   , _mainWin( mainWin )
{
    QVBoxLayout *outerBox = new QVBoxLayout( this, 0, 0 );	// border, spacing

    // The topmost check box: "Enabled".

    _enabled = new QCheckBox( i18n( "&Enabled" ), this );
    outerBox->addWidget( _enabled, 0 );
    outerBox->addSpacing( 7 );
    outerBox->addStretch();

    connect( _enabled,	SIGNAL( toggled	    ( bool ) ),
	     this,	SLOT  ( enableFields( bool ) ) );


    // All other widgets of this page are grouped together in a
    // separate subwidget so they can all be enabled / disabled
    // together.
    _fields  = new QWidget( this );
    outerBox->addWidget( _fields, 1 );

    QVBoxLayout *fieldsBox = new QVBoxLayout( _fields );


    // Grid layout for the edit fields, their labels, some
    // explanatory text and the "recurse?" check box.

    QGridLayout *grid = new QGridLayout( 7,	// rows
					 2,	// cols
					 4 );	// spacing
    fieldsBox->addLayout( grid, 0 );
    fieldsBox->addStretch();
    fieldsBox->addSpacing( 5 );

    grid->setColStretch( 0, 0 ); // column for field labels - dont' stretch
    grid->setColStretch( 1, 1 ); // column for edit fields - stretch as you like


    // Edit fields for cleanup action title and command line.

    QLabel *label;
    _title	= new QLineEdit( _fields );					grid->addWidget( _title,   0, 1 );
    _command	= new QLineEdit( _fields );					grid->addWidget( _command, 1, 1 );
    label	= new QLabel( _title,	i18n( "&Title:"		), _fields );	grid->addWidget( label,	   0, 0 );
    label	= new QLabel( _command, i18n( "&Command Line:"	), _fields );	grid->addWidget( label,	   1, 0 );

    label = new QLabel( i18n( "%p Full Path" ), _fields );
    grid->addWidget( label, 2, 1 );

    label = new QLabel( i18n( "%n File / Directory Name Without Path" ), _fields );
    grid->addWidget( label, 3, 1 );

    label = new QLabel( i18n( "%t KDE Trash Directory" ), _fields );
    grid->addWidget( label, 4, 1 );


    // "Recurse into subdirs" check box

    _recurse = new QCheckBox( i18n( "&Recurse into Subdirectories" ), _fields );
    grid->addWidget( _recurse, 5, 1 );

    // "Ask for confirmation" check box

    _askForConfirmation = new QCheckBox( i18n( "&Ask for Confirmation" ), _fields );
    grid->addWidget( _askForConfirmation, 6, 1 );


    // The "Works for..." check boxes, grouped together in a button group.

    Q3ButtonGroup *worksFor = new Q3ButtonGroup( i18n( "Works for..." ), _fields );
    QVBoxLayout *worksForBox = new QVBoxLayout( worksFor, 15, 2 );
    fieldsBox->addWidget( worksFor, 0 );
    fieldsBox->addSpacing( 5 );
    fieldsBox->addStretch();

    _worksForDir	= new QCheckBox( i18n( "&Directories"		), worksFor );
    _worksForFile	= new QCheckBox( i18n( "&Files"			), worksFor );
    _worksForDotEntry	= new QCheckBox( i18n( "<Files> P&seudo Entries"), worksFor );

    worksForBox->addWidget( _worksForDir	, 1 );
    worksForBox->addWidget( _worksForFile	, 1 );
    worksForBox->addWidget( _worksForDotEntry	, 1 );

    worksForBox->addSpacing( 5 );
    _worksForProtocols = new QComboBox( false, worksFor );
    worksForBox->addWidget( _worksForProtocols, 1 );

    _worksForProtocols->insertItem( i18n( "On Local Machine Only ('file:/' Protocol)" ) );
    _worksForProtocols->insertItem( i18n( "Network Transparent (ftp, smb, tar, ...)" ) );


    // Grid layout for combo boxes at the bottom

    grid = new QGridLayout( 1,		// rows
			    2,		// cols
			    4 );	// spacing

    fieldsBox->addLayout( grid, 0 );
    fieldsBox->addSpacing( 5 );
    fieldsBox->addStretch();
    int row = 0;


    // The "Refresh policy" combo box

    _refreshPolicy = new QComboBox( false, _fields );
    grid->addWidget( _refreshPolicy, row, 1 );

    label = new QLabel( _refreshPolicy, i18n( "Refresh &Policy:" ), _fields );
    grid->addWidget( label, row++, 0 );


    // Caution: The order of those entries must match the order of
    // 'enum RefreshPolicy' in 'kcleanup.h'!
    //
    // I don't like this one bit. The ComboBox should provide something better
    // than mere numeric IDs. One of these days I'm going to rewrite this
    // thing!

    _refreshPolicy->insertItem( i18n( "No Refresh"			) );
    _refreshPolicy->insertItem( i18n( "Refresh This Entry"		) );
    _refreshPolicy->insertItem( i18n( "Refresh This Entry's Parent"	) );
    _refreshPolicy->insertItem( i18n( "Assume Entry Has Been Deleted"	) );


    outerBox->activate();
    setMinimumSize( sizeHint() );
}


void
KCleanupPropertiesPage::enableFields( bool active )
{
    _fields->setEnabled( active );
}


void
KCleanupPropertiesPage::setFields( const KCleanup * cleanup )
{
    _id = cleanup->id();
    _enabled->setChecked		( cleanup->enabled()		);
    _title->setText			( cleanup->title()		);
    _command->setText			( cleanup->command()		);
    _recurse->setChecked		( cleanup->recurse()		);
    _askForConfirmation->setChecked	( cleanup->askForConfirmation() );
    _worksForDir->setChecked		( cleanup->worksForDir()	);
    _worksForFile->setChecked		( cleanup->worksForFile()	);
    _worksForDotEntry->setChecked	( cleanup->worksForDotEntry()	);
    _worksForProtocols->setCurrentItem	( cleanup->worksLocalOnly() ? 0 : 1 );
    _refreshPolicy->setCurrentItem	( cleanup->refreshPolicy()	);

    enableFields( cleanup->enabled() );
}


KCleanup
KCleanupPropertiesPage::fields() const
{
    KCleanup cleanup( _id );

    cleanup.setEnabled			( _enabled->isChecked()		   );
    cleanup.setTitle			( _title->text()		   );
    cleanup.setCommand			( _command->text()		   );
    cleanup.setRecurse			( _recurse->isChecked()		   );
    cleanup.setAskForConfirmation	( _askForConfirmation->isChecked() );
    cleanup.setWorksForDir		( _worksForDir->isChecked()	   );
    cleanup.setWorksForFile		( _worksForFile->isChecked()	   );
    cleanup.setWorksLocalOnly		( _worksForProtocols->currentItem() == 0 ? true : false );
    cleanup.setWorksForDotEntry		( _worksForDotEntry->isChecked()   );
    cleanup.setRefreshPolicy		( (KCleanup::RefreshPolicy) _refreshPolicy->currentItem() );

    return cleanup;
}


/*--------------------------------------------------------------------------*/


KGeneralSettingsPage::KGeneralSettingsPage( KSettingsDialog *	dialog,
					    QWidget *		parent,
                                            k4dirstat *	mainWin )
    : KSettingsPage( dialog, parent )
    , _mainWin( mainWin )
    , _treeView( mainWin->treeView() )
{
    // Create layout and widgets.

    QVBoxLayout * layout	= new QVBoxLayout( this, 5,			// border
						   dialog->spacingHint() );	// spacing

    Q3VGroupBox * gbox		= new Q3VGroupBox( i18n( "Directory Reading" ), this );
    layout->addWidget( gbox );



    _crossFileSystems		= new QCheckBox( i18n( "Cross &File System Boundaries" ), gbox );
    _enableLocalDirReader	= new QCheckBox( i18n( "Use Optimized &Local Directory Read Methods" ), gbox );

    connect( _enableLocalDirReader,	SIGNAL( stateChanged( int ) ),
	     this,			SLOT  ( checkEnabledState() ) );

    layout->addSpacing( 10 );

    gbox			= new Q3VGroupBox( i18n( "Animation" ), this );
    layout->addWidget( gbox );

    _enableToolBarAnimation	= new QCheckBox( i18n( "P@cM@n Animation in Tool &Bar" ), gbox );
    _enableTreeViewAnimation	= new QCheckBox( i18n( "P@cM@n Animation in Directory &Tree" ), gbox );
    layout->addSpacing( 10 );
    
    Q3VGroupBox * excludeBox	= new Q3VGroupBox( i18n( "&Exclude Rules" ), this );
    layout->addWidget( excludeBox );
    
    _excludeRulesListView	= new Q3ListView( excludeBox );
    _excludeRulesListView->addColumn( i18n( "Exclude Rule (Regular Expression)" ), 300 );
    _excludeRuleContextMenu	= 0;

    QGroupBox * buttonBox	= new Q3HGroupBox( excludeBox );
    _addExcludeRuleButton	= new QPushButton( i18n( "&Add"    ), buttonBox );
    _editExcludeRuleButton	= new QPushButton( i18n( "&Edit"   ), buttonBox );
    _deleteExcludeRuleButton	= new QPushButton( i18n( "&Delete" ), buttonBox );
    
    connect( _excludeRulesListView,	SIGNAL( rightButtonClicked        ( Q3ListViewItem *, const QPoint &, int ) ),
             this,			SLOT  ( showExcludeRuleContextMenu( Q3ListViewItem *, const QPoint &, int ) ) );
    
    connect( _addExcludeRuleButton,	SIGNAL( clicked()        ),
	     this,			SLOT  ( addExcludeRule() ) );
    
    connect( _editExcludeRuleButton,	SIGNAL( clicked()         ),
	     this,			SLOT  ( editExcludeRule() ) );
    
    connect( _deleteExcludeRuleButton,	SIGNAL( clicked()           ),
	     this,			SLOT  ( deleteExcludeRule() ) );

    connect( _excludeRulesListView,	SIGNAL( doubleClicked( Q3ListViewItem *, const QPoint &, int ) ),
	     this, 			SLOT  ( editExcludeRule() ) );
}


KGeneralSettingsPage::~KGeneralSettingsPage()
{
    if ( _excludeRuleContextMenu )
	delete _excludeRuleContextMenu;
}


void
KGeneralSettingsPage::showExcludeRuleContextMenu( Q3ListViewItem *, const QPoint &pos, int )
{
    if ( ! _excludeRuleContextMenu )
    {
        _excludeRuleContextMenu = new Q3PopupMenu( 0 );
	_excludeRuleContextMenu->insertItem( i18n( "&Edit"   ), this, SLOT( editExcludeRule  () ) );
	_excludeRuleContextMenu->insertItem( i18n( "&Delete" ), this, SLOT( deleteExcludeRule() ) );
    }

    if ( _excludeRuleContextMenu && _excludeRulesListView->currentItem() )
    {
	_excludeRuleContextMenu->move( pos.x(), pos.y() );
	_excludeRuleContextMenu->show();
    }
}


void
KGeneralSettingsPage::apply()
{
    KConfigGroup config = KGlobal::config()->group( "Directory Reading" );

    config.writeEntry( "CrossFileSystems",	_crossFileSystems->isChecked()		);
    config.writeEntry( "EnableLocalDirReader", _enableLocalDirReader->isChecked()	);

    config = KGlobal::config()->group( "Animation" );
    //config.setGroup( "Animation" );
    config.writeEntry( "ToolbarPacMan",	_enableToolBarAnimation->isChecked()	);
    config.writeEntry( "DirTreePacMan",	_enableTreeViewAnimation->isChecked()	);

    _mainWin->initPacMan( _enableToolBarAnimation->isChecked() );
    _treeView->enablePacManAnimation( _enableTreeViewAnimation->isChecked() );

    config = KGlobal::config()->group( "Exclude" );
    //config.setGroup( "Exclude" );
    
    QStringList excludeRulesStringList;
    KExcludeRules::excludeRules()->clear();
    Q3ListViewItem * item = _excludeRulesListView->firstChild();
    
    while ( item )
    {
	QString ruleText = item->text(0);
	excludeRulesStringList.append( ruleText );
	// kdDebug() << "Adding exclude rule " << ruleText << endl;
	KExcludeRules::excludeRules()->add( new KExcludeRule( QRegExp( ruleText ) ) );
	item = item->nextSibling();
    }

    config.writeEntry( "ExcludeRules", excludeRulesStringList );
}


void
KGeneralSettingsPage::revertToDefaults()
{
    _crossFileSystems->setChecked( false );
    _enableLocalDirReader->setChecked( true );

    _enableToolBarAnimation->setChecked( true );
    _enableTreeViewAnimation->setChecked( false );
    
    _excludeRulesListView->clear();
    _editExcludeRuleButton->setEnabled( false );
    _deleteExcludeRuleButton->setEnabled( false );
}


void
KGeneralSettingsPage::setup()
{
    KConfigGroup config = KGlobal::config()->group("Directory Reading");

    _crossFileSystems->setChecked	( config.readEntry( "CrossFileSystems"	, false) );
    _enableLocalDirReader->setChecked	( config.readEntry( "EnableLocalDirReader" , true ) );

    _enableToolBarAnimation->setChecked ( _mainWin->pacManEnabled() );
    _enableTreeViewAnimation->setChecked( _treeView->doPacManAnimation() );

    _excludeRulesListView->clear();
    KExcludeRule * excludeRule = KExcludeRules::excludeRules()->first();

    while ( excludeRule )
    {
	// _excludeRulesListView->insertItem();
        new Q3ListViewItem( _excludeRulesListView, excludeRule->regexp().pattern() );
	excludeRule = KExcludeRules::excludeRules()->next();
    }
    
    checkEnabledState();
}


void
KGeneralSettingsPage::checkEnabledState()
{
    _crossFileSystems->setEnabled( _enableLocalDirReader->isChecked() );

    int excludeRulesCount = _excludeRulesListView->childCount();
    
    _editExcludeRuleButton->setEnabled  ( excludeRulesCount > 0 );
    _deleteExcludeRuleButton->setEnabled( excludeRulesCount > 0 );
}


void
KGeneralSettingsPage::addExcludeRule()
{
    bool ok;
    QString text = QInputDialog::getText( i18n( "New exclude rule" ),
					  i18n( "Regular expression for new exclude rule:" ),
					  QLineEdit::Normal,
					  QString::null,
					  &ok,
					  this );
    if ( ok && ! text.isEmpty() )
    {
        _excludeRulesListView->insertItem( new Q3ListViewItem ( _excludeRulesListView, text ) );
	
    }
    
    checkEnabledState();
}


void
KGeneralSettingsPage::editExcludeRule()
{
    Q3ListViewItem * item = _excludeRulesListView->currentItem();

    if ( item )
    {
	bool ok;
	QString text = QInputDialog::getText( i18n( "Edit exclude rule" ),
					      i18n( "Exclude rule (regular expression):" ),
					      QLineEdit::Normal,
					      item->text(0),
					      &ok,
					      this );
	if ( ok )
	{
	    if ( text.isEmpty() )
		_excludeRulesListView->removeItem( item );
	    else
		item->setText( 0, text );
	}
    }
    
    checkEnabledState();
}


void
KGeneralSettingsPage::deleteExcludeRule()
{
    Q3ListViewItem * item = _excludeRulesListView->currentItem();

    if ( item )
    {
	QString excludeRule  = item->text(0);
	int result = KMessageBox::questionYesNo( this,
						 i18n( "Really delete exclude rule \"%1\"?" ).arg( excludeRule ),
						 i18n( "Delete?" ) ); // Window title
	if ( result == KMessageBox::Yes )
	{
	    _excludeRulesListView->removeItem( item );
	}
    }

    checkEnabledState();
}


/*--------------------------------------------------------------------------*/


KTreemapPage::KTreemapPage( KSettingsDialog *	dialog,
					    QWidget *		parent,
                                            k4dirstat *	mainWin )
    : KSettingsPage( dialog, parent )
    , _mainWin( mainWin )
{
    // kdDebug() << k_funcinfo << endl;

    QVBoxLayout * layout = new QVBoxLayout( this, 0, 0 ); // parent, border, spacing

    Q3VBox * vbox	= new Q3VBox( this );
    vbox->setSpacing( dialog->spacingHint() );
    layout->addWidget( vbox );

    _squarify		= new QCheckBox( i18n( "S&quarify Treemap"	), vbox );
    _doCushionShading	= new QCheckBox( i18n( "Use C&ushion Shading"	), vbox );


    // Cushion parameters

    Q3VGroupBox * gbox	= new Q3VGroupBox( i18n( "Cushion Parameters" ), vbox );
    _cushionParams	= gbox;
    gbox->addSpace( 7 );
    gbox->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );

    QLabel * label	= new QLabel( i18n( "Ambient &Light" ), gbox );
    Q3HBox * hbox	= new Q3HBox( gbox );
    _ambientLight	= new QSlider ( MinAmbientLight, MaxAmbientLight, 10,	// min, max, pageStep
                                        DefaultAmbientLight, Qt::Horizontal, hbox );
    _ambientLightSB	= new QSpinBox( MinAmbientLight, MaxAmbientLight, 1,	// min, max, step
					hbox );
    _ambientLightSB->setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum ) );
    label->setBuddy( _ambientLightSB );

    gbox->addSpace( 7 );
    label		= new QLabel( i18n( "&Height Scale" ), gbox );
    hbox		= new Q3HBox( gbox );
    _heightScalePercent = new QSlider( MinHeightScalePercent, MaxHeightScalePercent, 10,   // min, max, pageStep
                                       DefaultHeightScalePercent, Qt::Horizontal, hbox );
    _heightScalePercentSB = new QSpinBox( MinHeightScalePercent, MaxHeightScalePercent, 1, // min, max, step
					  hbox );
    _heightScalePercentSB->setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum ) );
    label->setBuddy( _heightScalePercentSB );

    gbox->addSpace( 10 );
    _ensureContrast	= new QCheckBox( i18n( "Draw Lines if Lo&w Contrast"	), gbox );


    hbox		= new Q3HBox( gbox );
    _forceCushionGrid	= new QCheckBox( i18n( "Always Draw &Grid"		), hbox );
    addHStretch( hbox );

    _cushionGridColorL	= new QLabel( "	   " + i18n( "Gr&id Color: " ), hbox );
    _cushionGridColor	= new KColorButton( hbox );
    _cushionGridColorL->setBuddy( _cushionGridColor );
    _cushionGridColorL->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

    // addVStretch( vbox );


    // Plain treemaps parameters

    _plainTileParams	= new Q3HGroupBox( i18n( "Colors for Plain Treemaps" ), vbox );

    _plainTileParams->addSpace( 7 );
    label		= new QLabel( i18n( "&Files: " ), _plainTileParams );
    _fileFillColor	= new KColorButton( _plainTileParams );
    label->setBuddy( _fileFillColor );
    label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

    label		= new QLabel( "	   " + i18n( "&Directories: " ), _plainTileParams );
    _dirFillColor	= new KColorButton( _plainTileParams );
    label->setBuddy( _dirFillColor );
    label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

    label		= new QLabel( i18n( "Gr&id: " ), _plainTileParams );
    _outlineColor	= new KColorButton( _plainTileParams );
    label->setBuddy( _outlineColor );
    label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );


    // Misc

    QWidget * gridBox	= new QWidget( vbox );
    QGridLayout * grid	= new QGridLayout( gridBox, 2, 3, dialog->spacingHint() ); // rows, cols, spacing
    grid->setColStretch( 0, 0 ); // (col, stretch) don't stretch this column
    grid->setColStretch( 1, 0 ); // don't stretch
    grid->setColStretch( 2, 1 ); // stretch this as you like

    label		= new QLabel( i18n( "Hi&ghlight R&ectangle: " ), gridBox );
    _highlightColor	= new KColorButton( gridBox );
    label->setBuddy( _highlightColor );

    grid->addWidget( label,		0, 0 );
    grid->addWidget( _highlightColor,	0, 1 );


    label		= new QLabel( i18n( "Minim&um Treemap Tile Size: " ), gridBox );
    _minTileSize	= new QSpinBox( 0, 30, 1, gridBox ); // min, max, step, parent
    label->setBuddy( _minTileSize );

    grid->addWidget( label,		1, 0 );
    grid->addWidget( _minTileSize,	1, 1 );

    _autoResize		= new QCheckBox( i18n( "Auto-&Resize Treemap" ), vbox );



    // Connections

    connect( _ambientLight,		SIGNAL( valueChanged(int) ),
	     _ambientLightSB,		SLOT  ( setValue    (int) ) );

    connect( _ambientLightSB,		SIGNAL( valueChanged(int) ),
	     _ambientLight,		SLOT  ( setValue    (int) ) );


    connect( _heightScalePercent,	SIGNAL( valueChanged(int) ),
	     _heightScalePercentSB,	SLOT  ( setValue    (int) ) );

    connect( _heightScalePercentSB,	SIGNAL( valueChanged(int) ),
	     _heightScalePercent,	SLOT  ( setValue    (int) ) );


    connect( _doCushionShading, SIGNAL( stateChanged( int ) ), this, SLOT( checkEnabledState() ) );
    connect( _forceCushionGrid, SIGNAL( stateChanged( int ) ), this, SLOT( checkEnabledState() ) );

    checkEnabledState();
}


KTreemapPage::~KTreemapPage()
{
    // NOP
}


void
KTreemapPage::apply()
{
    KConfigGroup config = KGlobal::config()->group( "Treemaps" );

    config.writeEntry( "Squarify",		_squarify->isChecked()			);
    config.writeEntry( "CushionShading",	_doCushionShading->isChecked()		);
    config.writeEntry( "AmbientLight",		_ambientLight->value()			);
    config.writeEntry( "HeightScaleFactor",	_heightScalePercent->value() / 100.0	);
    config.writeEntry( "EnsureContrast",	_ensureContrast->isChecked()		);
    config.writeEntry( "ForceCushionGrid",	_forceCushionGrid->isChecked()		);
    config.writeEntry( "MinTileSize",		_minTileSize->value()			);
    config.writeEntry( "AutoResize",		_autoResize->isChecked()		);
    config.writeEntry( "CushionGridColor",	_cushionGridColor->color()		);
    config.writeEntry( "OutlineColor",		_outlineColor->color()			);
    config.writeEntry( "FileFillColor",	_fileFillColor->color()			);
    config.writeEntry( "DirFillColor",		_dirFillColor->color()			);
    config.writeEntry( "HighlightColor",	_highlightColor->color()		);

    if ( treemapView() )
    {
	treemapView()->readConfig();
	treemapView()->rebuildTreemap();
    }
}


void
KTreemapPage::revertToDefaults()
{
    _squarify->setChecked( true );
    _doCushionShading->setChecked( true );

    _ambientLight->setValue( DefaultAmbientLight );
    _heightScalePercent->setValue( DefaultHeightScalePercent );
    _ensureContrast->setChecked( true );
    _forceCushionGrid->setChecked( false );
    _minTileSize->setValue( DefaultMinTileSize );
    _autoResize->setChecked( true );

    _cushionGridColor->setColor ( QColor( 0x80, 0x80, 0x80 ) );
    _outlineColor->setColor	( QColor(Qt::black)          );
    _fileFillColor->setColor	( QColor( 0xde, 0x8d, 0x53 ) );
    _dirFillColor->setColor	( QColor( 0x10, 0x7d, 0xb4 ) );
    _highlightColor->setColor	( QColor(Qt::red)	     );
}


void
KTreemapPage::setup()
{
    KConfigGroup config = KGlobal::config()->group("Treemaps");

    _squarify->setChecked		( config.readEntry( "Squarify"		, true	) );
    _doCushionShading->setChecked	( config.readEntry( "CushionShading"	, true	) );

    _ambientLight->setValue		( config.readEntry( "AmbientLight"		  , DefaultAmbientLight       ) );
    _heightScalePercent->setValue( (int) ( 100 *  config.readEntry ( "HeightScaleFactor", DefaultHeightScaleFactor  ) ) );
    _ensureContrast->setChecked		( config.readEntry( "EnsureContrast"	, true	) );
    _forceCushionGrid->setChecked	( config.readEntry( "ForceCushionGrid"	, false ) );
    _minTileSize->setValue		( config.readEntry ( "MinTileSize"		, DefaultMinTileSize ) );
    _autoResize->setChecked		( config.readEntry( "AutoResize"		, true	) );

    _cushionGridColor->setColor ( readColorEntry( config, "CushionGridColor"	, QColor( 0x80, 0x80, 0x80 ) ) );
    _outlineColor->setColor	( readColorEntry( config, "OutlineColor"	, QColor(Qt::black)	     ) );
    _fileFillColor->setColor	( readColorEntry( config, "FileFillColor"	, QColor( 0xde, 0x8d, 0x53 ) ) );
    _dirFillColor->setColor	( readColorEntry( config, "DirFillColor"	, QColor( 0x10, 0x7d, 0xb4 ) ) );
    _highlightColor->setColor	( readColorEntry( config, "HighlightColor"	, QColor(Qt::red)	     ) );

    _ambientLightSB->setValue( _ambientLight->value() );
    _heightScalePercentSB->setValue( _heightScalePercent->value() );

    checkEnabledState();
}


void
KTreemapPage::checkEnabledState()
{
    _cushionParams->setEnabled( _doCushionShading->isChecked() );
    _plainTileParams->setEnabled( ! _doCushionShading->isChecked() );

    if ( _doCushionShading->isChecked() )
    {
	_cushionGridColor->setEnabled ( _forceCushionGrid->isChecked() );
	_cushionGridColorL->setEnabled( _forceCushionGrid->isChecked() );
	_ensureContrast->setEnabled   ( ! _forceCushionGrid->isChecked() );
    }
}


QColor
KTreemapPage::readColorEntry( KConfigGroup config, const char * entryName, QColor defaultColor )
{
    return config.readEntry( entryName, defaultColor );
}



void
addHStretch( QWidget * parent )
{
    QWidget * stretch = new QWidget( parent );
    stretch->setSizePolicy( QSizePolicy( QSizePolicy::Expanding,	// hor
					 QSizePolicy::Minimum,		// vert
					 1,				// hstretch
					 0 ) );				// vstretch
}


void
addVStretch( QWidget * parent )
{
    QWidget * stretch = new QWidget( parent );
    stretch->setSizePolicy( QSizePolicy( QSizePolicy::Minimum,		// hor
					 QSizePolicy::Expanding,	// vert
					 0,				// hstretch
					 1 ) );				// vstretch
}


// EOF
