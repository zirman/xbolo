#import <Cocoa/Cocoa.h>

@interface GSKeyCodeFieldCell : NSActionCell {
  unsigned short keyCode;
}
- (id)initWithKeyCode:(unsigned short)aKeyCode;
- (void)setKeyCode:(unsigned short)aKeyCode;
- (unsigned short)keyCode;
- (IBAction)takeKeyValueFrom:(id)sender;
@end

@interface GSKeyCodeField : NSControl {
  unsigned int modifiers;
}
- (void)setKeyCode:(unsigned short)aKeyCode;
- (unsigned short)keyCode;
- (IBAction)takeKeyValueFrom:(id)sender;
@end
