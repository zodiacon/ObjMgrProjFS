#pragma once

#include "ObjectManager.h"

class ObjectManagerProjection {
public:
	HRESULT Init(PCWSTR root);
	HRESULT Start();

	void Term();

private:
	static HRESULT EndDirectoryEnumerationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ const GUID* enumerationId);
	static HRESULT StartDirectoryEnumerationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ const GUID* enumerationId);
	static HRESULT GetDirectoryEnumerationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ const GUID* enumerationId,
		_In_opt_ PCWSTR searchExpression,
		_In_ PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle);
	static HRESULT GetPlaceholderInformationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData);
	static HRESULT GetFileDataCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ UINT64 byteOffset,
		_In_ UINT32 length);

	HRESULT DoEndDirectoryEnumerationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ const GUID* enumerationId);
	HRESULT DoStartDirectoryEnumerationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ const GUID* enumerationId);
	HRESULT DoGetDirectoryEnumerationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ const GUID* enumerationId,
		_In_opt_ PCWSTR searchExpression,
		_In_ PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle);
	HRESULT DoGetPlaceholderInformationCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData);
	HRESULT DoGetFileDataCallback(
		_In_ const PRJ_CALLBACK_DATA* callbackData,
		_In_ UINT64 byteOffset,
		_In_ UINT32 length);

private:
	static int GetObjectSize(PCWSTR directory, ObjectNameAndType const& info);
	static std::wstring GetObjectData(PCWSTR directory, ObjectNameAndType const& info);

	struct GUIDComparer {
		bool operator()(const GUID& lhs, const GUID& rhs) const {
			return memcmp(&lhs, &rhs, sizeof(rhs)) < 0;
		}
	};

	struct EnumInfo {
		std::vector<ObjectNameAndType> Objects;
		int Index{ -1 };
	};
	std::wstring m_RootDir;
	PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT m_VirtContext;
	std::map<GUID, EnumInfo, GUIDComparer> m_Enumerations;
};

