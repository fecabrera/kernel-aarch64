import "std";
import "scheduler";
import "filesystem/vfs";
import "filesystem/fs";

fn command_ls(argc: int64, argv: uint8**) -> int64 {
    let filename: uint8* = argc > 1 ? argv[1] : "/";
    
    let proc = scheduler_get_current_process();
    
    let node = vfs_get_node_for_path(filename, proc->cwd);
    if (node == null) {
        println("\"%s\" not found!", filename);
        return -1;
    }

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        println("\"%s\" is not a folder!", filename);
        return -2;
    }

    until((node->attrs & FS_NODE_ATTRS_FLAG_LINK) == 0)
        node = node->child;

    let current = node->child;
    until(current == null) {
        defer current = current->next;
        if ((current->attrs & FS_NODE_ATTRS_FLAG_HIDDEN) != 0)
            continue;
        
        println("%s", current->name);
    }

    return 0;
}

