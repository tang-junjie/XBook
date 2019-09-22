/*
 * file:		fs/vfs.c
 * auther:		Jason Hu
 * time:		2019/8/5
 * copyright:	(C) 2018-2019 by Book OS developers. All rights reserved.
 */

#include <book/config.h>
#include <book/arch.h>
#include <book/slab.h>
#include <book/debug.h>
#include <share/string.h>
#include <share/math.h>
#include <book/deviceio.h>
#include <book/vmalloc.h>
#include <book/vmarea.h>

#include <fs/partition.h>
#include <fs/bofs/bofs.h>
#include <fs/bofs/file.h>
#include <fs/interface.h>

PUBLIC int SysOpen(const char *path, unsigned int flags)
{
	return BOFS_Open(path, flags);
}

PUBLIC int SysLseek(int fd, unsigned int offset, char flags)
{
	return BOFS_Lseek(fd, offset, flags);
}

PUBLIC int SysRead(int fd, void *buffer, unsigned int size)
{
	return BOFS_Read(fd, buffer, size);
}

PUBLIC int SysWrite(int fd, void *buffer, unsigned int size)
{
	return BOFS_Write(fd, buffer, size);
}

PUBLIC int SysClose(int fd)
{
	return BOFS_Close(fd);
}

PUBLIC int SysStat(const char *path, void *buf)
{	
	return BOFS_Stat(path, (struct BOFS_Stat *)buf);
}

PUBLIC int SysUnlink(const char *pathname)
{
	return BOFS_Unlink(pathname);
}

PUBLIC int SysIoctl(int fd, int cmd, int arg)
{
	return BOFS_Ioctl(fd, cmd, arg);
}

PUBLIC int SysAccess(const char *pathname, int mode)
{
	return BOFS_Access(pathname, mode);
}

PUBLIC int SysGetMode(const char* pathname)
{
	return BOFS_GetMode(pathname);
}

PUBLIC int SysSetMode(const char* pathname, int mode)
{
	return BOFS_SetMode(pathname, mode);
}

PUBLIC int SysMakeDir(const char *pathname)
{
	return BOFS_MakeDir(pathname);
}

PUBLIC int SysRemoveDir(const char *pathname)
{
	return BOFS_RemoveDir(pathname);
}

PUBLIC int SysMountDir(const char *devpath, const char *dirpath)
{
	return BOFS_MountDir(devpath, dirpath);
}

PUBLIC int SysUnmountDir(const char *dirpath)
{
	return BOFS_UnmountDir(dirpath);
}

PUBLIC int SysGetCWD(char* buf, unsigned int size)
{
	return BOFS_GetCWD(buf, size);
}

PUBLIC int SysChangeCWD(const char *pathname)
{
	return BOFS_ChangeCWD(pathname);
}

PUBLIC int SysChangeName(const char *pathname, char *name)
{
	return BOFS_ResetName(pathname, name);
}

PUBLIC DIR *SysOpenDir(const char *pathname)
{
	struct BOFS_Dir *dir = BOFS_OpenDir(pathname);
	if (dir == NULL)
		return NULL;

	return (DIR *)dir;
}

PUBLIC void SysCloseDir(DIR *dir)
{
	BOFS_CloseDir((struct BOFS_Dir *)dir);
}

PUBLIC int SysReadDir(DIR *dir, DIRENT *buf)
{
	struct BOFS_DirEntry *dirEntry = BOFS_ReadDir((struct BOFS_Dir *)dir);
	if (dirEntry == NULL)
		return -1;

	/* 复制目录项内容 */
	buf->inode = dirEntry->inode;
	buf->type = dirEntry->type;
	memset(buf->name, 0, NAME_MAX);
	strcpy(buf->name, dirEntry->name);

	/* 获取节点信息，并填充到缓冲区中 */
	struct BOFS_Inode inode;
	
	struct BOFS_Dir *bdir = (struct BOFS_Dir *)dir;

	BOFS_LoadInodeByID(&inode, dirEntry->inode, bdir->superBlock);
	
	buf->deviceID = inode.deviceID;
	buf->otherDeviceID = inode.otherDeviceID;
	buf->mode = inode.mode;
	buf->links = inode.links;
	buf->size = inode.size;
	buf->crttime = inode.crttime;
	buf->mdftime = inode.mdftime;
	buf->acstime = inode.acstime;

	return 0;
}

PUBLIC void SysRewindDir(DIR *dir)
{
	BOFS_RewindDir((struct BOFS_Dir *)dir);
}

/* 同步磁盘上的数据到文件系统 */
#define SYNC_DISK_DATA

/* 数据是在软盘上的 */
#define DATA_ON_FLOPPY
#define FLOPPY_DATA_ADDR	0x80042000


/* 数据是在软盘上的 */
#define DATA_ON_IDE

/* 要写入文件系统的文件 */
#define FILE_ID 1

#if FILE_ID == 1
	#define FILE_NAME "/bin/init"
	#define FILE_SECTORS 30
#elif FILE_ID == 2
	#define FILE_NAME "/bin/shell"
	#define FILE_SECTORS 30
#elif FILE_ID == 3
	#define FILE_NAME "/bin/test"
	#define FILE_SECTORS 100
#endif

#define FILE_SIZE (FILE_SECTORS * SECTOR_SIZE)

PRIVATE void WriteDataToFS()
{
	#ifdef SYNC_DISK_DATA
	char *buf = kmalloc(FILE_SECTORS * SECTOR_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		Panic("kmalloc for buf failed!\n");
	}
	
	/* 把文件加载到文件系统中来 */
	int fd = SysOpen(FILE_NAME, O_CREAT | O_RDWR);
    if (fd < 0) {
        printk("file open failed!\n");
		return;
    }
	
	memset(buf, 0, FILE_SECTORS * SECTOR_SIZE);
	
	/* 根据数据存在的不同形式选择不同的加载方式 */
	#ifdef DATA_ON_FLOPPY
		memcpy(buf, (void *)FLOPPY_DATA_ADDR, FILE_SIZE);
	#endif

	#ifdef DATA_ON_IDE


	#endif
	
	if (SysWrite(fd, buf, FILE_SIZE) != FILE_SIZE) {
		printk("write failed!\n");
	}

	SysLseek(fd, 0, SEEK_SET);
	if (SysRead(fd, buf, FILE_SIZE) != FILE_SIZE) {
		printk("read failed!\n");
	}



	kfree(buf);
	SysClose(fd);
	printk("load file sucess!\n");
	//Panic("test");
	#endif
}

/**
 * InitVFS - 初始化虚拟文件系统
 * 
 * 虚拟文件系统的作用是，一个抽象的接口层，而不是做具体的文件管理
 * 操作，只是起到文件接口的统一，在这里进行抽象化。
 */
PUBLIC void InitFileSystem()
{
	PART_START("VFS");

	/* 初始化BOFS */
	BOFS_Init();

	/* 挂载第二个分区 */
	SysMakeDir("/mnt/c");

	SysMountDir("/dev/hda1", "/mnt/c");

	WriteDataToFS();

/*
	DIR *dir;
	DIRENT dirent;
	
	dir = SysOpenDir("/mnt/c");
	if (dir != NULL) {
		
		while (!SysReadDir(dir, &dirent)) {
			printk("dir name %s type %d inode %d size %d\n",
				dirent.name, dirent.type, dirent.inode, dirent.size);	
		}
	}*/
	/*
    int fd = SysOpen("/test2", O_CREAT | O_RDWR);
    if (fd < 0) {
        printk("file open failed!\n");
    }

    SysClose(fd);

	fd = SysOpen("/mnt/c/test", O_CREAT | O_RDWR);
    if (fd < 0) {
        printk("file open failed!\n");
    }

	char buf[32];
	memset(buf, 0x5a, 32); 
	if (SysWrite(fd, buf, 32) != 32) {
		printk("write failed!\n");
	}
	memset(buf, 0, 32);
	
	SysLseek(fd, 0, SEEK_SET);

	if (SysRead(fd, buf, 32) != 32) {
		printk("read failed!\n");
	}
	printk("%x %x\n", buf[0], buf[31]);
    SysClose(fd);

	SysMakeDir("/mnt/c/dir");*/
/*
	dir = SysOpenDir("/mnt/c");
	if (dir != NULL) {
		
		while (!SysReadDir(dir, &dirent)) {
			printk("dir name %s type %d inode %d size %d\n",
				dirent.name, dirent.type, dirent.inode, dirent.size);	
		}
	}*/
	/*
	if (SysRemoveDir("/mnt/c/dir")) {
		printk("remove dir failed!\n");
	}

	if (SysUnlink("/mnt/c/test")) {
		printk("unlink dir failed!\n");
	}*/

	PART_END();
}
