// Modmenu.cpp : This file contains the main-function. This is the entry- and exit-point of the execution.
//

#include <cstdint>
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include "csgo.hpp"
using namespace std;

int screenX = GetSystemMetrics(SM_CXSCREEN);
int screenY = GetSystemMetrics(SM_CYSCREEN);

int EnemyPen = 0x000000FF;//Red from https://imagecolorpicker.com/color-code/ff0000, endian swap?
int FriendPen = 0x0000FF00;//Green?
HBRUSH EnemyBrush = CreateSolidBrush(EnemyPen);
HBRUSH FriendBrush = CreateSolidBrush(FriendPen);

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

DWORD GetProcId(const wchar_t* procName)
{
	DWORD procId = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(procEntry);

		if (Process32First(hSnap, &procEntry))
		{
			do
			{
				if (!_wcsicmp(procEntry.szExeFile, procName))
				{
					procId = procEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(hSnap, &procEntry));

		}
	}
	CloseHandle(hSnap);
	return procId;
}
DWORD procId = GetProcId(L"csgo.exe");
HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, procId);
uintptr_t moduleBase = GetModuleBaseAddress(procId,L"client.dll");
HDC hdc = GetDC(FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9"));

template<typename T> T RPM(SIZE_T address) {
	//The buffer for data that is going to be read from memory
	T buffer;

	//The actual RPM
	ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), NULL);

	//Return our buffer
	return buffer;
}
struct view_matrix_t {
	float* operator[ ](int index) {
		return matrix[index];
	}
	float matrix[4][4];
};
struct Vector3 {
	float x, y, z;
};
Vector3 WorldToScreen(const Vector3 pos, view_matrix_t matrix) {
	float _x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
	float _y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];

	float w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];

	float inv_w = 1.f / w;
	_x *= inv_w;
	_y *= inv_w;

	float x = screenX * .5f;
	float y = screenY * .5f;

	x += 0.5f * _x * screenX + 0.5f;
	y -= 0.5f * _y * screenY + 0.5f;

	return { x,y,w };
}


/*
Takes a Point on the Screen and creates a rectangle with +width/+height
E.g. 10,10,5,5 will draw a rect with edges (10,10),(10,15),(15,10),(15,15)
*/
void DrawFilledRect(int x, int y, int w, int h, HBRUSH brush) {
	RECT rect = {x, y, x+w, y+h};
	FillRect(hdc, &rect, brush);
}
void DrawBorderBox(int x, int y, int w, int h, int thickness, HBRUSH brush) {
	DrawFilledRect(x, y, w, thickness, brush);
	DrawFilledRect(x, y, thickness, h, brush);
	DrawFilledRect(x+w, y, thickness, h, brush);
	DrawFilledRect(x, y+h, w+thickness, thickness, brush);
}
void DrawLine(float StartX, float StartY, float EndX, float EndY, int pen) {
	int a, b = 0;
	HPEN hOPen;
	HPEN hNPen = CreatePen(PS_SOLID, 2, pen);// penstyle, width, color
	hOPen = (HPEN)SelectObject(hdc, hNPen);
	MoveToEx(hdc, StartX, StartY, NULL); //start
	a = LineTo(hdc, EndX, EndY); //end
	DeleteObject(SelectObject(hdc, hOPen));
}

void printMatrix(view_matrix_t mat) {
	for (int n=0; n<4; n++) {
		for (int m=0; m<4; m++) {
			cout << mat[n][m] << " ";
		}
		cout << endl;
	}
	cout << endl;
}

int main()
{
	//cout << screenX << "x" << screenY << endl;
	//printOsList(osList);
	//cout << moduleBase << endl;//Is 0???

	//To hide the console, change from debugMode to releaseMode!

	int fps = 60; //Desired target fps
	int ms = 1000 / fps;
	int draws = true; //15sec should be enough to test
	while (draws) {
		//DrawFilledRect(200,200,50,50, EnemyBrush);//Works!
		//DrawBorderBox(201, 201, 47, 47, 1, FriendBrush);//Works!

		view_matrix_t vm = RPM<view_matrix_t>(moduleBase + ::hazedumper::signatures::dwViewMatrix);
		//printMatrix(vm); //Looks legit!
		/*0.684756 0.474925 1.91301e-08 815.756
			- 0.399069 0.575387 1.13466 - 131.16
			- 0.48511 0.699441 - 0.525304 203.979
			- 0.48499 0.699268 - 0.525175 210.929*/

		int localteam = RPM<int>(RPM<DWORD>(moduleBase + ::hazedumper::signatures::dwEntityList) + ::hazedumper::netvars::m_iTeamNum);
		// cout << localteam; //2=t 3=ct 1=spec

		//cout << procId << endl; //Works!

		for (int i = 1; i < 10; i++) {
			uintptr_t pEnt = RPM<DWORD>(moduleBase + ::hazedumper::signatures::dwEntityList + (i * 0x10));
			int health = RPM<int>(pEnt + ::hazedumper::netvars::m_iHealth);
			int team = RPM<int>(pEnt + ::hazedumper::netvars::m_iTeamNum);

			Vector3 pos = RPM<Vector3>(pEnt + ::hazedumper::netvars::m_vecOrigin);
			DWORD boneMat = RPM<DWORD>(pEnt + ::hazedumper::netvars::m_dwBoneMatrix);
			//https://guidedhacking.com/threads/how-to-find-the-bone-matrix-use-m_dwbonematrix.14005/
			Vector3 head;
			head.x = RPM<FLOAT>(boneMat + 0x30 * 8 + 0x0C);
			head.y = RPM<FLOAT>(boneMat + 0x30 * 8 + 0x1C);
			head.z = RPM<FLOAT>(boneMat + 0x30 * 8 + 0x2C) +7.5f; //Without offset headpos is eye-height
			Vector3 screenpos = WorldToScreen(pos, vm);
			Vector3 screenhead = WorldToScreen(head, vm);
			float height = screenhead.y - screenpos.y;
			float width = height / 2.4f;

			if (screenpos.z >= .01f && team != localteam && health > 0 && health < 101) {
				DrawBorderBox(screenpos.x - (width / 2),screenpos.y, width, height, 1, EnemyBrush);
				DrawLine(screenX / 2, screenY - (screenY >> 3), screenpos.x, screenpos.y, EnemyPen);
			}
			if (screenpos.z >= .01f && team == localteam && health > 0 && health < 101) {
				DrawBorderBox(screenpos.x - (width / 2), screenpos.y, width, height, 1, FriendBrush);
			}
		}

		Sleep(ms);
	}
    return 0;
}