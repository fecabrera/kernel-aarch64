#include <debug.h>
#include <mm/heap.h>
#include <dsa/hashmap.h>
#include "module.h"

static struct hashmap64 _devices;

void io_init()
{
    hashmap64_init(&_devices, 10);
}

static struct io_module *_io_get_module(char *name)
{
    uintptr_t module_ptr;
    if (!hashmap64_get(&_devices, name, &module_ptr))
        return NULL;

    return (struct io_module *)module_ptr;
}

int io_register_module(char *name, uint8_t attrs, io_handler_t read, io_handler_t write)
{
    if (_io_get_module(name) != NULL)
    {
        printk("[io] there's already a \"%s\" module!\r\n", name);
        return -1;
    }

    struct io_module *module = (struct io_module *)kmalloc(sizeof(struct io_module));
    module->attrs = attrs;
    module->read = read;
    module->write = write;

    hashmap64_set(&_devices, name, (uintptr_t)module);

    return 0;
}

int io_unregister_module(char *name)
{
    struct io_module *module = _io_get_module(name);
    if (module == NULL)
    {
        printk("[io] module \"%s\" not found!\r\n", name);
        return -1;
    }

    hashmap64_remove(&_devices, name);

    return 0;
}

int io_read(char *name, uint8_t *buffer, size_t n)
{
    struct io_module *module = _io_get_module(name);
    if (module == NULL)
    {
        printk("[io] module \"%s\" not found!\r\n", name);
        return -1;
    }

    return module->read(buffer, n);
}

int io_write(char *name, uint8_t *buffer, size_t n)
{
    struct io_module *module = _io_get_module(name);
    if (module == NULL)
    {
        printk("[io] module \"%s\" not found!\r\n", name);
        return -1;
    }

    return module->write(buffer, n);
}