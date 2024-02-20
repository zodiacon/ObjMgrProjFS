#include "pch.h"
#include "ObjectManager.h"
#include <winternl.h>
#include "Helpers.h"

typedef struct _OBJECT_DIRECTORY_INFORMATION {
	UNICODE_STRING Name;
	UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION, * POBJECT_DIRECTORY_INFORMATION;

#define DIRECTORY_QUERY  0x0001

extern "C" {
	NTSTATUS NTAPI NtOpenDirectoryObject(
		_Out_ PHANDLE hDirectory,
		_In_ ACCESS_MASK AccessMask,
		_In_ POBJECT_ATTRIBUTES ObjectAttributes);

	NTSTATUS NTAPI NtQuerySymbolicLinkObject(
		_In_ HANDLE LinkHandle,
		_Inout_ PUNICODE_STRING LinkTarget,
		_Out_opt_ PULONG ReturnedLength);

	NTSTATUS NTAPI NtQueryDirectoryObject(
		_In_  HANDLE hDirectory,
		_Out_ POBJECT_DIRECTORY_INFORMATION DirectoryEntryBuffer,
		_In_  ULONG DirectoryEntryBufferSize,
		_In_  BOOLEAN  bOnlyFirstEntry,
		_In_  BOOLEAN bFirstEntry,
		_In_  PULONG  EntryIndex,
		_Out_ PULONG  BytesReturned);
	NTSTATUS NTAPI NtOpenSymbolicLinkObject(
		_Out_  PHANDLE LinkHandle,
		_In_   ACCESS_MASK DesiredAccess,
		_In_   POBJECT_ATTRIBUTES ObjectAttributes);

}

#pragma comment(lib, "ntdll")

std::vector<ObjectNameAndType> ObjectManager::EnumDirectoryObjects(PCWSTR path, PCWSTR objectName, std::function<bool(PCWSTR)> compare) {
	std::vector<ObjectNameAndType> objects;
	HANDLE hDirectory;
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name;
	std::wstring spath(path);
	if (spath[0] != L'\\')
		spath = L'\\' + spath;

	std::wstring object(objectName ? objectName : L"");
	object = Helpers::Normalize(object);

	//
	// denormalize
	//
	spath = Helpers::Normalize(spath, true);

	RtlInitUnicodeString(&name, spath.c_str());
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	if (!NT_SUCCESS(NtOpenDirectoryObject(&hDirectory, DIRECTORY_QUERY, &attr)))
		return objects;

	objects.reserve(128);
	BYTE buffer[1 << 12];
	auto info = reinterpret_cast<OBJECT_DIRECTORY_INFORMATION*>(buffer);
	bool first = true;
	ULONG size, index = 0;
	for (;;) {
		auto start = index;
		if (!NT_SUCCESS(NtQueryDirectoryObject(hDirectory, info, sizeof(buffer), FALSE, first, &index, &size)))
			break;
		first = false;
		for (ULONG i = 0; i < index - start; i++) {
			ObjectNameAndType data;
			auto& p = info[i];
			data.Name = Helpers::Normalize(std::wstring(p.Name.Buffer, p.Name.Length / sizeof(WCHAR)));
			if(compare && !compare(data.Name.c_str()))
				continue;
			data.TypeName = std::wstring(p.TypeName.Buffer, p.TypeName.Length / sizeof(WCHAR));
			if(!objectName)
				objects.push_back(std::move(data));
			if (objectName && _wcsicmp(object.c_str(), data.Name.c_str()) == 0) {
				objects.push_back(std::move(data));
				break;
			}
		}
	}
	::CloseHandle(hDirectory);
	return objects;

}

bool ObjectManager::DirectoryExists(PCWSTR path) {
	std::wstring spath(path);
	if (spath[0] != L'\\')
		spath = L"\\" + spath;
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name;
	RtlInitUnicodeString(&name, spath.c_str());
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	HANDLE hDirectory;
	auto status = NtOpenDirectoryObject(&hDirectory, DIRECTORY_QUERY, &attr);
	::CloseHandle(hDirectory);
	return NT_SUCCESS(status);
}

std::optional<ObjectNameAndType> ObjectManager::ObjectExists(PCWSTR path) {
	std::wstring spath(path);
	if (spath[0] != L'\\')
		spath = L"\\" + spath;

	auto bs = spath.rfind(L'\\');
	if (bs == std::wstring::npos)
		return {};

	auto objects = EnumDirectoryObjects(spath.substr(0, bs).c_str(), spath.substr(bs + 1).c_str());
	return objects.empty() ? std::optional<ObjectNameAndType>() : objects[0];
}

std::wstring ObjectManager::GetSymbolicLinkTarget(PCWSTR path) {
	std::wstring spath(path);
	if (spath[0] != L'\\')
		spath = L"\\" + spath;

	HANDLE hLink;
	OBJECT_ATTRIBUTES attr;
	std::wstring target;
	UNICODE_STRING name;
	RtlInitUnicodeString(&name, spath.c_str());
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	if (NT_SUCCESS(NtOpenSymbolicLinkObject(&hLink, GENERIC_READ, &attr))) {
		WCHAR buffer[1 << 10];
		UNICODE_STRING result;
		result.Buffer = buffer;
		result.MaximumLength = sizeof(buffer);
		if (NT_SUCCESS(NtQuerySymbolicLinkObject(hLink, &result, nullptr)))
			target.assign(result.Buffer, result.Length / sizeof(WCHAR));
		::CloseHandle(hLink);
	}
	return target;
}
