#include "pch.h"
#include "ObjectManagerProjection.h"
#include <assert.h>

HRESULT ObjectManagerProjection::Init(PCWSTR root) {
	GUID instanceId = GUID_NULL;
	std::wstring instanceFile(root);
	instanceFile += L"\\_obgmgrproj.guid";

	if (!::CreateDirectory(root, nullptr)) {
		//
		// failed, does it exist?
		//
		if (::GetLastError() != ERROR_ALREADY_EXISTS)
			return HRESULT_FROM_WIN32(::GetLastError());

		//
		// get instance ID (if any)
		//
		auto hFile = ::CreateFile(instanceFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hFile != INVALID_HANDLE_VALUE && ::GetFileSize(hFile, nullptr) == sizeof(GUID)) {
			DWORD ret;
			::ReadFile(hFile, &instanceId, sizeof(instanceId), &ret, nullptr);
			::CloseHandle(hFile);
		}
	}
	if (instanceId == GUID_NULL) {
		::CoCreateGuid(&instanceId);
		//
		// write instance ID
		//
		auto hFile = ::CreateFile(instanceFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, nullptr);
		if (hFile != INVALID_HANDLE_VALUE) {
			DWORD ret;
			::WriteFile(hFile, &instanceId, sizeof(instanceId), &ret, nullptr);
			::CloseHandle(hFile);
		}
	}

	auto hr = ::PrjMarkDirectoryAsPlaceholder(root, nullptr, nullptr, &instanceId);
	if (FAILED(hr))
		return hr;

	m_RootDir = root;
	return hr;

}

HRESULT ObjectManagerProjection::Start() {
	PRJ_CALLBACKS cb{};
	cb.StartDirectoryEnumerationCallback = StartDirectoryEnumerationCallback;
	cb.EndDirectoryEnumerationCallback = EndDirectoryEnumerationCallback;
	cb.GetDirectoryEnumerationCallback = GetDirectoryEnumerationCallback;
	cb.GetPlaceholderInfoCallback = GetPlaceholderInformationCallback;
	cb.GetFileDataCallback = GetFileDataCallback;

	PRJ_STARTVIRTUALIZING_OPTIONS options{};
	auto hr = ::PrjStartVirtualizing(m_RootDir.c_str(), &cb, this, &options, &m_VirtContext);
	return hr;
}

void ObjectManagerProjection::Term() {
	::PrjStopVirtualizing(m_VirtContext);
}

HRESULT ObjectManagerProjection::StartDirectoryEnumerationCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId) {
	return ((ObjectManagerProjection*)callbackData->InstanceContext)->DoStartDirectoryEnumerationCallback(callbackData, enumerationId);
}

HRESULT ObjectManagerProjection::GetDirectoryEnumerationCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId, PCWSTR searchExpression, PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle) {
	return ((ObjectManagerProjection*)callbackData->InstanceContext)->DoGetDirectoryEnumerationCallback(callbackData, enumerationId, searchExpression, dirEntryBufferHandle);
}

HRESULT ObjectManagerProjection::GetPlaceholderInformationCallback(const PRJ_CALLBACK_DATA* callbackData) {
	return ((ObjectManagerProjection*)callbackData->InstanceContext)->DoGetPlaceholderInformationCallback(callbackData);
}

HRESULT ObjectManagerProjection::GetFileDataCallback(const PRJ_CALLBACK_DATA* callbackData, UINT64 byteOffset, UINT32 length) {
	return ((ObjectManagerProjection*)callbackData->InstanceContext)->DoGetFileDataCallback(callbackData, byteOffset, length);
}

HRESULT ObjectManagerProjection::EndDirectoryEnumerationCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId) {
	return ((ObjectManagerProjection*)callbackData->InstanceContext)->DoEndDirectoryEnumerationCallback(callbackData, enumerationId);
}

// implementation

HRESULT ObjectManagerProjection::DoEndDirectoryEnumerationCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId) {
	m_Enumerations.erase(*enumerationId);
	return S_OK;
}

HRESULT ObjectManagerProjection::DoStartDirectoryEnumerationCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId) {
	EnumInfo info;
	m_Enumerations.insert({ *enumerationId, std::move(info) });
	return S_OK;
}

HRESULT ObjectManagerProjection::DoGetDirectoryEnumerationCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId, 
	PCWSTR searchExpression, PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle) {

	auto it = m_Enumerations.find(*enumerationId); 
	if(it == m_Enumerations.end())
		return E_INVALIDARG;

	auto& info = it->second;


	if (info.Index < 0 || (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RESTART_SCAN)) {
		auto compare = [&](auto name) {
			return ::PrjFileNameMatch(name, searchExpression);
			};
		info.Objects = ObjectManager::EnumDirectoryObjects(callbackData->FilePathName, nullptr, compare);
		std::ranges::sort(info.Objects, [](auto const& item1, auto const& item2) { return ::PrjFileNameCompare(item1.Name.c_str(), item2.Name.c_str()) < 0; });
		info.Index = 0;
	}

	if (info.Objects.empty())
		return S_OK;

	while (info.Index < info.Objects.size()) {
		PRJ_FILE_BASIC_INFO itemInfo{};
		auto& item = info.Objects[info.Index];
		itemInfo.IsDirectory = item.TypeName == L"Directory";
		itemInfo.FileSize = itemInfo.IsDirectory ? 0 : GetObjectSize((callbackData->FilePathName + std::wstring(L"\\") + item.Name).c_str(), item);
		
		if (FAILED(::PrjFillDirEntryBuffer(
			(itemInfo.IsDirectory ? item.Name : (item.Name + L"." + item.TypeName)).c_str(), 
			&itemInfo, dirEntryBufferHandle)))
			break;
		info.Index++;
		if (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RETURN_SINGLE_ENTRY)
			break;
	}

	return S_OK;
}

HRESULT ObjectManagerProjection::DoGetPlaceholderInformationCallback(const PRJ_CALLBACK_DATA* callbackData) {
	auto path = callbackData->FilePathName;
	auto dir = ObjectManager::DirectoryExists(path);
	std::optional<ObjectNameAndType> object;
	if (!dir)
		object = ObjectManager::ObjectExists(path);
	if(!dir && !object)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

	PRJ_PLACEHOLDER_INFO info{};
	info.FileBasicInfo.IsDirectory = dir;
	info.FileBasicInfo.FileSize = dir ? 0 : GetObjectSize(path, object.value());
	return PrjWritePlaceholderInfo(m_VirtContext, callbackData->FilePathName, &info, sizeof(info));
}

HRESULT ObjectManagerProjection::DoGetFileDataCallback(const PRJ_CALLBACK_DATA* callbackData, UINT64 byteOffset, UINT32 length) {
	auto object = ObjectManager::ObjectExists(callbackData->FilePathName);
	if (!object)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

	auto buffer = ::PrjAllocateAlignedBuffer(m_VirtContext, length);
	if (!buffer)
		return E_OUTOFMEMORY;

	auto data = GetObjectData(callbackData->FilePathName, object.value());
	memcpy(buffer, (PBYTE)data.c_str() + byteOffset, length);
	auto hr = ::PrjWriteFileData(m_VirtContext, &callbackData->DataStreamId, buffer, byteOffset, length);
	::PrjFreeAlignedBuffer(buffer);

	return hr;
}

int ObjectManagerProjection::GetObjectSize(PCWSTR fullname, ObjectNameAndType const& info) {
	return (int)GetObjectData(fullname, info).length() * sizeof(WCHAR);
}

std::wstring ObjectManagerProjection::GetObjectData(PCWSTR fullname, ObjectNameAndType const& info) {
	std::wstring target;
	if (info.TypeName == L"SymbolicLink") {
		target = ObjectManager::GetSymbolicLinkTarget(fullname);
	}
	auto result = std::format(L"Name: {}\nType: {}\n", info.Name, info.TypeName);
	if (!target.empty())
		result = std::format(L"{}Target: {}\n", result, target);
	return result;
}

