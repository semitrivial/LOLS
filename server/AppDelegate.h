//
//  AppDelegate.h
//  LOLS
//
//  Created by Sam on 19/01/2015.
//  Copyright (c) 2015 The Farr Institute. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (weak) IBOutlet NSPopUpButton *combobox;
@property (weak) IBOutlet NSButton *casebox;
@property (weak) IBOutlet NSButton *searchbutton;
@property (weak) IBOutlet NSBox *resultsbox;
@property (weak) IBOutlet NSButton *loadbutton;
@property (weak) IBOutlet NSTextField *loadstatus;
@property (weak) IBOutlet NSTextField *resultstext;

@property (weak) IBOutlet NSTextField *searchbox;

- (IBAction)search_clicked:(id)sender;
- (IBAction)loadbutton_clicked:(id)sender;
- (IBAction)combobox_action:(id)sender;

@end


