#import "GSXBoloController.h"
#import "GSBoloView.h"
#import "GSRobot.h"
#import "GSStatusBar.h"
#import "GSBuilderStatusView.h"
#import "GSKeyCodeField.h"

#include "bolo.h"
#include "server.h"
#include "client.h"
#include "resolver.h"
#include "tracker.h"
#include "errchk.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static NSString * const GSHostMapString              = @"GSHostMapString";
static NSString * const GSHostUPnPBool               = @"GSHostUPnPBool";
static NSString * const GSHostPortNumber             = @"GSHostPortNumber";
static NSString * const GSHostPasswordBool           = @"GSHostPasswordBool";
static NSString * const GSHostPasswordString         = @"GSHostPasswordString";
static NSString * const GSHostTimeLimitBool          = @"GSHostTimeLimitBool";
static NSString * const GSHostTimeLimitString        = @"GSHostTimeLimitString";
static NSString * const GSHostHiddenMinesBool        = @"GSHostHiddenMinesBool";
static NSString * const GSHostTrackerBool            = @"GSHostTrackerBool";
static NSString * const GSHostGameTypeNumber         = @"GSHostGameTypeNumber";
static NSString * const GSHostDominationTypeNumber   = @"GSHostDominationTypeNumber";
static NSString * const GSHostDominationBaseControlString = @"GSHostDominationBaseControlString";
static NSString * const GSJoinAddressString          = @"GSJoinAddressString";
static NSString * const GSJoinPortNumber             = @"GSJoinPortNumber";
static NSString * const GSJoinPasswordBool           = @"GSJoinPasswordBool";
static NSString * const GSJoinPasswordString         = @"GSJoinPasswordString";
static NSString * const GSTrackerString              = @"GSTrackerString";

static NSString * const GSPrefPaneIdentifierString   = @"GSPrefPaneIdentifierString";
static NSString * const GSPlayerNameString           = @"GSPlayerNameString";
static NSString * const GSKeyConfigDict              = @"GSKeyConfigDict";
static NSString * const GSAutoSlowdownBool           = @"GSAutoSlowdownBool";
static NSString * const GSAccelerateString           = @"GSAccelerateString";
static NSString * const GSBrakeString                = @"GSBrakeString";
static NSString * const GSTurnLeftString             = @"GSTurnLeftString";
static NSString * const GSTurnRightString            = @"GSTurnRightString";
static NSString * const GSLayMineString              = @"GSLayMineString";
static NSString * const GSShootString                = @"GSShootString";
static NSString * const GSIncreaseAimString          = @"GSIncreaseAimString";
static NSString * const GSDecreaseAimString          = @"GSDecreaseAimString";
static NSString * const GSUpString                   = @"GSUpString";
static NSString * const GSDownString                 = @"GSDownString";
static NSString * const GSLeftString                 = @"GSLeftString";
static NSString * const GSRightString                = @"GSRightString";
static NSString * const GSTankViewString             = @"GSTankViewString";
static NSString * const GSPillViewString             = @"GSPillViewString";

static NSString * const GSMuteBool                   = @"GSMuteBool";

// bolo toolbar prefs
static NSString * const GSBuilderToolInteger         = @"GSBuilderToolInteger";

static NSString * const GSPlayerInfoImage            = @"PlayerInfo";
static NSString * const GSKeyConfigImage             = @"KeyConfig";

static NSString * const GSTankCenterImage            = @"TankCenter";
static NSString * const GSPillCenterImage            = @"PillCenter";
static NSString * const GSZoomInImage                = @"ZoomIn";
static NSString * const GSZoomOutImage               = @"ZoomOut";

// toolbar item identifiers
static NSString * const GSPreferencesToolbar               = @"GSPreferencesToolbar";
static NSString * const GSToolbarPlayerInfoItemIdentifier  = @"GSToolbarPlayerInfoItemIdentifier";
static NSString * const GSToolbarKeyConfigItemIdentifier   = @"GSToolbarKeyConfigItemIdentifier";

static NSString * const GSBoloToolbar                      = @"GSBoloToolbar";
static NSString * const GSZoomInItemIdentifier             = @"GSZoomInItemIdentifier";
static NSString * const GSZoomOutItemIdentifier            = @"GSZoomOutItemIdentifier";
static NSString * const GSBoloToolItemIdentifier           = @"GSBoloToolItemIdentifier";
static NSString * const GSTankCenterItemIdentifier         = @"GSTankCenterItemIdentifier";
static NSString * const GSPillCenterItemIdentifier         = @"GSPillCenterItemIdentifier";

static NSString * const GSShowStatusBool                   = @"GSShowStatusBool";
static NSString * const GSShowAllegianceBool               = @"GSShowAllegianceBool";
static NSString * const GSShowMessagesBool                 = @"GSShowMessagesBool";

// tracker table view columns
static NSString * const GSHostPlayerColumn                 = @"GSHostPlayerColumn";
static NSString * const GSMapNameColumn                    = @"GSMapNameColumn";
static NSString * const GSPlayersColumn                    = @"GSPlayersColumn";
static NSString * const GSPasswordColumn                   = @"GSPasswordColumn";
static NSString * const GSAllowJoinColumn                  = @"GSAllowJoinColumn";
static NSString * const GSPausedColumn                     = @"GSPausedColumn";
static NSString * const GSHostnameColumn                   = @"GSHostnameColumn";
static NSString * const GSPortColumn                       = @"GSPortColumn";

// message panel
static NSString * const GSMessageTarget                   = @"GSMessageTarget";

static GSXBoloController *controller = nil;

// sound
static BOOL muteBool;

static NSMutableArray *bubblessounds;
static NSMutableArray *builderdeathsounds;
static NSMutableArray *buildsounds;
static NSMutableArray *sinksounds;
static NSMutableArray *superboomsounds;
static NSMutableArray *explosionsounds;
static NSMutableArray *farbuildsounds;
static NSMutableArray *farbuilderdeathsounds;
static NSMutableArray *farexplosionsounds;
static NSMutableArray *farhittanksounds;
static NSMutableArray *farhitterrainsounds;
static NSMutableArray *farhittreesounds;
static NSMutableArray *farshotsounds;
static NSMutableArray *farsinksounds;
static NSMutableArray *farsuperboomsounds;
static NSMutableArray *fartreesounds;
static NSMutableArray *minesounds;
static NSMutableArray *msgreceivedsounds;
static NSMutableArray *pillshotsounds;
static NSMutableArray *hittanksounds;
static NSMutableArray *tankshotsounds;
static NSMutableArray *hitterrainsounds;
static NSMutableArray *hittreesounds;
static NSMutableArray *treesounds;

static NSMutableArray *speechSynthesizers;

static struct ListNode trackerlist;

static const float kZoomLevels[] = {
    0.5,
    0.75,
    1.0,
    1.5,
    2.0
};

#define DEFAULT_ZOOM 2
#define MAX_ZOOM (sizeof(kZoomLevels) / sizeof(*kZoomLevels) - 1)

// set the key
int setKey(NSMutableDictionary *dict, NSWindow *win, GSKeyCodeField *field, NSString *newObject);

static void registercallback(int status);
static void getlisttrackerstatus(int status);

@interface GSXBoloController (Private)

// bolo callbacks
- (void)setPlayerStatus:(NSString *)aString;
- (void)setPillStatus:(NSString *)aString;
- (void)setBaseStatus:(NSString *)aString;
- (void)setTankStatusBars;
- (void)refresh:(NSTimer *)aTimer;
- (void)printMessage:(NSAttributedString *)string;
- (void)setJoinProgressStatusTextField:(NSString *)aString;
- (void)setJoinProgressIndicator:(NSString *)aString;

// callbacks from client thread
- (void)joinSuccess;
- (void)joinDNSError:(NSString *)eString;
- (void)joinTimeOut;
- (void)joinConnectionRefused;
- (void)joinNetUnreachable;
- (void)joinHostUnreachable;
- (void)joinBadVersion;
- (void)joinDisallow;
- (void)joinBadPassword;
- (void)joinServerFull;
- (void)joinTimeLimit;
- (void)joinBannedPlayer;
- (void)joinServerError;
- (void)joinConnectionReset;

- (void)registerSuccess;

// callback for tracker dns error
- (void)trackerDNSError:(NSString *)eString;

// callbacks from registertracker
- (void)registerTrackerTimeOut;
- (void)registerTrackerConnectionRefused;
- (void)registerTrackerHostDown;
- (void)registerTrackerHostUnreachable;
- (void)registerTrackerBadVersion;
- (void)registerTrackerTCPPortBlocked;
- (void)registerTrackerUDPPortBlocked;
- (void)registerTrackerConnectionReset;

// callbacks from getlisttracker
- (void)getListTrackerTimeOut;
- (void)getListTrackerConnectionRefused;
- (void)getListTrackerHostDown;
- (void)getListTrackerHostUnreachable;
- (void)getListTrackerBadVersion;
- (void)getListTrackerSuccess;

// robots
- (BOOL)_validateRobotMenuItem: (NSMenuItem *)item;
- (void)setupRobotsMenu;
@end

@implementation GSXBoloController

// NIB methods

- (void)awakeFromNib {
  NSUserDefaults *defaults;
  NSSound *sound;

  controller = self;
  defaults = [NSUserDefaults standardUserDefaults];
  [defaults registerDefaults:[NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultPreferences" ofType:@"plist"]]];

  // init toolbar items
  toolbarPlayerInfoItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSToolbarPlayerInfoItemIdentifier];
  [toolbarPlayerInfoItem setLabel:NSLocalizedString(@"Player Info", nil)];
  [toolbarPlayerInfoItem setImage:[NSImage imageNamed:GSPlayerInfoImage]];
  [toolbarPlayerInfoItem setTarget:self];
  [toolbarPlayerInfoItem setAction:@selector(prefPane:)];

  toolbarKeyConfigItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSToolbarKeyConfigItemIdentifier];
  [toolbarKeyConfigItem setLabel:NSLocalizedString(@"Key Config", nil)];
  [toolbarKeyConfigItem setImage:[NSImage imageNamed:GSKeyConfigImage]];
  [toolbarKeyConfigItem setTarget:self];
  [toolbarKeyConfigItem setAction:@selector(prefPane:)];
  [toolbarKeyConfigItem setEnabled:YES];

  // init the preferences toolbar
  prefToolbar = [[NSToolbar alloc] initWithIdentifier:GSPreferencesToolbar];
  [prefToolbar setDelegate:self];
  [prefToolbar setSelectedItemIdentifier:GSToolbarPlayerInfoItemIdentifier];
  [preferencesWindow setToolbar:prefToolbar];

  // init bolo toolbar items
  builderToolItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSBoloToolItemIdentifier];
  [builderToolItem setLabel:NSLocalizedString(@"Builder Tool", nil)];
  [builderToolItem setView:builderToolView];
  [builderToolItem setMinSize:[builderToolView frame].size];

  tankCenterItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSTankCenterItemIdentifier];
  [tankCenterItem setLabel:NSLocalizedString(@"Tank Center", nil)];
  [tankCenterItem setImage:[NSImage imageNamed:GSTankCenterImage]];
  [tankCenterItem setTarget:self];
  [tankCenterItem setAction:@selector(tankCenter:)];

  pillCenterItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSPillCenterItemIdentifier];
  [pillCenterItem setLabel:NSLocalizedString(@"Pill Center", nil)];
  [pillCenterItem setImage:[NSImage imageNamed:GSPillCenterImage]];
  [pillCenterItem setTarget:self];
  [pillCenterItem setAction:@selector(pillCenter:)];

  zoomInItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSZoomInItemIdentifier];
  [zoomInItem setLabel:NSLocalizedString(@"Zoom In", nil)];
  [zoomInItem setImage:[NSImage imageNamed:GSZoomInImage]];
  [zoomInItem setTarget:self];
  [zoomInItem setAction:@selector(zoomIn:)];

  zoomOutItem = [[NSToolbarItem alloc] initWithItemIdentifier:GSZoomOutItemIdentifier];
  [zoomOutItem setLabel:NSLocalizedString(@"Zoom Out", nil)];
  [zoomOutItem setImage:[NSImage imageNamed:GSZoomOutImage]];
  [zoomOutItem setTarget:self];
  [zoomOutItem setAction:@selector(zoomOut:)];

  // init the builder toolbar
  boloToolbar = [[NSToolbar alloc] initWithIdentifier:GSBoloToolbar];
  [boloToolbar setDelegate:self];
  [boloToolbar setAllowsUserCustomization:YES];
  [boloWindow setToolbar:boloToolbar];

  // init status bars
  [playerShellsStatusBar setType:GSVerticalBar];
  [playerMinesStatusBar setType:GSVerticalBar];
  [playerArmourStatusBar setType:GSVerticalBar];
  [playerTreesStatusBar setType:GSVerticalBar];
  [baseShellsStatusBar setType:GSHorizontalBar];
  [baseMinesStatusBar setType:GSHorizontalBar];
  [baseArmourStatusBar setType:GSHorizontalBar];

  // we don't need no stink'n windows menu
  [newGameWindow setExcludedFromWindowsMenu:YES];
  [boloWindow setExcludedFromWindowsMenu:YES];
  [statusPanel setExcludedFromWindowsMenu:YES];
  [allegiancePanel setExcludedFromWindowsMenu:YES];
  [messagesPanel setExcludedFromWindowsMenu:YES];
  [preferencesWindow setExcludedFromWindowsMenu:YES];

  // keep panels visible
  [statusPanel setHidesOnDeactivate:NO];
  [allegiancePanel setHidesOnDeactivate:NO];
  [messagesPanel setHidesOnDeactivate:NO];

  // init the host pane
  [self setHostMapString:[defaults stringForKey:GSHostMapString]];
  [self setHostUPnPBool:[defaults integerForKey:GSHostUPnPBool]];
  if([defaults boolForKey:@"GSHostPortNumberRandom"])
    [self setHostPortNumber:(random() % 40000) + 2000];
  else
    [self setHostPortNumber:[defaults integerForKey:GSHostPortNumber]];
  [self setHostPasswordBool:[defaults boolForKey:GSHostPasswordBool]];
  [self setHostPasswordString:[defaults stringForKey:GSHostPasswordString]];
  [self setHostTimeLimitBool:[defaults boolForKey:GSHostTimeLimitBool]];
  [self setHostTimeLimitString:[defaults stringForKey:GSHostTimeLimitString]];
  [self setHostHiddenMinesBool:[defaults boolForKey:GSHostHiddenMinesBool]];
  [self setHostTrackerBool:[defaults boolForKey:GSHostTrackerBool]];
  [self setHostGameTypeNumber:[defaults integerForKey:GSHostGameTypeNumber]];

  // init host domination pane
  [self setHostDominationTypeNumber:[defaults integerForKey:GSHostDominationTypeNumber]];
  [self setHostDominationBaseControlString:[defaults stringForKey:GSHostDominationBaseControlString]];

  // init the join pane
  [self setJoinAddressString:[defaults stringForKey:GSJoinAddressString]];
  [self setJoinPortNumber:[defaults integerForKey:GSJoinPortNumber]];
  [self setJoinPasswordBool:[defaults boolForKey:GSJoinPasswordBool]];
  [self setJoinPasswordString:[defaults stringForKey:GSJoinPasswordString]];

  // init tracker string
  [self setTrackerString:[defaults stringForKey:GSTrackerString]];

  // init the pref panes
  [self setPrefPaneIdentifierString:[defaults stringForKey:GSPrefPaneIdentifierString]];
  [self setPlayerNameString:[defaults stringForKey:GSPlayerNameString]];
  [self setAutoSlowdownBool:[defaults boolForKey:GSAutoSlowdownBool]];
  [self revertKeyConfig:self];

  // init the show variables
  [self setShowStatusBool:[defaults boolForKey:GSShowStatusBool]];
  [self setShowAllegianceBool:[defaults boolForKey:GSShowAllegianceBool]];
  [self setShowMessagesBool:[defaults boolForKey:GSShowMessagesBool]];

  // init bolo window
  [self setBuilderToolInteger:[defaults integerForKey:GSBuilderToolInteger]];

  // init allegiance panel
  playerInfoArray = [[NSMutableArray alloc] init];

  // init messages panel
  [self setMessageTargetInteger:[defaults integerForKey:GSMessageTarget]];

  // init the key config
  [self setKeyConfigDict:[defaults dictionaryForKey:GSKeyConfigDict]];

  // schedule a timer
  [NSTimer scheduledTimerWithTimeInterval:0.0625 target:self selector:@selector(refresh:) userInfo:nil repeats:YES];

  // init sound
  [self setMuteBool:[defaults boolForKey:GSMuteBool]];

  // allocate sounds
  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"bubbles" ofType:@"aiff"] byReference:NO];
  bubblessounds = [[NSMutableArray alloc] init];
  [bubblessounds addObject:sound];
  [bubblessounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"build" ofType:@"aiff"] byReference:NO];
  buildsounds = [[NSMutableArray alloc] init];
  [buildsounds addObject:sound];
  [buildsounds addObject:[sound copy]];
  [buildsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"builderdeath" ofType:@"aiff"] byReference:NO];
  builderdeathsounds = [[NSMutableArray alloc] init];
  [builderdeathsounds addObject:sound];
  [builderdeathsounds addObject:[sound copy]];
  [builderdeathsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"explosion" ofType:@"aiff"] byReference:NO];
  explosionsounds = [[NSMutableArray alloc] init];
  [explosionsounds addObject:sound];
  [explosionsounds addObject:[sound copy]];
  [explosionsounds addObject:[sound copy]];
  [explosionsounds addObject:[sound copy]];
  [explosionsounds addObject:[sound copy]];
  [explosionsounds addObject:[sound copy]];
  [explosionsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fbuild" ofType:@"aiff"] byReference:NO];
  farbuildsounds = [[NSMutableArray alloc] init];
  [farbuildsounds addObject:sound];
  [farbuildsounds addObject:[sound copy]];
  [farbuildsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fbuilderdeath" ofType:@"aiff"] byReference:NO];
  farbuilderdeathsounds = [[NSMutableArray alloc] init];
  [farbuilderdeathsounds addObject:sound];
  [farbuilderdeathsounds addObject:[sound copy]];
  [farbuilderdeathsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fexplosion" ofType:@"aiff"] byReference:NO];
  farexplosionsounds = [[NSMutableArray alloc] init];
  [farexplosionsounds addObject:sound];
  [farexplosionsounds addObject:[sound copy]];
  [farexplosionsounds addObject:[sound copy]];
  [farexplosionsounds addObject:[sound copy]];
  [farexplosionsounds addObject:[sound copy]];
  [farexplosionsounds addObject:[sound copy]];
  [farexplosionsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fhittank" ofType:@"aiff"] byReference:NO];
  farhittanksounds = [[NSMutableArray alloc] init];
  [farhittanksounds addObject:sound];
  [farhittanksounds addObject:[sound copy]];
  [farhittanksounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fhitterrain" ofType:@"aiff"] byReference:NO];
  farhitterrainsounds = [[NSMutableArray alloc] init];
  [farhitterrainsounds addObject:sound];
  [farhitterrainsounds addObject:[sound copy]];
  [farhitterrainsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fhittree" ofType:@"aiff"] byReference:NO];
  farhittreesounds = [[NSMutableArray alloc] init];
  [farhittreesounds addObject:sound];
  [farhittreesounds addObject:[sound copy]];
  [farhittreesounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fshot" ofType:@"aiff"] byReference:NO];
  farshotsounds = [[NSMutableArray alloc] init];
  [farshotsounds addObject:sound];
  [farshotsounds addObject:[sound copy]];
  [farshotsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fsink" ofType:@"aiff"] byReference:NO];
  farsinksounds = [[NSMutableArray alloc] init];
  [farsinksounds addObject:sound];
  [farsinksounds addObject:[sound copy]];
  [farsinksounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fsuperboom" ofType:@"aiff"] byReference:NO];
  farsuperboomsounds = [[NSMutableArray alloc] init];
  [farsuperboomsounds addObject:sound];
  [farsuperboomsounds addObject:[sound copy]];
  [farsuperboomsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ftree" ofType:@"aiff"] byReference:NO];
  fartreesounds = [[NSMutableArray alloc] init];
  [fartreesounds addObject:sound];
  [fartreesounds addObject:[sound copy]];
  [fartreesounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"hittank" ofType:@"aiff"] byReference:NO];
  hittanksounds = [[NSMutableArray alloc] init];
  [hittanksounds addObject:sound];
  [hittanksounds addObject:[sound copy]];
  [hittanksounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"hitterrain" ofType:@"aiff"] byReference:NO];
  hitterrainsounds = [[NSMutableArray alloc] init];
  [hitterrainsounds addObject:sound];
  [hitterrainsounds addObject:[sound copy]];
  [hitterrainsounds addObject:[sound copy]];
  [hitterrainsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"hittree" ofType:@"aiff"] byReference:NO];
  hittreesounds = [[NSMutableArray alloc] init];
  [hittreesounds addObject:sound];
  [hittreesounds addObject:[sound copy]];
  [hittreesounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mine" ofType:@"aiff"] byReference:NO];
  minesounds = [[NSMutableArray alloc] init];
  [minesounds addObject:sound];
  [minesounds addObject:[sound copy]];
  [minesounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"msgreceived" ofType:@"aiff"] byReference:NO];
  msgreceivedsounds = [[NSMutableArray alloc] init];
  [msgreceivedsounds addObject:sound];
  [msgreceivedsounds addObject:[sound copy]];
  [msgreceivedsounds addObject:[sound copy]];
  [msgreceivedsounds makeObjectsPerformSelector:@selector(release)];
	
  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"pillshot" ofType:@"aiff"] byReference:NO];
  pillshotsounds = [[NSMutableArray alloc] init];
  [pillshotsounds addObject:sound];
  [pillshotsounds addObject:[sound copy]];
  [pillshotsounds addObject:[sound copy]];
  [pillshotsounds addObject:[sound copy]];
  [pillshotsounds addObject:[sound copy]];
  [pillshotsounds addObject:[sound copy]];
  [pillshotsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sink" ofType:@"aiff"] byReference:NO];
  sinksounds = [[NSMutableArray alloc] init];
  [sinksounds addObject:sound];
  [sinksounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"superboom" ofType:@"aiff"] byReference:NO];
  superboomsounds = [[NSMutableArray alloc] init];
  [superboomsounds addObject:sound];
  [superboomsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"tankshot" ofType:@"aiff"] byReference:NO];
  tankshotsounds = [[NSMutableArray alloc] init];
  [tankshotsounds addObject:sound];
  [tankshotsounds addObject:[sound copy]];
  [tankshotsounds addObject:[sound copy]];
  [tankshotsounds addObject:[sound copy]];
  [tankshotsounds addObject:[sound copy]];
  [tankshotsounds makeObjectsPerformSelector:@selector(release)];

  sound = [[NSSound alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"tree" ofType:@"aiff"] byReference:NO];
  treesounds = [[NSMutableArray alloc] init];
  [treesounds addObject:sound];
  [treesounds addObject:[sound copy]];
  [treesounds addObject:[sound copy]];
  [treesounds makeObjectsPerformSelector:@selector(release)];

  speechSynthesizers = [[NSMutableArray alloc] init];
  [speechSynthesizers addObject:[[NSSpeechSynthesizer alloc] init]];
  [speechSynthesizers addObject:[[NSSpeechSynthesizer alloc] init]];
  [speechSynthesizers makeObjectsPerformSelector:@selector(release)];

  zoomLevel = DEFAULT_ZOOM;

  // bug in interface builder prevents this outlet from working
  joinProgressStatusTextField = [[NSTextField alloc] initWithFrame:NSMakeRect(76, 60, 193, 17)];
  [joinProgressStatusTextField setBordered:NO];
  [joinProgressStatusTextField setEditable:NO];
  [joinProgressStatusTextField setDrawsBackground:NO];
  [[joinProgressWindow contentView] addSubview:joinProgressStatusTextField];

  joinProgressIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(18, 18, 172, 20)];
  [[joinProgressWindow contentView] addSubview:joinProgressIndicator];

  // setup double clicking in tracker
  [joinTrackerTableView setTarget:self];
  [joinTrackerTableView setDoubleAction:@selector(joinOK:)];

  // TCMPortMapper
  portMapper = [TCMPortMapper sharedInstance];
  [portMapper setUserID:[NSString stringWithFormat:@"%d", getpid()]];
 
  // set up robots menu
  [self setupRobotsMenu];
}

// accessor methods

- (void)setHostMapString:(NSString *)aString {
  [hostMapString release];
  hostMapString = [aString copy];
  [hostMapField setStringValue:[[NSFileManager defaultManager] displayNameAtPath:aString]];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSHostMapString];
}

- (void)setHostUPnPBool:(BOOL)aBool {
  hostUPnPBool = aBool;
  [hostUPnPSwitch setState:aBool ? NSOnState : NSOffState];
  [hostPortField setEnabled:!aBool];
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSHostUPnPBool];
}

- (void)setHostPortNumber:(int)aNumber {
  hostPortNumber = aNumber;
  [hostPortField setIntValue:aNumber];
  [[NSUserDefaults standardUserDefaults] setInteger:aNumber forKey:GSHostPortNumber];
}

- (void)setHostPasswordBool:(BOOL)aBool {
  hostPasswordBool = aBool;
  [hostPasswordSwitch setState:aBool ? NSOnState : NSOffState];
  [hostPasswordField setEnabled:aBool];
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSHostPasswordBool];
}

- (void)setHostPasswordString:(NSString *)aString {
  [hostPasswordString release];
  hostPasswordString = [aString copy];
  [hostPasswordField setStringValue:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSHostPasswordString];
}

- (void)setHostTimeLimitBool:(BOOL)aBool {
  hostTimeLimitBool = aBool;
  [hostTimeLimitSwitch setState:aBool ? NSOnState : NSOffState];
  [hostTimeLimitSlider setEnabled:aBool];
  [hostTimeLimitField setEnabled:aBool];
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSHostTimeLimitBool];
}

- (void)setHostTimeLimitString:(NSString *)aString {
  NSScanner *scanner;
  int hours, minutes, seconds;

  scanner = [NSScanner scannerWithString:aString];
  [scanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@":"]];
  [scanner scanInt:&hours];
  [scanner scanInt:&minutes];
  [scanner scanInt:&seconds];
  [hostTimeLimitString release];
  hostTimeLimitString = [aString copy];
  [hostTimeLimitSlider setIntValue:hours*3600 + minutes*60 + seconds];
  [hostTimeLimitField setStringValue:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSHostTimeLimitString];
}

- (void)setHostHiddenMinesBool:(BOOL)aBool {
  hostHiddenMinesBool = aBool;
  [hostHiddenMinesSwitch setState:aBool ? NSOnState : NSOffState];

  if (aBool) {
    [hostHiddenMinesTextField setStringValue:@"Mines May Be Hidden"];
  }
  else {
    [hostHiddenMinesTextField setStringValue:@"Mines Will Always Be Visible"];
  }

  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSHostHiddenMinesBool];
}

- (void)setHostTrackerBool:(BOOL)aBool {
  hostTrackerBool = aBool;
  [hostTrackerSwitch setState:hostTrackerBool ? NSOnState : NSOffState];
  [hostTrackerField setEnabled:hostTrackerBool];
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSHostTrackerBool];
}

- (void)setHostGameTypeNumber:(int)aNumber {
  hostGameTypeNumber = aNumber;
  [hostGameTypeMenu selectItemAtIndex:aNumber];
  [hostGameTypeTab selectTabViewItemAtIndex:aNumber];
  [[NSUserDefaults standardUserDefaults] setInteger:aNumber forKey:GSHostGameTypeNumber];
}

- (void)setHostDominationTypeNumber:(int)aNumber {
  hostDominationTypeNumber = aNumber;
  [hostDominationTypeMatrix selectCellWithTag:aNumber];
  [[NSUserDefaults standardUserDefaults] setInteger:aNumber forKey:GSHostDominationTypeNumber];
}

- (void)setHostDominationBaseControlString:(NSString *)aString {
  NSScanner *scanner;
  int hours, minutes, seconds;
  scanner = [NSScanner scannerWithString:aString];
  [scanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@":"]];
  [scanner scanInt:&hours];
  [scanner scanInt:&minutes];
  [scanner scanInt:&seconds];
  [hostDominationBaseControlString release];
  hostDominationBaseControlString = [aString copy];
  [hostDominationBaseControlSlider setIntValue:hours*3600 + minutes*60 + seconds];
  [hostDominationBaseControlField setStringValue:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSHostDominationBaseControlString];
}

- (void)setJoinAddressString:(NSString *)newJoinAddressString {
  [joinAddressString release];
  joinAddressString = [newJoinAddressString copy];
  [joinAddressField setStringValue:newJoinAddressString];
  [[NSUserDefaults standardUserDefaults] setObject:newJoinAddressString forKey:GSJoinAddressString];
}

- (void)setJoinPortNumber:(int)aNumber {
  joinPortNumber = aNumber;
  [joinPortField setIntValue:aNumber];
  [[NSUserDefaults standardUserDefaults] setInteger:aNumber forKey:GSJoinPortNumber];
}

- (void)setJoinPasswordBool:(BOOL)aBool {
  joinPasswordBool = aBool;
  [joinPasswordSwitch setState:aBool ? NSOnState : NSOffState];
  [joinPasswordField setEnabled:aBool];
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSJoinPasswordBool];
}

- (void)setJoinPasswordString:(NSString *)aString {
  [joinPasswordString release];
  joinPasswordString = [aString copy];
  [joinPasswordField setStringValue:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSJoinPasswordString];
}

- (void)setJoinTrackerArray:(NSArray *)aArray {
  [joinTrackerArray release];
  joinTrackerArray = [aArray mutableCopy];
  [joinTrackerArray sortUsingDescriptors:[joinTrackerTableView sortDescriptors]];
  [joinTrackerTableView reloadData];
}

- (void)setTrackerString:(NSString *)aString {
  [trackerString release];
  trackerString = [aString copy];
  [hostTrackerField setStringValue:aString];
  [joinTrackerField setStringValue:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSTrackerString];
}

- (void)setPrefPaneIdentifierString:(NSString *)aString {
  [prefToolbar setSelectedItemIdentifier:aString];
  [prefTab selectTabViewItemWithIdentifier:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSPrefPaneIdentifierString];
}

- (void)setPlayerNameString:(NSString *)aString {
  [playerNameString release];
  playerNameString = [aString copy];
  [prefPlayerNameField setStringValue:aString];
  [[NSUserDefaults standardUserDefaults] setObject:aString forKey:GSPlayerNameString];
}

- (void)setKeyConfigDict:(NSDictionary *)aDict {
  [keyConfigDict release];
  keyConfigDict = [aDict copy];
  [[NSUserDefaults standardUserDefaults] setObject:aDict forKey:GSKeyConfigDict];
}

- (void)setAutoSlowdownBool:(BOOL)aBool {
TRY
  autoSlowdownBool = aBool;
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSAutoSlowdownBool];

  if (keyevent(BRAKEMASK, aBool) == -1) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)setShowStatusBool:(BOOL)aBool {
  showStatusBool = aBool;
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSShowStatusBool];
}

- (void)setShowAllegianceBool:(BOOL)aBool {
  showAllegianceBool = aBool;
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSShowAllegianceBool];
}

- (void)setShowMessagesBool:(BOOL)aBool {
  showMessagesBool = aBool;
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSShowMessagesBool];
}

- (void)setBuilderToolInteger:(int)anInt {
  builderToolInt = anInt;
  [builderToolMatrix selectCellWithTag:anInt];
  [[NSUserDefaults standardUserDefaults] setInteger:anInt forKey:GSBuilderToolInteger];
}

- (void)setMessageTargetInteger:(int)anInt {
  messageTargetInt = anInt;
  [messageTargetMatrix selectCellWithTag:anInt];
  [[NSUserDefaults standardUserDefaults] setInteger:anInt forKey:GSMessageTarget];
}

- (void)setMuteBool:(BOOL)aBool {
  muteBool = aBool;
  [[NSUserDefaults standardUserDefaults] setBool:aBool forKey:GSMuteBool];
}

// IBAction methods

- (IBAction)closeGame:(id)sender {
  NSEnumerator *enumerator;
  TCMPortMapping *portMapping;

  [boloWindow orderOut:self];
  [statusPanel orderOut:self];
  [allegiancePanel orderOut:self];
  [messagesPanel orderOut:self];

  /* remove port mapping */
  enumerator = [[portMapper portMappings] objectEnumerator];

  while ((portMapping = [enumerator nextObject])) {
    [portMapper removePortMapping:portMapping];
  }

  [portMapper start];

  stopclient();

  if (server.setup) {
    stopserver();
  }

  [self newGame:self];
}

- (IBAction)hostChoose:(id)sender {
  if ([hostMapString length] > 0) {
    [[NSOpenPanel openPanel]
      beginSheetForDirectory:[hostMapString stringByDeletingLastPathComponent]
      file:[hostMapString lastPathComponent]
      types:[NSArray arrayWithObjects:@"map", NSFileTypeForHFSTypeCode('BMAP'), nil]
      modalForWindow:newGameWindow
      modalDelegate:self
      didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
      contextInfo:NULL];
  }
  else {
    [[NSOpenPanel openPanel]
      beginSheetForDirectory:nil
      file:nil
      types:[NSArray arrayWithObjects:@"map", NSFileTypeForHFSTypeCode('BMAP'), nil]
      modalForWindow:newGameWindow
      modalDelegate:self
      didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
      contextInfo:NULL];
  }
}

- (IBAction)hostUPnPSwitch:(id)sender {
  BOOL aBool;
  aBool = [sender state] == NSOnState;
  [self setHostUPnPBool:aBool];
  if (!aBool) {
    [hostPortField selectText:self];
  }
}

- (IBAction)hostPort:(id)sender {
  [self setHostPortNumber:[sender intValue]];
}

- (IBAction)hostPasswordSwitch:(id)sender {
  BOOL aBool;
  aBool = [sender state] == NSOnState;
  [self setHostPasswordBool:aBool];
  if (aBool) {
    [hostPasswordField selectText:self];
  }
}

- (IBAction)hostPassword:(id)sender {
  [self setHostPasswordString:[sender stringValue]];
}

- (IBAction)hostTimeLimitSwitch:(id)sender {
  BOOL aBool;
  aBool = [sender state] == NSOnState;
  [self setHostTimeLimitBool:aBool];
  if (aBool) {
    [hostTimeLimitField selectText:self];
  }
}

- (IBAction)hostTimeLimit:(id)sender {
  NSString *aString;
  if ([sender isMemberOfClass:[NSTextField class]]) {
    aString = [sender stringValue];
  }
  else {
    int hours, minutes, seconds;
    int time;
    time = [sender intValue];
    hours = time/3600;
    minutes = (time%3600)/60;
    seconds = (time%3600)%60;
    aString =
      [NSString stringWithFormat:@"%@:%@:%@",
        (hours   >= 10 ? [NSString stringWithFormat:@"%d", hours  ] : [NSString stringWithFormat:@"0%d", hours  ]),
        (minutes >= 10 ? [NSString stringWithFormat:@"%d", minutes] : [NSString stringWithFormat:@"0%d", minutes]),
        (seconds >= 10 ? [NSString stringWithFormat:@"%d", seconds] : [NSString stringWithFormat:@"0%d", seconds])];
  }
  [self setHostTimeLimitString:aString];
  [hostTimeLimitField selectText:self];
}

- (IBAction)hostHiddenMinesSwitch:(id)sender {
  BOOL aBool;
  aBool = [sender state] == NSOnState;
  [self setHostHiddenMinesBool:aBool];
}

- (IBAction)hostTrackerSwitch:(id)sender {
  BOOL aBool;
  aBool = [sender state] == NSOnState;
  [self setHostTrackerBool:aBool];
  if (aBool) {
    [hostTrackerField selectText:self];
  }
}

- (IBAction)hostGameType:(id)sender {
  [self setHostGameTypeNumber:[sender indexOfSelectedItem]];
}

- (IBAction)hostDominationType:(id)sender {
  [self setHostDominationTypeNumber:[[hostDominationTypeMatrix selectedCell] tag]];
}

- (IBAction)hostDominationBaseControl:(id)sender {
  NSString *aString;
  if ([sender isMemberOfClass:[NSTextField class]]) {
    aString = [sender stringValue];
  }
  else {
    int hours, minutes, seconds;
    int time;
    time = [sender intValue];
    hours = time/3600;
    minutes = (time%3600)/60;
    seconds = (time%3600)%60;
    aString =
      [NSString stringWithFormat:@"%@:%@:%@",
        (hours   >= 10 ? [NSString stringWithFormat:@"%d", hours  ] : [NSString stringWithFormat:@"0%d", hours  ]),
        (minutes >= 10 ? [NSString stringWithFormat:@"%d", minutes] : [NSString stringWithFormat:@"0%d", minutes]),
        (seconds >= 10 ? [NSString stringWithFormat:@"%d", seconds] : [NSString stringWithFormat:@"0%d", seconds])];
  }

  [self setHostDominationBaseControlString:aString];
  [hostDominationBaseControlField selectText:self];
}

- (void)portMapperDidFinishWork:(NSNotification *)aNotification {
  TCMPortMapping *mapping;
  NSEnumerator *enumerator;

TRY
  enumerator = [[portMapper portMappings] objectEnumerator];

  while ((mapping = [enumerator nextObject])) {
    if ([mapping localPort] == getservertcpport()) {
      if ([mapping mappingStatus] == TCMPortMappingStatusMapped && [mapping transportProtocol] == TCMPortMappingTransportProtocolBoth) {
        if (hostTrackerBool) {
          startserverthreadwithtracker([trackerString UTF8String], [mapping externalPort], [playerNameString UTF8String], [[hostMapString lastPathComponent] UTF8String], registercallback);
        }
        else {
          [[NSNotificationCenter defaultCenter] removeObserver:self name:TCMPortMapperDidFinishWorkNotification object:portMapper];

          if (hostTrackerBool) {  /* not using UPnP but registering with tracker */
            if (startserverthreadwithtracker([trackerString UTF8String], getservertcpport(), [playerNameString UTF8String], [[hostMapString lastPathComponent] UTF8String], registercallback)) LOGFAIL(errno)
          }
          else {
            if (startserverthread()) LOGFAIL(errno)
          }

          break;
        }
      }
      else {
        NSEnumerator *enumerator;
        TCMPortMapping *portMapping;

        [joinProgressWindow orderOut:self];
        [joinProgressIndicator stopAnimation:self];
        [NSApp endSheet:joinProgressWindow];

        [[NSNotificationCenter defaultCenter] removeObserver:self name:TCMPortMapperDidFinishWorkNotification object:portMapper];

        enumerator = [[portMapper portMappings] objectEnumerator];

        while ((portMapping = [enumerator nextObject])) {
          [portMapper removePortMapping:portMapping];
        }

        [portMapper start];

        NSBeginAlertSheet(@"UPnP Failed", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"UPnP was unable to map to a port.  Please check your router settings.");

        if (stopserver()) LOGFAIL(errno)
      }
    }
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (IBAction)hostOK:(id)sender {
  NSData *mapData;

TRY
  if ([hostMapString length] == 0) {
    NSBeginAlertSheet(@"No map chosen.", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Please choose a map.");
  }
  else if ((mapData = [NSData dataWithContentsOfFile:hostMapString]) == nil) {
    NSBeginAlertSheet(@"Error occured when openning map.", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Please try another map.");
    [self setHostMapString:[NSString string]];
  }
  else {
    NSScanner *scanner;
    int hours, minutes, seconds, timelimit;
    struct Domination domination;
    int paused;

    scanner = [NSScanner scannerWithString:hostTimeLimitString];
    [scanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@":"]];
    [scanner scanInt:&hours];
    [scanner scanInt:&minutes];
    [scanner scanInt:&seconds];
    timelimit = hours*3600 + minutes*60 + seconds;

    domination.type = hostDominationTypeNumber;
    scanner = [NSScanner scannerWithString:hostDominationBaseControlString];
    [scanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@":"]];
    [scanner scanInt:&hours];
    [scanner scanInt:&minutes];
    [scanner scanInt:&seconds];
    domination.basecontrol = hours*3600 + minutes*60 + seconds;

    if([[NSUserDefaults standardUserDefaults] boolForKey:@"GSStartGameImmediately"]) {
      paused = 0;
    }
    else {
      paused = 1;
    }

    [joinProgressStatusTextField setStringValue:@"Starting..."];
    [joinProgressIndicator setIndeterminate:YES];
    [joinProgressIndicator setDoubleValue:0.0];
    [joinProgressIndicator startAnimation:self];
    [NSApp beginSheet:joinProgressWindow modalForWindow:newGameWindow modalDelegate:self didEndSelector:nil contextInfo:nil];

    if (setupserver(paused, [mapData bytes], [mapData length], hostUPnPBool ? 0 : hostPortNumber, hostPasswordBool ? [hostPasswordString UTF8String] : NULL, hostTimeLimitBool ? timelimit : 0, hostHiddenMinesBool, 0, hostGameTypeNumber, &domination)) LOGFAIL(errno)

    /* if using UPnP */
    if (hostUPnPBool) {
      [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(portMapperDidFinishWork:) name:TCMPortMapperDidFinishWorkNotification object:portMapper];
      [portMapper addPortMapping:[TCMPortMapping portMappingWithLocalPort:getservertcpport() desiredExternalPort:getservertcpport() transportProtocol:TCMPortMappingTransportProtocolBoth userInfo:nil]];
      [portMapper start];

      [joinProgressStatusTextField setStringValue:@"UPnP Mapping Port..."];
    }
    else if (hostTrackerBool) {  /* not using UPnP but registering with tracker */
      if (startserverthreadwithtracker([trackerString UTF8String], getservertcpport(), [playerNameString UTF8String], [[hostMapString lastPathComponent] UTF8String], registercallback)) LOGFAIL(errno)
    }
    else {  /* not using UPnP and not registering with tracker */
      if (startserverthread()) LOGFAIL(errno)
      if (startclient("localhost", getservertcpport(), [playerNameString UTF8String], hostPasswordBool ? [hostPasswordString UTF8String] : NULL)) LOGFAIL(errno)
    }
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  case ECORFILE:
    [joinProgressWindow orderOut:self];
    [joinProgressIndicator stopAnimation:self];
    [NSApp endSheet:joinProgressWindow];
    NSBeginAlertSheet(@"Unable to Open Map File", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Please choose another map.");
    CLEARERRLOG
    break;

  case EINCMPAT:
    [joinProgressWindow orderOut:self];
    [joinProgressIndicator stopAnimation:self];
    [NSApp endSheet:joinProgressWindow];
    NSBeginAlertSheet(@"Incomaptile Map Version", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Please choose another map.");
    CLEARERRLOG
    break;

  case EADDRINUSE:
    [joinProgressWindow orderOut:self];
    [joinProgressIndicator stopAnimation:self];
    [NSApp endSheet:joinProgressWindow];
    NSBeginAlertSheet(@"Port Unavailable", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Please choose another port.");
    CLEARERRLOG
    break;

  default:
    [joinProgressWindow orderOut:self];
    [joinProgressIndicator stopAnimation:self];
    [NSApp endSheet:joinProgressWindow];
    NSBeginAlertSheet(@"Unexpected Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Error #%d: %s", ERROR, strerror(ERROR));
    CLEARERRLOG
    break;
  }
END
}

- (IBAction)joinAddress:(id)sender {
  [self setJoinAddressString:[sender stringValue]];
}

- (IBAction)joinPort:(id)sender {
  [self setJoinPortNumber:[sender intValue]];
}

- (IBAction)joinPasswordSwitch:(id)sender {
  BOOL aBool;
  aBool = [sender state] == NSOnState;
  [self setJoinPasswordBool:aBool];

  if (aBool) {
    [joinPasswordField selectText:self];
  }
}

- (IBAction)joinPassword:(id)sender {
  [self setJoinPasswordString:[sender stringValue]];
}

- (IBAction)tracker:(id)sender {
  [self setTrackerString:[sender stringValue]];
}

- (IBAction)joinTrackerRefresh:(id)sender {
TRY
  [joinTrackerTableView deselectAll:self];
  [joinProgressStatusTextField setStringValue:@""];
  [joinProgressIndicator setIndeterminate:YES];
  [joinProgressIndicator setDoubleValue:0.0];
  [joinProgressIndicator startAnimation:self];
  [NSApp beginSheet:joinProgressWindow modalForWindow:newGameWindow modalDelegate:self didEndSelector:nil contextInfo:nil];
  
  // get tracker list from bolo
  if (initlist(&trackerlist)) LOGFAIL(errno)
  if (listtracker([trackerString UTF8String], &trackerlist, getlisttrackerstatus)) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (IBAction)joinOK:(id)sender {
TRY
  [joinProgressStatusTextField setStringValue:@""];
  [joinProgressIndicator setIndeterminate:YES];
  [joinProgressIndicator setDoubleValue:0.0];
  [joinProgressIndicator startAnimation:self];
  [NSApp beginSheet:joinProgressWindow modalForWindow:newGameWindow modalDelegate:self didEndSelector:nil contextInfo:nil];
  if (startclient([joinAddressString UTF8String], joinPortNumber, [playerNameString UTF8String], joinPasswordBool ? [joinPasswordString UTF8String] : NULL)) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (IBAction)joinCancel:(id)sender {
  NSEnumerator *enumerator;
  TCMPortMapping *portMapping;

TRY
  /* remove notifications from portmapper */
  [[NSNotificationCenter defaultCenter] removeObserver:self name:TCMPortMapperDidFinishWorkNotification object:portMapper];

  /* remove all port maps */
  enumerator = [[portMapper portMappings] objectEnumerator];

  while ((portMapping = [enumerator nextObject])) {
    [portMapper removePortMapping:portMapping];
  }

  [portMapper start];

  if (client_running) {
    if (stopclient()) LOGFAIL(errno)
  }

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

  /* close modal window */
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];

CLEANUP
  switch (ERROR) {
  case 0:
    return;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

- (IBAction)statusPanel:(id)sender {
  if ([statusPanel isKeyWindow]) {
    [boloWindow makeKeyAndOrderFront:self];
  }
  else {
    [statusPanel makeKeyAndOrderFront:self];
  }
}

- (IBAction)allegiancePanel:(id)sender {
  if ([allegiancePanel isKeyWindow]) {
    [boloWindow makeKeyAndOrderFront:self];
  }
  else {
    [allegiancePanel makeKeyAndOrderFront:self];
  }
}

- (IBAction)messagesPanel:(id)sender {
  if ([messagesPanel isKeyWindow]) {
    [boloWindow makeKeyAndOrderFront:self];
  }
  else {
    [messagesPanel makeKeyAndOrderFront:self];
  }
}

- (IBAction)newGame:(id)sender {
  [newGameWindow makeKeyAndOrderFront:self];
}

- (IBAction)toggleJoin:(id)sender {
  togglejoingame();
}

- (IBAction)toggleMute:(id)sender {
  [self setMuteBool:!muteBool];
}

- (IBAction)gamePauseResumeMenu:(id)sender {
  pauseresumegame();
}

- (IBAction)kickPlayer:(id)sender {
  kickplayer([sender tag]);
}

- (IBAction)banPlayer:(id)sender {
  banplayer([sender tag]);
}

- (IBAction)unbanPlayer:(id)sender {
  unbanplayer([sender tag]);
}

- (IBAction)scrollUp:(id)sender {
  NSScreen *screen;
  NSRect rect;
  NSPoint nspoint;
  CGPoint cgpoint;

  rect = [boloView visibleRect];
  rect.origin.y += 64.0/kZoomLevels[zoomLevel];
  [boloView scrollRectToVisible:rect];

  nspoint = [NSEvent mouseLocation];
  if ([boloView mouse:[boloView convertPoint:[boloWindow convertScreenToBase:nspoint] fromView:[boloWindow contentView]] inRect:[boloView visibleRect]] && (screen = [boloWindow screen])) {
    cgpoint.x = nspoint.x;
    cgpoint.y = [screen frame].size.height - nspoint.y + 64.0;
    CGWarpMouseCursorPosition(cgpoint);
  }
}

- (IBAction)scrollDown:(id)sender {
  NSScreen *screen;
  NSRect rect;
  NSPoint nspoint;
  CGPoint cgpoint;

  rect = [boloView visibleRect];
  rect.origin.y -= 64.0/kZoomLevels[zoomLevel];
  [boloView scrollRectToVisible:rect];

  nspoint = [NSEvent mouseLocation];
  if ([boloView mouse:[boloView convertPoint:[boloWindow convertScreenToBase:nspoint] fromView:[boloWindow contentView]] inRect:[boloView visibleRect]] && (screen = [boloWindow screen])) {
    cgpoint.x = nspoint.x;
    cgpoint.y = [screen frame].size.height - nspoint.y - 64.0;
    CGWarpMouseCursorPosition(cgpoint);
  }
}

- (IBAction)scrollLeft:(id)sender {
  NSScreen *screen;
  NSRect rect;
  NSPoint nspoint;
  CGPoint cgpoint;

  rect = [boloView visibleRect];
  rect.origin.x -= 64.0/kZoomLevels[zoomLevel];
  [boloView scrollRectToVisible:rect];

  nspoint = [NSEvent mouseLocation];
  if ([boloView mouse:[boloView convertPoint:[boloWindow convertScreenToBase:nspoint] fromView:[boloWindow contentView]] inRect:[boloView visibleRect]] && (screen = [boloWindow screen])) {
    cgpoint.x = nspoint.x + 64.0;
    cgpoint.y = [screen frame].size.height - nspoint.y;
    CGWarpMouseCursorPosition(cgpoint);
  }
}

- (IBAction)scrollRight:(id)sender {
  NSScreen *screen;
  NSRect rect;
  NSPoint nspoint;
  CGPoint cgpoint;

  rect = [boloView visibleRect];
  rect.origin.x += 64.0/kZoomLevels[zoomLevel];
  [boloView scrollRectToVisible:rect];

  nspoint = [NSEvent mouseLocation];
  if ([boloView mouse:[boloView convertPoint:[boloWindow convertScreenToBase:nspoint] fromView:[boloWindow contentView]] inRect:[boloView visibleRect]] && (screen = [boloWindow screen])) {
    cgpoint.x = nspoint.x - 64.0;
    cgpoint.y = [screen frame].size.height - nspoint.y;
    CGWarpMouseCursorPosition(cgpoint);
  }
}

- (IBAction)requestAlliance:(id)sender {
  int i;
  NSIndexSet *set;
  uint16_t players;
  int gotlock = 0;

TRY
  players = 0;
  set = [playerInfoTableView selectedRowIndexes];

  for (i = 0; (i = [set indexGreaterThanOrEqualToIndex:i]) != NSNotFound; i++) {
    players |= 1 << [[[playerInfoArray objectAtIndex:i] objectForKey:@"Index"] intValue];
  }

  // lock client
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  if (requestalliance(players)) LOGFAIL(errno)

  // unlock client
  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  if (gotlock) {
    unlockclient();
  }

  switch (ERROR) {
  case 0:
    return;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

- (IBAction)leaveAlliance:(id)sender {
  int i;
  NSIndexSet *set;
  uint16_t players;
  int gotlock = 0;

TRY
  players = 0;
  set = [playerInfoTableView selectedRowIndexes];

  for (i = 0; (i = [set indexGreaterThanOrEqualToIndex:i]) != NSNotFound; i++) {
    players |= 1 << [[[playerInfoArray objectAtIndex:i] objectForKey:@"Index"] intValue];
  }

  // lock client
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  if (leavealliance(players)) LOGFAIL(errno)

  // unlock client
  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (IBAction)sendMessage:(id)sender {
TRY
  if (sendmessage([[messageTextField stringValue] UTF8String], messageTargetInt)) LOGFAIL(errno)
  [messageTextField selectText:self];

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (IBAction)messageTarget:(id)sender {
  id cell;

  if ((cell = [sender selectedCell]) != nil) {
    [self setMessageTargetInteger:[cell tag]];
  }
}

- (IBAction)showPrefs:(id)sender {
  [preferencesWindow makeKeyAndOrderFront:self];
}

- (IBAction)prefPane:(id)sender {
  [self setPrefPaneIdentifierString:[sender itemIdentifier]];
}

- (IBAction)prefPlayerName:(id)sender {
  [self setPlayerNameString:[sender stringValue]];
}

- (IBAction)revertKeyConfig:(id)sender {
  NSDictionary *dict;
  dict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:GSKeyConfigDict];
  [prefAccelerateField setStringValue:[[dict allKeysForObject:GSAccelerateString] objectAtIndex:0]];
  [prefBrakeField setStringValue:[[dict allKeysForObject:GSBrakeString] objectAtIndex:0]];
  [prefTurnLeftField setStringValue:[[dict allKeysForObject:GSTurnLeftString] objectAtIndex:0]];
  [prefTurnRightField setStringValue:[[dict allKeysForObject:GSTurnRightString] objectAtIndex:0]];
  [prefLayMineField setStringValue:[[dict allKeysForObject:GSLayMineString] objectAtIndex:0]];
  [prefShootField setStringValue:[[dict allKeysForObject:GSShootString] objectAtIndex:0]];
  [prefIncreaseAimField setStringValue:[[dict allKeysForObject:GSIncreaseAimString] objectAtIndex:0]];
  [prefDecreaseAimField setStringValue:[[dict allKeysForObject:GSDecreaseAimString] objectAtIndex:0]];
  [prefUpField setStringValue:[[dict allKeysForObject:GSUpString] objectAtIndex:0]];
  [prefDownField setStringValue:[[dict allKeysForObject:GSDownString] objectAtIndex:0]];
  [prefLeftField setStringValue:[[dict allKeysForObject:GSLeftString] objectAtIndex:0]];
  [prefRightField setStringValue:[[dict allKeysForObject:GSRightString] objectAtIndex:0]];
  [prefTankViewField setStringValue:[[dict allKeysForObject:GSTankViewString] objectAtIndex:0]];
  [prefPillViewField setStringValue:[[dict allKeysForObject:GSPillViewString] objectAtIndex:0]];
  [prefAutoSlowdownSwitch setState:[[NSUserDefaults standardUserDefaults] boolForKey:GSAutoSlowdownBool] ? NSOnState : NSOffState];
}

- (IBAction)applyKeyConfig:(id)sender {
  NSMutableDictionary *dict;
  dict = [NSMutableDictionary dictionary];
  if
    (
      !(
        setKey(dict, preferencesWindow, prefAccelerateField, GSAccelerateString) ||
        setKey(dict, preferencesWindow, prefBrakeField, GSBrakeString) ||
        setKey(dict, preferencesWindow, prefTurnLeftField, GSTurnLeftString) ||
        setKey(dict, preferencesWindow, prefTurnRightField, GSTurnRightString) ||
        setKey(dict, preferencesWindow, prefLayMineField, GSLayMineString) ||
        setKey(dict, preferencesWindow, prefShootField, GSShootString) ||
        setKey(dict, preferencesWindow, prefIncreaseAimField, GSIncreaseAimString) ||
        setKey(dict, preferencesWindow, prefDecreaseAimField, GSDecreaseAimString) ||
        setKey(dict, preferencesWindow, prefUpField, GSUpString) ||
        setKey(dict, preferencesWindow, prefDownField, GSDownString) ||
        setKey(dict, preferencesWindow, prefLeftField, GSLeftString) ||
        setKey(dict, preferencesWindow, prefRightField, GSRightString) ||
        setKey(dict, preferencesWindow, prefTankViewField, GSTankViewString) ||
        setKey(dict, preferencesWindow, prefPillViewField, GSPillViewString)
      )
    )
  {
    [self setKeyConfigDict:dict];
    [self setAutoSlowdownBool:[prefAutoSlowdownSwitch state] == NSOnState];
  }
}

// map interface actions

- (IBAction)zoomIn:(id)sender {
  NSSize size;
  NSRect visRect;

  if (zoomLevel < MAX_ZOOM) {
    zoomLevel++;
    float zoom = kZoomLevels[zoomLevel];
    visRect = [boloView visibleRect];
    size.width = 4096.0*zoom;
    size.height = 4096.0*zoom;
    [boloView setFrameSize:size];
    [boloView setBoundsSize:NSMakeSize(4096.0, 4096.0)];
    [boloView scrollPoint:NSMakePoint(visRect.origin.x + 0.25*visRect.size.width, visRect.origin.y + 0.25*visRect.size.height)];
    [boloView setNeedsDisplay:YES];
  }
}

- (IBAction)zoomOut:(id)sender {
  NSSize size;
  NSRect visRect;

  if (zoomLevel > 0) {
    zoomLevel--;
    float zoom = kZoomLevels[zoomLevel];
    visRect = [boloView visibleRect];
    size.width = 4096.0*zoom;
    size.height = 4096.0*zoom;
    [boloView setFrameSize:size];
    [boloView setBoundsSize:NSMakeSize(4096.0, 4096.0)];
    [boloView scrollPoint:NSMakePoint(visRect.origin.x - 0.5*visRect.size.width, visRect.origin.y - 0.5*visRect.size.height)];
    [boloView setNeedsDisplay:YES];
  }
}

- (IBAction)builderTool:(id)sender {
  id cell;

  if ((cell = [sender selectedCell]) != nil) {
    [self setBuilderToolInteger:[cell tag]];
  }
}

- (IBAction)builderToolMenu:(id)sender {
	[self setBuilderToolInteger:[sender tag]];
}

- (IBAction)tankCenter:(id)sender {
  NSRect rect;
  int gotlock = 0;

TRY
  rect = [boloView visibleRect];

  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  rect.origin.x = ((client.players[client.player].tank.x + 0.5)*16.0) - rect.size.width*0.5;
  rect.origin.y = ((FWIDTH - (client.players[client.player].tank.y + 0.5))*16.0) - rect.size.height*0.5;

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

  [boloView scrollRectToVisible:rect];

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (IBAction)pillCenter:(id)sender {
  Pointi square;
  NSRect rect;
  int i, j;
  int gotlock = 0;

TRY
  rect = [boloView visibleRect];
  square.x = (rect.origin.x + rect.size.width*0.5)/16.0;
  square.y = FWIDTH - ((rect.origin.y + rect.size.height*0.5)/16.0);

  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  for (i = 0; i < client.npills; i++) {
    if (
        client.pills[i].owner == client.player &&
        client.pills[i].armour != ONBOARD && client.pills[i].armour != 0 &&
        square.x == client.pills[i].x && square.y == client.pills[i].y
        ) {
      for (j = (i + 1)%client.npills; j != i; j = (j + 1)%client.npills) {
        if (
            client.pills[j].owner == client.player &&
            client.pills[j].armour != ONBOARD && client.pills[j].armour != 0
            ) {
          square.x = client.pills[j].x;
          square.y = client.pills[j].y;
          if (unlockclient()) LOGFAIL(errno)
          gotlock = 0;
          rect.origin.x = ((square.x + 0.5)*16.0) - rect.size.width*0.5;
          rect.origin.y = ((FWIDTH - (square.y + 0.5))*16.0) - rect.size.height*0.5;
          SUCCESS
        }
      }
      
      square.x = client.pills[i].x;
      square.y = client.pills[i].y;
      if (unlockclient()) LOGFAIL(errno)
      gotlock = 0;
      rect.origin.x = ((square.x + 0.5)*16.0) - rect.size.width*0.5;
      rect.origin.y = ((FWIDTH - (square.y + 0.5))*16.0) - rect.size.height*0.5;
      SUCCESS
    }
  }
  
  for (i = 0; i < client.npills; i++) {
    if (
        client.pills[i].owner == client.player &&
        client.pills[i].armour != ONBOARD && client.pills[i].armour != 0
        ) {
      square.x = client.pills[i].x;
      square.y = client.pills[i].y;
      if (unlockclient()) LOGFAIL(errno)
      gotlock = 0;
      rect.origin.x = ((square.x + 0.5)*16.0) - rect.size.width*0.5;
      rect.origin.y = ((FWIDTH - (square.y + 0.5))*16.0) - rect.size.height*0.5;
      SUCCESS
    }
  }

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    [boloView scrollRectToVisible:rect];
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

// key event methods
- (void)keyEvent:(BOOL)event forKey:(unsigned short)keyCode {
  NSString *object;

TRY
  object = [keyConfigDict objectForKey:[NSString stringWithFormat:@"%d", keyCode]];
  if (object != nil) {
    if ([object isEqualToString:GSAccelerateString]) {
      if (autoSlowdownBool) {
        if (keyevent(ACCELMASK, event) == -1) LOGFAIL(errno)
        if (keyevent(BRAKEMASK, !event) == -1) LOGFAIL(errno)
      }
      else {
        if (keyevent(ACCELMASK, event) == -1) LOGFAIL(errno)
      }
    }
    else if ([object isEqualToString:GSBrakeString]) {
      if (!autoSlowdownBool) {
        if (keyevent(BRAKEMASK, event) == -1) LOGFAIL(errno)
      }
    }
    else if ([object isEqualToString:GSTurnLeftString]) {
      if (keyevent(TURNLMASK, event) == -1) LOGFAIL(errno)
    }
    else if ([object isEqualToString:GSTurnRightString]) {
      if (keyevent(TURNRMASK, event) == -1) LOGFAIL(errno)
    }
    else if ([object isEqualToString:GSLayMineString]) {
      if (keyevent(LMINEMASK, event) == -1) LOGFAIL(errno)
    }
    else if ([object isEqualToString:GSShootString]) {
      if (keyevent(SHOOTMASK, event) == -1) LOGFAIL(errno)
    }
    else if ([object isEqualToString:GSIncreaseAimString]) {
      if (keyevent(INCREMASK, event) == -1) LOGFAIL(errno)
    }
    else if ([object isEqualToString:GSDecreaseAimString]) {
      if (keyevent(DECREMASK, event) == -1) LOGFAIL(errno)
    }
    else if ([object isEqualToString:GSUpString]) {
      if (event) {
        [self scrollUp:self];
      }
    }
    else if ([object isEqualToString:GSDownString]) {
      if (event) {
        [self scrollDown:self];
      }
    }
    else if ([object isEqualToString:GSLeftString]) {
      if (event) {
        [self scrollLeft:self];
      }
    }
    else if ([object isEqualToString:GSRightString]) {
      if (event) {
        [self scrollRight:self];
      }
    }
    else if ([object isEqualToString:GSTankViewString]) {
      if (event) {
        [self tankCenter:self];
      }
    }
    else if ([object isEqualToString:GSPillViewString]) {
      if (event) {
        [self pillCenter:self];
      }
    }
  }
  else if (keyCode == 18 && event)
    [self setBuilderToolInteger: 0];
  else if (keyCode == 19 && event)
    [self setBuilderToolInteger: 1];
  else if (keyCode == 20 && event)
    [self setBuilderToolInteger: 2];
  else if (keyCode == 21 && event)
    [self setBuilderToolInteger: 3];
  else if (keyCode == 23 && event)
    [self setBuilderToolInteger: 4];

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

// mouse event method
- (void)mouseEvent:(Pointi)point {
  int gotlock = 0;

TRY
  if (ispointinrect(makerect(0, 0, WIDTH, WIDTH), point)) {
    // lock client
    if (lockclient()) LOGFAIL(errno)
    gotlock = 1;

    buildercommand(builderToolInt, point);

    // unlock client
    if (unlockclient()) LOGFAIL(errno)
    gotlock = 0;
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

// validate menu items method

- (BOOL)validateMenuItem:(id)menuItem {
  SEL action;
  action = [menuItem action];
	if (action == @selector(newGame:)) {
    return !client_running;
	}
	else if (action == @selector(closeGame:)) {
    return client_running;
	}
	else if (action == @selector(statusPanel:)) {
		return client_running;
	}
  else if (action == @selector(allegiancePanel:)) {
    return client_running;
  }
	else if (action == @selector(messagesPanel:)) {
		return client_running;
	}
	else if (action == @selector(hostGameType:)) {
    switch ([menuItem tag]) {
    case kDominationGameType:  // Domination
      return YES;

    case kCTFGameType:  // Capture the Flag
      return NO;

    case kKOTHGameType:  // King of the Hill
      return NO;

    case kBallGameType:  // Kill the Man with the Ball
      return NO;

    case kBodyGameType:  // Body Count
      return NO;

    default:
      return NO;
    }
	}
	else if (action == @selector(builderToolMenu:)) {
		[menuItem setState:[menuItem tag] == builderToolInt ? NSOnState : NSOffState];
		return YES;
	}
	else if (action == @selector(gamePauseResumeMenu:)) {
    lockserver();

    if (server.setup) {
      [menuItem setState:server.pause ? NSOnState: NSOffState];
      unlockserver();
      return YES;
    }
    else {
      unlockserver();
      return NO;
    }
  }
	else if (action == @selector(toggleJoin:)) {
    lockserver();

    if (server.setup) {
      [menuItem setState:server.allowjoin ? NSOnState: NSOffState];
      unlockserver();
      return YES;
    }
    else {
      unlockserver();
      return NO;
    }
  }
	else if (action == @selector(toggleMute:)) {
    [menuItem setState:muteBool];
    return YES;
  }
	else if (action == @selector(kickPlayer:)) {
    lockserver();

    if (server.setup) {
      int player, ret;

      player = [menuItem tag];

      if (server.players[player].cntlsock != -1) {
        [menuItem setTitle:[NSString stringWithFormat:@"%@@%@", [NSString stringWithString:[NSString stringWithCString:server.players[player].name encoding:NSUTF8StringEncoding]], [NSString stringWithString:[NSString stringWithCString:inet_ntoa(server.players[player].addr.sin_addr) encoding:NSASCIIStringEncoding]]]];
        lockclient();
        ret = player != client.player;
        unlockclient();
      }
      else {
        ret = NO;
      }

      unlockserver();
      [menuItem setHidden:!ret];
      return ret;
    }
    else {
      unlockserver();
      [menuItem setHidden:YES];
      return NO;
    }
    return YES;
  }
	else if (action == @selector(banPlayer:)) {
    lockserver();

    if (server.setup) {
      int player, ret;

      player = [menuItem tag];

      if (server.players[player].cntlsock != -1) {
        [menuItem setTitle:[NSString stringWithFormat:@"%@@%@", [NSString stringWithString:[NSString stringWithCString:server.players[player].name encoding:NSUTF8StringEncoding]], [NSString stringWithString:[NSString stringWithCString:inet_ntoa(server.players[player].addr.sin_addr) encoding:NSASCIIStringEncoding]]]];
        lockclient();
        ret = player != client.player;
        unlockclient();
      }
      else {
        ret = NO;
      }

      unlockserver();
      [menuItem setHidden:!ret];
      return ret;
    }
    else {
      unlockserver();
      [menuItem setHidden:YES];
      return NO;
    }
  }
    else if (action == @selector(choseRobotMenuItem:)) return [self _validateRobotMenuItem: menuItem];
	else {
		return YES;
	}
}

// NSApplication delegate methods

- (void)applicationDidBecomeActive:(NSNotification *)aNotification {
  [statusPanel setFloatingPanel:YES];
  [allegiancePanel setFloatingPanel:YES];
  [messagesPanel setFloatingPanel:YES];
}

- (void)applicationWillResignActive:(NSNotification *)aNotification {
  [statusPanel setFloatingPanel:NO];
  [allegiancePanel setFloatingPanel:NO];
  [messagesPanel setFloatingPanel:NO];
}

- (BOOL)applicationOpenUntitledFile:(NSApplication *)theApplication {
  if (!client_running) {
    [newGameWindow makeKeyAndOrderFront:self];
    if([[NSUserDefaults standardUserDefaults] boolForKey: @"GSStartGameImmediately"])
      [self hostOK:nil];
    else
    {
        NSString *tojoin = [[NSUserDefaults standardUserDefaults] objectForKey: @"GSJoinGameImmediately"];
        if(tojoin)
        {
            NSArray *components = [tojoin componentsSeparatedByString: @":"];
            [self setJoinAddressString: [components objectAtIndex: 0]];
            [self setJoinPortNumber: [[components objectAtIndex: 1] intValue]];
            [self joinOK: nil];
        }
    }
    return YES;
  }
  else {
    return NO;
  }
}

// host with a map file

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename {
  if (!client_running) {
    [self setHostMapString:filename];
    [newGameTabView selectFirstTabViewItem:self];
    [newGameWindow makeKeyAndOrderFront:self];
    return YES;
  }
  else {
    return NO;
  }
}

// NSMenu delegate methods
- (void)menuNeedsUpdate:(NSMenu *)menu {
  int i;

  for (i = [menu numberOfItems] - 1; i >= 0; i--) {
    [menu removeItemAtIndex:i];
  }

  lockserver();

  if (server.setup) {
    struct ListNode *node;
    int i;
    struct BannedPlayer *bannedplayer;
    NSMenuItem *menuItem;

    i = 0;
    node = nextlist(&server.bannedplayers);

    while (node) {
      bannedplayer = ptrlist(node);
      menuItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"%@@%@", [NSString stringWithString:[NSString stringWithCString:bannedplayer->name encoding:NSUTF8StringEncoding]], [NSString stringWithString:[NSString stringWithCString:inet_ntoa(bannedplayer->sin_addr) encoding:NSASCIIStringEncoding]]] action:@selector(unbanPlayer:) keyEquivalent:@""];
      [menuItem setTarget:self];
      [menuItem setTag:i];
      [menu addItem:menuItem];
      [menuItem release];
      i++;
      node = nextlist(node);
    }
  }

  unlockserver();
}

// NSSheet delegate methods

- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
  if (returnCode == NSOKButton) {
    [self setHostMapString:[[sheet filenames] objectAtIndex:0]];
  }
}

// NSToolbar methods

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
  if (toolbar == prefToolbar) {
    if ([itemIdentifier isEqual:GSToolbarPlayerInfoItemIdentifier]) {
      return toolbarPlayerInfoItem;
    }
    else if ([itemIdentifier isEqual:GSToolbarKeyConfigItemIdentifier]) {
      return toolbarKeyConfigItem;
    }
  }
  else if (toolbar == boloToolbar) {
    if ([itemIdentifier isEqual:GSBoloToolItemIdentifier]) {
      return builderToolItem;
    }
    else if ([itemIdentifier isEqual:GSTankCenterItemIdentifier]) {
      return tankCenterItem;
    }
    else if ([itemIdentifier isEqual:GSPillCenterItemIdentifier]) {
      return pillCenterItem;
    }
    else if ([itemIdentifier isEqual:GSZoomInItemIdentifier]) {
      return zoomInItem;
    }
    else if ([itemIdentifier isEqual:GSZoomOutItemIdentifier]) {
      return zoomOutItem;
    }
  }
  return nil;
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  if (toolbar == prefToolbar) {
    return [NSArray arrayWithObjects:GSToolbarPlayerInfoItemIdentifier, GSToolbarKeyConfigItemIdentifier, nil];
  }
  else if (toolbar == boloToolbar) {
    return [NSArray arrayWithObjects:GSBoloToolItemIdentifier, GSTankCenterItemIdentifier, GSPillCenterItemIdentifier, GSZoomInItemIdentifier, GSZoomOutItemIdentifier, NSToolbarSeparatorItemIdentifier, NSToolbarSpaceItemIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
  }
  else {
    return [NSArray array];
  }
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  if (toolbar == prefToolbar) {
    return [NSArray arrayWithObjects:GSToolbarPlayerInfoItemIdentifier, GSToolbarKeyConfigItemIdentifier, nil];
  }
  else if (toolbar == boloToolbar) {
    return [NSArray arrayWithObjects:GSBoloToolItemIdentifier, NSToolbarFlexibleSpaceItemIdentifier, GSTankCenterItemIdentifier, GSPillCenterItemIdentifier, NSToolbarSeparatorItemIdentifier, GSZoomInItemIdentifier, GSZoomOutItemIdentifier, nil];
  }
  else {
    return [NSArray array];
  }
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
  if (toolbar == prefToolbar) {
    return [NSArray arrayWithObjects:GSToolbarPlayerInfoItemIdentifier, GSToolbarKeyConfigItemIdentifier, nil];
  }
  else if (toolbar == boloToolbar) {
    return [NSArray array];
  }
  else {
    return [NSArray array];
  }
}

// NSWindow delegate methods
- (void)windowDidBecomeKey:(NSNotification *)aNotification {
  if ([aNotification object] == statusPanel) {
    [self setShowStatusBool:YES];
  }
  else if ([aNotification object] == allegiancePanel) {
    [self setShowAllegianceBool:YES];
  }
  else if ([aNotification object] == messagesPanel) {
    [self setShowMessagesBool:YES];
  }
}

- (void)windowWillClose:(NSNotification *)aNotification {
  if ([aNotification object] == statusPanel) {
    [self setShowStatusBool:NO];
  }
  else if ([aNotification object] == allegiancePanel) {
    [self setShowAllegianceBool:NO];
  }
  else if ([aNotification object] == messagesPanel) {
    [self setShowMessagesBool:NO];
  }
}

- (void)setPlayerStatus:(NSString *)aString {
  NSEnumerator *enumerator;
  NSAttributedString *name;
  NSMutableDictionary *playerRecord;
  NSString *indexString;
  NSImageView *imageView;
  int gotlock = 0;
  int player;

TRY
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  player = [aString intValue];

  switch (player) {
  case 0x0:
    imageView = player0StatusImageView;
    break;

  case 0x1:
    imageView = player1StatusImageView;
    break;

  case 0x2:
    imageView = player2StatusImageView;
    break;

  case 0x3:
    imageView = player3StatusImageView;
    break;

  case 0x4:
    imageView = player4StatusImageView;
    break;

  case 0x5:
    imageView = player5StatusImageView;
    break;

  case 0x6:
    imageView = player6StatusImageView;
    break;

  case 0x7:
    imageView = player7StatusImageView;
    break;

  case 0x8:
    imageView = player8StatusImageView;
    break;

  case 0x9:
    imageView = player9StatusImageView;
    break;

  case 0xa:
    imageView = playerAStatusImageView;
    break;

  case 0xb:
    imageView = playerBStatusImageView;
    break;

  case 0xc:
    imageView = playerCStatusImageView;
    break;

  case 0xd:
    imageView = playerDStatusImageView;
    break;

  case 0xe:
    imageView = playerEStatusImageView;
    break;

  case 0xf:
    imageView = playerFStatusImageView;
    break;

  default:
    imageView = nil;
    break;
  }

  indexString = [NSString stringWithFormat:@"%d", player];

  if (client.players[player].connected) {
    enumerator = [playerInfoArray objectEnumerator];

    while ((playerRecord = [enumerator nextObject])) {
      if ([[playerRecord objectForKey:@"Index"] isEqualToString:indexString]) {
        break;
      }
    }

    if (playerRecord == nil) {
      playerRecord = [[NSMutableDictionary alloc] init];
      [playerInfoArray addObject:playerRecord];
      [playerRecord release];
    }

    [playerRecord setObject:indexString forKey:@"Index"];

    if (client.players[client.player].seq - client.players[player].lastupdate >= 3*TICKSPERSEC) {
      name = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:client.players[player].name] attributes:[NSDictionary dictionaryWithObjectsAndKeys:[NSColor whiteColor], NSForegroundColorAttributeName, [NSColor redColor], NSBackgroundColorAttributeName, nil]];
    }
    else if (client.players[client.player].seq - client.players[player].lastupdate >= TICKSPERSEC) {
      name = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:client.players[player].name] attributes:[NSDictionary dictionaryWithObjectsAndKeys:[NSColor yellowColor], NSBackgroundColorAttributeName, nil]];
    }
    else {
      name = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:client.players[player].name] attributes:[NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSBackgroundColorAttributeName, nil]];
    }

    [playerRecord setObject:name forKey:@"Player"];
    [name release];

    if (client.player == player) {
      [imageView setImage:[NSImage imageNamed:@"PlayerStatFriendly"]];
    }
    else if (
      (client.players[client.player].alliance & (1 << player)) &&
      (client.players[player].alliance & (1 << client.player))
    ) {
      [imageView setImage:[NSImage imageNamed:@"PlayerStatAlliedFriendly"]];
    }
    else {
      [imageView setImage:[NSImage imageNamed:@"PlayerStatHostile"]];
    }
  }
  else {
    enumerator = [playerInfoArray objectEnumerator];

    while ((playerRecord = [enumerator nextObject])) {
      if ([[playerRecord objectForKey:@"Index"] isEqualToString:indexString]) {
        [playerInfoArray removeObject:playerRecord];
        break;
      }
    }

    [imageView setImage:nil];
  }

  [playerInfoTableView reloadData];

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)setPillStatus:(NSString *)aString {
  NSImageView *imageView;
  int gotlock = 0;
  int pill;

TRY
  pill = [aString intValue];

  switch (pill) {
  case 0x0:
    imageView = pill0StatusImageView;
    break;

  case 0x1:
    imageView = pill1StatusImageView;
    break;

  case 0x2:
    imageView = pill2StatusImageView;
    break;

  case 0x3:
    imageView = pill3StatusImageView;
    break;

  case 0x4:
    imageView = pill4StatusImageView;
    break;

  case 0x5:
    imageView = pill5StatusImageView;
    break;

  case 0x6:
    imageView = pill6StatusImageView;
    break;

  case 0x7:
    imageView = pill7StatusImageView;
    break;

  case 0x8:
    imageView = pill8StatusImageView;
    break;

  case 0x9:
    imageView = pill9StatusImageView;
    break;

  case 0xa:
    imageView = pillAStatusImageView;
    break;

  case 0xb:
    imageView = pillBStatusImageView;
    break;

  case 0xc:
    imageView = pillCStatusImageView;
    break;

  case 0xd:
    imageView = pillDStatusImageView;
    break;

  case 0xe:
    imageView = pillEStatusImageView;
    break;

  case 0xf:
    imageView = pillFStatusImageView;
    break;

  default:
    imageView = nil;
    break;
  }

  if (lockclient()) LOGFAIL(errno)

  if (pill < client.npills) {
    switch (client.pills[pill].armour) {
    case 0:
      [imageView setImage:[NSImage imageNamed:@"PillStatDead"]];
      break;

    case ONBOARD:
      if (client.pills[pill].owner == client.player) {
        [imageView setImage:[NSImage imageNamed:@"PillStatIntFriendly"]];
      }
      else if (
        client.pills[pill].owner != NEUTRAL &&
        (
          (client.players[client.pills[pill].owner].alliance & (1 << client.player)) &&
          (client.players[client.player].alliance & (1 << client.pills[pill].owner))
        )
      ) {
        [imageView setImage:[NSImage imageNamed:@"PillStatIntAllied"]];
      }
      else {
        [imageView setImage:[NSImage imageNamed:@"PillStatIntHostile"]];
      }

      break;

    default:
      if (client.pills[pill].owner == client.player) {
        [imageView setImage:[NSImage imageNamed:@"PillStatFriendly"]];
      }
      else if (client.pills[pill].owner == NEUTRAL) {
        [imageView setImage:[NSImage imageNamed:@"PillStatNeutral"]];
      }
      else if (
        client.pills[pill].owner != NEUTRAL &&
        (
          (client.players[client.pills[pill].owner].alliance & (1 << client.player)) &&
          (client.players[client.player].alliance & (1 << client.pills[pill].owner))
        )
      ) {
        [imageView setImage:[NSImage imageNamed:@"PillStatFriendly"]];
      }
      else {
        [imageView setImage:[NSImage imageNamed:@"PillStatHostile"]];
      }

      break;
    }
  }
  else {
    [imageView setImage:nil];
  }

  if (unlockclient()) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)setBaseStatus:(NSString *)aString {
  NSImageView *imageView;
  int gotlock = 0;
  int base;

TRY
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  base = [aString intValue];

  switch (base) {
  case 0x0:
    imageView = base0StatusImageView;
    break;

  case 0x1:
    imageView = base1StatusImageView;
    break;

  case 0x2:
    imageView = base2StatusImageView;
    break;

  case 0x3:
    imageView = base3StatusImageView;
    break;

  case 0x4:
    imageView = base4StatusImageView;
    break;

  case 0x5:
    imageView = base5StatusImageView;
    break;

  case 0x6:
    imageView = base6StatusImageView;
    break;

  case 0x7:
    imageView = base7StatusImageView;
    break;

  case 0x8:
    imageView = base8StatusImageView;
    break;

  case 0x9:
    imageView = base9StatusImageView;
    break;

  case 0xa:
    imageView = baseAStatusImageView;
    break;

  case 0xb:
    imageView = baseBStatusImageView;
    break;

  case 0xc:
    imageView = baseCStatusImageView;
    break;

  case 0xd:
    imageView = baseDStatusImageView;
    break;

  case 0xe:
    imageView = baseEStatusImageView;
    break;

  case 0xf:
    imageView = baseFStatusImageView;
    break;

  default:
    assert(0);
    break;
  }

  if (base < client.nbases) {
    if (client.bases[base].owner == NEUTRAL) {
      [imageView setImage:[NSImage imageNamed:@"BaseStatNeutral"]];
    }
    else if (client.bases[base].armour < MINBASEARMOUR) {
      [imageView setImage:[NSImage imageNamed:@"BaseStatDead"]];
    }
    else if (client.bases[base].owner == client.player) {
      [imageView setImage:[NSImage imageNamed:@"BaseStatFriendly"]];
    }
    else if (
      client.bases[base].owner != NEUTRAL &&
      (
        (client.players[client.bases[base].owner].alliance & (1 << client.player)) &&
        (client.players[client.player].alliance & (1 << client.bases[base].owner))
      )
    ) {
      [imageView setImage:[NSImage imageNamed:@"BaseStatFriendly"]];
    }
    else {
      [imageView setImage:[NSImage imageNamed:@"BaseStatHostile"]];
    }
  }
  else {
    [imageView setImage:nil];
  }

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)setTankStatusBars {
  [playerKillsTextField setStringValue:[NSString stringWithFormat:@"%d", client.kills]];
  [playerDeathsTextField setStringValue:[NSString stringWithFormat:@"%d", client.deaths]];
  [playerShellsStatusBar setValue:((float)client.shells)/MAXSHELLS];
  [playerMinesStatusBar setValue:((float)client.mines)/MAXMINES];
  [playerArmourStatusBar setValue:((float)client.armour)/MAXARMOUR];
  [playerTreesStatusBar setValue:((float)client.trees)/MAXTREES];
}

- (void)refresh:(NSTimer *)aTimer {
  int i;
  int base = -1;
  float dist = 8.0, dist2;
  int gotlock = 0;
  static int counter = 0;

TRY
  if (client_running) {
    counter++;

    if (lockclient()) LOGFAIL(errno)
    gotlock = 1;

    if (client.player != -1) {
      /* make sure tank is visible if just spawned */
      int centerTank = 0;
      if (client.spawned) {
        if(client.deaths == 0)
          centerTank = 1; // center it if this is the very first spawn
        else
        {
          NSRect rect;
          rect.size.width = rect.size.height = 16 * 16 * kZoomLevels[zoomLevel];
          rect.origin.x = ((client.players[client.player].tank.x + 0.5)*16.0) - rect.size.width*0.5;
          rect.origin.y = ((FWIDTH - (client.players[client.player].tank.y + 0.5))*16.0) - rect.size.height*0.5;

          if (unlockclient()) LOGFAIL(errno)
          gotlock = 0;

          [boloView scrollRectToVisible:rect];  /* potential to call drawRect: which locks client */
        }
        if (unlockclient()) LOGFAIL(errno)
        gotlock = 1;

        client.spawned = 0;
      }
      if(centerTank)
        [self tankCenter:nil];
      [GSBoloView refresh];

      if (counter%2) {
        switch (client.players[client.player].builderstatus) {
        case kBuilderGoto:
        case kBuilderWork:
        case kBuilderWait:
        case kBuilderReturn:
          {
            Vec2f diff;
            float newdir;
          
            diff = sub2f(client.players[client.player].builder, client.players[client.player].tank);
            newdir = vec2dir(diff);
          
            [builderStatusView setState:GSBuilderStatusViewStateDirection];
            [builderStatusView setDir:newdir];
          }
          
          break;
          
        case kBuilderParachute:
          [builderStatusView setState:GSBuilderStatusViewStateDead];
          break;
          
        case kBuilderReady:
          [builderStatusView setState:GSBuilderStatusViewStateReady];
          break;
          
        default:
          assert(0);
          break;
        }
      
        // update base status bars
        for (i = 0; i < client.nbases; i++) {
          if (
              client.bases[i].owner != NEUTRAL &&
              (client.players[client.bases[i].owner].alliance & (1 << client.player)) &&
              (client.players[client.player].alliance & (1 << client.bases[i].owner))
              ) {
            dist2 = mag2f(sub2f(client.players[client.player].tank, make2f(client.bases[i].x + 0.5, client.bases[i].y + 0.5)));
          
            if (dist2 < dist) {
              base = i;
              dist = dist2;
            }
          }
        }
      
        if (base != -1) {
          [baseArmourStatusBar setValue:(float)client.bases[base].armour/(float)MAXBASEARMOUR];
          [baseShellsStatusBar setValue:(float)client.bases[base].shells/(float)MAXBASESHELLS];
          [baseMinesStatusBar setValue:(float)client.bases[base].mines/(float)MAXBASEMINES];
        }
        else {
          [baseShellsStatusBar setValue:0.0];
          [baseMinesStatusBar setValue:0.0];
          [baseArmourStatusBar setValue:0.0];
        }
      }
    }

    if (unlockclient()) LOGFAIL(errno)
    gotlock = 0;
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)printMessage:(NSAttributedString *)aString {
  NSAttributedString *newline;

  newline = [[NSAttributedString alloc] initWithString:@"\n"];
  [[messagesTextView textStorage] appendAttributedString:newline];
  [[messagesTextView textStorage] appendAttributedString:aString];
  [messagesTextView didChangeText];
  [messagesTextView scrollPoint:NSMakePoint(0.0f, NSHeight([messagesTextView frame]) - NSHeight([messagesTextView visibleRect]))];

//  int i;
//  if (!muteBool) {
//    for (i = 0; i < [speechSynthesizers count]; i++) {
//      NSSpeechSynthesizer *speechSynthesizer;
//
//      speechSynthesizer = [speechSynthesizers objectAtIndex:i];
//
//      if (![speechSynthesizer isSpeaking]) {
//        [speechSynthesizer startSpeakingString:[aString string]];
//        break;
//      }
//    }
//  }
	
  [newline release];
}


// NSTableView delegate methods

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
  NSTableView *table;
  int i;
  NSDictionary *row;

  table = [aNotification object];

  if (table == playerInfoTableView) {
    if ([table numberOfSelectedRows] == 0) {
      [requestAllianceButton setEnabled:FALSE];
      [leaveAllianceButton setEnabled:FALSE];
    }
    else {
      [requestAllianceButton setEnabled:TRUE];
      [leaveAllianceButton setEnabled:TRUE];
    }
  }
  else if (table == joinTrackerTableView) {
    i = [[table selectedRowIndexes] firstIndex];

    if (i != NSNotFound) {
      row = [joinTrackerArray objectAtIndex:i];
      [self setJoinAddressString:[row objectForKey:GSHostnameColumn]];
      [self setJoinPortNumber:[[row objectForKey:GSPortColumn] intValue]];
    }
  }
}


// NSTableView dataSource methods

- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  if (aTableView == playerInfoTableView) {
    return [playerInfoArray count];
  }
  else if (aTableView == joinTrackerTableView) {
    return [joinTrackerArray count];
  }
  else {
    return 0;
  }
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex {
  if (aTableView == playerInfoTableView) {
    NSParameterAssert(rowIndex >= 0 && rowIndex < [playerInfoArray count]);
    return [[playerInfoArray objectAtIndex:rowIndex] objectForKey:[aTableColumn identifier]];
  }
  else if (aTableView == joinTrackerTableView) {
    NSParameterAssert(rowIndex >= 0 && rowIndex < [joinTrackerArray count]);
    return [[joinTrackerArray objectAtIndex:rowIndex] objectForKey:[aTableColumn identifier]];
  }
  else {
    return nil;
  }
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex {
  if (aTableView == playerInfoTableView) {
    NSParameterAssert(rowIndex >= 0 && rowIndex < [playerInfoArray count]);
    [[playerInfoArray objectAtIndex:rowIndex] setObject:anObject forKey:[aTableColumn identifier]];
  }
}

- (void)tableView:(NSTableView *)aTableView sortDescriptorsDidChange:(NSArray *)oldDescriptors {
  if (aTableView == joinTrackerTableView) {  /* sorts joinTrackerArray */
    int index;
    NSDictionary *selection;

    index = [[aTableView selectedRowIndexes] firstIndex];

    if (index != NSNotFound) {
      selection = [joinTrackerArray objectAtIndex:index];
    }

    [joinTrackerArray sortUsingDescriptors:[aTableView sortDescriptors]];
    [aTableView reloadData];

    if (index != NSNotFound) {
      [aTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:[joinTrackerArray indexOfObject:selection]] byExtendingSelection:NO];
    }
  }
}

//

- (void)setJoinProgressStatusTextField:(NSString *)aString {
  [joinProgressStatusTextField setStringValue:aString];
}

- (void)setJoinProgressIndicator:(NSString *)aString {
  [joinProgressIndicator setIndeterminate:NO];
  [joinProgressIndicator setDoubleValue:[aString doubleValue]];
}

- (void)joinSuccess {
  [boloView setNeedsDisplay:YES];
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  [newGameWindow orderOut:self];
  [boloWindow makeKeyAndOrderFront:self];

  if (showStatusBool) {
    [statusPanel orderFront:self];
  }

  if (showAllegianceBool) {
    [allegiancePanel orderFront:self];
  }

  [[messagesTextView textStorage] deleteCharactersInRange:NSMakeRange(0, [[messagesTextView textStorage] length])];

  if (showMessagesBool) {
    [messagesPanel orderFront:self];
  }
}

- (void)joinDNSError:(NSString *)eString {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Error Resolving Hostname", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, eString);
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinTimeOut {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Network Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Connection establishment timed out without establishing a connection.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinConnectionRefused {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Connection Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"The attempt to connect was forcefully rejected.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinNetUnreachable {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Network Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"The network is not reachable from this host.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinHostUnreachable {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Network Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"The remote host is not reachable from this host.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinBadVersion {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Server version doesn't match.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinDisallow {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Host is not allowing new players in the game.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinBadPassword {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Password rejected.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinServerFull {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Server is full.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinTimeLimit {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Time limit reached on server.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinBannedPlayer {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Host has banned you from the game.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinServerError {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Protocol error.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)joinConnectionReset {
TRY
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Server Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Connection Reset by Peer.");
  if (stopclient()) LOGFAIL(errno)

  if (server.setup) {
    if (stopserver()) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerSuccess {
TRY
  if (startclient("localhost", getservertcpport(), [playerNameString UTF8String], hostPasswordBool ? [hostPasswordString UTF8String] : NULL)) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)trackerDNSError:(NSString *)eString {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Error Resolving Tracker", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, eString);

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerTimeOut {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Timed Out", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Could not connect to the tracker.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerConnectionRefused {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Connection Refused", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Connection was refused by the tracker.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerHostDown {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Host Down", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Tracker is not responding.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerHostUnreachable {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Host Unreachable", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Check your network connectivity.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerBadVersion {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Incompatible Version", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Tracker version doesn't match.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerTCPPortBlocked {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Failed to Verify TCP Port Forwarded", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Try setting up port forwarding in your router.  If you have port forwarding setup in your router, uncheck UPnP and try again.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerUDPPortBlocked {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Failed to Verify UDP Port Forwarded", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Try setting up port forwarding in your router.  If you have port forwarding setup in your router, uncheck UPnP and try again.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)registerTrackerConnectionReset {
TRY
  if (stopserver()) LOGFAIL(errno)
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Connection Error", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Connection Reset by Peer.");

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }
END
}

- (void)getListTrackerTimeOut {
  stoptracker();
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Timed Out", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Could not connect to the tracker.");
}

- (void)getListTrackerConnectionRefused {
  stoptracker();
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Connection Refused", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Connection was refused by the tracker.");
}

- (void)getListTrackerHostDown {
  stoptracker();
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Host Down", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Tracker is not responding.");
}

- (void)getListTrackerHostUnreachable {
  stoptracker();
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Tracker Host Unreachable", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Check your network connectivity.");
}

- (void)getListTrackerBadVersion {
  stoptracker();
  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];
  NSBeginAlertSheet(@"Incompatible Version", @"OK", nil, nil, newGameWindow, self, nil, nil, nil, @"Tracker version doesn't match.");
}

- (void)getListTrackerSuccess {
  NSMutableArray *table;
  NSArray *keys, *objects;
  struct ListNode *node;

  stoptracker();

  [joinProgressWindow orderOut:self];
  [joinProgressIndicator stopAnimation:self];
  [NSApp endSheet:joinProgressWindow];

  // convert list to NSArray for displaying
  table = [NSMutableArray array];
  keys = [NSArray arrayWithObjects:GSHostPlayerColumn, GSMapNameColumn, GSPlayersColumn, GSPasswordColumn, GSAllowJoinColumn, GSPausedColumn, GSHostnameColumn, GSPortColumn, nil];

  for (node = nextlist(&trackerlist); node; node = nextlist(node)) {
    struct TrackerHostList *hostlist;
    hostlist = ptrlist(node);

    objects = [NSArray arrayWithObjects:
      [NSString stringWithCString:(char *)hostlist->game.playername encoding:NSASCIIStringEncoding],
      [NSString stringWithCString:(char *)hostlist->game.mapname encoding:NSASCIIStringEncoding],
      [NSString stringWithFormat:@"%d", hostlist->game.nplayers],
      hostlist->game.passreq ? @"Yes" : @"No",
      hostlist->game.allowjoin ? @"Yes" : @"No",
      hostlist->game.pause ? @"Yes" : @"No",
      [NSString stringWithUTF8String:inet_ntoa(hostlist->addr)],
      [NSString stringWithFormat:@"%d", hostlist->game.port],
      nil];
    [table addObject:[NSDictionary dictionaryWithObjects:objects forKeys:keys]];
  }

  // free list
  clearlist(&trackerlist, free);

  // update table
  [self setJoinTrackerArray:table];
}

// robots!

- (BOOL)_validateRobotMenuItem: (NSMenuItem *)item
{
    [robotLock lock];
    [item setState: [item representedObject] == robot ? NSOnState : NSOffState];
    [robotLock unlock];
    return YES;
}

- (void)choseRobotMenuItem: (id)sender
{
    GSRobot *newRobot = [sender representedObject];
    [robotLock lock];
    if(newRobot != robot)
    {
        NSError *err = [newRobot load];
        if(err)
        {
            [NSApp presentError: err];
        }
        else
        {
            [robot unload];
            [robot release];
            robot = [newRobot retain];
        }
    }
    [robotLock unlock];
}

- (void)setupRobotsMenu
{
    NSArray *robots = [GSRobot availableRobots];
    
    NSMenuItem *autoMenu = nil;
    NSString *autoName = [[NSUserDefaults standardUserDefaults] stringForKey: @"GSAutomaticallyLoadRobot"];
    
    if([robots count])
    {
        NSMenu *menu = [[NSMenu alloc] initWithTitle: @"Robots"];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle: @"Manual Control" action: @selector(choseRobotMenuItem:) keyEquivalent: @""];
        [menu addItem: item];
        [item release];
        
        [menu addItem: [NSMenuItem separatorItem]];
        
        NSEnumerator *enumerator = [robots objectEnumerator];
        GSRobot *r;
        while((r = [enumerator nextObject]))
        {
            NSMenuItem *item = [[NSMenuItem alloc] initWithTitle: [r name] action: @selector(choseRobotMenuItem:) keyEquivalent: @""];
            [item setRepresentedObject: r];
            [menu addItem: item];
            if(autoName && [[r name] hasPrefix: autoName])
                autoMenu = item;
            [item release];
        }
        
        NSMenuItem *robotItem = [[NSMenuItem alloc] initWithTitle: @"Robots" action: NULL keyEquivalent: @""];
        [robotItem setSubmenu: menu];
        [[NSApp mainMenu] addItem: robotItem];
        [robotItem release];
        [menu release];
    }
    
    if(autoMenu)
        [self choseRobotMenuItem: autoMenu];
}

- (void)clientLoopUpdate
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [robotLock lock];
    [robot step];
    [robotLock unlock];
    [pool release];
}

- (void)sendMessageToRobot: (NSString *)string
{
    [robotLock lock];
    NSString *myPrefix = [NSString stringWithFormat: @"%s:", client.name];
    if(![string hasPrefix: myPrefix])
      [robot receivedMessage: string];
    [robotLock unlock];
}    

@end

void setplayerstatus(int player) {
  NSAutoreleasePool *pool;
  assert(player >= 0 && player < MAXPLAYERS);
  pool = [[NSAutoreleasePool alloc] init];
  [controller performSelectorOnMainThread:@selector(setPlayerStatus:) withObject:[NSString stringWithFormat:@"%d", player] waitUntilDone:NO];
  [pool release];
}

void setpillstatus(int pill) {
  NSAutoreleasePool *pool;
  assert(pill >= 0 && pill < MAXPILLS);
  pool = [[NSAutoreleasePool alloc] init];
  [controller performSelectorOnMainThread:@selector(setPillStatus:) withObject:[NSString stringWithFormat:@"%d", pill] waitUntilDone:NO];
  [pool release];
}

void setbasestatus(int base) {
  NSAutoreleasePool *pool;
  assert(base >= 0 && base < MAXBASES);
  pool = [[NSAutoreleasePool alloc] init];
  [controller performSelectorOnMainThread:@selector(setBaseStatus:) withObject:[NSString stringWithFormat:@"%d", base] waitUntilDone:NO];
  [pool release];
}

void settankstatus() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [controller performSelectorOnMainThread:@selector(setTankStatusBars) withObject:nil waitUntilDone:NO];
  [pool release];
}

void playsound(int snd) {
  if (!muteBool) {
    int i;
    NSAutoreleasePool *pool;
    NSArray *array;

    pool = [[NSAutoreleasePool alloc] init];

    switch (snd) {
    case kBubblesSound:
      array = bubblessounds;
      break;

    case kBuildSound:
      array = buildsounds;
      break;

    case kBuilderDeathSound:
      array = builderdeathsounds;
      break;

    case kExplosionSound:
      array = explosionsounds;
      break;

    case kFarBuildSound:
      array = farbuildsounds;
      break;

    case kFarBuilderDeathSound:
      array = farbuilderdeathsounds;
      break;

    case kFarExplosionSound:
      array = farexplosionsounds;
      break;

    case kFarHitTankSound:
      array = farhittanksounds;
      break;

    case kFarHitTerrainSound:
      array = farhitterrainsounds;
      break;

    case kFarHitTreeSound:
      array = farhittreesounds;
      break;

    case kFarShotSound:
      array = farshotsounds;
      break;

    case kFarSinkSound:
      array = farsinksounds;
      break;

    case kFarSuperBoomSound:
      array = farsuperboomsounds;
      break;

    case kFarTreeSound:
      array = fartreesounds;
      break;

    case kHitTankSound:
      array = hittanksounds;
      break;

    case kHitTerrainSound:
      array = hitterrainsounds;
      break;

    case kHitTreeSound:
      array = hittreesounds;
      break;

    case kMineSound:
      array = minesounds;
      break;
			
	case kMsgReceivedSound:
	  array = msgreceivedsounds;
	  break;

    case kPillShotSound:
      array = pillshotsounds;
      break;

    case kSinkSound:
      array = sinksounds;
      break;

    case kSuperBoomSound:
      array = superboomsounds;
      break;

    case kTankShotSound:
      array = tankshotsounds;
      break;

    case kTreeSound:
      array = treesounds;
      break;

    default:
      array = nil;
      break;
    }

    // start playing sound
    for (i = 0; i < [array count]; i++) {
      if (![[array objectAtIndex:i] isPlaying]) {
        [[array objectAtIndex:i] play];
        break;
      }
    }

    [pool release];
  }
}

void printmessage(int type, const char *text) {
  NSAutoreleasePool *pool;
  NSDictionary *attr;
  NSAttributedString *attrstring;

  assert(type == MSGEVERYONE || type == MSGALLIES || type == MSGNEARBY || type == MSGGAME);

  pool = [[NSAutoreleasePool alloc] init];

  switch (type) {
  case MSGEVERYONE:
    attr = [[NSDictionary alloc] init];
    break;

  case MSGALLIES:
    attr = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor purpleColor], NSForegroundColorAttributeName, nil];
    break;

  case MSGNEARBY:
    attr = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName, nil];
    break;

  case MSGGAME:
    attr = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName, nil];
    break;

  default:
    assert(0);
    break;
  }

  NSString *string = [NSString stringWithCString:text encoding:NSUTF8StringEncoding];
  attrstring = [[NSAttributedString alloc] initWithString:string attributes:attr];
  [controller performSelectorOnMainThread:@selector(printMessage:) withObject:attrstring waitUntilDone:NO];

  if(type == MSGEVERYONE || type == MSGALLIES || type == MSGNEARBY) {
	playsound(kMsgReceivedSound);
	[controller sendMessageToRobot: string];
  }
  
  [attrstring release];
  [attr release];
  [pool release];
}

void joinprogress(int statuscode, float progress) {
  NSAutoreleasePool *pool;
  pool = [[NSAutoreleasePool alloc] init];

  switch (statuscode) {
  /* status udpates */
  case kJoinRESOLVING:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Resolving Hostname..." waitUntilDone:NO];
    break;

  case kJoinCONNECTING:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Connecting..." waitUntilDone:NO];
    break;

  case kJoinSENDJOIN:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Sending Client Data" waitUntilDone:NO];
    break;

  case kJoinRECVPREAMBLE:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Receiving Server Data" waitUntilDone:NO];
    break;

  case kJoinRECVMAP:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Receving Map Data" waitUntilDone:NO];
    [controller performSelectorOnMainThread:@selector(setJoinProgressIndicator:) withObject:[NSString stringWithFormat:@"%f", progress] waitUntilDone:NO];
    break;

  case kJoinSUCCESS:
    [controller performSelectorOnMainThread:@selector(joinSuccess) withObject:nil waitUntilDone:NO];
    break;

  /* errors */
  case kJoinEHOSTNOTFOUND:
    [controller performSelectorOnMainThread:@selector(joinDNSError:) withObject:[NSString stringWithCString:hstrerror(HOST_NOT_FOUND) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kJoinEHOSTNORECOVERY:
    [controller performSelectorOnMainThread:@selector(joinDNSError:) withObject:[NSString stringWithCString:hstrerror(NO_RECOVERY) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kJoinEHOSTNODATA:
    [controller performSelectorOnMainThread:@selector(joinDNSError:) withObject:[NSString stringWithCString:hstrerror(NO_DATA) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kJoinETIMEOUT:
    [controller performSelectorOnMainThread:@selector(joinTimeOut) withObject:nil waitUntilDone:NO];
    break;

  case kJoinECONNREFUSED:
    [controller performSelectorOnMainThread:@selector(joinConnectionRefused) withObject:nil waitUntilDone:NO];
    break;

  case kJoinENETUNREACH:
    [controller performSelectorOnMainThread:@selector(joinNetUnreachable) withObject:nil waitUntilDone:NO];
    break;

  case kJoinEHOSTUNREACH:
    [controller performSelectorOnMainThread:@selector(joinHostUnreachable) withObject:nil waitUntilDone:NO];
    break;

  case kJoinEBADVERSION:
    [controller performSelectorOnMainThread:@selector(joinBadVersion) withObject:nil waitUntilDone:NO];
    break;

  case kJoinEDISALLOW:
    [controller performSelectorOnMainThread:@selector(joinDisallow) withObject:nil waitUntilDone:NO];
    break;

  case kJoinEBADPASS:
    [controller performSelectorOnMainThread:@selector(joinBadPassword) withObject:nil waitUntilDone:NO];
    break;

  case kJoinESERVERFULL:
    [controller performSelectorOnMainThread:@selector(joinServerFull) withObject:nil waitUntilDone:NO];
    break;

  case kJoinETIMELIMIT:
    [controller performSelectorOnMainThread:@selector(joinTimeLimit) withObject:nil waitUntilDone:NO];
    break;

  case kJoinEBANNEDPLAYER:
    [controller performSelectorOnMainThread:@selector(joinBannedPlayer) withObject:nil waitUntilDone:NO];
    break;

  case kJoinESERVERERROR:
    [controller performSelectorOnMainThread:@selector(joinServerError) withObject:nil waitUntilDone:NO];
    break;

  case kJoinECONNRESET:
    [controller performSelectorOnMainThread:@selector(joinConnectionReset) withObject:nil waitUntilDone:NO];
    break;

  default:
    assert(0);
    break;
  }

  [pool release];
}

void registercallback(int status) {
  NSAutoreleasePool *pool;
  pool = [[NSAutoreleasePool alloc] init];

  switch (status) {
  case kRegisterRESOLVING:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Resolving Tracker..." waitUntilDone:NO];
    break;

  case kRegisterCONNECTING:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Connecting to Tracker..." waitUntilDone:NO];
    break;

  case kRegisterSENDINGDATA:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Sending Data..." waitUntilDone:NO];
    break;

  case kRegisterTESTINGTCP:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Testing TCP Port..." waitUntilDone:NO];
    break;

  case kRegisterTESTINGUDP:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Testing UDP Port..." waitUntilDone:NO];
    break;

  case kRegisterSUCCESS:
    [controller performSelectorOnMainThread:@selector(registerSuccess) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterEHOSTNORECOVERY:
    [controller performSelectorOnMainThread:@selector(trackerDNSError:) withObject:[NSString stringWithCString:hstrerror(NO_RECOVERY) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;
  
  case kRegisterEHOSTNOTFOUND:
    [controller performSelectorOnMainThread:@selector(trackerDNSError:) withObject:[NSString stringWithCString:hstrerror(HOST_NOT_FOUND) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kRegisterEHOSTNODATA:
    [controller performSelectorOnMainThread:@selector(trackerDNSError:) withObject:[NSString stringWithCString:hstrerror(NO_DATA) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kRegisterETIMEOUT:
    [controller performSelectorOnMainThread:@selector(registerTrackerTimeOut) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterECONNREFUSED:
    [controller performSelectorOnMainThread:@selector(registerTrackerConnectionRefused) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterEHOSTDOWN:
    [controller performSelectorOnMainThread:@selector(registerTrackerHostDown) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterEHOSTUNREACH:
    [controller performSelectorOnMainThread:@selector(registerTrackerHostUnreachable) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterEBADVERSION:
    [controller performSelectorOnMainThread:@selector(registerTrackerBadVersion) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterETCPCLOSED:
    [controller performSelectorOnMainThread:@selector(registerTrackerTCPPortBlocked) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterEUDPCLOSED:
    [controller performSelectorOnMainThread:@selector(registerTrackerUDPPortBlocked) withObject:nil waitUntilDone:NO];
    break;

  case kRegisterECONNRESET:
    [controller performSelectorOnMainThread:@selector(registerTrackerConnectionReset) withObject:nil waitUntilDone:NO];
    break;

  default:
    assert(0);
    break;
  }

  [pool release];
}

void getlisttrackerstatus(int status) {
  NSAutoreleasePool *pool;
  pool = [[NSAutoreleasePool alloc] init];

  switch (status) {
  case kGetListTrackerRESOLVING:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Resolving Tracker..." waitUntilDone:NO];
    break;

  case kGetListTrackerCONNECTING:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Connecting to Tracker..." waitUntilDone:NO];
    break;

  case kGetListTrackerGETTINGLIST:
    [controller performSelectorOnMainThread:@selector(setJoinProgressStatusTextField:) withObject:@"Receving Game List..." waitUntilDone:NO];
    break;

  case kGetListTrackerSUCCESS:
    [controller performSelectorOnMainThread:@selector(getListTrackerSuccess) withObject:nil waitUntilDone:NO];
    break;

  case kGetListTrackerECONNABORTED:  // error caused by user cancel
    break;

  case kGetListTrackerEPIPE:
    break;

  case kGetListTrackerEHOSTNOTFOUND:  // error from dns lookup
    [controller performSelectorOnMainThread:@selector(trackerDNSError:) withObject:[NSString stringWithCString:hstrerror(HOST_NOT_FOUND) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kGetListTrackerEHOSTNORECOVERY:
    [controller performSelectorOnMainThread:@selector(trackerDNSError:) withObject:[NSString stringWithCString:hstrerror(NO_RECOVERY) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kGetListTrackerEHOSTNODATA:
    [controller performSelectorOnMainThread:@selector(trackerDNSError:) withObject:[NSString stringWithCString:hstrerror(NO_DATA) encoding:NSUTF8StringEncoding] waitUntilDone:NO];
    break;

  case kGetListTrackerETIMEOUT:  // error caused by other factors
    [controller performSelectorOnMainThread:@selector(getListTrackerTimeOut) withObject:nil waitUntilDone:NO];
    break;

  case kGetListTrackerECONNREFUSED:
    [controller performSelectorOnMainThread:@selector(getListTrackerConnectionRefused) withObject:nil waitUntilDone:NO];
    break;

  case kGetListTrackerEHOSTDOWN:
    [controller performSelectorOnMainThread:@selector(getListTrackerHostDown) withObject:nil waitUntilDone:NO];
    break;

  case kGetListTrackerEHOSTUNREACH:
    [controller performSelectorOnMainThread:@selector(getListTrackerHostUnreachable) withObject:nil waitUntilDone:NO];
    break;

  case kGetListTrackerEBADVERSION:
    [controller performSelectorOnMainThread:@selector(getListTrackerBadVersion) withObject:nil waitUntilDone:NO];
    break;

  default:
    assert(0);
    break;
  }

  [pool release];
}

int setKey(NSMutableDictionary *dict, NSWindow *win, GSKeyCodeField *field, NSString *newObject) {
  NSString *key;
  id object;
  key = [field stringValue];
  object = [dict objectForKey:key];

  if (object != nil) {
    [[NSAlert alertWithMessageText:@"There is a key conflict." defaultButton:@"OK" alternateButton:nil otherButton:nil informativeTextWithFormat:@"There is a conflict with %@ and %@.  Change one of them.", object, newObject] beginSheetModalForWindow:win modalDelegate:nil didEndSelector:nil contextInfo:NULL];
    return -1;
  }
  else {
    [dict setObject:newObject forKey:key];
    return 0;
  }
}

void clientloopupdate(void)
{
  [controller clientLoopUpdate];
}
