#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <string>
#include <winioctl.h>  

#define MAX_PACKET_SIZE 0x10000
#define PIPE_OPEN_IO_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)  
#define PIPE_MSG_IO_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)  

static const std::string	gsc_szSymLink	= "\\\\.\\PipeClient_Test";
static HANDLE				gs_hDriver		= INVALID_HANDLE_VALUE;
static const std::string	gs_szPipeName	= "\\\\.\\pipe\\TestCommPipe";;

// IOCTL
typedef struct _KERNEL_IO_MSG_DATA
{
	CHAR szMessage[255];
} SKernelIOMsgData, *PKernelIOMsgData;

bool OpenPipePacket()
{
	auto bRet = DeviceIoControl(gs_hDriver, PIPE_OPEN_IO_CODE, nullptr, 0, nullptr, 0, 0, NULL);
	printf("Open IO completed! Result: %d\n", bRet);

	return (bRet == TRUE);
}

bool MessagePacket()
{
	printf("Enter message: ");

	auto szMessage = std::string("");
	std::cin >> szMessage;

	SKernelIOMsgData pData;
	strcpy(pData.szMessage, szMessage.c_str());

	auto bRet = DeviceIoControl(gs_hDriver, PIPE_MSG_IO_CODE, &pData, sizeof(pData), &pData, sizeof(pData), 0, NULL);
	printf("Msg IO completed! Result: %d\n", bRet);

	return true;
}

// PIPE
bool IsValidHandle(HANDLE hTarget)
{
	auto dwInfo = 0UL;
	if (GetHandleInformation(hTarget, &dwInfo) == false)
		return false;
	return true;
}

bool Read(HANDLE hPipe, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead)
{
	if (!hPipe)
	{
		printf("Target pipe must be open\n");
		return false;
	}

	auto bSuccess = ::ReadFile(hPipe, lpBuffer, dwNumberOfBytesToRead, NULL, NULL);
	if (!bSuccess)
	{
		printf("ReadFile fail, Error: %u\n", GetLastError());
		return false;
	}
	return true;
}

template <class T>
bool Read(HANDLE hPipe, T& data) 
{ 
	return Read(hPipe, &data, sizeof(T));
}

DWORD WINAPI InstanceThread(LPVOID lpvParam)
{
	printf("InstanceThread created, receiving and processing messages.\n");

	auto hPipe = (HANDLE)lpvParam;
	if (!hPipe)
		return 0;

	BYTE pData[MAX_PACKET_SIZE];
	while (true)
	{
		if (Read(hPipe, pData) == false)
		{
			auto dwError = GetLastError();
			if (dwError == ERROR_BROKEN_PIPE)
			{
				printf("IO Client disconnected\n");
				break;
			}
			else if (!hPipe || IsValidHandle(hPipe) == false)
			{
				printf("Client has closed the connection or pipe got broken\n");
				break;
			}
			else
			{
				printf("IO ReadFile fail! Error: %u\n", dwError);
				break;
			}
		}

		auto szData = reinterpret_cast<const char*>(pData);
		printf("*** Pipe msg: %s\n", szData);

		FlushFileBuffers(hPipe);
		Sleep(100);
	}

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	printf("InstanceThread exitting.\n");
	return 1;
}

DWORD WINAPI CreatePipeServer(LPVOID)
{
	SECURITY_DESCRIPTOR sd;
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, static_cast<PACL>(0), FALSE);

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = FALSE;

	while (true)
	{
		auto hPipe = CreateNamedPipeA(gs_szPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, MAX_PACKET_SIZE, MAX_PACKET_SIZE, 0, &sa);
		if (!hPipe || hPipe == INVALID_HANDLE_VALUE)
		{
			printf("CreateNamedPipeA fail, Error: %u\n", GetLastError());
			return 0;
		}
		printf("Pipe handle succesfully created: %p\n", hPipe);

		auto bConnect = ::ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (bConnect)
		{
			printf("Client connected, creating a processing thread.\n");

			// Create a thread for this client. 
			auto dwThreadId = 0UL;
			auto hThread	= CreateThread(NULL, 0, InstanceThread, (LPVOID)hPipe, 0, &dwThreadId);

			if (!hThread || hThread == INVALID_HANDLE_VALUE)
			{
				printf("CreateThread fail, Error: %u\n", GetLastError());
				return 0;
			}
			CloseHandle(hThread);
		}
		else
		{
			CloseHandle(hPipe); // The client could not connect, so close the pipe. 
		}
	}
	return 0;
}


// Routine
int main()
{
	printf("Communication CLI started! Target device: %s\n", gsc_szSymLink.c_str());

	gs_hDriver = CreateFileA(gsc_szSymLink.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (!gs_hDriver || gs_hDriver == INVALID_HANDLE_VALUE)
	{
		printf("CreateFileA fail! Error: %u", GetLastError());
		return 1;
	}
	printf("Handle succesfully created: %p\n", gs_hDriver);

	auto hPipeServerThread = CreateThread(0, 0, CreatePipeServer, 0, 0, 0);

	printf("--------------------------------------\n");

	char pInput = '0';
	while (pInput != 'x')
	{
		printf("Please select:\n1 --> Open pipe packet\n2 --> Message packet\nx --> Exit\n");
		std::cin >> pInput;

		switch (pInput)
		{
			case '1':
			{
				OpenPipePacket();
			} break;
			
			case '2':
			{
				MessagePacket();
			} break;

			case 'x':
				return 0;

			default:
				continue;
		}
	}

    return 0;
}

