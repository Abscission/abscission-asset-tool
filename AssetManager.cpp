
#include <Windows.h>
#include <iostream>
#include "include\zlib.h"

#pragma pack(push, 1)
struct Header {
	char FileID[4];

	int NumberOfEntries;
	int DataLength;

	bool Compressed;
};

struct IndexEntry {
	char Name[12];
	int Position;
	int Length;
};
#pragma pack(pop)

typedef unsigned char Byte;

int main(int argc, char* argv[]) {
	int NumberOfAssets = argc - 2;
	
	if (NumberOfAssets < 1) {
		std::cout << "Usage: " << argv[0] << " [asset1] [asset2]... [assetN] [AssetFile.aaf]";
		return 1;
	}

	Header FileHeader = {};
	*(int*)FileHeader.FileID = 0x41454741;
	FileHeader.NumberOfEntries = NumberOfAssets;
	FileHeader.Compressed = true;

	IndexEntry* Indexes = (IndexEntry*)VirtualAlloc(0, sizeof(IndexEntry) * NumberOfAssets, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	Byte** CompressedFiles = (Byte**)VirtualAlloc(0, sizeof(char*) * NumberOfAssets, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	int* OriginalSizes= new int[NumberOfAssets];
	int* CompressedSizes = new int[NumberOfAssets];


	int RunningPosition = 0;

	//For each asset
	for (int i = 0; i < NumberOfAssets; i++){
		//Open the file
		HANDLE AssetFile = CreateFileA(argv[i + 1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (AssetFile == INVALID_HANDLE_VALUE) {
			std::cout << "Failed to open file: " << argv[i + 1] << '\n';
			return 1;
		}

		//Allocate memory for the read file, and copy the files contents into a buffer
		int FileSize = GetFileSize(AssetFile, NULL);
		void* UncompressedData = VirtualAlloc(0, FileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		ReadFile(AssetFile, UncompressedData, FileSize, NULL, NULL);
	
		//Allocate the compression boundary amount of memory. This is the maximum size the file could expand to if it is incompressable data.
		int MaxSize = compressBound(FileSize);
		CompressedFiles[i] = (Byte*)VirtualAlloc(0, MaxSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		unsigned long CompressedSize = MaxSize;
		compress(CompressedFiles[i], &CompressedSize, (Byte*)UncompressedData, FileSize);
		std::cout << "\nFile:  " << argv[i + 1] << "\nOriginalSize: " << FileSize << " bytes\nCompressedSize: " << CompressedSize << " bytes\n\n";

		Indexes[i].Length = CompressedSize + 8;
		Indexes[i].Position = RunningPosition;

		RunningPosition += CompressedSize + 8;

		OriginalSizes[i] = FileSize;
		CompressedSizes[i] = CompressedSize;
	}

	FileHeader.DataLength = RunningPosition;

	//Create the file in memory (inefficient as two copies of most data are created, but good enough for our purposes)
	int SizeOfFile = sizeof(Header) + (sizeof(IndexEntry) * NumberOfAssets) + RunningPosition;

	void* FileBuffer = VirtualAlloc(0, SizeOfFile, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	
	memcpy(FileBuffer, &FileHeader, sizeof(FileHeader));
	memcpy((void*)((char*)FileBuffer + sizeof(FileHeader)), Indexes, sizeof(IndexEntry) * NumberOfAssets);
	
	for (int i = 0; i < NumberOfAssets; i++) {
		*(int*)((char*)FileBuffer + sizeof(Header) + (sizeof(IndexEntry) * NumberOfAssets) + Indexes[i].Position) = OriginalSizes[i];
		*((int*)((char*)FileBuffer + sizeof(Header) + (sizeof(IndexEntry) * NumberOfAssets) + Indexes[i].Position) + 1) = CompressedSizes[i];
		memcpy((void*)((int*)((char*)FileBuffer + sizeof(Header) + (sizeof(IndexEntry) * NumberOfAssets) + Indexes[i].Position) + 2), CompressedFiles[i], CompressedSizes[i]);
	}

	//Write the file
	std::cout << "Creating asset file " << argv[argc - 1] << '\n';

	HANDLE ArchiveFile = CreateFileA(argv[argc - 1], GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	WriteFile(ArchiveFile, FileBuffer, SizeOfFile, 0, 0);
}