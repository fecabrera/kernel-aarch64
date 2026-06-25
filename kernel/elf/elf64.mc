import "debug";
import "cpu";
import "memory";

// E_IDENT

const EI_MAGIC: uint32 = 0x7F454C46; // [0x7F, 'E', 'F', 'I']

enum elf_ei_class: uint8 {
    ELFCLASSNONE = 0x00,
    ELFCLASS32 = 0x01,
    ELFCLASS64 = 0x02,
}

enum elf_ei_data: uint8 {
    ELFDATANONE = 0x00,
    ELFDATA2LSB = 0x01,
    ELFDATA2MSB = 0x02,
}

enum elf_ei_version: uint8 {
    NONE = 0x00,
    CURRENT = 0x01,
}

enum elf_ei_osabi: uint8 {
    SYSV = 0x00,
    HPUX = 0x01,
    NETBSD = 0x02,
    LINUX = 0x03,
    GNUHURD = 0x04,
    SOLARIS = 0x06,
    AIX = 0x07,
    IRIX = 0x08,
    FREEBSD = 0x09,
    TRU64 = 0x0A,
    NOVELLMODESTO = 0x0B,
    OPENBSD = 0x0C,
    OPENVMS = 0x0D,
    NONSTOPKERNEL = 0x0E,
    AROS = 0x0F,
    FENIXOS = 0x10,
    CLOUDABI = 0x11,
    OPENVOS = 0x12,
    ARM_EABI = 0x40,
    STANDALONE = 0xFF,
}

// structs
 
@align(16)
struct e_ident {
    ei_mag:        uint32;
    ei_class:      elf_ei_class;
    ei_data:       elf_ei_data;
    ei_version:    uint8;
    ei_osabi:      elf_ei_osabi;
    ei_abiversion: uint8;
}

// ELF64_EHDR

enum elf64_et: uint16 {
    NONE = 0x0000,
    REL = 0x0001,
    EXEC = 0x0002,
    DYN = 0x0003,
    CORE = 0x0004,
}

enum elf64_em: uint16 {
    EM_NONE = 0x0000,
    EM_M32 = 0x0001,
    EM_SPARC = 0x0002,
    EM_386 = 0x0003,
    EM_68K = 0x0004,
    EM_88K = 0x0005,
    EM_IAMCU = 0x0006,
    EM_860 = 0x0007,
    EM_MIPS = 0x0008,
    EM_S370 = 0x0009,
    EM_MIPS_RS4_BE = 0x000a,
    EM_PARISC = 0x000f,
    EM_VPP500 = 0x0011,
    EM_SPARC32PLUS = 0x0012,
    EM_960 = 0x0013,
    EM_PPC = 0x0014,
    EM_PPC64 = 0x0015,
    EM_S390 = 0x0016,
    EM_SPU = 0x0017,
    EM_V800 = 0x0024,
    EM_FR20 = 0x0025,
    EM_RH32 = 0x0026,
    EM_RCE = 0x0027,
    EM_ARM = 0x0028,
    EM_ALPHA = 0x0029,
    EM_SH = 0x002A,
    EM_SPARCV9 = 0x002B,
    EM_TRICORE = 0x002C,
    EM_ARC = 0x002D,
    EM_H8_300 = 0x002E,
    EM_H8_300H = 0x002F,
    EM_H8S = 0x0030,
    EM_H8_500 = 0x0031,
    EM_IA_64 = 0x0032,
    EM_MIPS_X = 0x0033,
    EM_COLDFIRE = 0x0034,
    EM_68HC12 = 0x0035,
    EM_MMA = 0x0036,
    EM_PCP = 0x0037,
    EM_NCPU = 0x0038,
    EM_NDR1 = 0x0039,
    EM_STARCORE = 0x003A,
    EM_ME16 = 0x003B,
    EM_ST100 = 0x003C,
    EM_TINYJ = 0x003D,
    EM_X86_64 = 0x003E,
    EM_PDSP = 0x003F,
    EM_PDP10 = 0x0040,
    EM_PDP11 = 0x0041,
    EM_FX66 = 0x0042,
    EM_ST9PLUS = 0x0043,
    EM_ST7 = 0x0044,
    EM_68HC16 = 0x0045,
    EM_68HC11 = 0x0046,
    EM_68HC08 = 0x0047,
    EM_68HC05 = 0x0048,
    EM_SVX = 0x0049,
    EM_ST19 = 0x004A,
    EM_VAX = 0x004B,
    EM_CRIS = 0x004C,
    EM_JAVELIN = 0x004D,
    EM_FIREPATH = 0x004E,
    EM_ZSP = 0x004F,
    EM_MMIX = 0x0050,
    EM_HUANY = 0x0051,
    EM_PRISM = 0x0052,
    EM_AVR = 0x0053,
    EM_FR30 = 0x0054,
    EM_D10V = 0x0055,
    EM_D30V = 0x0056,
    EM_V850 = 0x0057,
    EM_M32R = 0x0058,
    EM_MN10300 = 0x0059,
    EM_MN10200 = 0x005A,
    EM_PJ = 0x005B,
    EM_OPENRISC = 0x005C,
    EM_ARC_COMPACT = 0x005D,
    EM_XTENSA = 0x005E,
    EM_VIDEOCORE = 0x005F,
    EM_TMM_GPP = 0x0060,
    EM_NS32K = 0x0061,
    EM_TPC = 0x0062,
    EM_SNP1K = 0x0063,
    EM_ST200 = 0x0064,
    EM_IP2K = 0x0065,
    EM_MAX = 0x0066,
    EM_CR = 0x0067,
    EM_F2MC16 = 0x0068,
    EM_MSP430 = 0x0069,
    EM_BLACKFIN = 0x006A,
    EM_SE_C33 = 0x006B,
    EM_SEP = 0x006C,
    EM_ARCA = 0x006D,
    EM_UNICORE = 0x006E,
    EM_EXCESS = 0x006F,
    EM_DXP = 0x0070,
    EM_ALTERA_NIOS2 = 0x0071,
    EM_CRX = 0x0072,
    EM_XGATE = 0x0073,
    EM_C166 = 0x0074,
    EM_M16C = 0x0075,
    EM_DSPIC30F = 0x0076,
    EM_CE = 0x0077,
    EM_M32C = 0x0078,
    EM_TSK3000 = 0x0083,
    EM_RS08 = 0x0084,
    EM_SHARC = 0x0085,
    EM_ECOG2 = 0x0086,
    EM_SCORE7 = 0x0087,
    EM_DSP24 = 0x0088,
    EM_VIDEOCORE3 = 0x0089,
    EM_LATTICEMICO32 = 0x008A,
    EM_SE_C17 = 0x008B,
    EM_TI_C6000 = 0x008C,
    EM_TI_C2000 = 0x008D,
    EM_TI_C5500 = 0x008E,
    EM_TI_ARP32 = 0x008F,
    EM_TI_PRU = 0x0090,
    EM_MMDSP_PLUS = 0x00A0,
    EM_CYPRESS_M8C = 0x00A1,
    EM_R32C = 0x00A2,
    EM_TRIMEDIA = 0x00A3,
    EM_QDSP6 = 0x00A4,
    EM_8051 = 0x00A5,
    EM_STXP7X = 0x00A6,
    EM_NDS32 = 0x00A7,
    EM_ECOG1 = 0x00A8,
    EM_ECOG1X = 0x00A8,
    EM_MAXQ30 = 0x00A9,
    EM_XIMO16 = 0x00AA,
    EM_MANIK = 0x00AB,
    EM_CRAYNV2 = 0x00AC,
    EM_RX = 0x00AD,
    EM_METAG = 0x00AE,
    EM_MCST_ELBRUS = 0x00AF,
    EM_ECOG16 = 0x00B0,
    EM_CR16 = 0x00B1,
    EM_ETPU = 0x00B2,
    EM_SLE9X = 0x00B3,
    EM_L10M = 0x00B4,
    EM_K10M = 0x00B5,
    EM_AARCH64 = 0x00B7,
    EM_AVR32 = 0x00B9,
    EM_STM8 = 0x00BA,
    EM_TILE64 = 0x00BB,
    EM_TILEPRO = 0x00BC,
    EM_MICROBLAZE = 0x00BD,
    EM_CUDA = 0x00BE,
    EM_TILEGX = 0x00BF,
    EM_CLOUDSHIELD = 0x00C0,
    EM_COREA_1ST = 0x00C1,
    EM_COREA_2ND = 0x00C2,
    EM_ARC_COMPACT2 = 0x00C3,
    EM_OPEN8 = 0x00C4,
    EM_RL78 = 0x00C5,
    EM_VIDEOCORE5 = 0x00C6,
    EM_78KOR = 0x00C7,
    EM_56800EX = 0x00C8,
    EM_BA1 = 0x00C9,
    EM_BA2 = 0x00CA,
    EM_XCORE = 0x00CB,
    EM_MCHP_PIC = 0x00CC,
    EM_INTEL205 = 0x00CD,
    EM_INTEL206 = 0x00CE,
    EM_INTEL207 = 0x00CF,
    EM_INTEL208 = 0x00D0,
    EM_INTEL209 = 0x00D1,
    EM_KM32 = 0x00D2,
    EM_KMX32 = 0x00D3,
    EM_KMX16 = 0x00D4,
    EM_KMX8 = 0x00D5,
    EM_KVARC = 0x00D6,
    EM_CDP = 0x00D7,
    EM_COGE = 0x00D8,
    EM_COOL = 0x00D9,
    EM_NORC = 0x00DA,
    EM_CSR_KALIMBA = 0x00DB,
    EM_Z80 = 0x00DC,
    EM_VISIUM = 0x00DD,
    EM_FT32 = 0x00DE,
    EM_MOXIE = 0x00DF,
    EM_AMDGPU = 0x00E0,
    EM_RISCV = 0x00F3,
}

struct elf64_ehdr {
    e_type:      elf64_et;
    e_machine:   elf64_em;
    e_version:   uint32;
    e_entry:     uint64;
    e_phoff:     uint64;
    e_shoff:     uint64;
    e_flags:     uint32;
    e_ehsize:    uint16;
    e_phentsize: uint16;
    e_phnum:     uint16;
    e_shentsize: uint16;
    e_shnum:     uint16;
    e_shstrndx:  uint16;
}

// PHDR

enum elf64_pt: uint32 {
    NULL = 0x00,
    LOAD = 0x01,
    DYNAMIC = 0x02,
    INTERP = 0x03,
    NOTE = 0x04,
    SHLIB = 0x05,
    PHDR = 0x06,
    TLS = 0x07,
    LOOS = 0x60000000,
    HIOS = 0x6FFFFFFF,
    GNU_EH_FRAME = elf64_pt::LOOS + 0x474E550,
    GNU_STACK = elf64_pt::LOOS + 0x474E551,
    GNU_RELRO = elf64_pt::LOOS + 0x474E552,
    GNU_PROPERTY = elf64_pt::LOOS + 0x474E553,
    SUNWBSS = 0x6FFFFFFA,
    SUNWSTACK = 0x6FFFFFFB,
    ARM_ARCHEXT = 0x70000000,
    ARM_UNWIND = 0x70000001,
}

struct elf64_phdr {
    p_type:   elf64_pt;
    p_flags:  uint32;
    p_offset: uint64;
    p_vaddr:  uint64;
    p_paddr:  uint64;
    p_filesz: uint64;
    p_memsz:  uint64;
    p_align:  uint64;
}

// ELF64_SHDR

enum elf64_sht: uint32 {
    NULL = 0x00,
    PROGBITS = 0x01,
    SYMTAB = 0x02,
    STRTAB = 0x03,
    RELA = 0x04,
    HASH = 0x05,
    DYNAMIC = 0x06,
    NOTE = 0x07,
    NOBITS = 0x08,
    REL = 0x09,
    SHLIB = 0x0A,
    DYNSYM = 0x0B,
    UNKNOWN12 = 0x0C,
    UNKNOWN13 = 0x0D,
    INIT_ARRAY = 0x0E,
    FINI_ARRAY = 0x0F,
    PREINIT_ARRAY = 0x10,
    GROUP = 0x11,
    SYMTAB_SHNDX = 0x12,
    GNU_INCREMENTAL_INPUTS = 0x6FFF4700,
    GNU_ATTRIBUTES = 0x6FFFFFF5,
    GNU_HASH = 0x6FFFFFF6,
    GNU_LIBLIST = 0x6FFFFFF7,
    CHECKSUM = 0x6FFFFFF8,
    SUNW_move = 0x6FFFFFFA,
    SUNW_COMDAT = 0x6FFFFFFB,
    SUNW_syminfo = 0x6FFFFFFC,
    GNU_verdef = 0x6FFFFFFD,
    GNU_verneed = 0x6FFFFFFE,
    GNU_versym = 0x6FFFFFFF,
    ARM_EXIDX = 0x70000001,
    ARM_PREEMPTMAP = 0x70000002,
    ARM_ATTRIBUTES = 0x70000003,
    ARM_DEBUGOVERLAY = 0x70000004,
    ARM_OVERLAYSECTION = 0x70000005,
}

enum elf64_shf: uint32 {
    WRITE            = (1 << 0),
    ALLOC            = (1 << 1),
    EXECINSTR        = (1 << 2),
    MERGE            = (1 << 4),
    STRINGS          = (1 << 5),
    INFO_LINK        = (1 << 6),
    LINK_ORDER       = (1 << 7),
    OS_NONCONFORMING = (1 << 8),
    GROUP            = (1 << 9),
    TLS              = (1 << 10),
    COMPRESSED       = (1 << 11),
}

struct elf64_shdr {
    sh_name:      uint32;
    sh_type:      elf64_sht;
    sh_flags:     elf64_shf;
    padding:      uint32;
    sh_addr:      uint64;
    sh_offset:    uint64;
    sh_size:      uint64;
    sh_link:      uint32;
    sh_info:      uint32;
    sh_addralign: uint64;
    sh_entsize:   uint64;    
}

// ELF64_SYM
 
enum elf64_shn: uint16 {
    UNDEF  = 0,
    LOPROC = 0xFF00,
    HIPROC = 0xFF1F,
    LOOS   = 0xFF20,
    HIOS   = 0xFF3F,
    ABS    = 0xFFF1,
    COMMON = 0xFFF2,
    XINDEX = 0xFFFF,
}

enum elf64_stv: uint8 {
    DEFAULT   = 0x00,
    INTERNAL  = 0x01,
    HIDDEN    = 0x02,
    PROTECTED = 0x03,
}

struct elf64_sym {
    st_name:  uint32;
    st_info:  uint8;
    st_other: elf64_stv;
    st_shndx: uint16;
    st_value: uint64;
    st_size:  uint64;
}

struct elf64_file {
    buf:       uint8*;
    ei:        struct e_ident*;
    ehdr:      struct elf64_ehdr*;
    phdr:      struct elf64_phdr*;
    phnum:     uint16;
    shdr:      struct elf64_shdr*;
    shnum:     uint16;
    symtab:    struct elf64_sym*;
    stnum:     uint64;
    strtab:    uint8*;
    shstrtab:  uint8*;
    relocated: bool;
    relerrs:   uint16;
    memsz:     uint64;
    got:       uint64*;   // one slot per symbol, filled with resolved addresses
}

// AArch64 static relocation types (ELF for the Arm 64-bit Architecture).
const R_AARCH64_ABS64: uint32 = 257;
const R_AARCH64_ADR_PREL_PG_HI21: uint32 = 275;
const R_AARCH64_ADD_ABS_LO12_NC: uint32 = 277;
const R_AARCH64_LDST8_ABS_LO12_NC: uint32 = 278;
const R_AARCH64_LDST16_ABS_LO12_NC: uint32 = 284;
const R_AARCH64_LDST32_ABS_LO12_NC: uint32 = 285;
const R_AARCH64_LDST64_ABS_LO12_NC: uint32 = 286;
const R_AARCH64_LDST128_ABS_LO12_NC: uint32 = 299;
const R_AARCH64_ADR_GOT_PAGE: uint32 = 311;
const R_AARCH64_LD64_GOT_LO12_NC: uint32 = 312;
const R_AARCH64_PREL32: uint32 = 261;
const R_AARCH64_PREL64: uint32 = 260;
const R_AARCH64_JUMP26: uint32 = 282;
const R_AARCH64_CALL26: uint32 = 283;

struct elf64_rela {
    r_offset: uint64;   // offset of the patch site within its section
    r_info:   uint64;   // (symbol index << 32) | relocation type
    r_addend: int64;    // constant addend
}

@private
fn elf64_shdr_get_data(buf: uint8*, shdr: struct elf64_shdr*) -> uint8* {
    return &buf[shdr->sh_offset];
}

@private
fn elf64_get_shstrtab(buf: uint8*, shdr: struct elf64_shdr*, i: uint16) -> uint8* {
    return elf64_shdr_get_data(buf, &shdr[i]);
}

@private
fn elf64_get_section_name(shdr: struct elf64_shdr*, shstrtab: uint8*) -> uint8* {
    if (shstrtab == null) return null;
    return &shstrtab[shdr->sh_name];
}

@private
fn elf64_get_symbol_name(sym: struct elf64_sym*, strtab: uint8*) -> uint8* {
    if (strtab == null) return null;
    return &strtab[sym->st_name];
}

fn is_elf(buf: uint8*) -> bool {
    return *(buf as uint32*) == be32(EI_MAGIC);
}

fn get_elf_type(file: struct elf64_file*) -> elf64_et {
    return file->ehdr->e_type;
}

fn elf_read(buf: uint8*, size: uint64, file: struct elf64_file*) -> int32 {
    file->buf = buf;
    file->memsz = 0;
    file->relocated = false;
    
    // get ptrs to section headers
    file->ei = buf as struct e_ident*;
    file->ehdr = &buf[sizeof(struct e_ident)] as struct elf64_ehdr*;

    // load program headers
    file->phdr = &buf[file->ehdr->e_phoff] as struct elf64_phdr*;
    file->phnum = file->ehdr->e_phnum;

    // load sections headers
    file->shdr = &buf[file->ehdr->e_shoff] as struct elf64_shdr*;
    file->shnum = file->ehdr->e_shnum;
    
    // load string table
    file->shstrtab = elf64_get_shstrtab(buf, file->shdr, file->ehdr->e_shstrndx);
    
    let i: uint16 = 0;
    while (i < file->ehdr->e_shnum) {
        let el = &file->shdr[i];
        case (el->sh_type) {
        when elf64_sht::SYMTAB:
            file->stnum = el->sh_size / el->sh_entsize;
            file->symtab = elf64_shdr_get_data(buf, el) as struct elf64_sym*;
            file->strtab = elf64_shdr_get_data(buf, &file->shdr[el->sh_link]);
        }
        i = i + 1;
    }
    
    // align sections and calculate bytes to allocate
    let status = elf64_calculate_and_pad(file);
    if (status < 0) {
        dprintk("[elf] calculate_and_pad() returned %d!\n", status);
        return -1;
    }

    dprintk("[elf] memsz: %u B\n", file->memsz);

    return 0;
}

fn elf64_load(file: struct elf64_file*, address: uint64) -> int32 {
    if (file->relocated) {
        dprintk("[elf] already relocated\n");
        return -1;
    }

    // rebase symbols
    let rebased: int32 = {
        let total: int32 = 0;

        let i: uint64 = 1;
        while (i < file->stnum) {
            if (elf64_try_rebase_symbol(&file->symtab[i], address, file->strtab, file->shdr))
                total = total + 1;
            i = i + 1;
        }

        emit total;
    };
    
    dprintk("[elf] rebased %d symbols!\n", rebased);

    // build a GOT: one slot per symbol holding its resolved (rebased) address
    file->got = alloc<uint64>(file->stnum);
    {
        let g: uint64 = 0;
        while (g < file->stnum) {
            file->got[g] = file->symtab[g].st_value;
            g = g + 1;
        }
    }

    // copy entries to the allocated memory
    let i: uint16 = 0;
    while (i < file->shnum) {
        elf64_try_relocate_section(&file->shdr[i], file->buf, address);
        i = i + 1;
    }

    // patch references now that every section and symbol has a runtime address
    elf64_apply_relocations(file, address);

    // mark it as relocated
    file->relocated = true;

    return rebased;
}

fn elf64_unload(file: struct elf64_file*) {
    dealloc(file->got);
}

fn elf64_locate_symbol(file: struct elf64_file*, const sym_name: uint8*) -> uint8* {
    let i: uint64 = 1;
    while (i < file->stnum) {
        let sym = &file->symtab[i];
        let name = elf64_get_symbol_name(sym, file->strtab);

        if (name != null and strcmp(name, sym_name) == 0) {
            dprintk("[elf] found symbol \"%s\" at 0x%p\n", sym_name, sym->st_value);
            return sym->st_value as uint8*;
        }
        
        i = i + 1;
    }
    return null;
}

@private
fn elf64_calculate_and_pad(file: struct elf64_file*) -> int32 {
    let et = get_elf_type(file);

    case (et) {
    when elf64_et::EXEC, elf64_et::DYN:
        // pass
    when elf64_et::REL:
        if (file->relocated) {
            dprintk("[elf] needs to be called before relocation\n");
            return -1;
        }
        
        let total: uint64 = 0;
        let i: uint16 = 0;
        while (i < file->shnum) {
            defer i = i + 1;

            let shdr = &file->shdr[i];

            if ((shdr->sh_flags & elf64_shf::ALLOC) == 0)
                continue;

            let align: uint64 = shdr->sh_addralign > 0 ? shdr->sh_addralign : 1;
            let padding = align - (total % align);

            total = total + padding;
            shdr->sh_addr = total;
            total = total + shdr->sh_size; 
        }
        file->memsz = total;
    else:
        dprintk("[elf] got invalid e_ident->type %d!\n", et);
        return -2;
    }

    return 0;
}

@private
fn elf64_try_rebase_symbol(sym: struct elf64_sym*, base_addr: uint64, strtab: uint8*,
                           shdr: struct elf64_shdr*) -> bool {
    let name = elf64_get_symbol_name(sym, strtab);

    dprintk("[elf] symbol");
    if (name != null) dprintk(" (%s)", name);
    dprintk(": ");
    
    case (sym->st_shndx) {
    when elf64_shn::UNDEF:
        dprintk("st_shndx=UNDEF, ask kernel\n", sym->st_value);
    when elf64_shn::ABS:
        dprintk("st_shndx=ABS, won't relocate\n", sym->st_value);
    when elf64_shn::COMMON:
        dprintk("st_shndx=COMMON, allocate space\n", sym->st_value);
    when elf64_shn::XINDEX:
        dprintk("st_shndx=XINDEX, fetch index from symtab\n");
    else:
        dprintk("st_shndx=%d, %p -> %p\n", sym->st_shndx, sym->st_value, base_addr + sym->st_value);
        sym->st_value = base_addr + shdr[sym->st_shndx].sh_addr + sym->st_value;

        return true;
    }

    return false;
}

@private
fn elf64_try_relocate_section(shdr: struct elf64_shdr*, buf: uint8*, base_addr: uint64) -> bool {
    if ((shdr->sh_flags & elf64_shf::ALLOC) == 0) return false;

    let dst = base_addr as uint8*;
    if (shdr->sh_type == elf64_sht::NOBITS) {
        set_bytes(&dst[shdr->sh_addr], 0, shdr->sh_size);
    } else {
        bytecopy(&dst[shdr->sh_addr], &buf[shdr->sh_offset], shdr->sh_size);
    }

    return true;
}

@private
fn elf64_page(addr: uint64) -> uint64 {
    return addr & ~(0xFFF as uint64);
}

/**
 * Patches the 12-bit immediate (bits [21:10]) of the ADD/LDST instruction at P.
 * The relocation value is masked to its low 12 bits, then scaled down by the
 * access-size log2 `shift` (0 for ADD and byte loads, 1/2/3/4 for 16/32/64/128-
 * bit LDST).
 */
@private
fn elf64_patch_lo12(P: uint64, value: uint64, shift: uint32) {
    let imm12 = ((value & 0xFFF) >> shift) as uint32;
    let insn = *(P as uint32*);
    insn = (insn & ~((0xFFF as uint32) << 10)) | (imm12 << 10);
    *(P as uint32*) = insn;
}

/**
 * Patches the 21-bit ADRP immediate (immlo at bits [30:29], immhi at bits
 * [23:5]) of the instruction at P with the page offset `imm`.
 */
@private
fn elf64_patch_adrp(P: uint64, imm: uint64) {
    let insn = *(P as uint32*);
    insn = insn & ~(((0x3 as uint32) << 29) | ((0x7FFFF as uint32) << 5));
    insn = insn | (((imm & 0x3) as uint32) << 29) | ((((imm >> 2) & 0x7FFFF) as uint32) << 5);
    *(P as uint32*) = insn;
}

/**
 * Applies one RELA entry. S is the relocated (absolute) symbol value, A the
 * addend, P the runtime address of the patch site. Handles the ABS64 / ADRP /
 * ADD / LDST*_LO12 subset a statically-linked module needs; other types (CALL26,
 * GOT, ...) are logged and skipped.
 */
@private
fn elf64_apply_one_relocation(rela: struct elf64_rela*, symtab: struct elf64_sym*,
                              strtab: uint8*, got: uint64*, site_base: uint64) -> bool {
    let type = (rela->r_info & 0xFFFFFFFF) as uint32;
    let sym_idx = rela->r_info >> 32;
    let sym = &symtab[sym_idx];

    // A non-null symbol still marked UNDEF was never resolved (no kernel export
    // table yet); applying the relocation against st_value=0 would silently
    // produce a wild branch/pointer, so reject the load instead.
    if (sym_idx != 0 and sym->st_shndx == elf64_shn::UNDEF) {
        printk("[elf] unresolved symbol \"%s\" at %p\n",
               elf64_get_symbol_name(sym, strtab), site_base + rela->r_offset);
        return false;
    }

    let S = sym->st_value;              // already rebased to a runtime address
    let A = rela->r_addend as uint64;
    let P = site_base + rela->r_offset;

    case (type) {
    when R_AARCH64_ABS64:
        *(P as uint64*) = S + A;
    when R_AARCH64_ADR_PREL_PG_HI21:
        elf64_patch_adrp(P, (elf64_page(S + A) - elf64_page(P)) >> 12);
    when R_AARCH64_ADD_ABS_LO12_NC, R_AARCH64_LDST8_ABS_LO12_NC:
        elf64_patch_lo12(P, S + A, 0);
    when R_AARCH64_LDST16_ABS_LO12_NC:
        elf64_patch_lo12(P, S + A, 1);
    when R_AARCH64_LDST32_ABS_LO12_NC:
        elf64_patch_lo12(P, S + A, 2);
    when R_AARCH64_LDST64_ABS_LO12_NC:
        elf64_patch_lo12(P, S + A, 3);
    when R_AARCH64_LDST128_ABS_LO12_NC:
        elf64_patch_lo12(P, S + A, 4);
    when R_AARCH64_ADR_GOT_PAGE:
        let slot = &got[sym_idx] as uint64;
        elf64_patch_adrp(P, (elf64_page(slot) - elf64_page(P)) >> 12);
    when R_AARCH64_LD64_GOT_LO12_NC:
        let slot = &got[sym_idx] as uint64;
        elf64_patch_lo12(P, slot, 3);
    when R_AARCH64_CALL26, R_AARCH64_JUMP26:
        let imm26 = (((S + A - P) >> 2) & 0x3FFFFFF) as uint32;
        let insn = *(P as uint32*);
        insn = (insn & ~(0x3FFFFFF as uint32)) | imm26;
        *(P as uint32*) = insn;
    when R_AARCH64_PREL32:
        *(P as uint32*) = (S + A - P) as uint32;
    when R_AARCH64_PREL64:
        *(P as uint64*) = S + A - P;
    else:
        printk("[elf] unhandled relocation type %u at %p\n", type, P);
        return false;
    }

    return true;
}

/**
 * Walks every SHT_RELA section and applies its entries to the (already copied)
 * target section it patches. Must run after sections are copied and symbols
 * rebased, so both S (symbol) and P (patch site) have runtime addresses.
 */
@private
fn elf64_apply_relocations(file: struct elf64_file*, base_addr: uint64) {
    file->relerrs = 0;

    let i: uint16 = 0;
    while (i < file->shnum) {
        defer i = i + 1;

        let rela_shdr = &file->shdr[i];
        if (rela_shdr->sh_type != elf64_sht::RELA)
            continue;

        // the section these relocations patch
        let target = &file->shdr[rela_shdr->sh_info];
        if ((target->sh_flags & elf64_shf::ALLOC) == 0)
            continue;

        let entries = elf64_shdr_get_data(file->buf, rela_shdr) as struct elf64_rela*;
        let count = rela_shdr->sh_size / sizeof(struct elf64_rela);
        let site_base = base_addr + target->sh_addr;

        let j: uint64 = 0;
        while (j < count) {
            defer j = j + 1;
            if (!elf64_apply_one_relocation(&entries[j], file->symtab, file->strtab, file->got, site_base))
                file->relerrs = file->relerrs + 1;
        }
    }
}

fn elf64_dump(file: struct elf64_file*) {
    elf64_dump_ehdr(file->ehdr);
    elf64_dump_ei(file->ei);
    elf64_dump_phdr(file->phdr, file->phnum);
    elf64_dump_shdr(file->shdr, file->shnum, file->shstrtab);
    elf64_dump_symtab(file->symtab, file->stnum, file->strtab);
}

@private
fn elf64_dump_ei(ei: struct e_ident*) {
    dprintk("e_ident:");
    dprintk("  ei_mag        = 0x%08X", ei->ei_mag);
    dprintk("  ei_class      = 0x%02X", ei->ei_class);
    dprintk("  ei_data       = 0x%02X", ei->ei_data);
    dprintk("  ei_version    = 0x%02X", ei->ei_version);
    dprintk("  ei_osabi      = 0x%02X", ei->ei_osabi);
    dprintk("  ei_abiversion = 0x%02X", ei->ei_abiversion);
    dprintk("");
}

@private
fn elf64_dump_ehdr(ehdr: struct elf64_ehdr*) {
    dprintk("elf64_ehdr:\n");
    dprintk("  e_type        = 0x%04X\n", ehdr->e_type);
    dprintk("  e_machine     = 0x%04X\n", ehdr->e_machine);
    dprintk("  e_version     = 0x%08X\n", ehdr->e_version);
    dprintk("  e_entry       = 0x%016X\n", ehdr->e_entry);
    dprintk("  e_phoff       = 0x%016X\n", ehdr->e_phoff);
    dprintk("  e_shoff       = 0x%016X\n", ehdr->e_shoff);
    dprintk("  e_flags       = 0x%08X\n", ehdr->e_flags);
    dprintk("  e_ehsize      = 0x%04X\n", ehdr->e_ehsize);
    dprintk("  e_phentsize   = 0x%04X\n", ehdr->e_phentsize);
    dprintk("  e_phnum       = 0x%04X\n", ehdr->e_phnum);
    dprintk("  e_shentsize   = 0x%04X\n", ehdr->e_shentsize);
    dprintk("  e_shnum       = 0x%04X\n", ehdr->e_shnum);
    dprintk("  e_shstrndxe   = 0x%04X\n", ehdr->e_shstrndx);
    dprintk("\n");
}

@private
fn elf64_dump_phdr(phdr: struct elf64_phdr*, phnum: uint16) {
    dprintk("program headers:\n");
    dprintk("%-8s %-8s %-8s %-16s %-16s %-8s %-8s %5s\n",
            "type", "flags", "offset", "vaddr", "paddr", "fsize",
            "msize", "align");

    let i: uint16 = 0;
    while (i < phnum) {
        elf64_dump_phdr_entry(&phdr[i]);
        i = i + 1;
    }
    dprintk("\n");
}

@private
fn elf64_dump_phdr_entry(phdr: struct elf64_phdr*) {
    dprintk("%08X %08X %8d %p %p %8d %8d %5d\n",
            phdr->p_type, phdr->p_flags, phdr->p_offset, phdr->p_vaddr,
            phdr->p_paddr, phdr->p_filesz, phdr->p_memsz, phdr->p_align);
}

@private
fn elf64_dump_shdr(shdr: struct elf64_shdr*, shnum: uint16, shstrtab: uint8*) {
    dprintk("section headers:\n");
    dprintk("%-30s %-8s %-8s %-16s %-16s %-4s %-8s %-8s %-5s %-4s\n",
            "name", "type", "flags", "addr", "offset", "size",
            "link", "info", "align", "size");

    let i: uint16 = 0;
    while (i < shnum) {
        elf64_dump_shdr_entry(&shdr[i], shstrtab);
        i = i + 1;
    }
    dprintk("\n");
}

@private
fn elf64_dump_shdr_entry(shdr: struct elf64_shdr*, shstrtab: uint8*) {
    let name = elf64_get_section_name(shdr, shstrtab);
    dprintk("%-30s %08X %08X %p %p %4llu %08X %08X %5llu %4llu\n",
            name == null ? "" : name, shdr->sh_type, shdr->sh_flags,
            shdr->sh_addr, shdr->sh_offset, shdr->sh_size, shdr->sh_link,
            shdr->sh_info, shdr->sh_addralign, shdr->sh_entsize);
}

@private
fn elf64_dump_symtab(symtab: struct elf64_sym*, symtab_num: uint64, strtab: uint8*) {
    dprintk("%-30s %-2s %-2s %-4s %-16s %-16s\n",
            "name", "i", "o", "idx", "value", "size");

    let i: uint64 = 0;
    while (i < symtab_num) {
        elf64_dump_symtab_entry(&symtab[i], strtab);
        i = i + 1;
    }
    dprintk("\n");
}

@private
fn elf64_dump_symtab_entry(sym: struct elf64_sym*, strtab: uint8*) {
    let name = elf64_get_symbol_name(sym, strtab);
    dprintk("%-30s %02X %02X %04X %016X %016X\n",
            name == null ? "" : name, sym->st_info, sym->st_other,
            sym->st_shndx, sym->st_value, sym->st_size);
}
