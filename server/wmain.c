
#include "lols.h"

#ifdef LOLS_WINDOWS

#include <Windows.h>
#include <Windowsx.h>
#include <CommCtrl.h>
#include <tchar.h>
#include <Shlwapi.h>
#include "resources.h"

/*
 * Embed XML manifest to enable pretty windows XP styles
 */
#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")
#pragma comment(lib, "Shlwapi.lib")

int LOLS_initiated = 0;
HWND combobox;
HWND searchbox;
HWND searchbtn;
HWND resultstxt;
HWND casebox;

void try_to_load_triples(HWND hnd);
void init_globals(HWND w);
void xdbg( char *txt );
void popup_message( char *title, char *txt );
void popup_error( char *err );
void populate_combobox(HWND w);
void perform_search( HWND w, char *search );
void set_results( char *r );

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDCANCEL:
          SendMessage(hDlg, WM_CLOSE, 0, 0);
          return TRUE;

        case LOAD_BUTTON:
          try_to_load_triples( hDlg );
          return TRUE;

        case SEARCH_BUTTON:
          {
            char search[MAX_STRING_LEN / 2 + 1];

            Edit_GetText(searchbox, search, MAX_STRING_LEN / 2);

            Edit_SetText(searchbox, "" );
            perform_search(hDlg, search);
          }
          return TRUE;

        case SEARCH_COMBOBOX:
          if ( HIWORD( wParam ) == CBN_SELCHANGE )
          {
            char combo[MAX_STRING_LEN+1];
            
            ComboBox_GetText( combobox, combo, MAX_STRING_LEN );

            Edit_Enable( casebox, strcmp( combo, "Label from IRI" ) );
            return TRUE;
          }
          return FALSE;
      }
      break;

  case WM_CLOSE:
    DestroyWindow(hDlg);
    return TRUE;

  case WM_DESTROY:
    PostQuitMessage(0);
    return TRUE;
  }

  return FALSE;
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE h0, LPTSTR lpCmdLine, int nCmdShow)
{
  HWND hDlg;
  MSG msg;
  BOOL ret;

  InitCommonControls();
  hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(MAIN_DLG), 0, DialogProc, 0);

  init_globals(hDlg);

  SendMessage( hDlg, DM_SETDEFID, SEARCH_BUTTON, 0 );
  SendMessage( searchbox, EM_SETCUEBANNER, TRUE, (long int) L"Search for..." );
  Edit_Enable( searchbtn, FALSE );
  Edit_Enable( searchbox, FALSE );
  Edit_Enable( combobox, FALSE );
  Edit_Enable( casebox, FALSE );

  populate_combobox(hDlg);

  ShowWindow(hDlg, nCmdShow);

  while((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
    if(ret == -1)
      return -1;

    if(!IsDialogMessage(hDlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}

void init_globals(HWND w)
{
  combobox = GetDlgItem(w, SEARCH_COMBOBOX);
  searchbox = GetDlgItem(w, SEARCHBOX);
  searchbtn = GetDlgItem( w, SEARCH_BUTTON );
  resultstxt = GetDlgItem(w, RESULTS_TEXT);
  casebox = GetDlgItem(w, CASE_SENS_CHECKBOX);
}

void try_to_load_triples(HWND hnd)
{
  OPENFILENAME ofn;
  char fname[MAX_PATH] = "", load_msg[MAX_PATH + 100], fname_short[MAX_PATH];
  FILE *fp;
  HWND load_button, load_txt;

  ZeroMemory(&ofn, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hnd;
  ofn.lpstrFilter = "N-Triples Files (*.nt)\0*.nt\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = fname;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
  ofn.lpstrDefExt = "nt";

  if ( !GetOpenFileName( &ofn ) )
    return;

  if ( LOLS_initiated )
  {
    popup_error( "Error: An N-Triples file is already loaded.  Use another instance of LOLS to load another file." );
    return;
  }

  init_lols();

  fp = fopen( fname, "r" );

  if ( !fp )
  {
    popup_error( "Could not open file, for some reason" );
    return;
  }

  parse_lols_file(fp);

  sprintf( fname_short, "%s", fname );
  PathStripPath( fname_short );
  sprintf( load_msg, "Successfully loaded %s", fname_short );

  load_button = GetDlgItem(hnd, LOAD_BUTTON);
  load_txt = GetDlgItem(hnd, ONTOLOGY_LOADED_TEXT);

  DestroyWindow( load_button );

  Edit_SetText( load_txt, load_msg );

  Edit_Enable( searchbtn, TRUE );
  Edit_Enable( searchbox, TRUE );
  Edit_Enable( combobox, TRUE );
  Edit_Enable( casebox, TRUE );

  SetFocus( GetDlgItem( hnd, SEARCHBOX ) );
}

void popup_message( char *title, char *txt )
{
  MessageBox(NULL, txt, title, MB_OK );
}

void popup_error( char *err )
{
  popup_message( "Error", err );
}

void xdbg( char *txt )
{
  MessageBox(NULL, txt, "Note", MB_OK);
}

void populate_combobox(HWND w)
{
  HWND box = GetDlgItem(w, SEARCH_COMBOBOX);
  const char *ComboBoxItems[] =
  {
    "Short IRI from label",
    "Full IRI from label",
    "Label from IRI"
  };
  int count = 3;
  int i;

  for ( i = 0; i < count; i++ )
    SendMessage( box, CB_ADDSTRING, 0, (long int)ComboBoxItems[i] );

  SendMessage( box, CB_SETCURSEL, 0, 0 );
}

// Temporarily assume case insensitive
void perform_search( HWND w, char *search )
{
  char combo[MAX_STRING_LEN+1];
  trie **data, **ptr;
  int fShortIRI = 0;
  int len;
  char *results, *rptr;
  int checkbox, case_sens;

  ComboBox_GetText( combobox, combo, MAX_STRING_LEN );

  checkbox = SendMessage( casebox, BM_GETSTATE, 0, 0 );
  case_sens = checkbox & BST_CHECKED;

  if ( !strcmp( combo, "Label from IRI" ) )
    data = get_labels_by_iri( search );
  else if ( !strcmp( combo, "Full IRI from label" ) || !strcmp( combo, "Short IRI from label" ) )
  {
    if ( case_sens )
      data = get_iris_by_label( search );
    else
      data = get_iris_by_label_case_insensitive( search );

    if ( !strcmp( combo, "Short IRI from label" ) )
      fShortIRI = 1;
  }
  else
  {
    popup_error( "Invalid search type selected" );
    return;
  }

  if ( !data || !*data )
  {
    set_results( "No results." );
    return;
  }

  for ( len = 0, ptr = data; *ptr; ptr++ )
    len += strlen( trie_to_static( *ptr ) ) + 1;

  CREATE( results, char, len+1 );

  for ( rptr = results, ptr = data; *ptr; ptr++ )
  {
    sprintf( rptr, "%s\n", fShortIRI ? get_url_shortform( trie_to_static(*ptr) ) : trie_to_static(*ptr) );
    rptr = &rptr[strlen(rptr)];
  }

  *rptr = '\0';

  set_results( results );

  free( results );
}

void set_results( char *r )
{
  Edit_SetText( resultstxt, r );
}

#endif
