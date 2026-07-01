import "libc/string";
import "memory";
import "string";
import "endian";
import "align";

@if (IS_KERNEL) {
    import "debug";
    
    const fdt_print = printk;
}
@else {
    import "libc/stdio";

    const fdt_print = printf;
}

// value expected in fdt_header::magic, identifying a flattened device tree blob
const FDT_MAGIC: uint32 = 0xD00DFEED;

/**
 * Status codes returned by the fdt parsing routines
 */
enum fdt_status: int64 {
    FDT_SUCCESS         = 0,    // operation completed successfully
    FDT_UNKNOWN_TOKEN   = -1,   // an unrecognized token was found in dt_struct
}

/**
 * Tokens found in the device tree structure block (dt_struct). All tokens are
 * stored big-endian in the blob.
 */
enum fdt_token: uint32 {
    FDT_BEGIN_NODE      = 0x00000001,   // start of a node, followed by its name
    FDT_END_NODE        = 0x00000002,   // end of the current node
    FDT_PROP            = 0x00000003,   // a property, followed by an fdt_prop
    FDT_NOP             = 0x00000004,   // no-op, skipped when parsing
    FDT_END             = 0x00000009,   // end of the structure block
}

/**
 * The flattened device tree header, located at the start of the blob. All
 * fields are stored big-endian.
 */
struct fdt_header {
    magic:              uint32;     // must equal FDT_MAGIC
    totalsize:          uint32;     // total size of the blob in bytes
    off_dt_struct:      uint32;     // offset of the structure block from the header
    off_dt_strings:     uint32;     // offset of the strings block from the header
    off_mem_rsvmap:     uint32;     // offset of the memory reservation block
    version:            uint32;     // format version
    last_comp_version:  uint32;     // lowest version this blob is backwards compatible with
    boot_cpuid_phys:    uint32;     // physical id of the boot cpu
    size_dt_strings:    uint32;     // size of the strings block in bytes
    size_dt_struct:     uint32;     // size of the structure block in bytes
}

/**
 * An FDT_BEGIN_NODE entry in the structure block: a token followed by the
 * node's null-terminated name.
 */
struct fdt_node {
    token:              fdt_token;  // FDT_BEGIN_NODE
    name:               char[];     // null-terminated node name
}

/**
 * An FDT_PROP entry in the structure block: a token, the value length, an
 * offset into the strings block for the name, then the value bytes.
 */
struct fdt_prop {
    token:              fdt_token;  // FDT_PROP
    length:             uint32;     // length of the value in bytes
    nameoff:            uint32;     // offset of the property name in the strings block
    value:              char[];     // property value bytes
}

/**
 * fdt file descriptor: holds the raw blob along with cached pointers into it
 * and the parsed node tree.
 */
struct fdt_file {
    data:               byte*;          // pointer to the raw device tree blob
    header:             fdt_header*;    // the blob header (== data)
    dt_struct:          fdt_token*;     // start of the structure block
    dt_strings:         char*;          // start of the strings block
    tree:               fdt_tree_node*; // root of the parsed tree, built by fdt_file_build_tree
}

/**
 * A parsed node in the device tree. Children and sibling nodes are stored as
 * intrusive singly linked lists.
 */
struct fdt_tree_node {
    dt_node:            fdt_node*;              // the underlying node in the blob
    props:              fdt_tree_prop*  = null; // head of this node's property list
    next:               fdt_tree_node*  = null; // next sibling under the same parent
    children:           fdt_tree_node*  = null; // head of this node's child list
}

/**
 * A parsed property, forming a singly linked list within its owning node.
 */
struct fdt_tree_prop {
    dt_prop:            fdt_prop*;              // the underlying property in the blob
    next:               fdt_tree_prop*  = null; // next property in the same node
}

/**
 * Initializes an fdt_file
 *
 * @param self: the fdt_file to initialize
 * @param data: pointer to the raw flattened device tree blob
 */
fn fdt_file_init(self: fdt_file*, data: byte*) {
    self->data = data;
    self->header = data as fdt_header*;
    self->dt_strings = &data[be32(self->header->off_dt_strings)] as char*;
    self->dt_struct = &data[be32(self->header->off_dt_struct)] as fdt_token*;
}

/**
 * Cleans up an fdt_file
 *
 * @param self: the fdt_file to destroy
 */
fn fdt_file_destroy(self: fdt_file*) {
    if (self->tree != null)
        fdt_tree_node_destroy(self->tree);
}

/**
 * Walks the file's dt_struct and builds the tree
 *
 * @param self: the fdt_file whose tree will be built
 */
fn fdt_file_build_tree(self: fdt_file*) {
    fdt_tree_node_init(self->dt_struct, &self->tree);
}

/**
 * Walks the file's tree and retrieves the node for an specific path
 *
 * @param self: the fdt_file to search
 * @param path: the null-terminated node path, e.g. "/soc/uart"
 *
 * @return: the fdt_tree_node for that path, or null if not found
 */
fn fdt_file_find_node(self: fdt_file*, path: char*) -> fdt_tree_node* {
    return fdt_tree_find_node(self->tree, path);
}

/**
 * Walks the tree and prints its contents
 *
 * @param self: the fdt_file whose tree will be printed
 */
fn fdt_file_dump_tree(self: fdt_file*) {
    fdt_dump_tree_node(self->tree, self->dt_strings, 0);
}

/**
 * Walks the dt_struct and print its data
 *
 * @param self: the fdt_file to print
 *
 * @return: fdt_status::FDT_SUCCESS on success
 */
fn fdt_file_dump_data(self: fdt_file*) -> fdt_status {
    let token = self->dt_struct;

    until (token == null) {
        case (be32(*token)) {
        when fdt_token::FDT_BEGIN_NODE:
            fdt_print("fdt_begin_node: %s\n", fdt_get_node_name(token as fdt_node*));
            token = fdt_parse_begin_node(token);
        when fdt_token::FDT_END_NODE:
            fdt_print("fdt_end_node\n");
            token = fdt_parse_end_node(token);
        when fdt_token::FDT_PROP:
            fdt_print("  fdt_prop: %s\n", fdt_get_prop_name(token as fdt_prop*, self->dt_strings));
            token = fdt_parse_prop(token);
        when fdt_token::FDT_END:
            fdt_print("fdt_end\n");
            break;
        when fdt_token::FDT_NOP:
            fdt_print("fdt_nop\n");
            token = fdt_parse_nop(token);
        else:
            fdt_print("unknown token: 0x%08X\n", be32(*token));
            return fdt_status::FDT_UNKNOWN_TOKEN;
        }
    }

    return fdt_status::FDT_SUCCESS;
}

/**
 * Initializes an fdt_tree_node and walks its contents
 *
 * @param token: the FDT_BEGIN_NODE token to parse from
 * @param out: receives the newly allocated fdt_tree_node
 *
 * @return: the next fdt_token after this node, or null on error
 */
fn fdt_tree_node_init(token: fdt_token*, out: fdt_tree_node**) -> fdt_token* {
    let node = new<fdt_tree_node>();
    node->dt_node = token as fdt_node*;
    node->props = null;
    node->children = null;
    node->next = null;
    *out = null;

    token = fdt_parse_begin_node(token);

    let last_child: fdt_tree_node* = null;
    let last_prop: fdt_tree_prop* = null;

    until (token == null) {
        case (be32(*token)) {
        when fdt_token::FDT_BEGIN_NODE:
            let child: fdt_tree_node* = null;
            token = fdt_tree_node_init(token, &child);

            if (last_child == null)
                node->children = child;
            else
                last_child->next = child;
            
            last_child = child;
        when fdt_token::FDT_END_NODE:
            *out = node;
            return fdt_parse_end_node(token);
        when fdt_token::FDT_PROP:
            let prop: fdt_tree_prop* = null;
            token = fdt_tree_prop_init(token, &prop);

            if (last_prop == null)
                node->props = prop;
            else
                last_prop->next = prop;
            
            last_prop = prop;
        when fdt_token::FDT_END:
            break;
        when fdt_token::FDT_NOP:
            token = fdt_parse_nop(token);
        else:
            break;
        }
    }

    // error path, run clean up
    fdt_tree_node_destroy(node);

    return null;
}

/**
 * Walks the node and cleans up its contents
 *
 * @param node: the fdt_tree_node to destroy, along with its props and children
 */
fn fdt_tree_node_destroy(node: fdt_tree_node*) {
    if (node == null) return;
    
    let prop = node->props;
    until (prop == null) {
        let next_prop = prop->next;
        dealloc(prop);
        prop = next_prop;
    }

    let child = node->children;
    until (child == null) {
        let next_child = child->next;
        fdt_tree_node_destroy(child);
        child = next_child;
    }

    dealloc(node);
}

/**
 * Initializes an fdt_tree_prop
 *
 * @param token: the FDT_PROP token to parse from
 * @param out: receives the newly allocated fdt_tree_prop
 *
 * @return: the next fdt_token after this prop
 */
fn fdt_tree_prop_init(token: fdt_token*, out: fdt_tree_prop**) -> fdt_token* {
    let prop = new<fdt_tree_prop>();
    prop->dt_prop = token as fdt_prop*;
    prop->next = null;
    *out = prop;

    return fdt_parse_prop(token);
}

/**
 * Walks the fdt_tree_node and retrieves the node for the specific path
 *
 * @param root: the root fdt_tree_node to search from
 * @param path: the null-terminated node path, e.g. "/soc/uart"
 *
 * @return: the fdt_tree_node for that path, or null if not found
 */
fn fdt_tree_find_node(root: fdt_tree_node*, path: char*) -> fdt_tree_node* {
    if (root == null) return null;
    
    let name: string;
    string_init(&name);
    defer string_destroy(&name);

    let current = root;
    until (current == null) {
        case (*path) {
        when '\0':  // end of string
            string_push(&name, '\0');

            if (name.length > 1)
                // pathname is of type "/abc/def", `current` holds "/abc" so we
                // need to find "def"
                current = fdt_tree_node_find_child(current, name.data);
            
            // pathname is of type "/abc/def/" which needs to be resolved to
            // "/abc/def" `current` already holds "/abc/def" so return it
            return current;
        when '/':  // dir separator
            string_push(&name, '\0');

            if (name.length > 1)
                current = fdt_tree_node_find_child(current, name.data);
            
            // reset string
            string_reset(&name);
        else:
            // store next character
            string_push(&name, *path);
        }

        path = &path[1];
    }
    
    return null;
}

/**
 * Find the fdt_tree_node's child with a specific name
 *
 * @param node: the parent fdt_tree_node whose children are searched
 * @param name: the null-terminated child name to match
 *
 * @return: a child named `name` if exists, null otherwise
 */
fn fdt_tree_node_find_child(node: fdt_tree_node*, name: char*) -> fdt_tree_node* {
    let current = node->children;

    until (current == null) {
        if (strcmp(name, fdt_get_node_name(current->dt_node)) == 0)
            return current;
        
        current = current->next;
    }
    
    return null;
}

/**
 * Find the fdt_tree_node's prop with a specific name
 *
 * @param node: the fdt_tree_node whose props are searched
 * @param name: the null-terminated property name to match
 * @param dt_strings: the strings block used to resolve property names
 *
 * @return: a prop named `name` if exists, null otherwise
 */
fn fdt_tree_node_find_prop(node: fdt_tree_node*, name: char*, dt_strings: char*) -> fdt_tree_prop* {
    let current = node->props;

    until (current == null) {
        if (strcmp(name, fdt_get_prop_name(current->dt_prop, dt_strings)) == 0)
            return current;
        
        current = current->next;
    }
    
    return null;
}

/**
 * Walks the fdt_tree_node and prints its contents
 *
 * @param node: the fdt_tree_node to print
 * @param dt_strings: the strings block used to resolve property names
 * @param level: the current nesting depth, used for indentation
 */
fn fdt_dump_tree_node(node: fdt_tree_node*, dt_strings: char*, level: int32) {
    let name = fdt_get_node_name(node->dt_node);
    fdt_print("%s {\n", name[0] ? name : "/");
    
    let prop = node->props;
    until (prop == null) {
        fdt_dump_tree_prop(prop, dt_strings, level + 1);
        prop = prop->next;
    }

    let child = node->children;
    until (child == null) {
        fdt_dump_tree_node(child, dt_strings, level + 1);
        child = child->next;
    }
    
    fdt_print("}\n");
}

/**
 * Prints the contents of an fdt_tree_prop
 *
 * @param prop: the fdt_tree_prop to print
 * @param dt_strings: the strings block used to resolve the property name
 * @param level: the current nesting depth, used for indentation
 */
fn fdt_dump_tree_prop(prop: fdt_tree_prop*, dt_strings: char*, level: int32) {
    fdt_print("%s: \n", fdt_get_prop_name(prop->dt_prop, dt_strings));
}

/**
 * Parses an fdt_token::FDT_BEGIN_NODE
 *
 * @param token: the FDT_BEGIN_NODE token to skip past
 *
 * @return: next token to parse
 */
fn fdt_parse_begin_node(token: fdt_token*) -> fdt_token* {
    let node = token as fdt_node*;
    return (node->name as uint64 + aligned(strlen(node->name) + 1, 4)) as fdt_token*;
}

/**
 * Parses an fdt_token::FDT_END_NODE
 *
 * @param token: the FDT_END_NODE token to skip past
 *
 * @return: next token to parse
 */
fn fdt_parse_end_node(token: fdt_token*) -> fdt_token* {
    return &token[1];
}

/**
 * Parses an fdt_token::FDT_PROP
 *
 * @param token: the FDT_PROP token to skip past
 *
 * @return: next token to parse
 */
fn fdt_parse_prop(token: fdt_token*) -> fdt_token* {
    let prop = token as fdt_prop*;
    return (prop->value as uint64 + aligned(be32(prop->length), 4)) as fdt_token*;
}

/**
 * Parses an fdt_token::FDT_NOP
 *
 * @param token: the FDT_NOP token to skip past
 *
 * @return: next token to parse
 */
fn fdt_parse_nop(token: fdt_token*) -> fdt_token* {
    return &token[1];
}

/**
 * Returns the name of an fdt_node
 *
 * @param node: an fdt_node
 *
 * @return: a null-terminated string
 */
fn fdt_get_node_name(node: fdt_node*) -> char* {
    if (node == null or be32(node->token) != fdt_token::FDT_BEGIN_NODE)
        return null;
    
    return node->name;
}

/**
 * Returns the name of an fdt_prop
 *
 * @param prop: an fdt_prop
 * @param dt_strings: the strings block the property name is stored in
 *
 * @return: a null-terminated string
 */
fn fdt_get_prop_name(prop: fdt_prop*, dt_strings: char*) -> char* {
    if (prop == null or be32(prop->token) != fdt_token::FDT_PROP)
        return null;
    
    return &dt_strings[be32(prop->nameoff)];
}
