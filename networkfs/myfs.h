#ifndef MYFS
#define MYFS

#include <linux/fs.h>

#define BUFF_SIZE 512
#define NAME_LEN 256
#define INO_LEN 10

void networkfs_register(void);
void networkfs_unregister(void);
struct dentry *networkfs_mount(struct file_system_type *, int, const char *,
                               void *);
int networkfs_fill_super(struct super_block *, void *, int);
struct inode *networkfs_get_inode(struct super_block *, const struct inode *,
                                  umode_t, int);
void networkfs_kill_sb(struct super_block *);
struct dentry *networkfs_lookup(struct inode *, struct dentry *, unsigned int);
int networkfs_iterate(struct file *, struct dir_context *);
int networkfs_create(struct user_namespace *, struct inode *, struct dentry *,
                     umode_t, bool);
int networkfs_unlink(struct inode *, struct dentry *);
int networkfs_mkdir(struct user_namespace *, struct inode *, struct dentry *,
                    umode_t);
int networkfs_rmdir(struct inode *, struct dentry *);
ssize_t networkfs_read(struct file *, char *, size_t, loff_t *);
ssize_t networkfs_write(struct file *, const char *, size_t, loff_t *);
int networkfs_link(struct dentry *, struct inode *, struct dentry *);

#endif
