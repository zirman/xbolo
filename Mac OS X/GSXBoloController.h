/* GSXBoloController */

#import <Cocoa/Cocoa.h>
#include <sys/socket.h>
#import "TCMPortMapper/TCMPortMapper.h"

#include "vector.h"
#include "rect.h"
#include "bolo.h"
#include "server.h"
#include "client.h"

@class GSKeyCodeField, GSBoloView, GSRobot, GSStatusBar, GSBuilderStatusView;

@interface GSXBoloController : NSObject /*<NSToolbarDelegate>*/ {
  IBOutlet NSWindow *newGameWindow;
  IBOutlet NSWindow *boloWindow;
  IBOutlet NSWindow *joinProgressWindow;
  IBOutlet NSWindow *preferencesWindow;
  IBOutlet NSTabView *newGameTabView;
  IBOutlet NSPanel *statusPanel;
  IBOutlet NSPanel *allegiancePanel;
  IBOutlet NSPanel *messagesPanel;

  IBOutlet NSMenuItem *gamePauseResumeMenuItem;

  // host game outlets
  IBOutlet NSTextField *hostMapField;
  IBOutlet NSButton *hostUPnPSwitch;
  IBOutlet NSTextField *hostPortField;
  IBOutlet NSButton *hostPasswordSwitch;
  IBOutlet NSSecureTextField *hostPasswordField;
  IBOutlet NSButton *hostTimeLimitSwitch;
  IBOutlet NSSlider *hostTimeLimitSlider;
  IBOutlet NSButton *hostHiddenMinesSwitch;
  IBOutlet NSTextField *hostHiddenMinesTextField;
  IBOutlet NSButton *hostTrackerSwitch;
  IBOutlet NSTextField *hostTrackerField;
  IBOutlet NSTextField *hostTimeLimitField;
  IBOutlet NSPopUpButton *hostGameTypeMenu;
  IBOutlet NSTabView *hostGameTypeTab;

  // host domination outlets
  IBOutlet NSMatrix *hostDominationTypeMatrix;
  IBOutlet NSSlider *hostDominationBaseControlSlider;
  IBOutlet NSTextField *hostDominationBaseControlField;

  // join game outlets
  IBOutlet NSTextField *joinAddressField;
  IBOutlet NSTextField *joinPortField;
  IBOutlet NSButton *joinPasswordSwitch;
  IBOutlet NSSecureTextField *joinPasswordField;
  IBOutlet NSTextField *joinTrackerField;
  IBOutlet NSTableView *joinTrackerTableView;

  // join progress controls
  IBOutlet NSTextField *joinProgressStatusTextField;
  IBOutlet NSProgressIndicator *joinProgressIndicator;

  // preference outlets
  IBOutlet NSTabView *prefTab;
  IBOutlet NSTextField *prefPlayerNameField;
  IBOutlet GSKeyCodeField *prefAccelerateField;
  IBOutlet GSKeyCodeField *prefBrakeField;
  IBOutlet GSKeyCodeField *prefTurnLeftField;
  IBOutlet GSKeyCodeField *prefTurnRightField;
  IBOutlet GSKeyCodeField *prefLayMineField;
  IBOutlet GSKeyCodeField *prefShootField;
  IBOutlet GSKeyCodeField *prefIncreaseAimField;
  IBOutlet GSKeyCodeField *prefDecreaseAimField;
  IBOutlet NSButton *prefAutoSlowdownSwitch;
  IBOutlet GSKeyCodeField *prefUpField;
  IBOutlet GSKeyCodeField *prefDownField;
  IBOutlet GSKeyCodeField *prefLeftField;
  IBOutlet GSKeyCodeField *prefRightField;
  IBOutlet GSKeyCodeField *prefTankViewField;
  IBOutlet GSKeyCodeField *prefPillViewField;

  // Main View Outlets
  IBOutlet GSBoloView *boloView;
  IBOutlet NSView *builderToolView;

  /* Status Window Outlets */
  IBOutlet NSImageView *player0StatusImageView;
  IBOutlet NSImageView *player1StatusImageView;
  IBOutlet NSImageView *player2StatusImageView;
  IBOutlet NSImageView *player3StatusImageView;
  IBOutlet NSImageView *player4StatusImageView;
  IBOutlet NSImageView *player5StatusImageView;
  IBOutlet NSImageView *player6StatusImageView;
  IBOutlet NSImageView *player7StatusImageView;
  IBOutlet NSImageView *player8StatusImageView;
  IBOutlet NSImageView *player9StatusImageView;
  IBOutlet NSImageView *playerAStatusImageView;
  IBOutlet NSImageView *playerBStatusImageView;
  IBOutlet NSImageView *playerCStatusImageView;
  IBOutlet NSImageView *playerDStatusImageView;
  IBOutlet NSImageView *playerEStatusImageView;
  IBOutlet NSImageView *playerFStatusImageView;

  IBOutlet NSImageView *pill0StatusImageView;
  IBOutlet NSImageView *pill1StatusImageView;
  IBOutlet NSImageView *pill2StatusImageView;
  IBOutlet NSImageView *pill3StatusImageView;
  IBOutlet NSImageView *pill4StatusImageView;
  IBOutlet NSImageView *pill5StatusImageView;
  IBOutlet NSImageView *pill6StatusImageView;
  IBOutlet NSImageView *pill7StatusImageView;
  IBOutlet NSImageView *pill8StatusImageView;
  IBOutlet NSImageView *pill9StatusImageView;
  IBOutlet NSImageView *pillAStatusImageView;
  IBOutlet NSImageView *pillBStatusImageView;
  IBOutlet NSImageView *pillCStatusImageView;
  IBOutlet NSImageView *pillDStatusImageView;
  IBOutlet NSImageView *pillEStatusImageView;
  IBOutlet NSImageView *pillFStatusImageView;

  IBOutlet NSImageView *base0StatusImageView;
  IBOutlet NSImageView *base1StatusImageView;
  IBOutlet NSImageView *base2StatusImageView;
  IBOutlet NSImageView *base3StatusImageView;
  IBOutlet NSImageView *base4StatusImageView;
  IBOutlet NSImageView *base5StatusImageView;
  IBOutlet NSImageView *base6StatusImageView;
  IBOutlet NSImageView *base7StatusImageView;
  IBOutlet NSImageView *base8StatusImageView;
  IBOutlet NSImageView *base9StatusImageView;
  IBOutlet NSImageView *baseAStatusImageView;
  IBOutlet NSImageView *baseBStatusImageView;
  IBOutlet NSImageView *baseCStatusImageView;
  IBOutlet NSImageView *baseDStatusImageView;
  IBOutlet NSImageView *baseEStatusImageView;
  IBOutlet NSImageView *baseFStatusImageView;

  IBOutlet GSStatusBar *baseShellsStatusBar;
  IBOutlet GSStatusBar *baseMinesStatusBar;
  IBOutlet GSStatusBar *baseArmourStatusBar;

  IBOutlet GSBuilderStatusView *builderStatusView;

  IBOutlet NSTextField *playerKillsTextField;
  IBOutlet NSTextField *playerDeathsTextField;
  IBOutlet GSStatusBar *playerShellsStatusBar;
  IBOutlet GSStatusBar *playerMinesStatusBar;
  IBOutlet GSStatusBar *playerArmourStatusBar;
  IBOutlet GSStatusBar *playerTreesStatusBar;

  // host pane data
  NSString *hostMapString;
  BOOL hostUPnPBool;
  int hostPortNumber;
  BOOL hostPasswordBool;
  NSString *hostPasswordString;
  BOOL hostTimeLimitBool;
  NSString *hostTimeLimitString;
  BOOL hostHiddenMinesBool;
  BOOL hostTrackerBool;

  // only supported type is domination
  int hostGameTypeNumber;

  // host domination data
  int hostDominationTypeNumber;
  NSString *hostDominationBaseControlString;

  // window visibility bools
  BOOL showStatusBool;
  BOOL showAllegianceBool;
  BOOL showMessagesBool;

  // join pane data
  NSString *joinAddressString;
  int joinPortNumber;
  BOOL joinPasswordBool;
  NSString *joinPasswordString;
  BOOL joinTrackerBool;
  NSMutableArray *joinTrackerArray;

  // tracker string
  NSString *trackerString;

  // pref objects
  NSToolbar *prefToolbar;
  NSToolbarItem *toolbarPlayerInfoItem;
  NSToolbarItem *toolbarKeyConfigItem;
  NSString *playerNameString;
  NSDictionary *keyConfigDict;
  BOOL autoSlowdownBool;

  // allegiance panel outlets
  IBOutlet NSTableView *playerInfoTableView;
  IBOutlet NSButton *requestAllianceButton;
  IBOutlet NSButton *leaveAllianceButton;

  // bolo tool matrix
  IBOutlet NSMatrix *builderToolMatrix;

  // bolo tool data
  int builderToolInt;

  // allegiance panel objects
  NSMutableArray *playerInfoArray;

  // messages panel outlets
  IBOutlet NSTextView *messagesTextView;
  IBOutlet NSTextField *messageTextField;
  IBOutlet NSMatrix *messageTargetMatrix;

  // message panel data
  int messageTargetInt;

  // toolbar
  NSToolbar *boloToolbar;
  NSToolbarItem *builderToolItem;
  NSToolbarItem *tankCenterItem;
  NSToolbarItem *pillCenterItem;
  NSToolbarItem *zoomInItem;
  NSToolbarItem *zoomOutItem;

  // zoom
  int zoomLevel;

  /* UPnP port mapper */
  TCMPortMapper *portMapper;

  // bot
  NSLock *robotLock;
  GSRobot *robot;
}

// IBAction methods

// host game actions
- (IBAction)hostUPnPSwitch:(id)sender;
- (IBAction)hostPort:(id)sender;
- (IBAction)hostPasswordSwitch:(id)sender;
- (IBAction)hostPassword:(id)sender;
- (IBAction)hostTimeLimitSwitch:(id)sender;
- (IBAction)hostTimeLimit:(id)sender;
- (IBAction)hostHiddenMinesSwitch:(id)sender;
- (IBAction)hostTrackerSwitch:(id)sender;

// domination controls
- (IBAction)hostDominationType:(id)sender;
- (IBAction)hostDominationBaseControl:(id)sender;

// join game actions
- (IBAction)joinPasswordSwitch:(id)sender;
- (IBAction)joinPassword:(id)sender;
- (IBAction)joinTrackerRefresh:(id)sender;

// shared actions for host and join
- (IBAction)tracker:(id)sender;

// pref pane actions
- (IBAction)showPrefs:(id)sender;
- (IBAction)prefPane:(id)sender;
- (IBAction)prefPlayerName:(id)sender;
- (IBAction)revertKeyConfig:(id)sender;
- (IBAction)applyKeyConfig:(id)sender;

// toolbar actions
- (IBAction)builderTool:(id)sender;
- (IBAction)builderToolMenu:(id)sender;
- (IBAction)tankCenter:(id)sender;
- (IBAction)pillCenter:(id)sender;
- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;

// menu actions
- (IBAction)closeGame:(id)sender;
- (IBAction)newGame:(id)sender;
- (IBAction)toggleJoin:(id)sender;
- (IBAction)toggleMute:(id)sender;
- (IBAction)gamePauseResumeMenu:(id)sender;
- (IBAction)kickPlayer:(id)sender;
- (IBAction)banPlayer:(id)sender;
- (IBAction)unbanPlayer:(id)sender;

// host actions
- (IBAction)hostChoose:(id)sender;
- (IBAction)hostOK:(id)sender;

// join actions
- (IBAction)joinPort:(id)sender;
- (IBAction)joinOK:(id)sender;

// join progress actions
- (IBAction)joinCancel:(id)sender;

// game actions
- (IBAction)statusPanel:(id)sender;
- (IBAction)allegiancePanel:(id)sender;
- (IBAction)messagesPanel:(id)sender;

// scroll view
- (IBAction)scrollUp:(id)sender;
- (IBAction)scrollDown:(id)sender;
- (IBAction)scrollLeft:(id)sender;
- (IBAction)scrollRight:(id)sender;

// allegiance panel actions
- (IBAction)requestAlliance:(id)sender;
- (IBAction)leaveAlliance:(id)sender;

// messages panel actions
- (IBAction)sendMessage:(id)sender;
- (IBAction)messageTarget:(id)sender;

// key event method
- (void)keyEvent:(BOOL)event forKey:(unsigned short)keyCode;

// mouse event method
- (void)mouseEvent:(Pointi)point;

// accessor methods
- (void)setHostMapString:(NSString *)aString;
- (void)setHostUPnPBool:(BOOL)aBool;
- (void)setHostPortNumber:(int)aInt;
- (void)setHostPasswordBool:(BOOL)aBool;
- (void)setHostPasswordString:(NSString *)aString;
- (void)setHostTimeLimitBool:(BOOL)aBool;
- (void)setHostTimeLimitString:(NSString *)aString;
- (void)setHostHiddenMinesBool:(BOOL)aBool;
- (void)setHostTrackerBool:(BOOL)aBool; 
- (void)setHostGameTypeNumber:(int)aInt;
- (void)setHostDominationTypeNumber:(int)newHostDominationTypeNumber;
- (void)setHostDominationBaseControlString:(NSString *)string;
- (void)setJoinAddressString:(NSString *)newJoinAddressString;
- (void)setJoinPortNumber:(int)newJoinPortNumber;
- (void)setJoinPasswordBool:(BOOL)aBool;
- (void)setJoinPasswordString:(NSString *)aString;
- (void)setJoinTrackerArray:(NSArray *)aArray;
- (void)setTrackerString:(NSString *)aString;
- (void)setPrefPaneIdentifierString:(NSString *)aString;
- (void)setPlayerNameString:(NSString *)aString;
- (void)setKeyConfigDict:(NSDictionary *)aDict;
- (void)setAutoSlowdownBool:(BOOL)aBool;
- (void)setShowStatusBool:(BOOL)aBool;
- (void)setShowAllegianceBool:(BOOL)aBool;
- (void)setShowMessagesBool:(BOOL)aBool;
- (void)setBuilderToolInteger:(int)anInt;
- (void)setMessageTargetInteger:(int)anInt;
- (void)setMuteBool:(BOOL)aBool;

@end

// bolo callbacksinitclient
void setplayerstatus(int player);
void setpillstatus(int pill);
void setbasestatus(int base);
void settankstatus();
void playsound(int sound);
void printmessage(int type, const char *text);
void joinprogress(int statuscode, float progress);
void trackerprogress(int statuscode);
void clientloopupdate(void);
