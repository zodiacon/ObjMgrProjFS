#pragma once

struct ObjectNameAndType {
	std::wstring Name;
	std::wstring TypeName;
};

class ObjectManager {
public:
	static std::vector<ObjectNameAndType> EnumDirectoryObjects(PCWSTR path, PCWSTR name = nullptr, std::function<bool(PCWSTR)> compare = nullptr);
	static bool DirectoryExists(PCWSTR path);
	static std::optional<ObjectNameAndType> ObjectExists(PCWSTR path);
	static std::wstring GetSymbolicLinkTarget(PCWSTR path);
};

