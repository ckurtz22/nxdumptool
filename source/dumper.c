#include <stdio.h>
#include <malloc.h>
#include <dirent.h>
#include <memory.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dumper.h"
#include "fsext.h"
#include "ui.h"
#include "util.h"

extern u64 freeSpace;

extern int breaks;

extern u32 currentFBWidth, currentFBHeight;

extern u64 gameCardSize;
extern char gameCardSizeStr[32];

extern char *hfs0_header;
extern u64 hfs0_offset, hfs0_size;
extern u32 hfs0_partition_cnt;

extern u64 gameCardTitleID;

static char strbuf[512] = {'\0'};

void workaroundPartitionZeroAccess(FsDeviceOperator* fsOperator)
{
	u32 handle;
	if (R_FAILED(fsDeviceOperatorGetGameCardHandle(fsOperator, &handle))) return;
	
	FsStorage gameCardStorage;
	if (R_FAILED(fsOpenGameCardStorage(&gameCardStorage, handle, 0))) return;
	
	fsStorageClose(&gameCardStorage);
}

bool getRootHfs0Header(FsDeviceOperator* fsOperator)
{
	u32 handle, magic;
	Result result;
	FsStorage gameCardStorage;
	
	//breaks = 0;
	//uiClearScreen();
	
	hfs0_partition_cnt = 0;
	
	workaroundPartitionZeroAccess(fsOperator);
	
	if (R_FAILED(result = fsDeviceOperatorGetGameCardHandle(fsOperator, &handle)))
	{
		//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle failed! (0x%08X)", result);
		//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		return false;
	}
	
	if (R_FAILED(result = fsOpenGameCardStorage(&gameCardStorage, handle, 0)))
	{
		//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage failed! (0x%08X)", result);
		//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		return false;
	}
	
	char *gamecard_header = (char*)malloc(GAMECARD_HEADER_SIZE);
	if (!gamecard_header)
	{
		fsStorageClose(&gameCardStorage);
		return false;
	}
	
	if (R_FAILED(result = fsStorageRead(&gameCardStorage, 0, gamecard_header, GAMECARD_HEADER_SIZE)))
	{
		//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "StorageRead failed! (0x%08X)", result);
		//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		
		free(gamecard_header);
		
		fsStorageClose(&gameCardStorage);
		
		return false;
	}
	
	u8 cardSize = (u8)gamecard_header[GAMECARD_SIZE_ADDR];
	
	switch(cardSize)
	{
		case 0xFA: // 1 GiB
			gameCardSize = GAMECARD_SIZE_1GiB;
			break;
		case 0xF8: // 2 GiB
			gameCardSize = GAMECARD_SIZE_2GiB;
			break;
		case 0xF0: // 4 GiB
			gameCardSize = GAMECARD_SIZE_4GiB;
			break;
		case 0xE0: // 8 GiB
			gameCardSize = GAMECARD_SIZE_8GiB;
			break;
		case 0xE1: // 16 GiB
			gameCardSize = GAMECARD_SIZE_16GiB;
			break;
		case 0xE2: // 32 GiB
			gameCardSize = GAMECARD_SIZE_32GiB;
			break;
		default:
			//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Invalid game card size value: 0x%02X", cardSize);
			//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
			
			free(gamecard_header);
			
			fsStorageClose(&gameCardStorage);
			
			return false;
	}
	
	convertSize(gameCardSize, gameCardSizeStr, sizeof(gameCardSizeStr) / sizeof(gameCardSizeStr[0]));
	
	memcpy(&hfs0_offset, gamecard_header + HFS0_OFFSET_ADDR, sizeof(u64));
	memcpy(&hfs0_size, gamecard_header + HFS0_SIZE_ADDR, sizeof(u64));
	
	free(gamecard_header);
	
	/*snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Root HFS0 offset: 0x%016lX", hfs0_offset);
	uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
	breaks++;
	
	snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Root HFS0 size: 0x%016lX", hfs0_size);
	uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
	breaks++;*/
	
	hfs0_header = (char*)malloc(hfs0_size);
	if (!hfs0_header)
	{
		//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to allocate buffer!");
		//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		
		gameCardSize = 0;
		memset(gameCardSizeStr, 0, sizeof(gameCardSizeStr));
		
		hfs0_offset = hfs0_size = 0;
		
		fsStorageClose(&gameCardStorage);
		
		return false;
	}
	
	if (R_FAILED(result = fsStorageRead(&gameCardStorage, hfs0_offset, hfs0_header, hfs0_size)))
	{
		//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "StorageRead failed! (0x%08X)", result);
		//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		
		gameCardSize = 0;
		memset(gameCardSizeStr, 0, sizeof(gameCardSizeStr));
		
		free(hfs0_header);
		hfs0_header = NULL;
		hfs0_offset = hfs0_size = 0;
		
		fsStorageClose(&gameCardStorage);
		
		return false;
	}
	
	memcpy(&magic, hfs0_header, sizeof(u32));
	magic = bswap_32(magic);
	
	if (magic != HFS0_MAGIC)
	{
		//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Magic word mismatch! 0x%08X != 0x%08X", magic, HFS0_MAGIC);
		//uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		
		gameCardSize = 0;
		memset(gameCardSizeStr, 0, sizeof(gameCardSizeStr));
		
		free(hfs0_header);
		hfs0_header = NULL;
		hfs0_offset = hfs0_size = 0;
		
		fsStorageClose(&gameCardStorage);
		
		return false;
	}
	
	memcpy(&hfs0_partition_cnt, hfs0_header + HFS0_FILE_COUNT_ADDR, sizeof(u32));
	
	fsStorageClose(&gameCardStorage);
	return true;
}

bool getHsf0PartitionDetails(u32 partition, u64 *out_offset, u64 *out_size)
{
	if (hfs0_header == NULL) return false;
	
	if (partition > (hfs0_partition_cnt - 1)) return false;
	
	hfs0_entry_table *entryTable = (hfs0_entry_table*)malloc(sizeof(hfs0_entry_table) * hfs0_partition_cnt);
	if (!entryTable) return false;
	
	memcpy(entryTable, hfs0_header + HFS0_ENTRY_TABLE_ADDR, sizeof(hfs0_entry_table) * hfs0_partition_cnt);
	
	switch(partition)
	{
		case 0: // update (contained within IStorage instance with partition ID 0)
		case 1: // normal or logo (depending on the gamecard type) (contained within IStorage instance with partition ID 0)
			*out_offset = (hfs0_offset + hfs0_size + entryTable[partition].file_offset);
			break;
		case 2:
			if (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT)
			{
				// secure (contained within IStorage instance with partition ID 1)
				*out_offset = 0;
			} else {
				// normal (contained within IStorage instance with partition ID 0)
				*out_offset = (hfs0_offset + hfs0_size + entryTable[partition].file_offset);
			}
			break;
		case 3: // secure (gamecard type 0x02) (contained within IStorage instance with partition ID 1)
			*out_offset = 0;
			break;
		default:
			break;
	}
	
	*out_size = entryTable[partition].file_size;
	
	free(entryTable);
	
	return true;
}

bool dumpGameCartridge(FsDeviceOperator* fsOperator, bool isFat32, bool dumpCert)
{
	u64 partitionOffset = 0, fileOffset = 0, totalSize = 0, paddingSize = 0, n;
	u64 partitionSizes[ISTORAGE_PARTITION_CNT];
	char partitionSizesStr[ISTORAGE_PARTITION_CNT][32] = {'\0'}, totalSizeStr[32] = {'\0'}, curSizeStr[32] = {'\0'}, paddingSizeStr[32] = {'\0'}, filename[128] = {'\0'}, timeStamp[16] = {'\0'};
	u32 handle, partition;
	Result result;
	FsStorage gameCardStorage;
	bool proceed = true, success = false;
	FILE *outFile = NULL;
	char *buf = NULL;
	u8 splitIndex = 0;
	int progress = 0;
	
	for(partition = 0; partition < ISTORAGE_PARTITION_CNT; partition++)
	{
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Getting partition #%u size...", partition);
		uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
		breaks++;
		
		workaroundPartitionZeroAccess(fsOperator);
		
		if (R_SUCCEEDED(result = fsDeviceOperatorGetGameCardHandle(fsOperator, &handle)))
		{
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle succeeded: 0x%08X", handle);
			uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			breaks++;
			
			if (R_SUCCEEDED(result = fsOpenGameCardStorage(&gameCardStorage, handle, partition)))
			{
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage succeeded: 0x%08X", handle);
				uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
				breaks++;
				
				if (R_SUCCEEDED(result = fsStorageGetSize(&gameCardStorage, &(partitionSizes[partition]))))
				{
					totalSize += partitionSizes[partition];
					convertSize(partitionSizes[partition], partitionSizesStr[partition], sizeof(partitionSizesStr[partition]) / sizeof(partitionSizesStr[partition][0]));
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Partition #%u size: %s (%lu bytes)", partition, partitionSizesStr[partition], partitionSizes[partition]);
					uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
					breaks += 2;
				} else {
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "StorageGetSize failed! (0x%08X)", result);
					uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
					proceed = false;
				}
				
				fsStorageClose(&gameCardStorage);
			} else {
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage failed! (0x%08X)", result);
				uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
				proceed = false;
			}
		} else {
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle failed! (0x%08X)", result);
			uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
			proceed = false;
		}
		
		syncDisplay();
	}
	
	if (proceed)
	{
		convertSize(totalSize, totalSizeStr, sizeof(totalSizeStr) / sizeof(totalSizeStr[0]));
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "XCI data size: %s (%lu bytes)", totalSizeStr, totalSize);
		uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
		breaks += 2;
		
		paddingSize = (gameCardSize - totalSize);
		convertSize(paddingSize, paddingSizeStr, sizeof(paddingSizeStr) / sizeof(paddingSizeStr[0]));
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "0xFF padding size: %s (%lu bytes)", paddingSizeStr, paddingSize);
		uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
		breaks++;
		
		if (gameCardSize <= freeSpace)
		{
			breaks++;
			
			getCurrentTimestamp(timeStamp, sizeof(timeStamp) / sizeof(timeStamp[0]));
			
			if (gameCardSize > SPLIT_FILE_MIN && isFat32)
			{
				snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci.%02u", gameCardTitleID, timeStamp, splitIndex);
			} else {
				snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci", gameCardTitleID, timeStamp);
			}
			
			outFile = fopen(filename, "wb");
			if (outFile)
			{
				buf = (char*)malloc(DUMP_BUFFER_SIZE);
				if (buf)
				{
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Dump procedure started. Writing output to \"%.*s\". Hold B to cancel.", (int)((gameCardSize > SPLIT_FILE_MIN && isFat32) ? (strlen(filename) - 3) : strlen(filename)), filename);
					uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
					breaks += 2;
					
					for(partition = 0; partition < ISTORAGE_PARTITION_CNT; partition++)
					{
						n = DUMP_BUFFER_SIZE;
						
						uiFill(0, breaks * 8, currentFBWidth, 8, 50, 50, 50);
						uiFill(0, (breaks + 3) * 8, currentFBWidth, 16, 50, 50, 50);
						
						snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Dumping partition #%u...", partition);
						uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
						
						syncDisplay();
						
						workaroundPartitionZeroAccess(fsOperator);
						
						if (R_SUCCEEDED(result = fsDeviceOperatorGetGameCardHandle(fsOperator, &handle)))
						{
							if (R_SUCCEEDED(result = fsOpenGameCardStorage(&gameCardStorage, handle, partition)))
							{
								for(partitionOffset = 0; partitionOffset < partitionSizes[partition]; partitionOffset += n, fileOffset += n)
								{
									if (DUMP_BUFFER_SIZE > (partitionSizes[partition] - partitionOffset)) n = (partitionSizes[partition] - partitionOffset);
									
									if (R_FAILED(result = fsStorageRead(&gameCardStorage, partitionOffset, buf, n)))
									{
										snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "StorageRead failed (0x%08X) at offset 0x%016lX for partition #%u", result, partitionOffset, partition);
										uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
										proceed = false;
										break;
									}
									
									if (gameCardSize > SPLIT_FILE_MIN && isFat32 && (fileOffset + n) < gameCardSize && (fileOffset + n) >= ((splitIndex + 1) * SPLIT_FILE_2GiB))
									{
										u64 new_file_chunk_size = ((fileOffset + n) - ((splitIndex + 1) * SPLIT_FILE_2GiB));
										u64 old_file_chunk_size = (n - new_file_chunk_size);
										
										if (old_file_chunk_size > 0)
										{
											if (fwrite(buf, 1, old_file_chunk_size, outFile) != old_file_chunk_size)
											{
												snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", fileOffset);
												uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
												proceed = false;
												break;
											}
										}
										
										fclose(outFile);
										
										splitIndex++;
										snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci.%02u", gameCardTitleID, timeStamp, splitIndex);
										
										outFile = fopen(filename, "wb");
										if (!outFile)
										{
											snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file for part #%u!", splitIndex);
											uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
											proceed = false;
											break;
										}
										
										if (new_file_chunk_size > 0)
										{
											if (fwrite(buf, 1, new_file_chunk_size, outFile) != new_file_chunk_size)
											{
												snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", fileOffset + old_file_chunk_size);
												uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
												proceed = false;
												break;
											}
										}
									} else {
										if (fwrite(buf, 1, n, outFile) != n)
										{
											snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", fileOffset);
											uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
											proceed = false;
											break;
										}
									}
									
									progress = (int)(((fileOffset + n) * 100) / gameCardSize);
									
									uiFill(currentFBWidth / 4, (breaks + 3) * 8, currentFBWidth / 2, 16, 0, 0, 0);
									uiFill(currentFBWidth / 4, (breaks + 3) * 8, (((fileOffset + n) * (currentFBWidth / 2)) / gameCardSize), 16, 0, 255, 0);
									
									convertSize(fileOffset + n, curSizeStr, sizeof(curSizeStr) / sizeof(curSizeStr[0]));
									snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%d%% [%s / %s]", progress, curSizeStr, gameCardSizeStr);
									
									uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
									uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, 255, 255, 255);
									
									syncDisplay();
									
									if ((fileOffset + n) < gameCardSize && ((fileOffset / DUMP_BUFFER_SIZE) % 10) == 0)
									{
										hidScanInput();
										
										u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
										if (keysDown & KEY_B)
										{
											uiDrawString("Process canceled", 0, (breaks + 7) * 8, 255, 0, 0);
											proceed = false;
											break;
										}
									}
								}
								
								if (fileOffset >= totalSize) success = true;
								
								// Support empty files
								if (!partitionSizes[partition])
								{
									uiFill(currentFBWidth / 4, (breaks + 3) * 8, currentFBWidth / 2, 16, 0, 255, 0);
									uiFill(currentFBWidth / 4, (breaks + 3) * 8, ((fileOffset * (currentFBWidth / 2)) / gameCardSize), 16, 0, 255, 0);
									
									convertSize(fileOffset, curSizeStr, sizeof(curSizeStr) / sizeof(curSizeStr[0]));
									snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%d%% [%s / %s]", progress, curSizeStr, gameCardSizeStr);
									
									uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
									uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, 255, 255, 255);
									
									syncDisplay();
								}
								
								if (!proceed)
								{
									uiFill(currentFBWidth / 4, (breaks + 3) * 8, (((fileOffset + n) * (currentFBWidth / 2)) / gameCardSize), 16, 255, 0, 0);
									breaks += 7;
								}
								
								fsStorageClose(&gameCardStorage);
							} else {
								breaks++;
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage failed for partition #%u! (0x%08X)", partition, result);
								uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
								proceed = false;
							}
						} else {
							breaks++;
							snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle failed for partition #%u! (0x%08X)", partition, result);
							uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
							proceed = false;
						}
						
						if (!proceed) break;
					}
					
					if (!success) free(buf);
				} else {
					uiDrawString("Failed to allocate memory for the dump process!", 0, breaks * 8, 255, 0, 0);
				}
				
				if (success)
				{
					// Add file padding
					memset(buf, 0xFF, DUMP_BUFFER_SIZE);
					
					uiFill(0, breaks * 8, currentFBWidth, 8, 50, 50, 50);
					uiFill(0, (breaks + 3) * 8, currentFBWidth, 16, 50, 50, 50);
					
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Writing 0xFF padding...");
					uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
					
					syncDisplay();
					
					for(partitionOffset = 0; partitionOffset < paddingSize; partitionOffset += n, fileOffset += n)
					{
						if (DUMP_BUFFER_SIZE > (paddingSize - partitionOffset)) n = (paddingSize - partitionOffset);
						
						if (gameCardSize > SPLIT_FILE_MIN && isFat32 && (fileOffset + n) < gameCardSize && (fileOffset + n) >= ((splitIndex + 1) * SPLIT_FILE_2GiB))
						{
							u64 new_file_chunk_size = ((fileOffset + n) - ((splitIndex + 1) * SPLIT_FILE_2GiB));
							u64 old_file_chunk_size = (n - new_file_chunk_size);
							
							if (old_file_chunk_size > 0)
							{
								if (fwrite(buf, 1, old_file_chunk_size, outFile) != old_file_chunk_size)
								{
									snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", fileOffset);
									uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
									proceed = false;
									break;
								}
							}
							
							fclose(outFile);
							
							splitIndex++;
							snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci.%02u", gameCardTitleID, timeStamp, splitIndex);
							
							outFile = fopen(filename, "wb");
							if (!outFile)
							{
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file for part #%u!", splitIndex);
								uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
								proceed = false;
								break;
							}
							
							if (new_file_chunk_size > 0)
							{
								if (fwrite(buf, 1, new_file_chunk_size, outFile) != new_file_chunk_size)
								{
									snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", fileOffset + old_file_chunk_size);
									uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
									proceed = false;
									break;
								}
							}
						} else {
							if (fwrite(buf, 1, n, outFile) != n)
							{
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", fileOffset);
								uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
								proceed = false;
								break;
							}
						}
						
						progress = (int)(((fileOffset + n) * 100) / gameCardSize);
						
						uiFill(currentFBWidth / 4, (breaks + 3) * 8, currentFBWidth / 2, 16, 0, 0, 0);
						uiFill(currentFBWidth / 4, (breaks + 3) * 8, (((fileOffset + n) * (currentFBWidth / 2)) / gameCardSize), 16, 0, 255, 0);
						
						convertSize(fileOffset + n, curSizeStr, sizeof(curSizeStr) / sizeof(curSizeStr[0]));
						snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%d%% [%s / %s]", progress, curSizeStr, gameCardSizeStr);
						
						uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
						uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, 255, 255, 255);
						
						syncDisplay();
						
						if ((fileOffset + n) < gameCardSize && ((fileOffset / DUMP_BUFFER_SIZE) % 10) == 0)
						{
							hidScanInput();
							
							u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
							if (keysDown & KEY_B)
							{
								uiDrawString("Process canceled", 0, (breaks + 7) * 8, 255, 0, 0);
								proceed = false;
								break;
							}
						}
					}
					
					fclose(outFile);
					
					breaks += 7;
					
					if (proceed)
					{
						if (!dumpCert)
						{
							if (gameCardSize > SPLIT_FILE_MIN && isFat32)
							{
								snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci.%02u", gameCardTitleID, timeStamp, 0);
							} else {
								snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci", gameCardTitleID, timeStamp);
							}
							
							outFile = fopen(filename, "rb+");
							if (!outFile)
							{
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Error: unable to remove game card certificate from finished dump!");
								uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
								proceed = false;
							}
							
							if (proceed)
							{
								fseek(outFile, CERT_OFFSET, SEEK_SET);
								fwrite(buf, 1, CERT_SIZE, outFile);
								fclose(outFile);
							}
						}
						
						if (proceed) uiDrawString("Process successfully completed!", 0, breaks * 8, 0, 255, 0);
					} else {
						success = false;
						
						if (gameCardSize > SPLIT_FILE_MIN && isFat32)
						{
							for(u8 i = 0; i <= splitIndex; i++)
							{
								snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci.%02u", gameCardTitleID, timeStamp, i);
								remove(filename);
							}
						} else {
							remove(filename);
						}
					}
					
					free(buf);
				} else {
					if (outFile) fclose(outFile);
					
					if (gameCardSize > SPLIT_FILE_MIN && isFat32)
					{
						for(u8 i = 0; i <= splitIndex; i++)
						{
							snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_%s.xci.%02u", gameCardTitleID, timeStamp, i);
							remove(filename);
						}
					} else {
						remove(filename);
					}
				}
			} else {
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file \"%s\"!", filename);
				uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
			}
		} else {
			uiDrawString("Error: not enough free space available in the SD card.", 0, breaks * 8, 255, 0, 0);
		}
	}
	
	breaks += 2;
	
	return success;
}

bool dumpRawPartition(FsDeviceOperator* fsOperator, u32 partition, bool doSplitting)
{
	u32 handle;
	Result result;
	u64 size, partitionOffset;
	bool success = false;
	char *buf;
	u64 off, n = DUMP_BUFFER_SIZE;
	FsStorage gameCardStorage;
	char totalSizeStr[32] = {'\0'}, curSizeStr[32] = {'\0'}, filename[128] = {'\0'}, timeStamp[16] = {'\0'};
	int progress = 0;
	FILE *outFile = NULL;
	u8 splitIndex = 0;
	
	workaroundPartitionZeroAccess(fsOperator);
	
	if (R_SUCCEEDED(result = fsDeviceOperatorGetGameCardHandle(fsOperator, &handle)))
	{
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle succeeded: 0x%08X", handle);
		uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
		breaks++;
		
		if (partition == 0)
		{
			u32 title_ver;
			u64 title_id;
			
			if (R_SUCCEEDED(fsDeviceOperatorUpdatePartitionInfo(fsOperator, handle, &title_ver, &title_id)))
			{
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "System Title Version: v%u", title_ver);
				uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
				breaks++;
				
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "System Title ID: %016lX", title_id);
				uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			} else {
				uiDrawString("Unable to get the system title-version from partition #0. Not a big problem, though", 0, breaks * 8, 255, 0, 0);
			}
			
			breaks++;
		}
		
		// Ugly hack
		// The IStorage instance returned for partition == 0 contains the gamecard header, the gamecard certificate, the root HFS0 header and:
		// * The "update" (0) partition and the "normal" (1) partition (for gamecard type 0x01)
		// * The "update" (0) partition, the "logo" (1) partition and the "normal" (2) partition (for gamecard type 0x02)
		// The IStorage instance returned for partition == 1 contains the "secure" partition (which can either be 2 or 3 depending on the gamecard type)
		// This ugly hack makes sure we just dump the *actual* raw HFS0 partition, without preceding data, padding, etc.
		// Oddly enough, IFileSystem instances actually point to the specified partition ID filesystem. I don't understand why it doesn't work like that for IStorage, but whatever
		// NOTE: Using partition == 2 returns error 0x149002, and using higher values probably do so, too
		
		if (R_SUCCEEDED(result = fsOpenGameCardStorage(&gameCardStorage, handle, (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? (partition < 2 ? 0 : 1) : (partition < 3 ? 0 : 1)))))
		{
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage succeeded: 0x%08X", handle);
			uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			breaks++;
			
			if (getHsf0PartitionDetails(partition, &partitionOffset, &size))
			{
				convertSize(size, totalSizeStr, sizeof(totalSizeStr) / sizeof(totalSizeStr[0]));
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Partition size: %s (%lu bytes)", totalSizeStr, size);
				uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
				breaks++;
				
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Partition offset (relative to IStorage instance): 0x%016lX", partitionOffset);
				uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
				breaks++;
				
				if (size <= freeSpace)
				{
					getCurrentTimestamp(timeStamp, sizeof(timeStamp) / sizeof(timeStamp[0]));
					
					if (size > SPLIT_FILE_MIN && doSplitting)
					{
						snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_part%u_%s.hfs0.%02u", gameCardTitleID, partition, timeStamp, splitIndex);
					} else {
						snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_part%u_%s.hfs0", gameCardTitleID, partition, timeStamp);
					}
					
					outFile = fopen(filename, "wb");
					if (outFile)
					{
						buf = (char*)malloc(DUMP_BUFFER_SIZE);
						if (buf)
						{
							snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Dumping raw HFS0 partition #%u to \"%.*s\". Hold B to cancel.", partition, (int)((size > SPLIT_FILE_MIN && doSplitting) ? (strlen(filename) - 3) : strlen(filename)), filename);
							uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
							breaks += 3;
							
							syncDisplay();
							
							for (off = 0; off < size; off += n)
							{
								if (DUMP_BUFFER_SIZE > (size - off)) n = (size - off);
								
								if (R_FAILED(result = fsStorageRead(&gameCardStorage, partitionOffset + off, buf, n)))
								{
									snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "StorageRead failed (0x%08X) at offset 0x%016lX", result, partitionOffset + off);
									uiDrawString(strbuf, 0, (breaks + 4) * 8, 255, 0, 0);
									break;
								}
								
								if (size > SPLIT_FILE_MIN && doSplitting && (off + n) < size && (off + n) >= ((splitIndex + 1) * SPLIT_FILE_2GiB))
								{
									u64 new_file_chunk_size = ((off + n) - ((splitIndex + 1) * SPLIT_FILE_2GiB));
									u64 old_file_chunk_size = (n - new_file_chunk_size);
									
									if (old_file_chunk_size > 0)
									{
										if (fwrite(buf, 1, old_file_chunk_size, outFile) != old_file_chunk_size)
										{
											snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", off);
											uiDrawString(strbuf, 0, (breaks + 4) * 8, 255, 0, 0);
											break;
										}
									}
									
									fclose(outFile);
									
									splitIndex++;
									snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_part%u_%s.hfs0.%02u", gameCardTitleID, partition, timeStamp, splitIndex);
									
									outFile = fopen(filename, "wb");
									if (!outFile)
									{
										snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file for part #%u!", splitIndex);
										uiDrawString(strbuf, 0, (breaks + 4) * 8, 255, 0, 0);
										break;
									}
									
									if (new_file_chunk_size > 0)
									{
										if (fwrite(buf, 1, new_file_chunk_size, outFile) != new_file_chunk_size)
										{
											snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", off + old_file_chunk_size);
											uiDrawString(strbuf, 0, (breaks + 4) * 8, 255, 0, 0);
											break;
										}
									}
								} else {
									if (fwrite(buf, 1, n, outFile) != n)
									{
										snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", off);
										uiDrawString(strbuf, 0, (breaks + 4) * 8, 255, 0, 0);
										break;
									}
								}
								
								progress = (int)(((off + n) * 100) / size);
								
								uiFill(currentFBWidth / 4, breaks * 8, currentFBWidth / 2, 16, 0, 0, 0);
								uiFill(currentFBWidth / 4, breaks * 8, (((off + n) * (currentFBWidth / 2)) / size), 16, 0, 255, 0);
								
								convertSize(off + n, curSizeStr, sizeof(curSizeStr) / sizeof(curSizeStr[0]));
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%d%% [%s / %s]", progress, curSizeStr, totalSizeStr);
								
								uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, (breaks * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
								uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, (breaks * 8) + 4, 255, 255, 255);
								
								syncDisplay();
								
								if ((off + n) < size && ((off / DUMP_BUFFER_SIZE) % 10) == 0)
								{
									hidScanInput();
									
									u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
									if (keysDown & KEY_B)
									{
										uiDrawString("Process canceled", 0, (breaks + 4) * 8, 255, 0, 0);
										break;
									}
								}
							}
							
							if (off >= size) success = true;
							
							// Support empty files
							if (!size)
							{
								uiFill(currentFBWidth / 4, breaks * 8, currentFBWidth / 2, 16, 0, 255, 0);
								
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "100%% [0 B / 0 B]");
								
								uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, (breaks * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
								uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, (breaks * 8) + 4, 255, 255, 255);
								
								syncDisplay();
							}
							
							if (success)
							{
								uiDrawString("Process successfully completed!", 0, (breaks + 4) * 8, 0, 255, 0);
							} else {
								uiFill(currentFBWidth / 4, breaks * 8, (((off + n) * (currentFBWidth / 2)) / size), 16, 255, 0, 0);
							}
							
							breaks += 4;
							
							free(buf);
						} else {
							uiDrawString("Failed to allocate memory for the dump process!", 0, breaks * 8, 255, 0, 0);
						}
						
						fclose(outFile);
						if (!success)
						{
							if (size > SPLIT_FILE_MIN && doSplitting)
							{
								for(u8 i = 0; i <= splitIndex; i++)
								{
									snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_part%u_%s.hfs0.%02u", gameCardTitleID, partition, timeStamp, i);
									remove(filename);
								}
							} else {
								remove(filename);
							}
						}
					} else {
						snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file \"%s\"!", filename);
						uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
					}
				} else {
					uiDrawString("Error: not enough free space available in the SD card.", 0, breaks * 8, 255, 0, 0);
				}
			} else {
				uiDrawString("Error: unable to get partition details from the root HFS0 header!", 0, breaks * 8, 255, 0, 0);
			}
			
			fsStorageClose(&gameCardStorage);
		} else {
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage failed! (0x%08X)", result);
			uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		}
	} else {
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle failed! (0x%08X)", result);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
	}
	
	breaks += 2;
	
	return success;
}

bool openPartitionFs(FsFileSystem* ret, FsDeviceOperator* fsOperator, u32 partition)
{
	u32 handle;
	Result result;
	bool success = false;
	
	if (R_SUCCEEDED(result = fsDeviceOperatorGetGameCardHandle(fsOperator, &handle)))
	{
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle succeeded: 0x%08X", handle);
		uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
		breaks++;
		
		if (R_SUCCEEDED(result = fsOpenGameCardFileSystem(ret, handle, partition)))
		{
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardFileSystem succeeded: 0x%08X", result);
			uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			breaks++;
			success = true;
		} else {
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardFileSystem failed! (0x%08X)", result);
			uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
			breaks += 2;
		}
	} else {
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle failed! (0x%08X)", result);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		breaks += 2;
	}
	
	return success;
}

bool copyFile(const char* source, const char* dest, bool doSplitting)
{
	bool success = false;
	char splitFilename[NAME_BUF_LEN] = {'\0'};
	size_t destLen = strlen(dest);
	FILE *inFile, *outFile;
	char *buf = NULL;
	u64 size, off, n = DUMP_BUFFER_SIZE;
	u8 splitIndex = 0;
	int progress = 0;
	char totalSizeStr[32] = {'\0'}, curSizeStr[32] = {'\0'};
	
	uiFill(0, breaks * 8, currentFBWidth, 8, 50, 50, 50);
	uiFill(0, (breaks + 3) * 8, currentFBWidth, 16, 50, 50, 50);
	
	snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Copying \"%s\"...", source);
	uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
	
	syncDisplay();
	
	if ((destLen + 1) < NAME_BUF_LEN)
	{
		inFile = fopen(source, "rb");
		if (inFile)
		{
			fseek(inFile, 0L, SEEK_END);
			size = ftell(inFile);
			rewind(inFile);
			
			convertSize(size, totalSizeStr, sizeof(totalSizeStr) / sizeof(totalSizeStr[0]));
			
			if (size > SPLIT_FILE_MIN && doSplitting) snprintf(splitFilename, sizeof(splitFilename) / sizeof(splitFilename[0]), "%s.%02u", dest, splitIndex);
			
			outFile = fopen(((size > SPLIT_FILE_MIN && doSplitting) ? splitFilename : dest), "wb");
			if (outFile)
			{
				buf = (char*)malloc(DUMP_BUFFER_SIZE);
				if (buf)
				{
					for (off = 0; off < size; off += n)
					{
						if (DUMP_BUFFER_SIZE > (size - off)) n = (size - off);
						
						if (fread(buf, 1, n, inFile) != n)
						{
							snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to read chunk from offset 0x%016lX", off);
							uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
							break;
						}
						
						if (size > SPLIT_FILE_MIN && doSplitting && (off + n) < size && (off + n) >= ((splitIndex + 1) * SPLIT_FILE_2GiB))
						{
							u64 new_file_chunk_size = ((off + n) - ((splitIndex + 1) * SPLIT_FILE_2GiB));
							u64 old_file_chunk_size = (n - new_file_chunk_size);
							
							if (old_file_chunk_size > 0)
							{
								if (fwrite(buf, 1, old_file_chunk_size, outFile) != old_file_chunk_size)
								{
									snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", off);
									uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
									break;
								}
							}
							
							fclose(outFile);
							
							splitIndex++;
							snprintf(splitFilename, sizeof(splitFilename) / sizeof(splitFilename[0]), "%s.%02u", dest, splitIndex);
							
							outFile = fopen(splitFilename, "wb");
							if (!outFile)
							{
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file for part #%u!", splitIndex);
								uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
								break;
							}
							
							if (fwrite(buf, 1, new_file_chunk_size, outFile) != new_file_chunk_size)
							{
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", off + old_file_chunk_size);
								uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
								break;
							}
						} else {
							if (fwrite(buf, 1, n, outFile) != n)
							{
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write chunk to offset 0x%016lX", off);
								uiDrawString(strbuf, 0, (breaks + 7) * 8, 255, 0, 0);
								break;
							}
						}
						
						progress = (int)(((off + n) * 100) / size);
						
						uiFill(currentFBWidth / 4, (breaks + 3) * 8, currentFBWidth / 2, 16, 0, 0, 0);
						uiFill(currentFBWidth / 4, (breaks + 3) * 8, (((off + n) * (currentFBWidth / 2)) / size), 16, 0, 255, 0);
						
						convertSize(off + n, curSizeStr, sizeof(curSizeStr) / sizeof(curSizeStr[0]));
						snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%d%% [%s / %s]", progress, curSizeStr, totalSizeStr);
						
						uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
						uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, 255, 255, 255);
						
						syncDisplay();
						
						if ((off + n) < size && ((off / DUMP_BUFFER_SIZE) % 10) == 0)
						{
							hidScanInput();
							
							u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
							if (keysDown & KEY_B)
							{
								uiDrawString("Process canceled", 0, (breaks + 7) * 8, 255, 0, 0);
								break;
							}
						}
					}
					
					if (off >= size) success = true;
					
					// Support empty files
					if (!size)
					{
						uiFill(currentFBWidth / 4, (breaks + 3) * 8, currentFBWidth / 2, 16, 0, 255, 0);
						
						snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "100%% [0 B / 0 B]");
						
						uiFill((currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, currentFBWidth - ((currentFBWidth / 4) + (currentFBWidth / 2) + 8), 8, 50, 50, 50);
						uiDrawString(strbuf, (currentFBWidth / 4) + (currentFBWidth / 2) + 8, ((breaks + 3) * 8) + 4, 255, 255, 255);
						
						syncDisplay();
					}
					
					if (!success)
					{
						uiFill(currentFBWidth / 4, (breaks + 3) * 8, (((off + n) * (currentFBWidth / 2)) / size), 16, 255, 0, 0);
						breaks += 7;
					}
					
					free(buf);
				} else {
					breaks++;
					uiDrawString("Failed to allocate memory for the dump process!", 0, breaks * 8, 255, 0, 0);
				}
				
				if (outFile) fclose(outFile);
				
				if (!success)
				{
					if (size > SPLIT_FILE_MIN && doSplitting)
					{
						for(u8 i = 0; i <= splitIndex; i++)
						{
							snprintf(splitFilename, sizeof(splitFilename) / sizeof(splitFilename[0]), "%s.%02u", dest, i);
							remove(splitFilename);
						}
					} else {
						remove(dest);
					}
				}
			} else {
				breaks++;
				uiDrawString("Failed to open output file!", 0, breaks * 8, 255, 255, 255);
			}
			
			fclose(inFile);
		} else {
			breaks++;
			uiDrawString("Failed to open input file!", 0, breaks * 8, 255, 255, 255);
		}
	} else {
		breaks++;
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Destination path is too long! \"%s\" (%lu)", dest, destLen);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
	}
	
	return success;
}

bool _copyDirectory(char* sbuf, size_t source_len, char* dbuf, size_t dest_len, bool splitting)
{
	struct dirent* ent;
	bool success = true;
	
	DIR *dir = opendir(sbuf);
	if (dir)
	{
		sbuf[source_len] = '/';
		dbuf[dest_len] = '/';
		
		while ((ent = readdir(dir)) != NULL)
		{
			if ((strlen(ent->d_name) == 1 && !strcmp(ent->d_name, ".")) || (strlen(ent->d_name) == 2 && !strcmp(ent->d_name, ".."))) continue;
			
			size_t d_name_len = strlen(ent->d_name);
			
			if ((source_len + 1 + d_name_len + 1) >= NAME_BUF_LEN || (dest_len + 1 + d_name_len + 1) >= NAME_BUF_LEN)
			{
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Filename too long! \"%s\" (%lu)", ent->d_name, d_name_len);
				uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
				breaks++;
				success = false;
				break;
			}
			
			strcpy(sbuf + source_len + 1, ent->d_name);
			strcpy(dbuf + dest_len + 1, ent->d_name);
			
			if (ent->d_type == DT_DIR)
			{
				mkdir(dbuf, 0744);
				if (!_copyDirectory(sbuf, source_len + 1 + d_name_len, dbuf, dest_len + 1 + d_name_len, splitting))
				{
					success = false;
					break;
				}
			} else {
				if (!copyFile(sbuf, dbuf, splitting))
				{
					success = false;
					break;
				}
			}
		}
		
		closedir(dir);
	} else {
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Error opening directory \"%s\"", dbuf);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		breaks++;
		success = false;
	}
	
	return success;
}

bool copyDirectory(const char* source, const char* dest, bool splitting)
{
	char sbuf[NAME_BUF_LEN], dbuf[NAME_BUF_LEN];
	size_t source_len = strlen(source), dest_len = strlen(dest);
	
	if (source_len + 1 >= NAME_BUF_LEN)
	{
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Source directory name too long! \"%s\" (%lu)", source, source_len);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		breaks++;
		return false;
	}
	
	if (dest_len + 1 >= NAME_BUF_LEN)
	{
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Destination directory name too long! \"%s\" (%lu)", dest, dest_len);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		breaks++;
		return false;
	}
	
	strcpy(sbuf, source);
	strcpy(dbuf, dest);
	
	mkdir(dbuf, 0744);
	
	return _copyDirectory(sbuf, source_len, dbuf, dest_len, splitting);
}

void removeDirectory(const char *path)
{
	struct dirent* ent;
	char cur_path[NAME_BUF_LEN] = {'\0'};
	
	DIR *dir = opendir(path);
	if (dir)
	{
		while ((ent = readdir(dir)) != NULL)
		{
			if ((strlen(ent->d_name) == 1 && !strcmp(ent->d_name, ".")) || (strlen(ent->d_name) == 2 && !strcmp(ent->d_name, ".."))) continue;
			
			snprintf(cur_path, sizeof(cur_path) / sizeof(cur_path[0]), "%s/%s", path, ent->d_name);
			
			if (ent->d_type == DT_DIR)
			{
				removeDirectory(cur_path);
			} else {
				remove(cur_path);
			}
		}
		
		closedir(dir);
		
		rmdir(path);
	}
}

bool getDirectorySize(const char *path, u64 *out_size)
{
	struct dirent* ent;
	char cur_path[NAME_BUF_LEN] = {'\0'};
	bool success = true;
	u64 total_size = 0, dir_size = 0;
	FILE *file = NULL;
	
	DIR *dir = opendir(path);
	if (dir)
	{
		while ((ent = readdir(dir)) != NULL)
		{
			if ((strlen(ent->d_name) == 1 && !strcmp(ent->d_name, ".")) || (strlen(ent->d_name) == 2 && !strcmp(ent->d_name, ".."))) continue;
			
			snprintf(cur_path, sizeof(cur_path) / sizeof(cur_path[0]), "%s/%s", path, ent->d_name);
			
			if (ent->d_type == DT_DIR)
			{
				if (getDirectorySize(cur_path, &dir_size))
				{
					total_size += dir_size;
					dir_size = 0;
				} else {
					success = false;
					break;
				}
			} else {
				file = fopen(cur_path, "rb");
				if (file)
				{
					fseek(file, 0L, SEEK_END);
					total_size += ftell(file);
					
					fclose(file);
					file = NULL;
				} else {
					success = false;
					break;
				}
			}
		}
		
		closedir(dir);
	} else {
		success = false;
	}
	
	if (success) *out_size = total_size;
	
	return success;
}

bool dumpPartitionData(FsDeviceOperator* fsOperator, u32 partition)
{
	FsFileSystem fs;
	int ret;
	bool success = false;
	u64 total_size;
	char dumpPath[128] = {'\0'}, timeStamp[16] = {'\0'}, totalSizeStr[32] = {'\0'};
	
	if (partition == 0 || partition == 1) workaroundPartitionZeroAccess(fsOperator);
	
	if (openPartitionFs(&fs, fsOperator, partition))
	{
		ret = fsdevMountDevice("gamecard", fs);
		if (ret != -1)
		{
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "fsdevMountDevice succeeded: %d", ret);
			uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			breaks++;
			
			if (getDirectorySize("gamecard:/", &total_size))
			{
				convertSize(total_size, totalSizeStr, sizeof(totalSizeStr) / sizeof(totalSizeStr[0]));
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Total partition data size: %s (%lu bytes)", totalSizeStr, total_size);
				uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
				breaks++;
				
				if (total_size <= freeSpace)
				{
					getCurrentTimestamp(timeStamp, sizeof(timeStamp) / sizeof(timeStamp[0]));
					snprintf(dumpPath, sizeof(dumpPath) / sizeof(dumpPath[0]), "sdmc:/%016lX_part%u_%s", gameCardTitleID, partition, timeStamp);
					
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Copying partition #%u data to \"%s\". Hold B to cancel.", partition, dumpPath);
					uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
					breaks++;
					
					if (copyDirectory("gamecard:/", dumpPath, true))
					{
						success = true;
						breaks += 7;
						uiDrawString("Process successfully completed!", 0, breaks * 8, 0, 255, 0);
					} else {
						removeDirectory(dumpPath);
					}
				} else {
					uiDrawString("Error: not enough free space available in the SD card.", 0, breaks * 8, 255, 0, 0);
				}
			} else {
				snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to get total partition data size!");
				uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
			}
			
			fsdevUnmountDevice("gamecard");
		} else {
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "fsdevMountDevice failed! (%d)", ret);
			uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		}
	} else {
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open partition #%u filesystem!", partition);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
	}
	
	breaks += 2;
	
	return success;
}

bool mountViewPartition(FsDeviceOperator *fsOperator, u32 partition)
{
	FsFileSystem fs;
	int ret;
	bool success = false;
	
	if (partition == 0 || partition == 1) workaroundPartitionZeroAccess(fsOperator);
	
	if (openPartitionFs(&fs, fsOperator, partition))
	{
		ret = fsdevMountDevice("view", fs);
		if (ret != -1)
		{
			//snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "fsdevMountDevice succeeded: %d", ret);
			//uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			//breaks++;
			
			success = true;
		} else {
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "fsdevMountDevice failed! (%d)", ret);
			uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		}
	} else {
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open partition #%u filesystem!", partition);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
	}
	
	return success;
}

bool dumpGameCertificate(FsDeviceOperator* fsOperator)
{
	u32 handle;
	Result result;
	FsStorage gameCardStorage;
	bool success = false;
	FILE *outFile = NULL;
	char timeStamp[16] = {'\0'}, filename[128] = {'\0'};
	char *buf = NULL;
	
	workaroundPartitionZeroAccess(fsOperator);
	
	if (R_SUCCEEDED(result = fsDeviceOperatorGetGameCardHandle(fsOperator, &handle)))
	{
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle succeeded: 0x%08X", handle);
		uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
		breaks++;
		
		if (R_SUCCEEDED(result = fsOpenGameCardStorage(&gameCardStorage, handle, 0)))
		{
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage succeeded: 0x%08X", handle);
			uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
			breaks++;
			
			if (CERT_SIZE <= freeSpace)
			{
				getCurrentTimestamp(timeStamp, sizeof(timeStamp) / sizeof(timeStamp[0]));
				snprintf(filename, sizeof(filename) / sizeof(filename[0]), "sdmc:/%016lX_cert_%s.bin", gameCardTitleID, timeStamp);
				
				outFile = fopen(filename, "wb");
				if (outFile)
				{
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Dumping game card certificate to file \"%s\"...", filename);
					uiDrawString(strbuf, 0, breaks * 8, 255, 255, 255);
					breaks++;
					
					syncDisplay();
					
					buf = (char*)malloc(DUMP_BUFFER_SIZE);
					if (buf)
					{
						if (R_SUCCEEDED(result = fsStorageRead(&gameCardStorage, CERT_OFFSET, buf, CERT_SIZE)))
						{
							if (fwrite(buf, 1, CERT_SIZE, outFile) == CERT_SIZE)
							{
								success = true;
								uiDrawString("Process successfully completed!", 0, breaks * 8, 0, 255, 0);
							} else {
								snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to write certificate data");
								uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
							}
						} else {
							snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "StorageRead failed (0x%08X) at offset 0x%08X", result, CERT_OFFSET);
							uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
						}
						
						free(buf);
					} else {
						uiDrawString("Failed to allocate memory for the dump process!", 0, breaks * 8, 255, 0, 0);
					}
					
					fclose(outFile);
					if (!success) remove(filename);
				} else {
					snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Failed to open output file \"%s\"!", filename);
					uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
				}
			} else {
				uiDrawString("Error: not enough free space available in the SD card.", 0, breaks * 8, 255, 0, 0);
			}
			
			fsStorageClose(&gameCardStorage);
		} else {
			snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "OpenGameCardStorage failed! (0x%08X)", result);
			uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
		}
	} else {
		snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "GetGameCardHandle failed! (0x%08X)", result);
		uiDrawString(strbuf, 0, breaks * 8, 255, 0, 0);
	}
	
	breaks += 2;
	
	return success;
}
