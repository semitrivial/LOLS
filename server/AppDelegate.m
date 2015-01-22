//
//  AppDelegate.m
//  LOLS for OS X
//
//  Created by Sam on 19/01/2015.
//  Copyright (c) 2015 The Farr Institute. All rights reserved.
//

#import "AppDelegate.h"
#include "lols.h"

int initiated_LOLS = 0;

void osx_popup( NSString *title, NSString *message )
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:title];
    [alert setInformativeText:message];
    [alert addButtonWithTitle:@"ok"];
    [alert runModal];
    return;
}

void osx_poperror( NSString *err )
{
    osx_popup( @"Error", err );
}

void dbgx(NSString *message)
{
    osx_popup( @"Debug", message );
}

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [_combobox setEnabled:NO];
    [_casebox setEnabled:NO];
    [_casebox setState:0];
    [_searchbutton setEnabled:NO];
    [_searchbox setEnabled:NO];
    [_resultstext setSelectable:YES];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}

- (IBAction)search_clicked:(id)sender {
    trie **data, **ptr;
    char *results, *rptr;
    char *search;
    NSString *NSsearch;
    int len;
    int case_sens;
    int combo;
    int fShortIRI = 0;
    
    NSsearch = _searchbox.stringValue;
    search = strdup(NSsearch.UTF8String);
    
    case_sens = _casebox.intValue;
    
    /*
     * 0 = short IRI from label
     * 1 = full IRI from label
     * 2 = label from IRI
     */
    combo = ((NSString *)_combobox.objectValue).intValue;
    
    if ( combo == 2 )
        data = get_labels_by_iri( search );
    else if ( combo == 1 || combo == 0 )
    {
        if ( case_sens )
            data = get_iris_by_label( search );
        else
            data = get_iris_by_label_case_insensitive( search );
        
        if ( combo == 0 )
            fShortIRI = 1;
    }
    
    if ( !data || !*data )
    {
        [_resultstext setStringValue:@"No Results"];
        free( search );
        return;
    }
    
    for ( len=0, ptr = data; *ptr; ptr++ )
        len += strlen( trie_to_static( *ptr ) ) + 1;
    
    CREATE( results, char, len+1 );
    
    for ( rptr = results, ptr = data; *ptr; ptr++ )
    {
        sprintf( rptr, "%s\n", fShortIRI ? get_url_shortform( trie_to_static( *ptr ) ) : trie_to_static( *ptr ) );
        rptr = &rptr[strlen(rptr)];
    }
    
    *rptr = '\0';
    
    [_resultstext setStringValue:[NSString stringWithUTF8String:results]];
    free( search );
}

- (IBAction)loadbutton_clicked:(id)sender {
    NSOpenPanel * openDlg = [NSOpenPanel openPanel];
    FILE *fp;
    const char *filename = NULL;
    NSString *pathless;
    
    [openDlg setCanChooseFiles:YES];
    [openDlg setCanChooseDirectories:YES];
    
    if ( [openDlg runModal] != NSOKButton )
        return;
    
    for ( NSURL * URL in [openDlg URLs] )
    {
        if ( filename )
        {
            osx_popup( @"Too many files", @"Please only select one file to load." );
            return;
        }
        filename = URL.path.UTF8String;
        pathless = URL.path.lastPathComponent;
    }
    
    [openDlg close];
    
    if ( !filename )
        return;
    
    fp = fopen( filename, "r" );
    
    if ( !fp )
    {
        osx_poperror( @"The file could not be loaded." );
        return;
    }
    
    if ( !initiated_LOLS )
    {
        initiated_LOLS = 1;
        init_lols();
    }
    
    parse_lols_file( fp );
    
    [_loadbutton setTransparent: YES];
    [_loadbutton setEnabled: NO];
    [_combobox setEnabled: YES];
    [_casebox setEnabled: YES];
    [_searchbutton setEnabled: YES];
    [_searchbox setEnabled: YES];
    
    [_loadstatus setStringValue:pathless];
    
}

- (IBAction)combobox_action:(id)sender {
    int combo = ((NSString *)_combobox.objectValue).intValue;
    
    [_casebox setEnabled:(combo!=2)];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}
@end

