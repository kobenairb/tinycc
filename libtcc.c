/*
 *  TCC - Tiny C Compiler
 *
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tcc.h"

/********************************************************/
/* global variables */

/* use GNU C extensions */
ST_DATA int gnu_ext = 1;

/* use TinyCC extensions */
ST_DATA int tcc_ext = 1;

/* XXX: get rid of this ASAP */
ST_DATA struct TCCState *tcc_state;

#ifdef CONFIG_TCC_BACKTRACE
ST_DATA int num_callers = 6;
ST_DATA const char **rt_bound_error_msg;
ST_DATA void *rt_prog_main;
#endif

/********************************************************/

#ifndef NOTALLINONE
#include "tccpp.c"
#include "tccgen.c"
#include "tccelf.c"
#include "tccrun.c"
#ifdef TCC_TARGET_I386
#include "i386-gen.c"
#endif
#ifdef TCC_TARGET_ARM
#include "arm-gen.c"
#endif
#ifdef TCC_TARGET_C67
#include "c67-gen.c"
#endif
#ifdef TCC_TARGET_X86_64
#include "x86_64-gen.c"
#endif
#ifdef CONFIG_TCC_ASM
#include "tccasm.c"
#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64
#include "i386-asm.c"
#endif
#endif
#ifdef TCC_TARGET_COFF
#include "tcccoff.c"
#endif
#ifdef TCC_TARGET_PE
#include "tccpe.c"
#endif
#endif /* ALL_IN_ONE */

/********************************************************/
#ifndef CONFIG_TCC_ASM
ST_FUNC void asm_instr(void)
{
    error("inline asm() not supported");
}
ST_FUNC void asm_global_instr(void)
{
    error("inline asm() not supported");
}
#endif

/********************************************************/

#ifdef _WIN32
static char *normalize_slashes(char *path)
{
    char *p;
    for (p = path; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return path;
}

static HMODULE tcc_module;

/* on win32, we suppose the lib and includes are at the location of 'tcc.exe' */
static void tcc_set_lib_path_w32(TCCState *s)
{
    char path[1024], *p;
    GetModuleFileNameA(tcc_module, path, sizeof path);
    p = tcc_basename(normalize_slashes(strlwr(path)));
    if (p - 5 > path && 0 == strncmp(p - 5, "/bin/", 5))
        p -= 5;
    else if (p > path)
        p--;
    *p = 0;
    tcc_set_lib_path(s, path);
}

#ifndef CONFIG_TCC_STATIC
void dlclose(void *p)
{
    FreeLibrary((HMODULE) p);
}
#endif

#ifdef LIBTCC_AS_DLL
BOOL WINAPI DllMain(HANDLE hDll, DWORD dwReason, LPVOID lpReserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
        tcc_module = hDll;
    return TRUE;
}
#endif
#endif

/********************************************************/
/* copy a string and truncate it. */
PUB_FUNC char *pstrcpy(char *buf, int buf_size, const char *s)
{
    char *q, *q_end;
    int c;

    if (buf_size > 0) {
        q = buf;
        q_end = buf + buf_size - 1;
        while (q < q_end) {
            c = *s++;
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    }
    return buf;
}

/* strcat and truncate. */
PUB_FUNC char *pstrcat(char *buf, int buf_size, const char *s)
{
    int len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

/* extract the basename of a file */
PUB_FUNC char *tcc_basename(const char *name)
{
    char *p = strchr(name, 0);
    while (p > name && !IS_PATHSEP(p[-1]))
        --p;
    return p;
}

PUB_FUNC char *tcc_fileextension(const char *name)
{
    char *b = tcc_basename(name);
    char *e = strrchr(b, '.');
    return e ? e : strchr(b, 0);
}

/********************************************************/
/* memory management */

#undef free
#undef malloc
#undef realloc

#ifdef MEM_DEBUG
int mem_cur_size;
int mem_max_size;
unsigned malloc_usable_size(void *);
#endif

PUB_FUNC void tcc_free(void *ptr)
{
#ifdef MEM_DEBUG
    mem_cur_size -= malloc_usable_size(ptr);
#endif
    free(ptr);
}

PUB_FUNC void *tcc_malloc(unsigned long size)
{
    void *ptr;
    ptr = malloc(size);
    if (!ptr && size)
        error("memory full");
#ifdef MEM_DEBUG
    mem_cur_size += malloc_usable_size(ptr);
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
#endif
    return ptr;
}

PUB_FUNC void *tcc_mallocz(unsigned long size)
{
    void *ptr;
    ptr = tcc_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *tcc_realloc(void *ptr, unsigned long size)
{
    void *ptr1;
#ifdef MEM_DEBUG
    mem_cur_size -= malloc_usable_size(ptr);
#endif
    ptr1 = realloc(ptr, size);
#ifdef MEM_DEBUG
    /* NOTE: count not correct if alloc error, but not critical */
    mem_cur_size += malloc_usable_size(ptr1);
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
#endif
    return ptr1;
}

PUB_FUNC char *tcc_strdup(const char *str)
{
    char *ptr;
    ptr = tcc_malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

PUB_FUNC void tcc_memstats(void)
{
#ifdef MEM_DEBUG
    printf("memory in use: %d\n", mem_cur_size);
#endif
}

#define free(p) use_tcc_free(p)
#define malloc(s) use_tcc_malloc(s)
#define realloc(p, s) use_tcc_realloc(p, s)

/********************************************************/
/* dynarrays */

PUB_FUNC void dynarray_add(void ***ptab, int *nb_ptr, void *data)
{
    int nb, nb_alloc;
    void **pp;

    nb = *nb_ptr;
    pp = *ptab;
    /* every power of two we double array size */
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = tcc_realloc(pp, nb_alloc * sizeof(void *));
        if (!pp)
            error("memory full");
        *ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

PUB_FUNC void dynarray_reset(void *pp, int *n)
{
    void **p;
    for (p = *(void ***) pp; *n; ++p, --*n)
        if (*p)
            tcc_free(*p);
    tcc_free(*(void **) pp);
    *(void **) pp = NULL;
}

/* we use our own 'finite' function to avoid potential problems with
   non standard math libs */
/* XXX: endianness dependent */
ST_FUNC int ieee_finite(double d)
{
    int *p = (int *) &d;
    return ((unsigned) ((p[1] | 0x800fffff) + 1)) >> 31;
}

/********************************************************/

ST_FUNC Section *new_section(TCCState *s1, const char *name, int sh_type, int sh_flags)
{
    Section *sec;

    sec = tcc_mallocz(sizeof(Section) + strlen(name));
    strcpy(sec->name, name);
    sec->sh_type = sh_type;
    sec->sh_flags = sh_flags;
    switch (sh_type) {
    case SHT_HASH:
    case SHT_REL:
    case SHT_RELA:
    case SHT_DYNSYM:
    case SHT_SYMTAB:
    case SHT_DYNAMIC:
        sec->sh_addralign = 4;
        break;
    case SHT_STRTAB:
        sec->sh_addralign = 1;
        break;
    default:
        sec->sh_addralign = 32; /* default conservative alignment */
        break;
    }

    if (sh_flags & SHF_PRIVATE) {
        dynarray_add((void ***) &s1->priv_sections, &s1->nb_priv_sections, sec);
    } else {
        sec->sh_num = s1->nb_sections;
        dynarray_add((void ***) &s1->sections, &s1->nb_sections, sec);
    }

    return sec;
}

static void free_section(Section *s)
{
    tcc_free(s->data);
}

/* realloc section and set its content to zero */
ST_FUNC void section_realloc(Section *sec, unsigned long new_size)
{
    unsigned long size;
    unsigned char *data;

    size = sec->data_allocated;
    if (size == 0)
        size = 1;
    while (size < new_size)
        size = size * 2;
    data = tcc_realloc(sec->data, size);
    if (!data)
        error("memory full");
    memset(data + sec->data_allocated, 0, size - sec->data_allocated);
    sec->data = data;
    sec->data_allocated = size;
}

/* reserve at least 'size' bytes in section 'sec' from
   sec->data_offset. */
ST_FUNC void *section_ptr_add(Section *sec, unsigned long size)
{
    unsigned long offset, offset1;

    offset = sec->data_offset;
    offset1 = offset + size;
    if (offset1 > sec->data_allocated)
        section_realloc(sec, offset1);
    sec->data_offset = offset1;
    return sec->data + offset;
}

/* reserve at least 'size' bytes from section start */
ST_FUNC void section_reserve(Section *sec, unsigned long size)
{
    if (size > sec->data_allocated)
        section_realloc(sec, size);
    if (size > sec->data_offset)
        sec->data_offset = size;
}

/* return a reference to a section, and create it if it does not
   exists */
ST_FUNC Section *find_section(TCCState *s1, const char *name)
{
    Section *sec;
    int i;
    for (i = 1; i < s1->nb_sections; i++) {
        sec = s1->sections[i];
        if (!strcmp(name, sec->name))
            return sec;
    }
    /* sections are created as PROGBITS */
    return new_section(s1, name, SHT_PROGBITS, SHF_ALLOC);
}

/* update sym->c so that it points to an external symbol in section
   'section' with value 'value' */
ST_FUNC void put_extern_sym2(
    Sym *sym, Section *section, unsigned long value, unsigned long size, int can_add_underscore)
{
    int sym_type, sym_bind, sh_num, info, other;
    ElfW(Sym) * esym;
    const char *name;
    char buf1[256];

    if (section == NULL)
        sh_num = SHN_UNDEF;
    else if (section == SECTION_ABS)
        sh_num = SHN_ABS;
    else
        sh_num = section->sh_num;

    if ((sym->type.t & VT_BTYPE) == VT_FUNC) {
        sym_type = STT_FUNC;
    } else if ((sym->type.t & VT_BTYPE) == VT_VOID) {
        sym_type = STT_NOTYPE;
    } else {
        sym_type = STT_OBJECT;
    }

    if (sym->type.t & VT_STATIC)
        sym_bind = STB_LOCAL;
    else
        sym_bind = STB_GLOBAL;

    if (!sym->c) {
        name = get_tok_str(sym->v, NULL);
#ifdef CONFIG_TCC_BCHECK
        if (tcc_state->do_bounds_check) {
            char buf[32];

            /* XXX: avoid doing that for statics ? */
            /* if bound checking is activated, we change some function
               names by adding the "__bound" prefix */
            switch (sym->v) {
#ifdef TCC_TARGET_PE
            /* XXX: we rely only on malloc hooks */
            case TOK_malloc:
            case TOK_free:
            case TOK_realloc:
            case TOK_memalign:
            case TOK_calloc:
#endif
            case TOK_memcpy:
            case TOK_memmove:
            case TOK_memset:
            case TOK_strlen:
            case TOK_strcpy:
            case TOK_alloca:
                strcpy(buf, "__bound_");
                strcat(buf, name);
                name = buf;
                break;
            }
        }
#endif
        other = 0;

#ifdef TCC_TARGET_PE
        if (sym->type.t & VT_EXPORT)
            other |= 1;
        if (sym_type == STT_FUNC && sym->type.ref) {
            int attr = sym->type.ref->r;
            if (FUNC_EXPORT(attr))
                other |= 1;
            if (FUNC_CALL(attr) == FUNC_STDCALL && can_add_underscore) {
                sprintf(buf1, "_%s@%d", name, FUNC_ARGS(attr) * PTR_SIZE);
                name = buf1;
                other |= 2;
                can_add_underscore = 0;
            }
        } else {
            if (find_elf_sym(tcc_state->dynsymtab_section, name))
                other |= 4;
            if (sym->type.t & VT_IMPORT)
                other |= 4;
        }
#endif
        if (tcc_state->leading_underscore && can_add_underscore) {
            buf1[0] = '_';
            pstrcpy(buf1 + 1, sizeof(buf1) - 1, name);
            name = buf1;
        }
        info = ELFW(ST_INFO)(sym_bind, sym_type);
        sym->c = add_elf_sym(symtab_section, value, size, info, other, sh_num, name);
    } else {
        esym = &((ElfW(Sym) *) symtab_section->data)[sym->c];
        esym->st_value = value;
        esym->st_size = size;
        esym->st_shndx = sh_num;
    }
}

ST_FUNC void put_extern_sym(Sym *sym, Section *section, unsigned long value, unsigned long size)
{
    put_extern_sym2(sym, section, value, size, 1);
}

/* add a new relocation entry to symbol 'sym' in section 's' */
ST_FUNC void greloc(Section *s, Sym *sym, unsigned long offset, int type)
{
    int c = 0;
    if (sym) {
        if (0 == sym->c)
            put_extern_sym(sym, NULL, 0, 0);
        c = sym->c;
    }
    /* now we can add ELF relocation info */
    put_elf_reloc(symtab_section, s, offset, type, c);
}

/********************************************************/

static void strcat_vprintf(char *buf, int buf_size, const char *fmt, va_list ap)
{
    int len;
    len = strlen(buf);
    vsnprintf(buf + len, buf_size - len, fmt, ap);
}

static void strcat_printf(char *buf, int buf_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    strcat_vprintf(buf, buf_size, fmt, ap);
    va_end(ap);
}

static void error1(TCCState *s1, int is_warning, const char *fmt, va_list ap)
{
    char buf[2048];
    BufferedFile **f;

    buf[0] = '\0';
    if (file) {
        for (f = s1->include_stack; f < s1->include_stack_ptr; f++)
            strcat_printf(buf,
                          sizeof(buf),
                          "In file included from %s:%d:\n",
                          (*f)->filename,
                          (*f)->line_num);
        if (file->line_num > 0) {
            strcat_printf(buf, sizeof(buf), "%s:%d: ", file->filename, file->line_num);
        } else {
            strcat_printf(buf, sizeof(buf), "%s: ", file->filename);
        }
    } else {
        strcat_printf(buf, sizeof(buf), "tcc: ");
    }
    if (is_warning)
        strcat_printf(buf, sizeof(buf), "warning: ");
    else
        strcat_printf(buf, sizeof(buf), "error: ");
    strcat_vprintf(buf, sizeof(buf), fmt, ap);

    if (!s1->error_func) {
        /* default case: stderr */
        fprintf(stderr, "%s\n", buf);
    } else {
        s1->error_func(s1->error_opaque, buf);
    }
    if (!is_warning || s1->warn_error)
        s1->nb_errors++;
}

LIBTCCAPI void tcc_set_error_func(TCCState *s,
                                  void *error_opaque,
                                  void (*error_func)(void *opaque, const char *msg))
{
    s->error_opaque = error_opaque;
    s->error_func = error_func;
}

/* error without aborting current compilation */
PUB_FUNC void error_noabort(const char *fmt, ...)
{
    TCCState *s1 = tcc_state;
    va_list ap;

    va_start(ap, fmt);
    error1(s1, 0, fmt, ap);
    va_end(ap);
}

PUB_FUNC void error(const char *fmt, ...)
{
    TCCState *s1 = tcc_state;
    va_list ap;

    va_start(ap, fmt);
    error1(s1, 0, fmt, ap);
    va_end(ap);
    /* better than nothing: in some cases, we accept to handle errors */
    if (s1->error_set_jmp_enabled) {
        longjmp(s1->error_jmp_buf, 1);
    } else {
        /* XXX: eliminate this someday */
        exit(1);
    }
}

PUB_FUNC void expect(const char *msg)
{
    error("%s expected", msg);
}

PUB_FUNC void warning(const char *fmt, ...)
{
    TCCState *s1 = tcc_state;
    va_list ap;

    if (s1->warn_none)
        return;

    va_start(ap, fmt);
    error1(s1, 1, fmt, ap);
    va_end(ap);
}

/********************************************************/
/* I/O layer */

ST_FUNC BufferedFile *tcc_open(TCCState *s1, const char *filename)
{
    int fd;
    BufferedFile *bf;

    if (strcmp(filename, "-") == 0)
        fd = 0, filename = "stdin";
    else
        fd = open(filename, O_RDONLY | O_BINARY);
    if ((s1->verbose == 2 && fd >= 0) || s1->verbose == 3)
        printf("%s %*s%s\n",
               fd < 0 ? "nf" : "->",
               (int) (s1->include_stack_ptr - s1->include_stack),
               "",
               filename);
    if (fd < 0)
        return NULL;
    bf = tcc_malloc(sizeof(BufferedFile));
    bf->fd = fd;
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer;
    bf->buffer[0] = CH_EOB; /* put eob symbol */
    pstrcpy(bf->filename, sizeof(bf->filename), filename);
#ifdef _WIN32
    normalize_slashes(bf->filename);
#endif
    bf->line_num = 1;
    bf->ifndef_macro = 0;
    bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    //    printf("opening '%s'\n", filename);
    return bf;
}

ST_FUNC void tcc_close(BufferedFile *bf)
{
    total_lines += bf->line_num;
    close(bf->fd);
    tcc_free(bf);
}

/* compile the C file opened in 'file'. Return non zero if errors. */
static int tcc_compile(TCCState *s1)
{
    Sym *define_start;
    char buf[512];
    volatile int section_sym;

#ifdef INC_DEBUG
    printf("%s: **** new file\n", file->filename);
#endif
    preprocess_init(s1);

    cur_text_section = NULL;
    funcname = "";
    anon_sym = SYM_FIRST_ANOM;

    /* file info: full path + filename */
    section_sym = 0; /* avoid warning */
    if (s1->do_debug) {
        section_sym = put_elf_sym(symtab_section,
                                  0,
                                  0,
                                  ELFW(ST_INFO)(STB_LOCAL, STT_SECTION),
                                  0,
                                  text_section->sh_num,
                                  NULL);
        getcwd(buf, sizeof(buf));
#ifdef _WIN32
        normalize_slashes(buf);
#endif
        pstrcat(buf, sizeof(buf), "/");
        put_stabs_r(buf, N_SO, 0, 0, text_section->data_offset, text_section, section_sym);
        put_stabs_r(file->filename, N_SO, 0, 0, text_section->data_offset, text_section, section_sym);
    }
    /* an elf symbol of type STT_FILE must be put so that STB_LOCAL
       symbols can be safely used */
    put_elf_sym(symtab_section, 0, 0, ELFW(ST_INFO)(STB_LOCAL, STT_FILE), 0, SHN_ABS, file->filename);

    /* define some often used types */
    int_type.t = VT_INT;

    char_pointer_type.t = VT_BYTE;
    mk_pointer(&char_pointer_type);

    func_old_type.t = VT_FUNC;
    func_old_type.ref = sym_push(SYM_FIELD, &int_type, FUNC_CDECL, FUNC_OLD);

#if defined(TCC_ARM_EABI) && defined(TCC_ARM_VFP)
    float_type.t = VT_FLOAT;
    double_type.t = VT_DOUBLE;

    func_float_type.t = VT_FUNC;
    func_float_type.ref = sym_push(SYM_FIELD, &float_type, FUNC_CDECL, FUNC_OLD);
    func_double_type.t = VT_FUNC;
    func_double_type.ref = sym_push(SYM_FIELD, &double_type, FUNC_CDECL, FUNC_OLD);
#endif

#if 0
    /* define 'void *alloca(unsigned int)' builtin function */
    {
        Sym *s1;

        p = anon_sym++;
        sym = sym_push(p, mk_pointer(VT_VOID), FUNC_CDECL, FUNC_NEW);
        s1 = sym_push(SYM_FIELD, VT_UNSIGNED | VT_INT, 0, 0);
        s1->next = NULL;
        sym->next = s1;
        sym_push(TOK_alloca, VT_FUNC | (p << VT_STRUCT_SHIFT), VT_CONST, 0);
    }
#endif

    define_start = define_stack;
    nocode_wanted = 1;

    if (setjmp(s1->error_jmp_buf) == 0) {
        s1->nb_errors = 0;
        s1->error_set_jmp_enabled = 1;

        ch = file->buf_ptr[0];
        tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
        parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM;
        next();
        decl(VT_CONST);
        if (tok != TOK_EOF)
            expect("declaration");

        /* end of translation unit info */
        if (s1->do_debug) {
            put_stabs_r(NULL, N_SO, 0, 0, text_section->data_offset, text_section, section_sym);
        }
    }
    s1->error_set_jmp_enabled = 0;

    /* reset define stack, but leave -Dsymbols (may be incorrect if
       they are undefined) */
    free_defines(define_start);

    gen_inline_functions();

    sym_pop(&global_stack, NULL);
    sym_pop(&local_stack, NULL);

    return s1->nb_errors != 0 ? -1 : 0;
}

LIBTCCAPI int tcc_compile_string(TCCState *s, const char *str)
{
    BufferedFile bf1, *bf = &bf1;
    int ret, len;
    char *buf;

    /* init file structure */
    bf->fd = -1;
    /* XXX: avoid copying */
    len = strlen(str);
    buf = tcc_malloc(len + 1);
    if (!buf)
        return -1;
    memcpy(buf, str, len);
    buf[len] = CH_EOB;
    bf->buf_ptr = buf;
    bf->buf_end = buf + len;
    pstrcpy(bf->filename, sizeof(bf->filename), "<string>");
    bf->line_num = 1;
    file = bf;
    ret = tcc_compile(s);
    file = NULL;
    tcc_free(buf);

    /* currently, no need to close */
    return ret;
}

/* define a preprocessor symbol. A value can also be provided with the '=' operator */
LIBTCCAPI void tcc_define_symbol(TCCState *s1, const char *sym, const char *value)
{
    BufferedFile bf1, *bf = &bf1;

    pstrcpy(bf->buffer, IO_BUF_SIZE, sym);
    pstrcat(bf->buffer, IO_BUF_SIZE, " ");
    /* default value */
    if (!value)
        value = "1";
    pstrcat(bf->buffer, IO_BUF_SIZE, value);

    /* init file structure */
    bf->fd = -1;
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + strlen(bf->buffer);
    *bf->buf_end = CH_EOB;
    bf->filename[0] = '\0';
    bf->line_num = 1;
    file = bf;

    s1->include_stack_ptr = s1->include_stack;

    /* parse with define parser */
    ch = file->buf_ptr[0];
    next_nomacro();
    parse_define();
    file = NULL;
}

/* undefine a preprocessor symbol */
LIBTCCAPI void tcc_undefine_symbol(TCCState *s1, const char *sym)
{
    TokenSym *ts;
    Sym *s;
    ts = tok_alloc(sym, strlen(sym));
    s = define_find(ts->tok);
    /* undefine symbol by putting an invalid name */
    if (s)
        define_undef(s);
}

static void tcc_cleanup(void)
{
    int i, n;

    if (NULL == tcc_state)
        return;
    tcc_state = NULL;

    /* free -D defines */
    free_defines(NULL);

    /* free tokens */
    n = tok_ident - TOK_IDENT;
    for (i = 0; i < n; i++)
        tcc_free(table_ident[i]);
    tcc_free(table_ident);

    /* free sym_pools */
    dynarray_reset(&sym_pools, &nb_sym_pools);
    /* string buffer */
    cstr_free(&tokcstr);
    /* reset symbol stack */
    sym_free_first = NULL;
    /* cleanup from error/setjmp */
    macro_ptr = NULL;
}

LIBTCCAPI TCCState *tcc_new(void)
{
    TCCState *s;
    char buffer[100];
    int a, b, c;

    tcc_cleanup();

    s = tcc_mallocz(sizeof(TCCState));
    if (!s)
        return NULL;
    tcc_state = s;
#ifdef _WIN32
    tcc_set_lib_path_w32(s);
#else
    tcc_set_lib_path(s, CONFIG_TCCDIR);
#endif
    s->output_type = TCC_OUTPUT_MEMORY;
    preprocess_new();

    /* we add dummy defines for some special macros to speed up tests
       and to have working defined() */
    define_push(TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___TIME__, MACRO_OBJ, NULL, NULL);

    /* standard defines */
    tcc_define_symbol(s, "__STDC__", NULL);
    tcc_define_symbol(s, "__STDC_VERSION__", "199901L");
#if defined(TCC_TARGET_I386)
    tcc_define_symbol(s, "__i386__", NULL);
    tcc_define_symbol(s, "__i386", NULL);
    tcc_define_symbol(s, "i386", NULL);
#endif
#if defined(TCC_TARGET_X86_64)
    tcc_define_symbol(s, "__x86_64__", NULL);
#endif
#if defined(TCC_TARGET_ARM)
    tcc_define_symbol(s, "__ARM_ARCH_4__", NULL);
    tcc_define_symbol(s, "__arm_elf__", NULL);
    tcc_define_symbol(s, "__arm_elf", NULL);
    tcc_define_symbol(s, "arm_elf", NULL);
    tcc_define_symbol(s, "__arm__", NULL);
    tcc_define_symbol(s, "__arm", NULL);
    tcc_define_symbol(s, "arm", NULL);
    tcc_define_symbol(s, "__APCS_32__", NULL);
#endif
#ifdef TCC_TARGET_PE
    tcc_define_symbol(s, "_WIN32", NULL);
#ifdef TCC_TARGET_X86_64
    tcc_define_symbol(s, "_WIN64", NULL);
#endif
#else
    tcc_define_symbol(s, "__unix__", NULL);
    tcc_define_symbol(s, "__unix", NULL);
    tcc_define_symbol(s, "unix", NULL);
#if defined(__FreeBSD__)
#define str(s) #s
    tcc_define_symbol(s, "__FreeBSD__", str(__FreeBSD__));
#undef str
#endif
#if defined(__linux)
    tcc_define_symbol(s, "__linux__", NULL);
    tcc_define_symbol(s, "__linux", NULL);
#endif
#endif
    /* tiny C specific defines */
    sscanf(TCC_VERSION, "%d.%d.%d", &a, &b, &c);
    sprintf(buffer, "%d", a * 10000 + b * 100 + c);
    tcc_define_symbol(s, "__TINYC__", buffer);

    /* tiny C & gcc defines */
#if defined TCC_TARGET_PE && defined TCC_TARGET_X86_64
    tcc_define_symbol(s, "__SIZE_TYPE__", "unsigned long long");
    tcc_define_symbol(s, "__PTRDIFF_TYPE__", "long long");
#else
    tcc_define_symbol(s, "__SIZE_TYPE__", "unsigned long");
    tcc_define_symbol(s, "__PTRDIFF_TYPE__", "long");
#endif

#ifdef TCC_TARGET_PE
    tcc_define_symbol(s, "__WCHAR_TYPE__", "unsigned short");
#else
    tcc_define_symbol(s, "__WCHAR_TYPE__", "int");
#endif

#ifndef TCC_TARGET_PE
    /* default library paths */
#if defined(TCC_TARGET_X86_64_CENTOS)
    tcc_add_library_path(s, CONFIG_SYSROOT "/usr/lib64");
    tcc_add_library_path(s, CONFIG_SYSROOT "/lib64");
    tcc_add_library_path(s, CONFIG_SYSROOT "/usr/local/lib64");
#else
    tcc_add_library_path(s, CONFIG_SYSROOT "/usr/lib");
    tcc_add_library_path(s, CONFIG_SYSROOT "/lib");
    tcc_add_library_path(s, CONFIG_SYSROOT "/usr/local/lib");
#endif
#endif

    /* no section zero */
    dynarray_add((void ***) &s->sections, &s->nb_sections, NULL);

    /* create standard sections */
    text_section = new_section(s, ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
    data_section = new_section(s, ".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
    bss_section = new_section(s, ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE);

    /* symbols are always generated for linking stage */
    symtab_section = new_symtab(s, ".symtab", SHT_SYMTAB, 0, ".strtab", ".hashtab", SHF_PRIVATE);
    strtab_section = symtab_section->link;

    /* private symbol table for dynamic symbols */
    s->dynsymtab_section = new_symtab(s,
                                      ".dynsymtab",
                                      SHT_SYMTAB,
                                      SHF_PRIVATE,
                                      ".dynstrtab",
                                      ".dynhashtab",
                                      SHF_PRIVATE);
    s->alacarte_link = 1;
    s->nocommon = 1;

#ifdef CHAR_IS_UNSIGNED
    s->char_is_unsigned = 1;
#endif
#if defined(TCC_TARGET_PE) && 0
    /* XXX: currently the PE linker is not ready to support that */
    s->leading_underscore = 1;
#endif
    if (s->section_align == 0)
        s->section_align = ELF_PAGE_SIZE;
#ifdef TCC_TARGET_I386
    s->seg_size = 32;
#endif
    return s;
}

LIBTCCAPI void tcc_delete(TCCState *s1)
{
    int i;

    tcc_cleanup();

    /* free all sections */
    for (i = 1; i < s1->nb_sections; i++)
        free_section(s1->sections[i]);
    dynarray_reset(&s1->sections, &s1->nb_sections);

    for (i = 0; i < s1->nb_priv_sections; i++)
        free_section(s1->priv_sections[i]);
    dynarray_reset(&s1->priv_sections, &s1->nb_priv_sections);

    /* free any loaded DLLs */
    for (i = 0; i < s1->nb_loaded_dlls; i++) {
        DLLReference *ref = s1->loaded_dlls[i];
        if (ref->handle)
            dlclose(ref->handle);
    }

    /* free loaded dlls array */
    dynarray_reset(&s1->loaded_dlls, &s1->nb_loaded_dlls);

    /* free library paths */
    dynarray_reset(&s1->library_paths, &s1->nb_library_paths);

    /* free include paths */
    dynarray_reset(&s1->cached_includes, &s1->nb_cached_includes);
    dynarray_reset(&s1->include_paths, &s1->nb_include_paths);
    dynarray_reset(&s1->sysinclude_paths, &s1->nb_sysinclude_paths);

    tcc_free(s1->tcc_lib_path);
    tcc_free(s1->runtime_mem);
    tcc_free(s1);
}

LIBTCCAPI int tcc_add_include_path(TCCState *s1, const char *pathname)
{
    char *pathname1;

    pathname1 = tcc_strdup(pathname);
    dynarray_add((void ***) &s1->include_paths, &s1->nb_include_paths, pathname1);
    return 0;
}

LIBTCCAPI int tcc_add_sysinclude_path(TCCState *s1, const char *pathname)
{
    char *pathname1;

    pathname1 = tcc_strdup(pathname);
    dynarray_add((void ***) &s1->sysinclude_paths, &s1->nb_sysinclude_paths, pathname1);
    return 0;
}

static int tcc_add_file_internal(TCCState *s1, const char *filename, int flags)
{
    const char *ext;
    ElfW(Ehdr) ehdr;
    int fd, ret, size;
    BufferedFile *saved_file;

    ret = -1;

    /* find source file type with extension */
    ext = tcc_fileextension(filename);
    if (ext[0])
        ext++;

    /* open the file */
    saved_file = file;
    file = tcc_open(s1, filename);
    if (!file) {
        if (flags & AFF_PRINT_ERROR)
            error_noabort("file '%s' not found", filename);
        goto the_end;
    }

    if (flags & AFF_PREPROCESS) {
        ret = tcc_preprocess(s1);
        goto the_end;
    }

    if (!ext[0] || !PATHCMP(ext, "c")) {
        /* C file assumed */
        ret = tcc_compile(s1);
        goto the_end;
    }

#ifdef CONFIG_TCC_ASM
    if (!strcmp(ext, "S")) {
        /* preprocessed assembler */
        ret = tcc_assemble(s1, 1);
        goto the_end;
    }

    if (!strcmp(ext, "s")) {
        /* non preprocessed assembler */
        ret = tcc_assemble(s1, 0);
        goto the_end;
    }
#endif

    fd = file->fd;
    /* assume executable format: auto guess file type */
    size = read(fd, &ehdr, sizeof(ehdr));
    lseek(fd, 0, SEEK_SET);
    if (size <= 0) {
        error_noabort("could not read header");
        goto the_end;
    }

    if (size == sizeof(ehdr) && ehdr.e_ident[0] == ELFMAG0 && ehdr.e_ident[1] == ELFMAG1
        && ehdr.e_ident[2] == ELFMAG2 && ehdr.e_ident[3] == ELFMAG3) {
        /* do not display line number if error */
        file->line_num = 0;
        if (ehdr.e_type == ET_REL) {
            ret = tcc_load_object_file(s1, fd, 0);
            goto the_end;
        }
#ifndef TCC_TARGET_PE
        if (ehdr.e_type == ET_DYN) {
            if (s1->output_type == TCC_OUTPUT_MEMORY) {
                void *h;
                h = dlopen(filename, RTLD_GLOBAL | RTLD_LAZY);
                if (h)
                    ret = 0;
            } else {
                ret = tcc_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
            }
            goto the_end;
        }
#endif
        error_noabort("unrecognized ELF file");
        goto the_end;
    }

    if (memcmp((char *) &ehdr, ARMAG, 8) == 0) {
        file->line_num = 0; /* do not display line number if error */
        ret = tcc_load_archive(s1, fd);
        goto the_end;
    }

#ifdef TCC_TARGET_COFF
    if (*(uint16_t *) (&ehdr) == COFF_C67_MAGIC) {
        ret = tcc_load_coff(s1, fd);
        goto the_end;
    }
#endif

#ifdef TCC_TARGET_PE
    ret = pe_load_file(s1, filename, fd);
#else
    /* as GNU ld, consider it is an ld script if not recognized */
    ret = tcc_load_ldscript(s1);
#endif
    if (ret < 0)
        error_noabort("unrecognized file type");

the_end:
    if (file)
        tcc_close(file);
    file = saved_file;
    return ret;
}

LIBTCCAPI int tcc_add_file(TCCState *s, const char *filename)
{
    if (s->output_type == TCC_OUTPUT_PREPROCESS)
        return tcc_add_file_internal(s, filename, AFF_PRINT_ERROR | AFF_PREPROCESS);
    else
        return tcc_add_file_internal(s, filename, AFF_PRINT_ERROR);
}

LIBTCCAPI int tcc_add_library_path(TCCState *s, const char *pathname)
{
    char *pathname1;

    pathname1 = tcc_strdup(pathname);
    dynarray_add((void ***) &s->library_paths, &s->nb_library_paths, pathname1);
    return 0;
}

/* find and load a dll. Return non zero if not found */
/* XXX: add '-rpath' option support ? */
ST_FUNC int tcc_add_dll(TCCState *s, const char *filename, int flags)
{
    char buf[1024];
    int i;

    for (i = 0; i < s->nb_library_paths; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", s->library_paths[i], filename);
        if (tcc_add_file_internal(s, buf, flags) == 0)
            return 0;
    }
    return -1;
}

/* the library name is the same as the argument of the '-l' option */
LIBTCCAPI int tcc_add_library(TCCState *s, const char *libraryname)
{
    char buf[1024];
    int i;

    /* first we look for the dynamic library if not static linking */
    if (!s->static_link) {
#ifdef TCC_TARGET_PE
        if (pe_add_dll(s, libraryname) == 0)
            return 0;
#else
        snprintf(buf, sizeof(buf), "lib%s.so", libraryname);
        if (tcc_add_dll(s, buf, 0) == 0)
            return 0;
#endif
    }
    /* then we look for the static library */
    for (i = 0; i < s->nb_library_paths; i++) {
        snprintf(buf, sizeof(buf), "%s/lib%s.a", s->library_paths[i], libraryname);
        if (tcc_add_file_internal(s, buf, 0) == 0)
            return 0;
    }
    return -1;
}

LIBTCCAPI int tcc_add_symbol(TCCState *s, const char *name, const void *val)
{
#ifdef TCC_TARGET_PE
    pe_putimport(s, 0, name, val);
#else
    add_elf_sym(symtab_section,
                (uplong) val,
                0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                0,
                SHN_ABS,
                name);
#endif
    return 0;
}

LIBTCCAPI int tcc_set_output_type(TCCState *s, int output_type)
{
    char buf[1024];

    s->output_type = output_type;

    if (!s->nostdinc) {
        /* default include paths */
        /* XXX: reverse order needed if -isystem support */
#ifndef TCC_TARGET_PE
        tcc_add_sysinclude_path(s, CONFIG_SYSROOT "/usr/local/include");
        tcc_add_sysinclude_path(s, CONFIG_SYSROOT "/usr/include");
#endif
        snprintf(buf, sizeof(buf), "%s/include", s->tcc_lib_path);
        tcc_add_sysinclude_path(s, buf);
#ifdef TCC_TARGET_PE
        snprintf(buf, sizeof(buf), "%s/include/winapi", s->tcc_lib_path);
        tcc_add_sysinclude_path(s, buf);
#endif
    }

    /* if bound checking, then add corresponding sections */
#ifdef CONFIG_TCC_BCHECK
    if (s->do_bounds_check) {
        /* define symbol */
        tcc_define_symbol(s, "__BOUNDS_CHECKING_ON", NULL);
        /* create bounds sections */
        bounds_section = new_section(s, ".bounds", SHT_PROGBITS, SHF_ALLOC);
        lbounds_section = new_section(s, ".lbounds", SHT_PROGBITS, SHF_ALLOC);
    }
#endif

    if (s->char_is_unsigned) {
        tcc_define_symbol(s, "__CHAR_UNSIGNED__", NULL);
    }

    /* add debug sections */
    if (s->do_debug) {
        /* stab symbols */
        stab_section = new_section(s, ".stab", SHT_PROGBITS, 0);
        stab_section->sh_entsize = sizeof(Stab_Sym);
        stabstr_section = new_section(s, ".stabstr", SHT_STRTAB, 0);
        put_elf_str(stabstr_section, "");
        stab_section->link = stabstr_section;
        /* put first entry */
        put_stabs("", 0, 0, 0, 0);
    }

    /* add libc crt1/crti objects */
#ifndef TCC_TARGET_PE
    if ((output_type == TCC_OUTPUT_EXE || output_type == TCC_OUTPUT_DLL) && !s->nostdlib) {
        if (output_type != TCC_OUTPUT_DLL)
            tcc_add_file(s, CONFIG_TCC_CRT_PREFIX "/crt1.o");
        tcc_add_file(s, CONFIG_TCC_CRT_PREFIX "/crti.o");
    }
#endif

#ifdef TCC_TARGET_PE
    snprintf(buf, sizeof(buf), "%s/lib", s->tcc_lib_path);
    tcc_add_library_path(s, buf);
#ifdef _WIN32
    if (GetSystemDirectory(buf, sizeof buf))
        tcc_add_library_path(s, buf);
#endif
#endif

    return 0;
}

#define WD_ALL 0x0001    /* warning is activated when using -Wall */
#define FD_INVERT 0x0002 /* invert value before storing */

typedef struct FlagDef
{
    uint16_t offset;
    uint16_t flags;
    const char *name;
} FlagDef;

static const FlagDef warning_defs[] = {
    {offsetof(TCCState, warn_unsupported), 0, "unsupported"},
    {offsetof(TCCState, warn_write_strings), 0, "write-strings"},
    {offsetof(TCCState, warn_error), 0, "error"},
    {offsetof(TCCState, warn_implicit_function_declaration),
     WD_ALL,
     "implicit-function-declaration"},
};

ST_FUNC int set_flag(TCCState *s, const FlagDef *flags, int nb_flags, const char *name, int value)
{
    int i;
    const FlagDef *p;
    const char *r;

    r = name;
    if (r[0] == 'n' && r[1] == 'o' && r[2] == '-') {
        r += 3;
        value = !value;
    }
    for (i = 0, p = flags; i < nb_flags; i++, p++) {
        if (!strcmp(r, p->name))
            goto found;
    }
    return -1;
found:
    if (p->flags & FD_INVERT)
        value = !value;
    *(int *) ((uint8_t *) s + p->offset) = value;
    return 0;
}

/* set/reset a warning */
LIBTCCAPI int tcc_set_warning(TCCState *s, const char *warning_name, int value)
{
    int i;
    const FlagDef *p;

    if (!strcmp(warning_name, "all")) {
        for (i = 0, p = warning_defs; i < countof(warning_defs); i++, p++) {
            if (p->flags & WD_ALL)
                *(int *) ((uint8_t *) s + p->offset) = 1;
        }
        return 0;
    } else {
        return set_flag(s, warning_defs, countof(warning_defs), warning_name, value);
    }
}

static const FlagDef flag_defs[] = {
    {offsetof(TCCState, char_is_unsigned), 0, "unsigned-char"},
    {offsetof(TCCState, char_is_unsigned), FD_INVERT, "signed-char"},
    {offsetof(TCCState, nocommon), FD_INVERT, "common"},
    {offsetof(TCCState, leading_underscore), 0, "leading-underscore"},
};

/* set/reset a flag */
PUB_FUNC int tcc_set_flag(TCCState *s, const char *flag_name, int value)
{
    return set_flag(s, flag_defs, countof(flag_defs), flag_name, value);
}

PUB_FUNC void tcc_print_stats(TCCState *s, int64_t total_time)
{
    double tt;
    tt = (double) total_time / 1000000.0;
    if (tt < 0.001)
        tt = 0.001;
    if (total_bytes < 1)
        total_bytes = 1;
    printf("%d idents, %d lines, %d bytes, %0.3f s, %d lines/s, %0.1f MB/s\n",
           tok_ident - TOK_IDENT,
           total_lines,
           total_bytes,
           tt,
           (int) (total_lines / tt),
           total_bytes / tt / 1000000.0);
}

/* set CONFIG_TCCDIR at runtime */
LIBTCCAPI void tcc_set_lib_path(TCCState *s, const char *path)
{
    tcc_free(s->tcc_lib_path);
    s->tcc_lib_path = tcc_strdup(path);
}

PUB_FUNC void set_num_callers(int n)
{
#ifdef CONFIG_TCC_BACKTRACE
    num_callers = n;
#endif
}
