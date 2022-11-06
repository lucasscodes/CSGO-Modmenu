#include <Windows.h>
#include <TlHelp32.h>
#include "csgo.hpp"
#include <iostream>
using namespace std;

// This file is stil broken and I need to debug it further more, 
// also here is stuff missing from the working version!

/* Offsets-Generator: https://github.com/frk1/hazedumper   */

#define dwEntityList ::hazedumper::signatures::dwEntityList
#define dwViewMatrix ::hazedumper::signatures::dwViewMatrix
#define m_iTeamNum ::hazedumper::netvars::m_iTeamNum
#define m_iHealth ::hazedumper::netvars::m_iHealth
#define m_vecOrigin ::hazedumper::netvars::m_vecOrigin

uintptr_t moduleBase;
HANDLE TargetProcess;
HPEN RedPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
HPEN GreenPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
HPEN BluePen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
RECT WBounds;
HWND EspHWND;

template<typename T> T RPM(SIZE_T address) {
	T buffer;
	ReadProcessMemory(TargetProcess, (LPCVOID)address, &buffer, sizeof(T), NULL);
	return buffer;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName)
{
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				if (!_wcsicmp(modEntry.szModule, modName))
				{
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}

struct Vector3 {
	float x, y, z;
};

struct view_matrix_t {
	float matrix[16];
};

struct Vector3 WorldToScreen(const struct Vector3 pos, struct view_matrix_t matrix) {
	struct Vector3 out;
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	int width = WBounds.right - WBounds.left;
	int height = WBounds.bottom + WBounds.left;

	out.x = width * .5f;
	out.y = height * .5f;

	out.x += 0.5f * _x * width + 0.5f;
	out.y -= 0.5f * _y * height + 0.5f;

	return out;
}

void Draw(HDC hdc, Vector3 foot, Vector3 head, HPEN pen) {
	float height = head.y - foot.y;
	float width = height / 2.4f;
	SelectObject(hdc, pen);
	Rectangle(hdc, foot.x - (width / 2), foot.y, head.x + (width / 2), head.y);
}

void printMatrix(view_matrix_t mat) {
	for (int n = 0; n < 4; n++) {
		for (int m = 0; m < 4; m++) {
			cout << mat.matrix[n*4 + m] << " ";
		}
		cout << endl;
	}
	cout << endl;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	std::cout << "WndProc called!..." << std::endl;
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC Memhdc;
		HDC hdc;
		HBITMAP Membitmap;

		int win_width = WBounds.right - WBounds.left;
		int win_height = WBounds.bottom - WBounds.top;

		hdc = BeginPaint(hwnd, &ps);
		Memhdc = CreateCompatibleDC(hdc);
		Membitmap = CreateCompatibleBitmap(hdc, win_width, win_height);
		SelectObject(Memhdc, Membitmap);
		FillRect(Memhdc, &WBounds, WHITE_BRUSH);

		view_matrix_t vm = RPM<view_matrix_t>(moduleBase + dwViewMatrix);
		//printMatrix(vm); // TODO: Seems to be broken!
		int localteam = RPM<int>(RPM<DWORD>(moduleBase + dwEntityList) + m_iTeamNum);

		for (int i = 1; i < 11; i++) {
			uintptr_t pEnt = RPM<DWORD>(moduleBase + dwEntityList + (i * 0x10));
			int team = RPM<int>(pEnt + m_iTeamNum);
			if (team == localteam) {
				int health = RPM<int>(pEnt + m_iHealth);
				Vector3 pos = RPM<Vector3>(pEnt + m_vecOrigin);
				// TODO: Got a better function for this in the first verison
				Vector3 head; head.x = pos.x; head.y = pos.y; head.z = pos.z + 72.f; //Height seems to be 72units
				Vector3 screenpos = WorldToScreen(pos, vm);
				Vector3 screenhead = WorldToScreen(head, vm);
				float height = screenhead.y - screenpos.y;
				float width = height / 2.4f;

				if (screenpos.z >= 0.01f && health > 0 && health < 101) {
					Draw(Memhdc, screenpos, screenhead, GreenPen);
				}
			}
			if (team != localteam) {
				int health = RPM<int>(pEnt + m_iHealth);
				Vector3 pos = RPM<Vector3>(pEnt + m_vecOrigin);
				Vector3 head; head.x = pos.x; head.y = pos.y; head.z = pos.z + 72.f;
				Vector3 screenpos = WorldToScreen(pos, vm);
				Vector3 screenhead = WorldToScreen(head, vm);
				float height = screenhead.y - screenpos.y;
				float width = height / 2.4f;

				if (screenpos.z >= 0.01f && health > 0 && health < 101) {
					Draw(Memhdc, screenpos, screenhead, RedPen);
				}
			}
		}
		BitBlt(hdc, 0, 0, win_width, win_height, Memhdc, 0, 0, SRCCOPY);
		DeleteObject(Membitmap);
		DeleteDC(Memhdc);
		DeleteDC(hdc);
		EndPaint(hwnd, &ps);
		ValidateRect(hwnd, &WBounds);
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

DWORD WorkLoop() {
	while (1) {
		InvalidateRect(EspHWND, &WBounds, true);
		Sleep(16); //16 ms * 60 fps ~ 1000 ms
		//Every 16ms take old draw away
		//std::cout << "test" << std::endl; //works!
	}
}

int main() {
	HWND GameHWND = FindWindowA(0, "Counter-Strike: Global Offensive - Direct3D 9");
	GetClientRect(GameHWND, &WBounds);
	DWORD dwPid; GetWindowThreadProcessId(GameHWND, &dwPid);
	//std::cout << dwPid << std::endl; //Works!
	HANDLE targetProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, dwPid);
	std::cout << "Trying to get moduleBaseAdress..." << std::endl;
	uintptr_t moduleBase = GetModuleBaseAddress(dwPid, L"client.dll");
	std::cout << "Found: " << moduleBase << std::endl;

	WNDCLASSEXA WClass;
	MSG Msg;
	WClass.cbSize = sizeof(WNDCLASSEXA);
	WClass.style = NULL;
	WClass.lpfnWndProc = WndProc; //Defines our logic-func as proc
	WClass.cbClsExtra = NULL;
	WClass.cbWndExtra = NULL;
	WClass.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongA(GameHWND, GWL_HINSTANCE));
	WClass.hIcon = NULL;
	WClass.hCursor = NULL;
	WClass.hbrBackground = WHITE_BRUSH;
	WClass.lpszMenuName = " ";
	WClass.lpszClassName = " ";
	WClass.hIconSm = NULL;
	RegisterClassExA(&WClass);


	std::cout << "Reached histance!" << std::endl;
	HINSTANCE Hinstance = NULL;
	EspHWND = CreateWindowExA(WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED, " ", " ", WS_POPUP, WBounds.left, WBounds.top, WBounds.right - WBounds.left, WBounds.bottom + WBounds.left, NULL, NULL, Hinstance, NULL);

	SetLayeredWindowAttributes(EspHWND, RGB(255, 255, 255), 255, LWA_COLORKEY);
	ShowWindow(EspHWND, 1);
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&WorkLoop, NULL, NULL, NULL);
	std::cout << "Reached while!" << std::endl;
	while (GetMessageA(&Msg, NULL, NULL, NULL) > 0) {
		TranslateMessage(&Msg);
		DispatchMessageA(&Msg);
		Sleep(1);
	}
	ExitThread(0);
	return 0;
}