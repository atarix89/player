// player.cpp : Defines he entry point for the application.
//

#include "stdafx.h"
#include "player.h"
#include "windows.h"
#include "mmsystem.h"
#include <string>
#include "commdlg.h"
#include "commctrl.h"
#include "Wmsdk.h"

#pragma comment( lib, "Winmm.lib" )
#pragma comment( lib, "comctl32.lib" )
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")  //short way to XP styles without .manifest (VC2005 or later)

#define MAX_THREADS 30
#define BUF_SIZE 255
#define MAX_LOADSTRING 100

using namespace std;

void OpenDevice();
void OnScroll(HWND hDlg, LPARAM SliderID);
DWORD WINAPI CreateSoundThread(LPVOID lParam);
DWORD WINAPI CreateRefreshThread(LPVOID lParam);
LPSTR loadAudioBlock(const char* filename, DWORD* blockSize);
void writeAudioBlock(HWAVEOUT hWaveOut, LPSTR block, DWORD size);

// Global Variables:
HINSTANCE hInst;						// current instance
TCHAR szTitle[MAX_LOADSTRING];			// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];	// the main window clss name

OPENFILENAME ofn;
HANDLE hSoundThread,hRefreshThread;
HWAVEOUT hWaveOut;			// device handle
WAVEFORMATEX wfx;			// look this up in your documentation
MMRESULT result;			// for waveOut return values
LPSTR block;				// pointer to the block
DWORD blockSize, size;		// holds the size of the block
char* PlayerState = new char[20];
char FileName[MAX_PATH] = "";
DWORD VolRight, VolLeft;
MMTIME TimePlayed;
int TimeMin,TimeSec,LengthSec,PointerPosBytes;
HWND hDlg;
LRESULT tbpos;
BOOL SliderInUse = false;

// Forward declarations of functions included in this code module:
BOOL				InitInstance(HINSTANCE, int);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	Player(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
	icex.dwSize=sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC=0;
    InitCommonControlsEx(&icex);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_PLAYER, szWindowClass, MAX_LOADSTRING);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PLAYER));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_PLAYER), NULL, Player);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK Player(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	UINT SliderID;
	wchar_t buf[12];
	LPVOID pData;
	HWND hLabel1 = GetDlgItem(hDlg,IDC_STATIC1);
	HWND hLabel2 = GetDlgItem(hDlg,IDC_STATIC2);
	HWND hLabel3 = GetDlgItem(hDlg,IDC_STATIC3);
	UNREFERENCED_PARAMETER(lParam);
	int nResults = AddFontResource("digital-7 (mono).ttf");
	HFONT TimeFont = CreateFont(50,0,0,0,0,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Digital-7 Mono"));
	HBRUSH BGBrush = CreateSolidBrush(RGB(200,210,200));
	char pct = '%';
	BYTE bpos;
	WORD buffer;
	HANDLE hFile;
	DWORD readBytes = 0;
	switch (message)
	{
	case WM_INITDIALOG:
		SendMessage(GetDlgItem(hDlg,IDC_STATIC1),WM_SETFONT, (WPARAM)TimeFont, 0);
		SetDlgItemText(hDlg, IDC_EDIT1, "choose file to play");
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case IDFOPEN:
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = NULL;
			ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
			ofn.lpstrFile = FileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
			ofn.lpstrDefExt = "";
			ofn.lpfnHook = NULL;
			if(GetOpenFileName(&ofn) != 0) //Don't do anything if Cancel is pressed inside dialog.
			{
				hFile = CreateFile(ofn.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
				ReadFile(hFile,&buffer,22, &readBytes, NULL);			//skip
				ReadFile(hFile,&buffer,2, &readBytes, NULL);			//channels					2
				wfx.nChannels = buffer;
				ReadFile(hFile,&buffer,4, &readBytes, NULL);			//sample rate				44100
				wfx.nSamplesPerSec = buffer;
				ReadFile(hFile,&buffer,6, &readBytes, NULL);			//byterate + block align	skip
				ReadFile(hFile,&buffer,2, &readBytes, NULL);			//sample size				16
				wfx.wBitsPerSample = buffer;
				CloseHandle(hFile);
				waveOutReset(hWaveOut);
				tbpos = 0;
				OpenDevice();
				hSoundThread = CreateThread(NULL, 0, CreateSoundThread, ofn.lpstrFile, 0, NULL);		// Create the thread to begin execution on its own.
				PlayerState = "playing";
				SendMessage(GetDlgItem(hDlg,IDC_VOLUME_SLIDER), TBM_SETPOS, TRUE, 70);
				SendMessage(GetDlgItem(hDlg,IDC_BALANCE_SLIDER), TBM_SETPOS, TRUE, 50);		
				VolRight=0xFFFF*0.35;
				VolRight=VolRight<<16;
				VolLeft=0xFFFF*0.35;
				waveOutSetVolume(hWaveOut, VolRight+VolLeft); 
				wsprintfW(buf, L"Volume %i%c", 70, pct);
				SetWindowTextW(hLabel2, buf);
				wsprintfW(buf, L"Center");
				SetWindowTextW(hLabel3, buf);
				hRefreshThread = CreateThread(NULL, 0, CreateRefreshThread, hDlg, 0, NULL);
				SetDlgItemText(hDlg, IDC_EDIT1, ofn.lpstrFile);
				SetWindowText(hDlg, PlayerState);
			}
			break;
		case IDPLAY:
			if(PlayerState=="playing")
			{
				tbpos = 0;
				waveOutReset(hWaveOut);
				TerminateThread(hSoundThread, NULL);
				OpenDevice();
				hSoundThread = CreateThread(NULL, 0, CreateSoundThread, ofn.lpstrFile, 0, NULL);		// Create the thread to begin execution on its own.
				Sleep(100);
				SetWindowText(hDlg, PlayerState);
			}
			else if(PlayerState=="paused")
			{
				waveOutRestart(hWaveOut);
				PlayerState="playing";
				SetWindowText(hDlg, PlayerState);
			}
			else if(ofn.lpstrFile)
			{
				tbpos = 0;
				OpenDevice();
				hSoundThread = CreateThread(NULL, 0, CreateSoundThread, ofn.lpstrFile, 0, NULL);		// Create the thread to begin execution on its own.
				PlayerState="playing";
				SetWindowText(hDlg, PlayerState);
			}
			waveOutClose(hWaveOut);
			break;
		case IDPAUSE:
			if(PlayerState=="playing")
			{
				waveOutPause(hWaveOut);
				PlayerState="paused";
				SetWindowText(hDlg, PlayerState);
			}
			else if(PlayerState=="paused")
			{
				waveOutRestart(hWaveOut);
				PlayerState="playing";
				SetWindowText(hDlg, PlayerState);
			}
			break;
		case IDSTOP:
			waveOutReset(hWaveOut);
			PlayerState="stopped";
			wsprintfW(buf, L"%02i:%02i", 0, 0);
			SetWindowTextW(hLabel1, buf);
			SetWindowText(hDlg, PlayerState);
			break;
		case IDCANCEL:
			DestroyWindow(hDlg);
			break;
		}
		return (INT_PTR)TRUE;
		break;
	case WM_DESTROY:
		DeleteObject(TimeFont);
		DeleteObject(BGBrush);
		PostQuitMessage(0);
		break;
	case  WM_HSCROLL:
		SliderID = GetDlgCtrlID((HWND)lParam);
		switch (SliderID)
		{
		case IDC_PROGRESS_SLIDER:
			switch (wParam)
			{
			default:
				SliderInUse = true;
				break;
			case TB_ENDTRACK:
				waveOutReset(hWaveOut);
				tbpos = SendMessage((HWND)lParam, TBM_GETPOS, NULL, NULL);
				OpenDevice();
				hSoundThread = CreateThread(NULL, 0, CreateSoundThread, ofn.lpstrFile, 0, NULL);		// Create the thread to begin execution on its own.
				SetWindowText(hDlg, PlayerState);
				SliderInUse = false;
				break;
			}
			break;
		case IDC_VOLUME_SLIDER:
			tbpos = SendMessage((HWND)lParam, TBM_GETPOS, NULL, NULL);
			wsprintfW(buf, L"Volume %ld%c", tbpos, pct);
			VolRight=0xFFFF/100*tbpos/2;			//half volume (/100*100/2 = 50% max) for right channel
			VolRight=VolRight<<16;
			VolLeft=0xFFFF/100*tbpos/2;				//and left channel
			waveOutSetVolume(hWaveOut, VolRight+VolLeft); 
			SetWindowTextW(hLabel2, buf);
			break;
		case IDC_BALANCE_SLIDER:
			tbpos = SendMessage((HWND)lParam, TBM_GETPOS, NULL, NULL);
			if (tbpos == 50 )
				wsprintfW(buf, L"Center");
			else if (tbpos < 50 )
				wsprintfW(buf, L"Left %ld%c", (50-tbpos)*2, pct);
			else if (tbpos > 50 )
				wsprintfW(buf, L"Right %ld%c", (tbpos-50)*2, pct);
			VolRight=0xFFFF/100*tbpos;				//possible full volume for right channel
			VolRight=VolRight<<16;					//16 bit left shift (conversion from WORD to high order of DWORD 0xFFFF -> 0x0000FFFF -> 0xFFFF0000)
			VolLeft=0xFFFF/100*(100-tbpos);			//OR left channel
			waveOutSetVolume(hWaveOut, VolRight+VolLeft); 
			SetWindowTextW(hLabel3, buf);
			break;
		}
	case WM_PAINT:
		if(PlayerState=="playing")
		{
			waveOutGetPosition(hWaveOut, &TimePlayed, sizeof(MMTIME));						//getting TIME_BYTES from device
			LengthSec = size / wfx.nAvgBytesPerSec;											//getting file length in seconds
			TimeSec = (PointerPosBytes + TimePlayed.u.sample) / wfx.nAvgBytesPerSec;		//converting TIME_BYTES to time in seconds
			float fTimeSec = TimeSec;
			if(SliderInUse == false)
				SendMessage(GetDlgItem(hDlg, IDC_PROGRESS_SLIDER), TBM_SETPOS, TRUE, fTimeSec/LengthSec*100);
			if(TimeSec<60)
				TimeMin = 0;
			else
				TimeMin = TimeSec/60;
			TimeSec = TimeSec%60;
			wsprintfW(buf, L"%02i:%02i", TimeMin, TimeSec);
			SetWindowTextW(hLabel1, buf);
		}
		break;
	case WM_CTLCOLORDLG:
		{
			SetBkMode((HDC)wParam,TRANSPARENT);
			return (LONG)BGBrush;
		}
	case WM_CTLCOLORSTATIC:
		        // Set the colour of the text for our timer
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_STATIC1)) 
        {
			// we're about to draw the static
			// set the text colour in (HDC)lParam
			SetBkColor((HDC)wParam, RGB(50,60,50));
			SetTextColor((HDC)wParam, RGB(0,180,0));
			return (LONG)BGBrush;
        }
		else if ((HWND)lParam != GetDlgItem(hDlg, IDC_STATIC1))
			SetBkMode((HDC)wParam,TRANSPARENT);
			return (LONG)BGBrush;
	case WM_CTLCOLORBTN:
		return (LONG)BGBrush;
	case WM_CTLCOLOREDIT:
		return (LONG)BGBrush;
        break;
	default:
		break;
	}
	return (INT_PTR)FALSE;
}

void OpenDevice() 
{ 
	/*
	* first we need to set up the WAVEFORMATEX structure. 
	* the structure describes the format of the audio.
	*/
	//wfx.nSamplesPerSec = 44100;	//sample rate
	//wfx.wBitsPerSample = 16;		//sample size
	//wfx.nChannels = 2;			//channels
	/*
	* WAVEFORMATEX also has other fields which need filling.
	* as long as the three fields above are filled this should
	* work for any PCM (pulse code modulation) format.
	* don't need to fill it if program can read this data from file (RIFF)
	*/
	wfx.cbSize = 0; /* size of _extra_ info */
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
	/*
	* try to open the default wave device. WAVE_MAPPER is
	* a constant defined in mmsystem.h, it always points to the
	* default wave device on the system (some people have 2 or
	* more sound cards).
	*/
	if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) 
	{
		OutputDebugString(TEXT("unable to open WAVE_MAPPER device\n"));
		ExitProcess(1);
	}
	/*
	* device is now open so print the success message
	* and then close the device again.
	*/
	OutputDebugString(TEXT("The Wave Mapper device was opened successfully!\n"));
//	waveOutClose(hWaveOut);
} 

void writeAudioBlock(HWAVEOUT hWaveOut, LPSTR block, DWORD size)
{
	WAVEHDR header;
	/*
	 * initialise the block header with the size
	 * and pointer.
	 */
	PointerPosBytes = blockSize*tbpos/100;				//moving pointer using trackbar
	if(PointerPosBytes%2 != 0)							//ensure divisibility by 2 to not mess up the channels from different samples. without this check it plays noise 1/2 of times.
		PointerPosBytes--;
	ZeroMemory(&header, sizeof(WAVEHDR));
	header.dwBufferLength = size - PointerPosBytes;
	header.lpData = block + PointerPosBytes;
	
	/*
	 * prepare the block for playback
	 */
	waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	/*
	 * write the block to the device. waveOutWrite returns immediately
	 * unless a synchronous driver is used (not often).
	*/
	waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
	/*
	 * wait a while for the block to play then start trying
	 * to unprepare the header. this will fail until the block has
	 * played.
	*/
	Sleep(500);
	while(waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING)
	Sleep(100);
}

LPSTR loadAudioBlock(const char* filename, DWORD* blockSize)
{
	HANDLE hFile= INVALID_HANDLE_VALUE;
	size = 0;
	DWORD readBytes = 0;
	void* block = NULL;
	/*
	 * open the file
	 */
	if((hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
		return NULL;
	/*
	 * get it's size, allocate memory and read the file
	 * into memory. don't use this on large files!
	 */
	do 
	{
		if((size = GetFileSize(hFile, NULL)) == 0) 
		break;
		if((block = HeapAlloc(GetProcessHeap(), 0, size)) == NULL)
		break;
		ReadFile(hFile, block, 220, &readBytes, NULL);
		ReadFile(hFile, block, size, &readBytes, NULL);
	} while(0);
	CloseHandle(hFile);
	*blockSize = size;
	return (LPSTR)block;
}

DWORD WINAPI CreateSoundThread(LPVOID lParam)	//second thread to avoid blocking controls until playback is finished when using syncronous mode
{
	LPTSTR path = (LPTSTR)lParam;
	if((block = loadAudioBlock((LPTSTR)lParam, &blockSize)) == NULL) 
	{
		OutputDebugString(TEXT("Unable to load file\n"));
		ExitProcess(1);
	}
	writeAudioBlock(hWaveOut, block, blockSize);
	return 0;
}


DWORD WINAPI CreateRefreshThread(LPVOID lParam)		//this thread is a simple loop that refresh player and controls every X seconds
{
	HWND hDlg = (HWND)lParam;
	while(1)
	{
		Sleep(500);
		SendMessage(hDlg, WM_PAINT, 0, 0);
	}
	return 0;
}