// Shree Ganeshay namha

// header files
#include <windows.h>
#include "header.h"

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

// CUI sathi stdio.h GUI saathi windows.h

// Global Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 1. This function is called by OS
// 2. OS laa disava mahnun declared globally
// 3. you cannot decide number of parametres of CALLBACK, tee kaam OS chh
// 

// int __stdcall WinMain asa lihu shakto
// int __stdcall WinMain(unsigned int, unsigned int , char*, int)
// pnn ithe error yeu shakto #define STRICT mule



// Entry-Point Function
int WINAPI WinMain
(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpszCmdLine,
	int iCmdShow// internalyy yoo iCmdShow SW_
)
{
	// variable declarations
	WNDCLASSEX wndclass;
	HWND hwnd = NULL;
	MSG msg;
	TCHAR lpszAppName[] = TEXT("RTR7_Prashant");


	//code
	
	// 1. Initialization of all members of struct WNDCLASSEX
	wndclass.cbSize		        =   sizeof(WNDCLASSEX); //----------------------------------Nantrr alaa
	wndclass.style		        =   CS_HREDRAW | CS_VREDRAW;// CS mahnjhe Class Style
	wndclass.cbClsExtra         =   0;
	wndclass.cbWndExtra         =   0;
	wndclass.lpfnWndProc        =   WndProc;
	wndclass.hInstance          =   hInstance;
	wndclass.hbrBackground      =   (HBRUSH)GetStockObject(WHITE_BRUSH);// My 1st api
	wndclass.hIcon		        =   LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));// 2nd api
	wndclass.hCursor            =   LoadCursor(NULL, IDC_ARROW);// 3rd api
	wndclass.lpszClassName      =   lpszAppName;
	wndclass.lpszMenuName       =   NULL;													
	wndclass.hIconSm            =   LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));//-----------------------Nantr aala
							   
	// 2. Register Above WNDCLASS
	RegisterClassEx(&wndclass); // My 4th api

    // Centering
	
	int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 3. Create Window
	hwnd = CreateWindow(
		lpszAppName,
		TEXT("My First WIN32 RTR-7 Program: Prashant Gharge"),
		WS_OVERLAPPEDWINDOW,
		screenWidth/2 - WIN_WIDTH/2,// x co-ordinate
		screenHeight/2 - WIN_HEIGHT/2,// y co-oridnate
		WIN_WIDTH,// Width
		WIN_HEIGHT,// Height
		NULL,// Parent Window Handle (BACH chi goshta)
		NULL,
		hInstance,
		NULL
	);// 5th API

	// 4. Show Window
	ShowWindow(hwnd, iCmdShow);// 6th API
	
	// 5. Update the window to paint its background
	UpdateWindow(hwnd);// 7th API

	// 6. Message Loop
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);// 8th API
		DispatchMessage(&msg);// 9th API
	}

	return((int)msg.wParam);
}

LRESULT CALLBACK WndProc(
   HWND hwnd, UINT iMsg, 
   WPARAM wParam, LPARAM lParam
)
{
	//code
	switch (iMsg)
	{
		case WM_DESTROY:
			PostQuitMessage(0);// 10th API
			break;
		default:
			break;
	}

	return(DefWindowProc(hwnd, iMsg, wParam, lParam));// 11th API
}

/*

	cl.exe /c /EHsc Window.c

	link.exe Window.obj User32.lib GDI32.lib /SUBSYSTEM:WINDOWS

*/
