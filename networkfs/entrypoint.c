#include <linux/module.h>

#include "myfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivanov Ivan");
MODULE_VERSION("0.01");

int networkfs_init(void) {
  networkfs_register();
  return 0;
}

void networkfs_exit(void) { networkfs_unregister(); }

module_init(networkfs_init);
module_exit(networkfs_exit);
