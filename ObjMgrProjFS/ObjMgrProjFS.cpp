// ObjMgrProjFS.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "ObjectManagerProjection.h"

#pragma comment(lib, "ProjectedFSLib")

int Error(HRESULT hr) {
	printf("Error (HR=0x%X)\n", hr);
	return hr;
}


int wmain(int argc, const wchar_t* argv[]) {
	if (argc < 2) {
		printf("Usage: ObjMgrProjFS <root_dir>\n");
		return 0;
	}

	ObjectManagerProjection omp;
	if (auto hr = omp.Init(argv[1]); hr != S_OK)
		return Error(hr);


	if (auto hr = omp.Start(); hr != S_OK)
		return Error(hr);

	printf("Press ENTER to stop virtualizing...\n");
	char buffer[3];
	gets_s(buffer);

	omp.Term();

	return 0;
}
