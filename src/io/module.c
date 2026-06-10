#include <debug.h>
#include <arch/cpu.h>
#include <mm/heap.h>
#include <dsa/hashmap.h>
#include <fs/vfs.h>
#include "module.h"

static struct hashmap64 _devices;
static struct fs_node *_dev_root = NULL;
static struct vfs_mount *dev_mp;

void io_init()
{
    _dev_root = vfs_create_dir("/", "dev", 0, NULL);
    if (_dev_root == NULL)
    {
        dprintk("[virtio_mmio@%x] cannot creat mountpoint \"%s\"!\r\n");
        hang();
    }
    dev_mp = vfs_create_mountpoint("/dev", NULL, NULL, &io_read, &io_write);

    hashmap64_init(&_devices, 10);
}

static struct io_module *_io_get_module(char *name)
{
    uintptr_t module_ptr;
    if (!hashmap64_get(&_devices, name, &module_ptr))
        return NULL;

    return (struct io_module *)module_ptr;
}

int io_register_module(char *name, uint64_t drv_info, io_handler_t read, io_handler_t write)
{
    if (_io_get_module(name) != NULL)
    {
        dprintk("[io] there's already a \"%s\" module!\r\n", name);
        return -1;
    }

    struct io_module *module = (struct io_module *)kmalloc(sizeof(struct io_module));
    module->drv_info = drv_info;
    module->read = read;
    module->write = write;

    if (fs_add_file_to_folder(_dev_root, name, 0, 0, NULL, dev_mp) == NULL)
    {
        dprintk("[io] cannot add \"%s\" to /dev!\r\n", name);
        return -1;
    }

    hashmap64_set(&_devices, name, (uintptr_t)module);

    return 0;
}

int io_unregister_module(char *name)
{
    struct io_module *module = _io_get_module(name);
    if (module == NULL)
    {
        dprintk("[io] module \"%s\" not found!\r\n", name);
        return -1;
    }

    if (fs_remove_child(_dev_root, name) < 0)
    {
        dprintk("[io] cannot remove \"%s\" from /dev!\r\n", name);
        return -1;
    }

    hashmap64_remove(&_devices, name);

    return 0;
}

int io_read(struct fs_node *node, uint8_t *buff, size_t count, size_t offset)
{
    dprintk("[io] read(): mod=\"%s\", buff=0x%08x, count=%d, offset=%d\r\n", node->name, buff, count, offset);

    struct io_module *module = _io_get_module(node->name);
    if (module == NULL)
    {
        dprintk("[io] module \"%s\" not found!\r\n", node->name);
        return -1;
    }

    return module->read(buff, count, offset, module->drv_info);
}

int io_write(struct fs_node *node, uint8_t *buff, size_t count, size_t offset)
{
    dprintk("[io] write(): mod=\"%s\", buff=0x%08x, count=%d, offset=%d\r\n", node->name, buff, count, offset);

    struct io_module *module = _io_get_module(node->name);
    if (module == NULL)
    {
        dprintk("[io] module \"%s\" not found!\r\n", node->name);
        return -1;
    }

    return module->write(buff, count, offset, module->drv_info);
}