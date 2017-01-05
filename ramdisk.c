#define FUSE_USE_VERSION 26


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fuse.h>
#include "assert.h"

//#define DEBUG
#define DEBUG1
//#define DEBUG2
#define max_Nlen 512

//Global declarations
typedef enum {d,f} nodeType;
char *new_node_name;
int size_node;
//char fileName[4] = "./a";
long mem_avail;
int max_nodes;

// function Declarations
void ramdisk_init();
static int ramdisk_getattr(const char *path, struct stat *st);
struct ramdisk_node* resolve_node(const char *in_path);
struct ramdisk_node* resolve_parent_node(const char *in_path);
int check_path(const char *in_path);
void add_node(struct ramdisk_node *inDir, struct ramdisk_node *new_node);
void remove_node(struct ramdisk_node *toDel);
int ramdisk_unlink(const char* path);
int ramdisk_rmdir(const char *path);
int ramdisk_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int ramdisk_mkdir(const char* path, mode_t mode);
static int ramdisk_rename(const char *from, const char *to);
int ramdisk_open(const char* path, struct fuse_file_info* fi);
int ramdisk_opendir(const char* path, struct fuse_file_info* fi);
int ramdisk_truncate(const char* path, off_t size);
static int ramdisk_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int ramdisk_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int ramdisk_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
static int ramdisk_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
static int ramdisk_utime(const char *path, struct utimbuf *ubuf);
void ramdisk_destroy(void* private_data);



struct ramdisk_node
{
	char *name;
	struct ramdisk_node *parent;
	struct ramdisk_node *child;
	struct ramdisk_node *next_sibling;
	nodeType isType;
	struct stat *meta;
//	size_t data_size;
	char *data;
};
struct ramdisk_node *root;


void ramdisk_init()
{
#ifdef DEBUG1
	printf("in ramdisk_init\n\n");
#endif
	size_node = sizeof(struct ramdisk_node) + sizeof(struct stat);
//	size_node = 0;
	time_t now;
	time(&now);
	root = (struct ramdisk_node *)malloc(sizeof(struct ramdisk_node));
	root->meta = (struct stat *)malloc(sizeof(struct stat));
	root->name = malloc(sizeof(char)*strlen("/"));
	strcpy(root->name, "/");
	root->parent = NULL;
	root->child = NULL;
	root->next_sibling = NULL;
	root->isType = d;

	root->meta->st_mode = S_IFDIR | 0755;
	root->meta->st_nlink = 2;
	root->meta->st_atime = now;
	root->meta->st_ctime = now;
	root->meta->st_mtime = now;
	root->meta->st_uid = getuid();
	root->meta->st_gid = getgid();
	root->meta->st_size = sizeof(struct ramdisk_node) + sizeof(struct stat);
}


static int ramdisk_getattr(const char *path, struct stat *st)
{
#ifdef DEBUG1
	printf("in getattr\n\n");
#endif
	if(check_path(path) == 0)
	{
		struct ramdisk_node* curr_node = resolve_node(path);
		st->st_uid = curr_node->meta->st_uid;
		st->st_gid = curr_node->meta->st_gid;
		st->st_atime = curr_node->meta->st_atime;
		st->st_mtime = curr_node->meta->st_mtime;
		st->st_ctime = curr_node->meta->st_ctime;
		st->st_mode = curr_node->meta->st_mode;
		st->st_nlink = curr_node->meta->st_nlink;
		st->st_size = curr_node->meta->st_size;
		return 0;
	}
	return -ENOENT;
}


struct ramdisk_node* resolve_node(const char *in_path)
{
#ifdef DEBUG1
	printf("in resolve_node\n\n");
#endif
	int found;
	char path[max_Nlen];
	strcpy(path, in_path);
	char *token;
	token = strtok(path, "/");
	if(token == NULL && strcmp(path, "/")) // path refers to the root directory itself
	{
		found = 1;
		return root;
	}
	else
	{
		struct ramdisk_node *curr_node = root;
		struct ramdisk_node *curr_child;
#ifdef DEBUG
		printf("curr_node->name: %s\n", curr_node->name);
#endif	
		while(token!=NULL)
		{
			curr_child = curr_node->child;
			while(curr_child != NULL)
			{
#ifdef DEBUG
				printf("inside while\n");
				printf("Token: %s\n", token);
				printf("curr_child->name: %s\n", curr_child->name);
#endif
				if(strcmp(curr_child->name, token) == 0)
				{
					found = 1;
					break;
				}
				else
				{
					curr_child = curr_child->next_sibling;
				}
			}
			if(found != 1)// if node not found, path is invalid and return with error
			{
				return NULL;
			}
			curr_node = curr_child;
			found = 0;
			token = strtok(NULL, "/");
		}
		return curr_node;
	}
	// should not reach here, serious error
	return NULL;
}


struct ramdisk_node* resolve_parent_node(const char *in_path)
{
#ifdef DEBUG1
	printf("in resolve_parent_node\n\n");
#endif
	int found;
	char path[max_Nlen];
	strcpy(path, in_path);
	char *token;
	token = strtok(path, "/");
	if(token == NULL && strcmp(path, "/")) // path refers to the root directory itself
	{
		found = 1;
		return root;
	}
	else
	{
		struct ramdisk_node *curr_node = root;
		struct ramdisk_node *curr_child;
#ifdef DEBUG
		printf("curr_node->name: %s\n", curr_node->name);
#endif	
		while(token!=NULL)
		{
			curr_child = curr_node->child;
			while(curr_child != NULL)
			{
#ifdef DEBUG
				printf("inside while\n");
				printf("Token: %s\n", token);
				printf("curr_child->name: %s\n", curr_child->name);
#endif
				if(strcmp(curr_child->name, token) == 0)
				{
					found = 1;
					break;
				}
				else
				{
					curr_child = curr_child->next_sibling;
				}
			}
			if(found != 1)
			{
				new_node_name = malloc(sizeof(char)*strlen(token));
				strcpy(new_node_name, token);
				token = strtok(NULL, "/");
				if(token == NULL) // we were at the last part of the path so return the current node
				{
#ifdef DEBUG
					printf("In Token == NULL\n");
#endif
					return curr_node;
				}
				else // invalid node found in between the path
				{
					return NULL;
				}
			}
			curr_node = curr_child;
			found = 0;
			token = strtok(NULL, "/");
		}
		return NULL; // if the whole path is valid and all directories already exist
	}
	// should not reach here, serious error
	return NULL;
}


int check_path(const char *in_path)
{
#ifdef DEBUG1
	printf("in check_path\n\n");
#endif
	int found;
	char path[max_Nlen];
	strcpy(path, in_path);
	char *token;
	token = strtok(path, "/");
	if(token == NULL && strcmp(path, "/")) // path refers to the root directory itself
	{
		found = 1;
		return 0;
	}
	else
	{
		struct ramdisk_node *curr_node = root;
		struct ramdisk_node *curr_child;
#ifdef DEBUG
		printf("curr_node->name: %s\n", curr_node->name);
#endif		
		while(token!=NULL)
		{
			curr_child = curr_node->child;
			while(curr_child != NULL)
			{
#ifdef DEBUG
				printf("inside while\n");
				printf("Token: %s\n", token);
				printf("curr_child->name: %s\n", curr_child->name);
#endif
				if(strcmp(curr_child->name, token) == 0)
				{
					found = 1;
					break;
				}
				else
				{
					curr_child = curr_child->next_sibling;
				}
			}
			if(found != 1)// if node not found, path is invalid and return with error
			{
				return 1;
			}
			curr_node = curr_child;
			found = 0;
			token = strtok(NULL, "/");
		}
		return 0;
	}
	// should not reach here, serious error
	return -1;
}


void add_node(struct ramdisk_node *inDir, struct ramdisk_node *new_node)
{
#ifdef DEBUG1
	printf("in add_node\n\n");
#endif
	new_node->parent = inDir;
	inDir->meta->st_nlink++;
	if(inDir->child == NULL)
	{
		inDir->child = new_node;
	}
	else
	{
		struct ramdisk_node *temp_node;
		temp_node = inDir->child;
		while(temp_node->next_sibling != NULL)
		{
			temp_node = temp_node->next_sibling;
		}
		temp_node->next_sibling = new_node;
	}
	mem_avail = mem_avail - size_node;
}


void remove_node(struct ramdisk_node *toDel)
{
#ifdef DEBUG1
	printf("in remove_node\n\n");
#endif
	mem_avail = mem_avail + size_node;
	if(toDel->isType == f)
	{
		mem_avail = mem_avail + (sizeof(char) * (strlen(toDel->data)));
	}
	struct ramdisk_node *fromDir, *temp_node;
	fromDir = toDel->parent;
	if(fromDir->child == toDel && toDel->next_sibling == NULL)// if this the only directory in parent and needs to be deleted
	{
		fromDir->child = NULL;
		fromDir->meta->st_nlink--;
		free(toDel->meta);
//		free(toDel->data);
//		free(toDel->name);
		free(toDel);
		return;
	}
	else
	{
		temp_node = fromDir->child;
		if(temp_node == toDel)
		{
			fromDir->child = temp_node->next_sibling;
			fromDir->meta->st_nlink--;
			free(toDel->meta);
//			free(toDel->data);
//			free(toDel->name);
			free(toDel);
			return;
		}
		else
		{
			for(temp_node = fromDir->child; temp_node != toDel; temp_node = temp_node->next_sibling)
			{
				if(temp_node->next_sibling == toDel)
				{
					temp_node->next_sibling = toDel->next_sibling;
					fromDir->meta->st_nlink--;
					free(toDel->meta);
//					free(toDel->data);
//					free(toDel->name);
					free(toDel);
					return;
				}
			}
		}
	}
}


int ramdisk_unlink(const char* path)
{
#ifdef DEBUG1
	printf("in unlink\n\n");
#endif
	if(check_path(path) != 0)
	{
		return -ENOENT;
	}
	struct ramdisk_node *toDel;
	toDel = resolve_node(path);
	
	time_t now;
	time(&now);
	toDel->parent->meta->st_ctime = now;
	toDel->parent->meta->st_mtime = now;

//	mem_avail = mem_avail + (sizeof(char) * (strlen(toDel->data)));
	remove_node(toDel);
	return 0;
}

int ramdisk_rmdir(const char *path)
{
#ifdef DEBUG1
	printf("in rmdir\n\n");
#endif
	if(check_path(path) != 0)
	{
		return -ENOENT;
	}
	struct ramdisk_node *toDel;
	toDel = resolve_node(path);
	if(toDel->child != NULL)
	{
		return -ENOTEMPTY;
	}

	time_t now;
	time(&now);
	toDel->parent->meta->st_ctime = now;
	toDel->parent->meta->st_mtime = now;

	remove_node(toDel);
	return 0;
}


int ramdisk_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	if((mem_avail - size_node)<0)
	{
		return -ENOMEM;
	}
#ifdef DEBUG1
	printf("in create\n\n");
	printf("%s\n", path);
#endif
	if(check_path(path) == 0)
	{
		return 0;
	}
	struct ramdisk_node *parent_node, *new_node;
	parent_node = resolve_parent_node(path);
	new_node = (struct ramdisk_node *)malloc(sizeof(struct ramdisk_node));
	new_node->meta = (struct stat *)malloc(sizeof(struct stat));
	new_node->name = malloc(sizeof(char)*strlen(new_node_name));
	new_node->data = malloc(0);
	if(parent_node == NULL)
	{
		return 0;
	}
	time_t now;
	time(&now);
	strcpy(new_node->name, new_node_name);
	new_node->child = NULL;
	new_node->next_sibling = NULL;
	new_node->isType = f;
	new_node->meta->st_mode = S_IFREG | mode;
	new_node->meta->st_nlink = 1;
	new_node->meta->st_atime = now;
	new_node->meta->st_ctime = now;
	new_node->meta->st_mtime = now;
	new_node->meta->st_uid = getuid();
	new_node->meta->st_gid = getgid();
	new_node->meta->st_size = 0;

	parent_node->meta->st_ctime = now;
	parent_node->meta->st_mtime = now;

	add_node(parent_node, new_node);
	return 0;
}

int ramdisk_mkdir(const char* path, mode_t mode)
{
#ifdef DEBUG1
	printf("in mkdir\n\n");
	printf("%s\n", path);
#endif
	if((mem_avail - size_node)<0)
	{
		return -ENOMEM;
	}
	if(check_path(path) == 0)
	{
		return 0;
	}
	struct ramdisk_node *parent_node, *new_node;
	parent_node = resolve_parent_node(path);
	new_node = (struct ramdisk_node *)malloc(sizeof(struct ramdisk_node));
	new_node->meta = (struct stat *)malloc(sizeof(struct stat));
	new_node->name = malloc(sizeof(char)*strlen(new_node_name));
	new_node->data = malloc(0);
	if(parent_node == NULL)
	{
		return 0;
	}
	
	strcpy(new_node->name, new_node_name);
	new_node->child = NULL;
	new_node->next_sibling = NULL;
	new_node->isType = d;
	new_node->meta->st_mode = S_IFDIR | 0755;
	new_node->meta->st_nlink = 2;
	time_t now;
	time(&now);
	new_node->meta->st_atime = now;
	new_node->meta->st_ctime = now;
	new_node->meta->st_mtime = now;
	new_node->meta->st_uid = getuid();
	new_node->meta->st_gid = getgid();
	new_node->meta->st_size = sizeof(struct ramdisk_node) + sizeof(struct stat);

	parent_node->meta->st_ctime = now;
	parent_node->meta->st_mtime = now;

	add_node(parent_node, new_node);
	return 0;
}


static int ramdisk_rename(const char *from, const char *to)
{
#ifdef DEBUG1
	printf("in rename\n\n");
#endif
#ifdef DEBUG
	printf("from: %s\n", from);
	printf("to: %s\n", to);
#endif
	if(check_path(from) != 0)
	{
		return -ENOENT;
	}
	if(check_path(to) == 0)
	{
		return 0;
	}
	struct ramdisk_node *curr_node, *new_node, *new_parent;
	curr_node = resolve_node(from);
	new_parent = resolve_parent_node(to);
//	printf("new_node_name: %s\n", new_node_name);

	new_node = (struct ramdisk_node *)malloc(sizeof(struct ramdisk_node));
	new_node->meta = (struct stat *)malloc(sizeof(struct stat));
	new_node->name = malloc(sizeof(char)*strlen(new_node_name));
	new_node->data = malloc(sizeof(char)*strlen(curr_node->data));
//	printf("here1\n");
	strcpy(new_node->name, new_node_name);
	new_node->parent = new_parent;
	new_node->child = curr_node->child;
	new_node->next_sibling = NULL;
	new_node->isType = curr_node->isType;
//	new_node->data_size = curr_node->data_size;
//	printf("here2\n");


	struct ramdisk_node *temp_node;
	temp_node = new_node->child;
	while(temp_node!= NULL)
	{
		temp_node->parent = new_node;
		temp_node = temp_node->next_sibling;
	}
	

	new_node->meta->st_uid = curr_node->meta->st_uid;
	new_node->meta->st_gid = curr_node->meta->st_gid;
	new_node->meta->st_atime = curr_node->meta->st_atime;
	new_node->meta->st_mtime = curr_node->meta->st_mtime;
	new_node->meta->st_ctime = curr_node->meta->st_ctime;
	new_node->meta->st_mode = curr_node->meta->st_mode;
	new_node->meta->st_nlink = curr_node->meta->st_nlink;
	new_node->meta->st_size = curr_node->meta->st_size;

//	printf("here3\n");
	strcpy(new_node->data, curr_node->data);

	remove_node(curr_node);
	add_node(new_parent, new_node);
	return 0;
}


int ramdisk_open(const char* path, struct fuse_file_info* fi)
{
#ifdef DEBUG1
	printf("in open\n\n");
#endif
	if(check_path(path) == 0)
	{
		return 0;
	}
	return -ENOENT;
}

int ramdisk_opendir(const char* path, struct fuse_file_info* fi)
{
#ifdef DEBUG1
	printf("in opendir\n\n");
#endif
	if(check_path(path) == 0)
	{
		return 0;
	}
	return -ENOENT;
}


int ramdisk_truncate(const char* path, off_t size)
{
#ifdef DEBUG1
	printf("in truncate: path = %s\n", path);
#endif
	if(check_path(path) != 0)
	{
		return -ENOENT;
	}
	struct ramdisk_node *file = resolve_node(path);
	if(file->isType == d)
	{
		return -EISDIR;
	}
	size_t curr_size = file->meta->st_size;
	if(curr_size > size)
	{
		file->data = realloc(file->data, (size));
		file->meta->st_size = size;
		mem_avail = mem_avail + curr_size - size;
	}
	return 0;
}


static int ramdisk_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef DEBUG1
	printf("in write: path = %s\n", path);
#endif
	if(check_path(path) != 0)
	{
		return -ENOENT;
	}
	struct ramdisk_node *file = resolve_node(path);	
#ifdef DEBUG
	printf("file name: %s\n", file->name);
#endif
	if(file->isType == d)
	{
		return -EISDIR;
	}
	
	size_t curr_size = file->meta->st_size;
	if(curr_size == 0) // if this is the first time data is being written, data string needs to be initialized
	{
#ifdef DEBUG
		printf("in write: first time\n");
		printf("size = %lu  ", size);
#endif
		if((mem_avail - (sizeof(char) * size)) < 0)
		{
			return -ENOMEM;
		}
		file->data = malloc(sizeof(char) * size);
//		printf("buf = %s  ", buf);
		memcpy(file->data, buf, size);
//		printf("file-> data = %s  ", file->data);
		file->meta->st_size = size;
#ifdef DEBUG
		printf("file->meta->st_size = %lu\n\n", file->meta->st_size);
#endif
		time_t now;
		time(&now);
		file->meta->st_atime = now;
		file->meta->st_ctime = now;
		file->meta->st_mtime = now;
		file->parent->meta->st_ctime = now;
		file->parent->meta->st_mtime = now;
		mem_avail = mem_avail - (sizeof(char) * size);
	}

	else if((offset + size) > curr_size)
	{
#ifdef DEBUG
		printf("in write: increasing size\n");
		printf("offset + size: %lu  ", offset + size);
#endif
		if((mem_avail - (sizeof(char) * (offset + size - curr_size)) ) < 0)
		{
			return -ENOMEM;
		}
		file->data = realloc(file->data, sizeof(char) * (offset + size));
		if(file->data == NULL)
		{
			return -ENOSPC;
		}
//		printf("buf = %s  ", buf);
		memcpy(file->data + offset, buf, size);
//		printf("file-> data = %s\n", file->data);
		file->meta->st_size = offset + size;
#ifdef DEBUG
		printf("file->meta->st_size = %lu\n\n", file->meta->st_size);
#endif
		time_t now;
		time(&now);
		file->meta->st_atime = now;
		file->meta->st_ctime = now;
		file->meta->st_mtime = now;
		file->parent->meta->st_ctime = now;
		file->parent->meta->st_mtime = now;
		mem_avail = mem_avail - (sizeof(char) * (offset + size - curr_size));
	}

	else // writing only in between of the file
	{
#ifdef DEBUG
		printf("in write: writing in between\n");
		printf("in else: %lu\n\n", curr_size);
#endif
//		file->data = realloc(file->data, sizeof(char) * (offset + size));
//		printf("buf = %s  ", buf);
		memcpy(file->data + offset, buf, size);
//		printf("file-> data = %s\n", file->data);
		time_t now;
		time(&now);
		file->meta->st_atime = now;
		file->meta->st_ctime = now;
		file->meta->st_mtime = now;
		file->parent->meta->st_ctime = now;
		file->parent->meta->st_mtime = now;
	}
	return size;
}


static int ramdisk_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
#ifdef DEBUG1
	printf("in read: path = %s\n", path);
#endif
	if(check_path(path) != 0)
	{
		return -ENOENT;
	}
	struct ramdisk_node *file = resolve_node(path);
	if(file->isType == d)
	{
		return -EISDIR;
	}
	size_t curr_size = file->meta->st_size;
	if(offset < curr_size)// to ensure we are reading data from inside the file
	{
#ifdef DEBUG
		printf("ramdisk_read curr_size = %lu  ", curr_size);
#endif
		if((offset + size) > curr_size) //if some part of the size lies outside the file's scope
		{
			size = curr_size - offset;
		}
#ifdef DEBUG
		printf("%lu\n", offset + size);
		printf("ramdisk_read file-> data = %s", file->data + offset);
#endif
		memcpy(buf, file->data + offset, size);
#ifdef DEBUG
		printf("ramdisk_read buf = %s\n", buf);
#endif
		time_t now;
		time(&now);
		file->meta->st_atime = now;
		return size;
	}
	else
	{
		return 0;
	}
}

int ramdisk_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
#ifdef DEBUG1
	printf("in readdir\n\n");
#endif
	if(check_path(path) != 0)
	{
		return -ENOENT;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	struct ramdisk_node *parent_node, *temp_node;
	parent_node = resolve_node(path);
	temp_node = parent_node->child;

	if(temp_node == NULL)
	{
		return 0;
	}

	filler(buf, temp_node->name, NULL, 0);
	while(temp_node->next_sibling != NULL)
	{
		temp_node = temp_node->next_sibling;
		filler(buf, temp_node->name, NULL, 0);
	}
	return 0;
}


static int ramdisk_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
#ifdef DEBUG1
	printf("in fsync\n\n");
#endif
	return 0;
}

static int ramdisk_utime(const char *path, struct utimbuf *ubuf)
{
#ifdef DEBUG1
	printf("in utime\n\n");
#endif
	return 0;
}

void ramdisk_destroy(void* private_data)
{
//	printf("works\n");
/*	FILE *fr = fopen(fileName, "wb");
	if (fr) {
		fwrite(root, sizeof(struct ramdisk_node), 1, fr);
    		fclose(fr);
	}
*/
}

// Referred from the hello.c example attached with the FUSE package
static struct fuse_operations ramdisk_oper = {
	.getattr	= ramdisk_getattr,
	.readdir	= ramdisk_readdir,
	.opendir	= ramdisk_opendir,
	.mkdir		= ramdisk_mkdir,
	.rmdir		= ramdisk_rmdir,
	.create		= ramdisk_create,
	.open		= ramdisk_open,
	.write		= ramdisk_write,
	.read		= ramdisk_read,
	.unlink		= ramdisk_unlink,
	.fsync		= ramdisk_fsync,
	.utime		= ramdisk_utime,
	.rename		= ramdisk_rename,
	.truncate	= ramdisk_truncate,
	.destroy	= ramdisk_destroy
};
//

int main(int argc, char *argv[])
{
/*	if(argc!=3)
	{
		printf("Too many or too less arguments\nUsage: ./ramdisk <mount_point> <size>\n");
		return -1;
	}
	mem_avail = atoi(argv[2]);
	if(mem_avail<1)
	{
		printf("Initialize the filesystem with at least 1MB memory\n");
		return -ENOMEM;
	}
*/	mem_avail = 250;	
	mem_avail = mem_avail *1024*1024;
//	printf("mem_avail = %ld\n", mem_avail);

//	argc--;

	ramdisk_init();
/*	max_nodes = mem_avail/sizeof(struct ramdisk_node);
	printf("%s\n", fileName);
	FILE *fr = fopen(fileName, "rb");
	printf("here\n");
	if(fr != NULL)
	{
		fread(root, sizeof(struct ramdisk_node), 1, fr);
		printf("here\n");
		fclose(fr);
	}
*/	fuse_main(argc, argv, &ramdisk_oper, NULL);
	return 0;
}
