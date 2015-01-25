// DeckControl.cpp : Defines the entry point for the console application.
//
/* -LICENSE-START- 
** Copyright (c) 2009 Blackmagic Design 
** 
** Permission is hereby granted, free of charge, to any person or organization 
** obtaining a copy of the software and accompanying documentation covered by 
** this license (the "Software") to use, reproduce, display, distribute, 
** execute, and transmit the Software, and to prepare derivative works of the 
** Software, and to permit third-parties to whom the Software is furnished to 
** do so, all subject to the following: 
**  
** The copyright notices in the Software and this entire statement, including 
** the above license grant, this restriction and the following disclaimer, 
** must be included in all copies of the Software, in whole or in part, and 
** all derivative works of the Software, unless such copies or derivative 
** works are solely in the form of machine-executable object code generated by 
** a source language processor. 
**  
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
** DEALINGS IN THE SOFTWARE. 
** -LICENSE-END- 
*/ 
/* Deck Control Sample Application.  
   This application assumes VTR is compatible with SONY RS-422 9 pin remote protocol.  
*/

#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <objbase.h>		// Necessary for COM
#include <comutil.h>
#include "DeckLinkAPI_h.h"
#include "stdafx.h"
#define COMM_BUFFER_SIZE		20 

HANDLE		hCommPort = INVALID_HANDLE_VALUE;

BOOL	openSerialDevice(char* serialName); 
void	closeSerialDevice();  
void	playCommand(void); 
void	timeCodeCommand(void); 
void	stopCommand(void);
void	errorMessage(DWORD dwError);

int _tmain(int argc, _TCHAR* argv[])
{
	IDeckLinkIterator*		deckLinkIterator; 
	IDeckLink*				deckLink; 
	int						numDevices = 0; 
	HRESULT					result; 

	printf("DeckControl Test Application\n\n"); 
	// Initialize COM on this thread
	result = CoInitialize(NULL);
	if (FAILED(result))
	{
		fprintf(stderr, "Initialization of COM failed - result = %08x.\n", result);
		return 1;
	}
	
	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)&deckLinkIterator);
	if (FAILED(result))
	{
		// Uninitalize COM on this thread
		CoUninitialize();
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		return 1;
	}

	// Enumerate all cards in this system 
	while (deckLinkIterator->Next(&deckLink) == S_OK) 
	{ 
		BSTR		deviceNameBSTR = NULL; 
		 
		// Increment the total number of DeckLink cards found 
		numDevices++; 
		if (numDevices > 1) 
			printf("\n\n");	 
		 
		// Print the model name of the DeckLink card 
		result = deckLink->GetModelName(&deviceNameBSTR); 
		if (result == S_OK) 
		{ 
			IDeckLinkAttributes*	deckLinkAttributes; 
 		 	HRESULT			attributeResult; 
			BSTR			serialNameBSTR = NULL; 
			BOOL			serialSupported; 
			 
			_bstr_t		deviceName(deviceNameBSTR);
			printf("Found Blackmagic device: %s\n", (char*)deviceName); 
			 
			attributeResult = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes); 
 			if (attributeResult != S_OK) 
			{ 
				fprintf(stderr, "Could not obtain the IDeckLinkAttributes interface"); 
			} 
			else 
			{ 
				attributeResult = deckLinkAttributes->GetFlag(BMDDeckLinkHasSerialPort, &serialSupported);	// are serial ports supported on device? 
				if (attributeResult == S_OK && serialSupported)			 
				{							 
					attributeResult = deckLinkAttributes->GetString(BMDDeckLinkSerialPortDeviceName, &serialNameBSTR);	// get serial port name 
					if (attributeResult == S_OK) 
					{
						_bstr_t		portName(serialNameBSTR); 
					
						printf("Serial port name: %s\n",(char*)portName); 				 

						if (openSerialDevice(portName)== TRUE)		// open serial port 
						{ 
							printf("Device opened\n"); 
							playCommand();									// Play deck,  
							printf("Delay 3 seconds\n"); 
							Sleep(3000); 
							timeCodeCommand();								// DisplayTC 
							printf("Delay 3 seconds\n"); 
 							Sleep(3000); 
							stopCommand();									// Stop deck 
							closeSerialDevice();							// close serial port 
						}					 
						else printf("Device open fail\n");									 
					}
 					else printf("Unable to get serial port device name\n"); 
 				} 
				else printf("Serial port not supported\n"); 

				// Release the attribute interface
				deckLinkAttributes->Release();
			} 
 		}		 
		deckLink->Release(); 
 	}

	// release the iterator
	deckLinkIterator->Release();

	// Uninitalize COM on this thread
	CoUninitialize();

	// If no DeckLink cards were found in the system, inform the users
	if (numDevices == 0) 
		printf("No Blackmagic Design devices were found.\n"); 
 	printf("\n");	

    return 0;
 } 

BOOL	openSerialDevice(char*  serialName) 
{ 
	BOOL			serialOpen = FALSE; 
	_bstr_t			portName(serialName);
	COMMTIMEOUTS	timeouts;

	printf("Opening COM port for read/write ...\n");
	hCommPort = CreateFile(serialName, GENERIC_READ | GENERIC_WRITE, 0, 
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hCommPort == INVALID_HANDLE_VALUE)
	{
		printf("FAIL: CreateFile(), LastError = %08X\n", GetLastError());
		goto bail;
	}

	serialOpen = TRUE;

	// set read timeouts
	timeouts.ReadIntervalTimeout = 0;
	timeouts.ReadTotalTimeoutConstant = 15;	// standard says the deck should replies within 9 ms
	timeouts.ReadTotalTimeoutMultiplier = 0;

	// mark write timeouts as not used
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;

	if (SetCommTimeouts(hCommPort, &timeouts)==FALSE)
		printf("Error setting timeouts\n");

bail:
 	return serialOpen; 
} 

void	closeSerialDevice() 
{ 
	if (hCommPort != INVALID_HANDLE_VALUE)
	{
		printf("Closing COM port ...\n");
		CloseHandle(hCommPort);
	}
	else 
		printf("Invalid serial handle\n");

	printf("Device closed\n"); 
 } 
  
 void	playCommand() 
 { 
	DWORD			byteCount=0;
	unsigned char	transmitBuffer[COMM_BUFFER_SIZE] = {0}; 
 	unsigned char	receiveBuffer[COMM_BUFFER_SIZE] = {0}; 
 	 
	printf("\nSending PlayCommand"); 
 	transmitBuffer[0] = 0x20;		// Play command 
 	transmitBuffer[1] = 0x01; 
 	transmitBuffer[2] = 0x21; 

	try
	{
		if (!WriteFile(hCommPort, transmitBuffer, 3, &byteCount, NULL))
		{
			printf("\nFAIL: WriteFile()\n");
			throw false;
		}

		if (!ReadFile(hCommPort, receiveBuffer, COMM_BUFFER_SIZE, &byteCount, NULL))
		{
			printf("\nFAIL: ReadFile()\n");
			throw false;
		}
		
		printf("Received: ");
		for (unsigned int i=0; i < byteCount; i++)
			printf(" %02X", receiveBuffer[i]);
		printf("\n");
	}
	catch (...)
	{
		DWORD dwError = GetLastError();					// additional debug information
		errorMessage(dwError);
	}
 } 
  
 void	stopCommand() 
 { 
	DWORD			byteCount=0;
 	unsigned char	transmitBuffer[COMM_BUFFER_SIZE] = {0}; 
 	unsigned char	receiveBuffer[COMM_BUFFER_SIZE] = {0}; 

	printf("\nSending StopCommand");	 
 	transmitBuffer[0] = 0x20;		// Stop command 
 	transmitBuffer[1] = 0x00; 
 	transmitBuffer[2] = 0x20; 

	try
	{
		if (!WriteFile(hCommPort, transmitBuffer, 3, &byteCount, NULL))
		{
			printf("\nFAIL: WriteFile()\n");
			throw false;
		}

		if (!ReadFile(hCommPort, receiveBuffer, COMM_BUFFER_SIZE, &byteCount, NULL))
		{
			printf("\nFAIL: ReadFile()\n");
			throw false;
		}

		printf("Received: ");
		for (unsigned int i=0; i < byteCount; i++)
			printf(" %02X", receiveBuffer[i]);
		printf("\n");
	}
	catch (...)
	{
		DWORD dwError = GetLastError();					// additional debug information
		errorMessage(dwError);
	}
 } 
 
 void	timeCodeCommand() 
{ 
	DWORD byteCount=0;
	unsigned char	transmitBuffer[COMM_BUFFER_SIZE] = {0}; 
 	unsigned char	receiveBuffer[COMM_BUFFER_SIZE] = {0}; 
	 
 	printf("\ntimeCodeCommand");	 

	transmitBuffer[0] = 0x61;		// Get Time Sense command 
 	transmitBuffer[1] = 0x0c; 
	transmitBuffer[2] = 0x03; 
 	transmitBuffer[3] = 0x70; 

	try
	{
		if (!WriteFile(hCommPort, transmitBuffer, 4, &byteCount, NULL))
		{
			printf("\nFAIL: WriteFile()\n");
			throw false;
		}

		if (!ReadFile(hCommPort, receiveBuffer, COMM_BUFFER_SIZE, &byteCount, NULL))
		{
			printf("\nFAIL: ReadFile()\n");
			throw false;
		}

		printf("\nReceived: "); 
		for (unsigned int i = 0; i < byteCount; i++)
			printf(" %02X", receiveBuffer[i]);
 		printf("\n");	 

		if ((receiveBuffer[0]==0x74) && ((receiveBuffer[1]==0x04) || (receiveBuffer[1]==0x06)))	// LTC time data 
		{ 
			printf("\nTC %02x:%02x:%02x:%02x\n",receiveBuffer[5],receiveBuffer[4],receiveBuffer[3],receiveBuffer[2]); 		 
		}
	}
	catch (...)
	{
		DWORD dwError = GetLastError();					// additional debug information
		errorMessage(dwError);
	}
}

void errorMessage(DWORD dwError)	// Provides a detailed error message
{
	LPVOID lpMsgBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

	printf("\nError: %08X: \n%s",dwError, (LPCTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);
}