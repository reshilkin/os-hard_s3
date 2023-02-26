#include "myfs.h"

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "http.h"

struct file_operations networkfs_dir_ops = {
    .iterate = networkfs_iterate,
};

struct file_operations networkfs_file_ops = {
    .read = networkfs_read,
    .write = networkfs_write,
};

struct inode_operations networkfs_inode_ops = {
    .lookup = networkfs_lookup,
    .create = networkfs_create,
    .unlink = networkfs_unlink,
    .mkdir = networkfs_mkdir,
    .rmdir = networkfs_rmdir,
    .link = networkfs_link,
};

struct file_system_type networkfs_fs_type = {.name = "networkfs",
                                             .mount = networkfs_mount,
                                             .kill_sb = networkfs_kill_sb};

struct entries entries;
struct content content;
char adv_name[NAME_LEN * 3];
char rwbuf[BUFF_SIZE];
char strbuf[BUFF_SIZE * 3];

void convert_str(char const *src, char *dst, size_t len) {
  size_t cur, i;
  cur = 0;
  for (i = 0; i < len; i++) {
    if ((src[i] < 'a' || src[i] > 'z') && (src[i] < 'A' || src[i] > 'Z') &&
        (src[i] < '0' || src[i] > '9')) {
      dst[cur] = '%';
      dst[cur + 1] = (char)(src[i] / 16 + (src[i] / 16 < 10 ? '0' : 'A' - 10));
      dst[cur + 2] = (char)(src[i] % 16 + (src[i] % 16 < 10 ? '0' : 'A' - 10));
      cur += 3;
    } else {
      dst[cur] = src[i];
      cur++;
    }
  }
  dst[cur] = 0;
}

char const *get_adv_name(char const *name) {
  convert_str(name, adv_name, strlen(name));
  return adv_name;
}

int networkfs_link(struct dentry *old_dentry, struct inode *parent_dir,
                   struct dentry *new_dentry) {
  char parent_s[INO_LEN];
  char src_s[INO_LEN];
  ino_t parent;
  ino_t src;

  parent = parent_dir->i_ino;
  src = old_dentry->d_inode->i_ino;
  sprintf(parent_s, "%lu", parent);
  sprintf(src_s, "%lu", src);

  int64_t res = networkfs_http_call(
      (char *)(parent_dir->i_sb->s_root->d_fsdata), "link", NULL, 0, 3,
      "source", src_s, "parent", parent_s, "name", new_dentry->d_name.name);

  if (res) {
    return res;
  }

  return 0;
}

ssize_t networkfs_read(struct file *filp, char *buffer, size_t len,
                       loff_t *offset) {
  char ino_s[INO_LEN];
  struct dentry *dentry;
  struct inode *inode;
  ino_t ino;

  dentry = filp->f_path.dentry;
  inode = dentry->d_inode;
  ino = inode->i_ino;
  sprintf(ino_s, "%lu", ino);

  int64_t res = networkfs_http_call((char *)(inode->i_sb->s_root->d_fsdata),
                                    "read", (char *)(&content),
                                    sizeof(struct content), 1, "inode", ino_s);

  if (res) {
    return -1;
  }

  if (*offset > content.content_length) {
    return 0;
  }

  if ((*offset + len) > content.content_length) {
    len = content.content_length - *offset;
  }

  res = copy_to_user(buffer, content.content + *offset, len);
  if (res) {
    return -1;
  }
  *offset += len;
  return len;
}

ssize_t networkfs_write(struct file *filp, const char *buffer, size_t len,
                        loff_t *offset) {
  char ino_s[INO_LEN];
  struct dentry *dentry;
  struct inode *inode;
  ino_t ino;

  if (len + *offset > BUFF_SIZE) {
    return -1;
  }

  int64_t res = copy_from_user(rwbuf + *offset, buffer, len);
  if (res) {
    return -1;
  }
  convert_str(rwbuf, strbuf, len + *offset);

  dentry = filp->f_path.dentry;
  inode = dentry->d_inode;
  ino = inode->i_ino;
  sprintf(ino_s, "%lu", ino);

  res = networkfs_http_call((char *)(inode->i_sb->s_root->d_fsdata), "write",
                            NULL, 0, 2, "inode", ino_s, "content", strbuf);

  if (res) {
    return -1;
  }
  *offset += len;
  return len;
}

int networkfs_rmdir(struct inode *parent_inode, struct dentry *child_dentry) {
  char root_s[INO_LEN];
  const char *name;
  ino_t root;

  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  sprintf(root_s, "%lu", root);

  int64_t res = networkfs_http_call(
      (char *)(parent_inode->i_sb->s_root->d_fsdata), "rmdir", NULL, 0, 2,
      "parent", root_s, "name", get_adv_name(name));

  if (res) {
    return res;
  }

  return 0;
}

int networkfs_mkdir(struct user_namespace *, struct inode *parent_inode,
                    struct dentry *child_dentry, umode_t) {
  char root_s[INO_LEN];
  ino_t new_inode;
  const char *name;
  struct inode *inode;
  ino_t root;

  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  sprintf(root_s, "%lu", root);

  int64_t res = networkfs_http_call(
      (char *)(parent_inode->i_sb->s_root->d_fsdata), "create",
      (char *)(&new_inode), sizeof(ino_t), 3, "parent", root_s, "name",
      get_adv_name(name), "type", "directory");

  if (res) {
    return res;
  }

  inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, new_inode);

  d_add(child_dentry, inode);
  return 0;
}

int networkfs_unlink(struct inode *parent_inode, struct dentry *child_dentry) {
  char root_s[INO_LEN];
  const char *name;
  ino_t root;

  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  sprintf(root_s, "%lu", root);

  int64_t res = networkfs_http_call(
      (char *)(parent_inode->i_sb->s_root->d_fsdata), "unlink", NULL, 0, 2,
      "parent", root_s, "name", get_adv_name(name));

  if (res) {
    return res;
  }

  return 0;
}

int networkfs_create(struct user_namespace *, struct inode *parent_inode,
                     struct dentry *child_dentry, umode_t mode, bool b) {
  ino_t new_inode;
  ino_t root;
  char root_s[INO_LEN];
  struct inode *inode;
  const char *name;

  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  sprintf(root_s, "%lu", root);

  int64_t res = networkfs_http_call(
      (char *)(parent_inode->i_sb->s_root->d_fsdata), "create",
      (char *)(&new_inode), sizeof(ino_t), 3, "parent", root_s, "name",
      get_adv_name(name), "type", "file");

  if (res) {
    return res;
  }

  inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, new_inode);
  d_add(child_dentry, inode);
  return 0;
}

int networkfs_iterate(struct file *filp, struct dir_context *ctx) {
  char fsname[NAME_LEN];
  struct dentry *dentry;
  struct inode *inode;
  unsigned long offset;
  unsigned char ftype;
  int stored;
  ino_t ino;
  ino_t dino;
  char ino_s[INO_LEN];

  dentry = filp->f_path.dentry;
  inode = dentry->d_inode;
  offset = filp->f_pos;
  stored = 0;
  ino = inode->i_ino;
  sprintf(ino_s, "%lu", ino);

  int64_t res = networkfs_http_call((char *)(inode->i_sb->s_root->d_fsdata),
                                    "list", (char *)(&entries),
                                    sizeof(struct entries), 1, "inode", ino_s);

  if (res) {
    return -1;
  }

  while (true) {
    if (offset == 0) {
      strcpy(fsname, ".");
      ftype = DT_DIR;
      dino = ino;
    } else if (offset == 1) {
      strcpy(fsname, "..");
      ftype = DT_DIR;
      dino = dentry->d_parent->d_inode->i_ino;
    } else if (offset - 2 < entries.entries_count) {
      strcpy(fsname, entries.entries[offset - 2].name);
      ftype = entries.entries[offset - 2].entry_type;
      dino = entries.entries[offset - 2].ino;
    } else {
      return stored;
    }
    dir_emit(ctx, fsname, strlen(fsname), dino, ftype);
    stored++;
    offset++;
    ctx->pos = offset;
  }
  return stored;
}

struct dentry *networkfs_lookup(struct inode *parent_inode,
                                struct dentry *child_dentry,
                                unsigned int flag) {
  char root_s[INO_LEN];
  ino_t root;
  struct inode *inode;
  struct entry_info e_info;

  const char *name = child_dentry->d_name.name;

  root = parent_inode->i_ino;
  sprintf(root_s, "%lu", root);

  int64_t res = networkfs_http_call(
      (char *)(parent_inode->i_sb->s_root->d_fsdata), "lookup",
      (char *)(&e_info), sizeof(struct entry_info), 2, "parent", root_s, "name",
      get_adv_name(name));

  if (res) {
    return NULL;
  }

  if (e_info.entry_type == DT_DIR) {
    inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, e_info.ino);
  } else {
    inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, e_info.ino);
  }

  d_add(child_dentry, inode);
  return child_dentry;
}

void networkfs_kill_sb(struct super_block *sb) { kfree(sb->s_root->d_fsdata); }

struct inode *networkfs_get_inode(struct super_block *sb,
                                  const struct inode *dir, umode_t mode,
                                  int i_ino) {
  struct inode *inode;

  inode = new_inode(sb);
  if (inode != NULL) {
    inode->i_ino = i_ino;
    inode->i_op = &networkfs_inode_ops;
    if (mode & S_IFDIR) {
      inode->i_fop = &networkfs_dir_ops;
    } else {
      inode->i_fop = &networkfs_file_ops;
    }
    inode_init_owner(&init_user_ns, inode, dir, mode | S_IRWXUGO);
  }
  return inode;
}

int networkfs_fill_super(struct super_block *sb, void *data, int silent) {
  struct inode *inode;

  inode = networkfs_get_inode(sb, NULL, S_IFDIR, 1000);
  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    return -ENOMEM;
  }
  return 0;
}

struct dentry *networkfs_mount(struct file_system_type *fs_type, int flags,
                               const char *token, void *data) {
  struct dentry *ret;
  char *tmp;

  ret = mount_nodev(fs_type, flags, data, networkfs_fill_super);

  tmp = kmalloc(36, GFP_KERNEL);
  memcpy(tmp, token, 36);
  ret->d_fsdata = tmp;

  return ret;
}

void networkfs_register(void) { register_filesystem(&networkfs_fs_type); }

void networkfs_unregister(void) { unregister_filesystem(&networkfs_fs_type); }
