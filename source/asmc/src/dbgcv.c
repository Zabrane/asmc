/***************************************************************************
*
*  This code is Public Domain.
*
*  ========================================================================
*
* Description:	Generate CodeView symbolic debug info ( Version 4 format )
*
****************************************************************************/

#include <stddef.h>
#include <globals.h>
#include <memalloc.h>
#include <parser.h>
#include <segment.h>
#include <fixup.h>
#include <dbgcv.h>
#include <linnum.h>
#include <direct.h>
#include <MD5.h>

#define EQUATESYMS 1 /* 1=generate info for EQUates ( -Zi3 ) */
#define GENPTRTYPE 0 /* generate a generic pointer type */

static char szCVCompiler[] = "Asmc Macro Assembler";

#pragma pack(push, 1)

typedef struct cv_file {
    char *name;
    uint_32 offset; /* offset into string table */
  } cv_file;

typedef struct CV_SECTION {
    uint_32 signature;
    uint_32 length;
  } CV_SECTION;

typedef struct dbgcv {
    union {
	uint_8		*ps;
	uint_16		*dw;
	uint_32		*dd;
	CV_SECTION	*sec;
	CFLAGSYM	*ps_cp;		/* S_COMPILE		*/
	REGSYM_16t	*ps_reg;	/* S_REGISTER_16t	*/
	CONSTSYM_16t	*ps_con;	/* S_CONSTANT_16t	*/
	UDTSYM_16t	*ps_udt;	/* S_UDT_16t		*/
	ENDARGSYM	*ps_eb;		/* S_END		*/
	OBJNAMESYM	*ps_on;		/* S_OBJNAME_ST		*/
	BPRELSYM16	*ps_br16;	/* S_BPREL16		*/
	PROCSYM16	*ps_p16;	/* S_xPROC16		*/
	LABELSYM16	*ps_l16;	/* S_LABEL16		*/
	BPRELSYM32_16t	*ps_br32;	/* S_BPREL32_16t	*/
	DATASYM32_16t	*ps_d32;	/* S_xDATA[32|16]_16t	*/
	PROCSYM32_16t	*ps_p32;	/* S_xPROC[32|16]_16t	*/
	REGREL32_16t	*ps_rr32;	/* S_REGREL32_16t	*/
	LABELSYM32	*ps_l32;	/* S_LABEL32_ST		*/
	COMPILESYM3	*s_cp;		/* S_COMPILE3		*/
	OBJNAMESYM	*s_on;		/* S_OBJNAME		*/
	ENVBLOCKSYM	*s_env;		/* S_ENVBLOCK		*/
	REGSYM		*s_reg;		/* S_REGISTER		*/
	CONSTSYM	*s_con;		/* S_CONSTANT		*/
	UDTSYM		*s_udt;		/* S_UDT		*/
	ENDARGSYM	*s_eb;		/* S_END		*/
	BPRELSYM32	*s_br32;	/* S_BPREL32		*/
	DATASYM32	*s_d32;		/* S_xDATA32		*/
	PROCSYM32	*s_p32;		/* S_xPROC32		*/
	REGREL32	*s_rr32;	/* S_REGREL32		*/
	LABELSYM32	*s_l32;		/* S_LABEL32		*/
    };
    union {
	uint_8		*pt;
	CV_ARRAY_16t	*pt_ar;
	CV_POINTER_16t	*pt_ptr;
	CV_BITFIELD_16t *pt_bf;
	CV_UNION_16t	*pt_un;
	CV_STRUCT_16t	*pt_st;
	CV_PROCEDURE_16t *pt_pr;
	CV_ARGLIST_16t	*pt_al;
	CV_FIELDLIST	*pt_fl;
	CV_MEMBER_16t	*pt_mbr;
	CV_ARRAY	*t_ar;
	CV_POINTER	*t_ptr;
	CV_BITFIELD	*t_bf;
	CV_UNION	*t_un;
	CV_STRUCT	*t_st;
	CV_PROCEDURE	*t_pr;
	CV_ARGLIST	*t_al;
	CV_FIELDLIST	*t_fl;
	CV_MEMBER	*t_mbr;
    };
    struct dsym *	symbols;
    struct dsym *	types;
    void *		param;
    unsigned		level;		/* nesting level */
    uint_32		currtype;	/* current type ( starts with 0x1000 ) */
    cv_file *		files;
    char *		currdir;
#if GENPTRTYPE
    uint_16		ptrtype[3];	/* generic 16-,32- and 64-bit pointer type */
#endif
  } dbgcv;

struct leaf32 {
    uint_16 leaf;
    uint_32 value32;
};

#pragma pack(pop)

const CV_PRIMITIVE cv_idx_type = { CV_IN_4BYTE, CV_SIGNED,  CV_TM_DIRECT, 0 };
const CV_PRIMITIVE cv_void     = { CV_SP_VOID,	CV_SPECIAL, CV_TM_DIRECT, 0 };
#if EQUATESYMS
const CV_PRIMITIVE cv_abs_type = { CV_SP_ABS,	CV_SPECIAL, CV_TM_DIRECT, 0 };
#endif

uint_8 *SetPrefixName( uint_8 *p, uint_8 *name, int len )
{
    if ( Options.debug_symbols < CV_SIGNATURE_C13 )
	*p++ = len;
    memcpy( p, name, len );
    p += len;
    if ( Options.debug_symbols == CV_SIGNATURE_C13 )
	*p++ = '\0';
    return p;
}

/* flush the segment buffer for symbols and types.
 * For OMF, the buffer is written to disk. For COFF, it's
 * more complicated, because we cannot write now, but also
 * don't know yet the final size of the segment. So a linked
 * list of buffer items has to be created. The contents will
 * later be written inside coff_write_header().
 */
#define checkflush( seg, curr, size, param ) seg->e.seginfo->flushfunc( seg, curr, size, param )

/* translate symbol's mem_type to a codeview typeref */

uint_32 GetTyperef( struct asym *sym, uint_8 Ofssize )
{
    CV_PRIMITIVE value = { CV_SP_NOTYPE, CV_SPECIAL, CV_TM_DIRECT, 0 };

    if ( ( sym->mem_type & MT_SPECIAL ) == 0 ) {
	int size = SizeFromMemtype( sym->mem_type, Ofssize, sym->type );
	if ( sym->mem_type & MT_FLOAT ) {
	    value.type = CV_REAL;
	    switch ( size ) {
	    case 4:  value.subt = CV_RC_REAL32; break;
	    case 8:  value.subt = CV_RC_REAL64; break;
	    case 10: value.subt = CV_RC_REAL80; break;
	    case 16: value.subt = CV_RC_REAL128; break;
	    }
	} else if ( size <= 16 ) {
	    if ( sym->mem_type & MT_SIGNED )
		value.type = CV_SIGNED;
	    else
		value.type = CV_UNSIGNED;
	    switch ( size ) {
	    case 1:  value.subt = CV_IN_1BYTE; break;
	    case 2:  value.subt = CV_IN_2BYTE; break;
	    case 4:  value.subt = CV_IN_4BYTE; break;
	    case 8:  value.subt = CV_IN_8BYTE; break;
	    case 16: value.subt = CV_IN_16BYTE; break;
	    case 6: /* v2.11: added ( FWORD ) */
		value.type = CV_SPECIAL;
		value.subt = CV_SP_VOID;
		value.mode = CV_TM_FPTR32;
		break;
	    }
	} else { /* v2.11: branch added */
	    /* problem: there's no integral size > 8 bytes.
	     * Masm v8+ sets 79h (=?) for 16-byte and 3h (=void) for 32-byte.
	     * jwasm sets uint64, which allows to view at least
	     * the lower 8 bytes.
	     */
	    value.type = CV_INT;
	    value.subt = CV_RI_UINT8;
	}
    } else {
	switch ( sym->mem_type ) {
	case MT_PTR:
	    /* v2.10 */
	    value.subt = CV_SP_VOID;
	    value.type = CV_SPECIAL;
	    switch ( sym->Ofssize ) {
	    case USE16:
		value.mode = ( sym->isfar ? CV_TM_FPTR : CV_TM_NPTR );
		break;
	    case USE32:
		value.mode = ( sym->isfar ? CV_TM_FPTR32 : CV_TM_NPTR32 );
		break;
	    case USE64:
		value.mode = CV_TM_NPTR64;
		if ( Options.debug_symbols == CV_SIGNATURE_C13 && (
		     sym->state == SYM_INTERNAL || sym->state == SYM_STACK ||
		     sym->state == SYM_STRUCT_FIELD || sym->state == SYM_TYPE ) ) {
		    switch ( sym->ptr_memtype ) {
		    case MT_BYTE:   value.uvalue = ST_64PUCHAR;	  break;
		    case MT_SBYTE:  value.uvalue = ST_64PCHAR;	  break;
		    case MT_WORD:   value.uvalue = ST_64PCHAR16;  break;
		    case MT_SWORD:  value.uvalue = ST_64PSHORT;	  break;
		    case MT_REAL2:  value.uvalue = ST_64PREAL16;  break;
		    case MT_DWORD:  value.uvalue = ST_64PUINT4;	  break;
		    case MT_SDWORD: value.uvalue = ST_64PINT4;	  break;
		    case MT_REAL4:  value.uvalue = ST_64PREAL32;  break;
		    case MT_QWORD:  value.uvalue = ST_64PUINT8;	  break;
		    case MT_SQWORD: value.uvalue = ST_64PINT8;	  break;
		    case MT_REAL8:  value.uvalue = ST_64PREAL64;  break;
		    case MT_OWORD:  value.uvalue = ST_64PUINT16;  break;
		    case MT_REAL16: value.uvalue = ST_64PREAL128; break;
		    }
		}
		break;
	    }
	    break;
	case MT_BITS:
	    if ( sym->cvtyperef )
		return( sym->cvtyperef );
	    break;
	case MT_NEAR: value.mode = CV_TM_NPTR; break;
	case MT_FAR:  value.mode = CV_TM_FPTR; break;
	case MT_TYPE:
	    for ( sym = sym->type; sym->type; sym = sym->type );
	    if ( sym->cvtyperef )
		return( sym->cvtyperef );
	    return( GetTyperef( sym, Ofssize ) );
	    break;
	}
    }

    return( value.uvalue );
}

/* calc size of a codeview item in symbols segment */

static uint_16 GetCVStructLen( struct asym *sym, uint_8 Ofssize )
{
    uint_16 len;
    switch ( sym->state ) {
    case SYM_TYPE:
	len = sizeof( UDTSYM ) - 1;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 )
	    len = sizeof( UDTSYM_16t ) - 1;
	break;
    default:
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    if ( sym->isproc && Options.debug_ext >= CVEX_REDUCED ) {
		len = ( Ofssize == USE16 ? sizeof( PROCSYM16 ) - 1 : sizeof( PROCSYM32_16t ) - 1 );
	    } else if ( sym->mem_type == MT_NEAR || sym->mem_type == MT_FAR ) {
		len = ( Ofssize == USE16 ? sizeof( LABELSYM16 ) - 1 : sizeof( LABELSYM32 ) - 1 );
#if EQUATESYMS
	    } else if ( sym->isequate ) {
		len = sizeof( CONSTSYM_16t ) + ( sym->value >= LF_NUMERIC ? 2 : 0 );
#endif
	    } else
		len = sizeof( DATASYM32_16t ) - 1;
	} else {
	    if ( sym->isproc && Options.debug_ext >= CVEX_REDUCED ) {
		len = sizeof( PROCSYM32 ) - 1;
	    } else if ( sym->mem_type == MT_NEAR || sym->mem_type == MT_FAR ) {
		len = sizeof( LABELSYM32 ) - 1;
#if EQUATESYMS
	    } else if ( sym->isequate ) {
		len = sizeof( CONSTSYM ) + ( sym->value >= LF_NUMERIC ? 2 : 0 );
#endif
	    } else
		len = sizeof( DATASYM32 ) - 1;
	}
    }
    return( len );
}

static void PadBytes( uint_8 *curr, uint_8 *base )
{
    static const char padtab[] = { LF_PAD1, LF_PAD2, LF_PAD3 };

    for( ;( curr - base ) & 3; curr++ ) {
	if ( Options.debug_symbols == CV_SIGNATURE_C13 )
	    *curr = '\0';
	else
	    *curr = padtab[3-((curr - base) & 3)];
    }
}

/* write a bitfield to $$TYPES */

static void cv_write_bitfield( struct dbgcv *cv, struct dsym *type, struct asym *sym )
{
    uint_32 size = sizeof( CV_BITFIELD );
    uint_32 tref = GetTyperef( (struct asym *)type, USE16 );

    if ( Options.debug_symbols == CV_SIGNATURE_C7 )
	size = sizeof( CV_BITFIELD_16t );
    cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
    sym->cvtyperef = cv->currtype++;
    cv->pt_bf->size = size - sizeof(uint_16);
    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	cv->pt_bf->leaf = LF_BITFIELD_16t;
	cv->pt_bf->length = sym->total_size;
	cv->pt_bf->position = sym->offset;
	cv->pt_bf->type = tref;
    } else {
	cv->t_bf->leaf = LF_BITFIELD;
	cv->t_bf->length = sym->total_size;
	cv->t_bf->position = sym->offset;
	cv->t_bf->type = tref;
    }
    cv->pt += size;
}

static void cv_write_array_type( struct dbgcv *cv, struct asym *sym,
	uint_32 elemtype, uint_8 Ofssize )
{
    uint_8 *tmp;
    int typelen;
    int size = sizeof( CV_ARRAY );

    if ( elemtype == 0 )
	elemtype = GetTyperef( sym, Ofssize );

    if ( Options.debug_symbols == CV_SIGNATURE_C7 )
	size = sizeof( CV_ARRAY_16t );
    typelen = ( sym->total_size >= LF_NUMERIC ? sizeof( uint_32 ) : 0 );
    size = ( size + 2 + typelen + 3 ) & ~3;
    cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
    cv->pt_ar->size = size - sizeof(uint_16);

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	cv->pt_ar->leaf = LF_ARRAY_16t;
	cv->pt_ar->elemtype = elemtype;
	cv->pt_ar->idxtype = cv_idx_type.uvalue; /* ok? */
	tmp = cv->pt_ar->data;
    } else {
	cv->t_ar->leaf = LF_ARRAY;
	cv->t_ar->elemtype = elemtype;
	cv->t_ar->idxtype = GetTyperef( sym, Ofssize );
	tmp = cv->t_ar->data;
    }

    if ( typelen ) {
	*(uint_16 *)tmp = LF_ULONG;
	tmp += sizeof( uint_16 );
	*(uint_32 *)tmp = sym->total_size;
	tmp += sizeof( uint_32 );
    } else {
	*(uint_16 *)tmp = sym->total_size;
	tmp += sizeof( uint_16 );
    }
    *tmp++ = NULLC; /* the array type name is empty */
    PadBytes( tmp, cv->types->e.seginfo->CodeBuffer );
    cv->pt += size;
    cv->currtype++;
    return;
}

/* create a pointer type for procedure params and locals.
 * the symbol's mem_type is MT_PTR.
 */

static uint_32 cv_write_ptr_type( struct dbgcv *cv, struct asym *sym )
{
    uint_32 attr, type;
    uint_32 size = sizeof( CV_POINTER );
    uint_32 leaf = LF_POINTER;

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	size = sizeof( CV_POINTER_16t );
	leaf = LF_POINTER_16t;
    }

    /* for untyped pointers & for function pointers don't create a type, just
     * return a void ptr.
     */
    if ( ( sym->ptr_memtype == MT_EMPTY && sym->target_type == NULL ) || sym->ptr_memtype == MT_PROC )
	return( GetTyperef( sym, sym->Ofssize ) );

    cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
    cv->pt_ptr->size = size - sizeof(uint_16);
    cv->pt_ptr->leaf = leaf;

    if ( sym->Ofssize == USE16 )
	attr = ( sym->isfar ? CV_PTR_FAR : CV_PTR_NEAR );
    else if ( sym->Ofssize == USE32 )
	attr = ( sym->isfar ? CV_PTR_FAR32 : CV_PTR_NEAR32 );
    else
	attr = CV_PTR_64;

    /* if indirection is > 1, define an untyped pointer - to be improved */
    if ( sym->is_ptr > 1 ) {
	type = GetTyperef( sym, sym->Ofssize );
    } else if ( sym->target_type ) {
	/* the target's typeref must have been set here */
	if ( sym->target_type->cvtyperef )
	    type = sym->target_type->cvtyperef;
	else
	    type = GetTyperef( sym, sym->Ofssize );
    } else { /* pointer to simple type */
	unsigned char tmpmt = sym->mem_type; /* the target type is tmp. copied to mem_type */
	sym->mem_type = sym->ptr_memtype;   /* thus GetTyperef() can be used */
	type = GetTyperef( sym, sym->Ofssize );
	sym->mem_type = tmpmt;
    }

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	cv->pt_ptr->attr = attr;
	cv->pt_ptr->utype = type;
	cv->pt_ptr->pmclass = 0;
	cv->pt_ptr->pmenum = 0;
    } else {
	cv->t_ptr->attr = attr;
	cv->t_ptr->utype = type;
	cv->t_ptr->pmclass = 0;
	cv->t_ptr->pmenum = 0;
    }
    cv->pt += size;
    return( cv->currtype++ );
}

/* structure for field enumeration callback function */
struct cv_counters {
    unsigned cnt; /* number of fields */
    uint_32 size; /* size of field list */
    uint_32 ofs;  /* current start offset for member */
};

static void cv_write_type( struct dbgcv *cv, struct asym *sym );

/* type of field enumeration callback function */
typedef void (* cv_enum_func)( struct dsym *, struct asym *, struct dbgcv *, struct cv_counters * );

/* field enumeration callback, does:
 * - count number of members in a field list
 * - calculate the size LF_MEMBER records inside the field list
 * - create types ( array, structure ) if not defined yet
 */

static void cv_cntproc( struct dsym *type, struct asym *mbr, struct dbgcv *cv, struct cv_counters *cc )
{
    int numsize;
    uint_32 offset;
    uint_32 size = sizeof( CV_MEMBER );
    if ( Options.debug_symbols == CV_SIGNATURE_C7 )
	size = sizeof( CV_MEMBER_16t );

    cc->cnt++;
    offset = ( type->sym.typekind == TYPE_RECORD ? 0 : mbr->offset + cc->ofs );
    numsize = ( offset >= LF_NUMERIC ? sizeof( uint_32 ) : 0 );
    cc->size += ( size + 2 + numsize + mbr->name_size + 1 + 3 ) & ~3;

    /* field cv_typeref can only be queried from SYM_TYPE items! */

    if ( mbr->mem_type == MT_TYPE && mbr->type->cvtyperef == 0 ) {
	cv->level++;
	cv_write_type( cv, mbr->type );
	cv->level--;
    } else if ( mbr->mem_type == MT_BITS && mbr->cvtyperef == 0 ) {
	cv_write_bitfield( cv, type, mbr );
    }
    if ( mbr->isarray ) {
	/* temporarily (mis)use ext_idx1 member to store the type;
	 * this field usually isn't used by struct fields */
	mbr->ext_idx1 = cv->currtype;
	cv_write_array_type( cv, mbr, 0, USE16 );
    }
}

/* field enumeration callback, does:
 * - create LF_MEMBER record
 */

static void cv_memberproc( struct dsym *type, struct asym *mbr, struct dbgcv *cv, struct cv_counters *cc )
{
    uint_32 offset;
    uint_32 index;
    int typelen;
    uint_8 *tmp;
    uint_32 size = sizeof( CV_MEMBER );
    uint_32 leaf = LF_MEMBER;

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	size = sizeof( CV_MEMBER_16t );
	leaf = LF_MEMBER_16t;
    }

    offset = ( type->sym.typekind == TYPE_RECORD ? 0 : mbr->offset + cc->ofs );
    typelen = ( offset >= LF_NUMERIC ? sizeof( uint_32 ) : 0 );
    size = ( size + 2 + typelen + 1 + mbr->name_size + 3 ) & ~3;
    cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
    cv->pt_mbr->leaf = leaf;

    if ( mbr->isarray ) {
	index = mbr->ext_idx1;
	mbr->ext_idx1 = 0; /* reset the temporarily used field */
    } else
	index = GetTyperef( mbr, USE16 );

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	cv->pt_mbr->index = index;
	cv->pt_mbr->attr.flags = 0;
	cv->pt_mbr->attr.access = CV_public;
	cv->pt_mbr->attr.mprop= CV_MTvanilla;
	tmp = cv->pt_mbr->offset;
    } else {
	cv->t_mbr->index = index;
	cv->t_mbr->attr.flags = 0;
	cv->t_mbr->attr.access = CV_public;
	cv->t_mbr->attr.mprop= CV_MTvanilla;
	tmp = cv->t_mbr->offset;
    }

    if ( typelen == 0 ) {
	*(uint_16 *)tmp = offset;
	tmp += sizeof( uint_16 );
    } else {
	*(uint_16 *)tmp = LF_ULONG;
	tmp += sizeof( uint_16 );
	*(uint_32 *)tmp = offset;
	tmp += sizeof( uint_32 );
    }
    tmp = SetPrefixName( tmp, mbr->name, mbr->name_size );
    PadBytes( tmp, cv->types->e.seginfo->CodeBuffer );
    cv->pt += size;
}

/* field enumeration function.
 * The MS debug engine has problem with anonymous members (both members
 * and embedded structs).
 * If such a member is included, the containing struct can't be "opened".
 * The OW debugger and PellesC debugger have no such problems.
 * However, for Masm-compatibility, anonymous members are avoided and
 * anonymous struct members or embedded anonymous structs are "unfolded"
 * in this function.
 */
static void cv_enum_fields( struct dsym *sym, cv_enum_func enumfunc, struct dbgcv *cv, struct cv_counters *cc )
{
    unsigned i;
    struct sfield  *curr;
    for ( curr = sym->e.structinfo->head, i = 0; curr; curr = curr->next ) {
	if ( curr->sym.name_size ) { /* has member a name? */
	    enumfunc( sym, &curr->sym, cv, cc );
	} else if ( curr->sym.type ) { /* is member a type (struct, union, record)? */
	    cc->ofs += curr->sym.offset;
	    cv_enum_fields( (struct dsym *)curr->sym.type, enumfunc, cv, cc );
	    cc->ofs -= curr->sym.offset;
	} else if ( sym->sym.typekind == TYPE_UNION ) {
	    /* v2.11: include anonymous union members.
	     * to make the MS debugger work with those members, they must have a name -
	     * a temporary name is generated below which starts with "@@".
	     */
	    char *pold = curr->sym.name;
	    char tmpname[8];
	    curr->sym.name_size = sprintf( tmpname, "@@%u", ++i );
	    curr->sym.name = tmpname;
	    enumfunc( sym, &curr->sym, cv, cc );
	    curr->sym.name = pold;
	    curr->sym.name_size = 0;
	}
    }
    return;
}

/* write a LF_PROCEDURE & LF_ARGLIST type for procedures */

static void cv_write_type_procedure( struct dbgcv *cv, struct asym *sym, int cnt )
{
    int size = sizeof( CV_PROCEDURE );
    int leaf = LF_PROCEDURE;
    int type = sizeof( CV_typ_t );
    CV_typ_t   *p_32;
    CV_typ16_t *p_16;
    struct dsym *param;

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	size = sizeof( CV_PROCEDURE_16t );
	leaf = LF_PROCEDURE_16t;
	type = sizeof( CV_typ16_t );
    }

    cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
    cv->pt_pr->size = size - sizeof(uint_16);
    cv->pt_pr->leaf = leaf;

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	cv->pt_pr->rvtype = cv_void.uvalue;
	cv->pt_pr->calltype = 0;
	cv->pt_pr->funcattr.all = 0;
	cv->pt_pr->parmcount = cnt;
	cv->pt_pr->arglist = ++cv->currtype;
    } else {
	cv->t_pr->rvtype = cv_void.uvalue;
	cv->t_pr->calltype = 0;
	cv->t_pr->funcattr.all = 0;
	cv->t_pr->parmcount = cnt;
	cv->t_pr->arglist = ++cv->currtype;
    }
    cv->pt += size;
    size = sizeof( CV_ARGLIST );
    leaf = LF_ARGLIST;
    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	size = sizeof( CV_ARGLIST_16t );
	leaf = LF_ARGLIST_16t;
    }

    size = size + cnt * type;
    cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
    cv->pt_al->size = size - sizeof(uint_16);
    cv->pt_al->leaf = leaf;

    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	cv->pt_al->count = cnt;
	p_16 = (CV_typ16_t *)(cv->pt + sizeof( CV_ARGLIST_16t ) );
    } else {
	cv->t_al->count = cnt;
	p_32 = (CV_typ_t *)(cv->pt + sizeof( CV_ARGLIST ) );
    }

    /* fixme: order might be wrong ( is "push" order ) */
    for ( param = ((struct dsym *)sym)->e.procinfo->paralist; param; param = param->nextparam ) {
	if ( ModuleInfo.defOfssize == USE64 )
	    p_32[--cnt] = param->sym.ext_idx1;
	else
	    *p_16++ = param->sym.ext_idx1;
    }
    cv->pt += size;
    cv->currtype++;
}

/* write a type. Items are dword-aligned,
 *    cv: debug state
 *   sym: type to dump
 */

static void cv_write_type( struct dbgcv *cv, struct asym *sym )
{
    struct dsym *type = (struct dsym *)sym;
    uint_8	*tmp;
    int		namesize;
    int		typelen;
    int		size;
    int		leaf;
    CV_prop_t	property;
    struct cv_counters count;

    /* v2.10: handle typedefs. when the types are enumerated,
     * typedefs are ignored.
     */
    if ( sym->typekind == TYPE_TYPEDEF ) {

	if ( sym->mem_type == MT_PTR ) {
#if GENPTRTYPE
	    /* for untyped void pointers use ONE generic definition */
	    if ( !sym->isfar ) {
		if ( cv->ptrtype[sym->Ofssize] ) {
		    sym->cv_typeref = cv->ptrtype[sym->Ofssize];
		    return;
		}
		cv->ptrtype[sym->Ofssize] = cv->currtype;
	    }
#endif
	    if ( sym->ptr_memtype != MT_PROC && sym->target_type && sym->target_type->cvtyperef == 0 ) {
		if ( cv->level == 0 ) /* avoid circles */
		    cv_write_type( cv, sym->target_type );
	    }
	    sym->cvtyperef = cv_write_ptr_type( cv, sym );
	}
	return;
    } else if ( sym->typekind == TYPE_NONE ) {
	return;
    }

    property.flags = 0;
    typelen = ( sym->total_size >= LF_NUMERIC ? sizeof( uint_32 ) : 0 );
    if ( cv->level )
	property.isnested = 1;

    /* Count the member fields. If a member's type is unknown, create it! */
    count.cnt = 0;
    count.size = 0;
    count.ofs = 0;
    cv_enum_fields( type, cv_cntproc, cv, &count );

    /* WinDbg wants embedded structs to have a name - else it won't allow to "open" it. */
    namesize = ( sym->name_size ? sym->name_size : 9 );	 /* 9 is sizeof("__unnamed") */

    sym->cvtyperef = cv->currtype++;
    switch ( type->sym.typekind ) {
    case TYPE_UNION:
	size = sizeof( CV_UNION );
	leaf = LF_UNION;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    size = sizeof( CV_UNION_16t );
	    leaf = LF_UNION_16t;
	}
	size = ( size + typelen + 1 + namesize + 3 ) & ~3;
	cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
	cv->pt_un->size = size - sizeof(uint_16);
	cv->pt_un->leaf = leaf;
	cv->pt_un->count = count.cnt;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    cv->pt_un->field = cv->currtype++;
	    cv->pt_un->property = property;
	    tmp = cv->pt_un->data;
	} else {
	    cv->t_un->field = cv->currtype++;
	    cv->t_un->property = property;
	    tmp = cv->t_un->data;
	}
	break;
    case TYPE_RECORD:
	property.packed = 1; /* is "packed" */
	/* no break */
    case TYPE_STRUCT:
	size = sizeof( CV_STRUCT );
	leaf = LF_STRUCTURE;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    size = sizeof( CV_STRUCT_16t );
	    leaf = LF_STRUCTURE_16t;
	}
	size = ( size + typelen + 1 + namesize + 3 ) & ~3;
	cv->pt = checkflush( cv->types, cv->pt, size, cv->param );
	cv->pt_st->size = size - sizeof(uint_16);
	cv->pt_st->leaf = leaf;
	cv->pt_st->count = count.cnt;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    cv->pt_st->field = cv->currtype++;
	    cv->pt_st->property = property;
	    cv->pt_st->derived = 0;
	    cv->pt_st->vshape = 0;
	    tmp = cv->pt_st->data;
	} else {
	    cv->t_st->field = cv->currtype++;
	    cv->t_st->property = property;
	    cv->t_st->derived = 0;
	    cv->t_st->vshape = 0;
	    tmp = cv->t_st->data;
	}
	break;
    }
    if ( typelen ) {
	((struct leaf32 *)tmp)->leaf = LF_ULONG;
	((struct leaf32 *)tmp)->value32 = sym->total_size;
	tmp += sizeof( struct leaf32 );
    } else {
	*(uint_16 *)tmp = sym->total_size;
	tmp += sizeof( uint_16 );
    }
    tmp = SetPrefixName( tmp, sym->name_size ? sym->name : "__unnamed", namesize );
    PadBytes( tmp, cv->types->e.seginfo->CodeBuffer );
    cv->pt += size;

    /* write the fieldlist record */
    cv->pt = checkflush( cv->types, cv->pt, sizeof( CV_FIELDLIST ), cv->param );
    size = sizeof(CV_FIELDLIST) + count.size;
    cv->pt_fl->size = size - sizeof(uint_16);
    cv->pt_fl->leaf = LF_FIELDLIST_16t;
    cv->pt += sizeof( CV_FIELDLIST );

    /* add the struct's members to the fieldlist. */
    count.ofs = 0;
    cv_enum_fields( type, cv_memberproc, cv, &count );
    return;
}

/* get register values for S_REGISTER */

uint_16 cv_get_register( struct asym *sym )
{
    uint_16 regno;
    uint_16 rc = 0;
    unsigned flags;
    int i;

    for ( i = 0; i < 2; i++ ) {
	if ( sym->regist[i] ) {
	    flags = GetValueSp( sym->regist[i] );
	    regno = 1 + GetRegNo( sym->regist[i] );
	    if ( flags & OP_R16 )
		regno += 8;
	    else if ( flags & OP_R32 )
		regno += 16;
	    else if ( flags & OP_SR )
		regno += 24;
	    rc |= regno << ( i * 8 );
	}
    }
    return( rc );
}

/*
 *  convert register number
 *	  0  1	2  3  4	 5  6  7
 * -----------------------------
 * from: ax cx dx bx sp bp si di
 *   to: ax bx cx dx si di bp sp
 */
static const uint_8 reg64[] = { 0, 2, 3, 1, 7, 6, 4, 5 };

uint_16 cv_get_x64_regno( uint_16 regno )
{
    if ( regno >= T_RAX && regno <= T_RDI )
	return( reg64[ regno - T_RAX ] + CV_AMD64_RAX );
    if ( regno >= T_R8 && regno <= T_R15 )
	return( regno - T_R8 + CV_AMD64_RAX + 8 );
    /* it's a 32-bit register r8d-r15d */
    return( regno - T_R8D + CV_AMD64_R8D );

}

/* write a symbol
 *    cv: debug state
 *   sym: symbol to write
 * the symbol has either state SYM_INTERNAL or SYM_TYPE.
 */

static void cv_write_symbol( struct dbgcv *cv, struct asym *sym )
{
    int		len;
    unsigned	ofs;
    enum fixup_types rlctype;
    uint_8	Ofssize;
    struct fixup *fixup;
    struct dsym *proc;
    struct dsym *lcl;
    int		i,j,k;
    int		cnt[2];
    struct	dsym *locals[2];
    struct	dsym *q;
    int		size;
    int		leaf;
    uint_16	typeref;

    Ofssize = GetSymOfssize( sym );
    len = GetCVStructLen( sym, Ofssize );
    cv->ps = checkflush( cv->symbols, cv->ps, 1 + sym->name_size + len, cv->param );

    if ( sym->state == SYM_TYPE ) {
	/* Masm does only generate an UDT for typedefs
	 * if the underlying type is "used" somewhere.
	 * example:
	 * LPSTR typedef ptr BYTE
	 * will only generate an S_UDT for LPSTR if either
	 * "LPSTR" or "ptr BYTE" is used in the source.
	 */
	size = sizeof( UDTSYM );
	leaf = S_UDT;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    size = sizeof( UDTSYM_16t );
	    leaf = S_UDT_16t;
	}
	cv->ps_udt->reclen = size - sizeof(uint_16) + sym->name_size;
	cv->ps_udt->rectyp = leaf;
	/* v2.10: pointer typedefs will now have a cv_typeref */

	if ( sym->cvtyperef )
	    leaf = sym->cvtyperef;
	else
	    leaf = GetTyperef( sym, Ofssize );

	if ( Options.debug_symbols == CV_SIGNATURE_C7 )
	    cv->ps_udt->typind = leaf;
	else
	    cv->s_udt->typind = leaf;

	/* Some typedefs won't get a valid type (<name> TYPEDEF PROTO ...).
	 * In such cases just skip the type!
	 */
	if ( leaf ) {
	    cv->ps += len;
	    cv->ps = SetPrefixName( cv->ps, sym->name, sym->name_size );
	}
	return;
    }

    /* rest is SYM_INTERNAL */
    /* there are 3 types of INTERNAL symbols:
     * - numeric constants ( equates, memtype MT_EMPTY )
     * - code labels, memtype == MT_NEAR | MT_FAR
     *	 - procs
     *	 - simple labels
     * - data labels, memtype != MT_NEAR | MT_FAR
     */

    if ( sym->isproc && Options.debug_ext >= CVEX_REDUCED ) { /* v2.10: no locals for -Zi0 */

	proc = (struct dsym *)sym;

	/* for PROCs, scan parameters and locals and create their types. */

	/* scan local symbols */

	locals[0] = proc->e.procinfo->paralist;
	locals[1] = proc->e.procinfo->locallist;
	for ( i = 0; i < 2; i++ ) {
	    if ( ModuleInfo.defOfssize == USE64 && i == 0 ) {
		for ( j = 0, lcl = locals[0]; lcl; q = lcl, lcl = lcl->nextparam )
		    j++;
	    }
	    cnt[i] = 0;
	    for ( lcl = locals[i]; lcl; lcl = lcl->nextparam ) {
		if ( !( ModuleInfo.defOfssize == USE64 && i == 0 ) )
		    q = lcl;
		cnt[i]++;
		typeref = ( q->sym.mem_type == MT_PTR ? cv_write_ptr_type( cv, &q->sym ) : GetTyperef( &q->sym, Ofssize ) );
		if ( q->sym.isarray ) {
		    cv_write_array_type( cv, &q->sym, typeref, Ofssize );
		    typeref = cv->currtype - 1;
		}
		q->sym.ext_idx1 = typeref;
		if ( ModuleInfo.defOfssize == USE64 && i == 0 ) {
		    for ( k = 1, j--, q = locals[0]; j > k; k++, q = q->nextparam );
		}
	    }
	}

	if ( Ofssize == USE16 ) {
	    PROCSYM16 *p = cv->ps_p16;
	    p->reclen = sizeof( PROCSYM16 ) - sizeof(uint_16) + sym->name_size;
	    p->rectyp = (sym->ispublic ? S_GPROC16 : S_LPROC16);
	    p->pParent = 0;  /* filled by CVPACK */
	    p->pEnd = 0;     /* filled by CVPACK */
	    p->pNext = 0;    /* filled by CVPACK */
	    p->len = sym->total_size;
	    p->DbgStart = ((struct dsym *)sym)->e.procinfo->size_prolog;
	    p->DbgEnd = sym->total_size;
	    p->off = 0;
	    p->seg = 0;
	    p->typind = cv->currtype; /* typeref LF_PROCEDURE */
	    p->flags.bAll = ( sym->mem_type == MT_FAR ? CV_PROCF_FAR : 0 );
	    rlctype = FIX_PTR16;
	    ofs = offsetof( PROCSYM16, off );
	} else {
	    PROCSYM32_16t *p = cv->ps_p32;
	    size = sizeof( PROCSYM32 );
	    leaf = (sym->ispublic ? S_GPROC32 : S_LPROC32 );
	    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
		size = sizeof( PROCSYM32_16t );
		leaf = ( sym->ispublic ? S_GPROC32_16t : S_LPROC32_16t );
	    }
	    p->reclen	= size - sizeof(uint_16) + sym->name_size;
	    p->rectyp	= leaf;
	    p->pParent	= 0; /* filled by CVPACK */
	    p->pEnd	= 0; /* filled by CVPACK */
	    p->pNext	= 0; /* filled by CVPACK */
	    p->len	= sym->total_size;
	    p->DbgStart = ((struct dsym *)sym)->e.procinfo->size_prolog;
	    p->DbgEnd	= sym->total_size;
	    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
		p->off = 0;
		p->seg = 0;
		p->typind = cv->currtype; /* typeref LF_PROCEDURE */
		p->flags.bAll = 0;
		if ( sym->mem_type == MT_FAR )
		    p->flags.CV_PFLAG_FAR = 1;
		if ( proc->e.procinfo->fpo )
		    p->flags.CV_PFLAG_NOFPO = 1;
		ofs = offsetof( PROCSYM32_16t, off );
	    } else {
		cv->s_p32->off = 0;
		cv->s_p32->seg = 0;
		cv->s_p32->typind = cv->currtype;
		cv->s_p32->flags.bAll = 0;
		if ( sym->mem_type == MT_FAR )
		    cv->s_p32->flags.CV_PFLAG_FAR = 1;
		if ( proc->e.procinfo->fpo )
		    cv->s_p32->flags.CV_PFLAG_NOFPO = 1;
		ofs = offsetof( PROCSYM32, off );
	    }
	    rlctype = FIX_PTR32;
	}
	cv_write_type_procedure( cv, sym, cnt[0] );

    } else if ( sym->mem_type == MT_NEAR || sym->mem_type == MT_FAR ) {

	if ( Ofssize == USE16 ) {
	    cv->ps_l16->reclen = sizeof( LABELSYM16 ) - sizeof(uint_16) + sym->name_size;
	    cv->ps_l16->rectyp = S_LABEL16;
	    cv->ps_l16->off = 0;
	    cv->ps_l16->seg = 0;
	    cv->ps_l16->flags.bAll = ( sym->mem_type == MT_FAR ? CV_PROCF_FAR : 0 );
	    rlctype = FIX_PTR16;
	    ofs = offsetof( LABELSYM16, off );
	} else {
	    cv->ps_l32->reclen = sizeof( LABELSYM32 ) - sizeof(uint_16) + sym->name_size;
	    if ( Options.debug_symbols == CV_SIGNATURE_C7 )
		cv->ps_l32->rectyp = S_LABEL32_ST;
	    else
		cv->ps_l32->rectyp = S_LABEL32;
	    cv->ps_l32->off = 0;
	    cv->ps_l32->seg = 0;
	    cv->ps_l32->flags.bAll = ( sym->mem_type == MT_FAR ? CV_PROCF_FAR : 0 );
	    rlctype = FIX_PTR32;
	    ofs = offsetof( LABELSYM32, off );
	}
#if EQUATESYMS
    } else if ( sym->isequate ) {
	cv->ps_con->reclen = len - sizeof(uint_16) + 1 + sym->name_size;
	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    cv->ps_con->rectyp = S_CONSTANT_16t;
	    cv->ps_con->typind = cv_abs_type.uvalue;
	    if ( sym->value >= LF_NUMERIC ) {
		uint_8 *tmp = cv->ps_con->name;
		cv->ps_con->value = LF_ULONG;
		*(uint_32 *)tmp = sym->value;
	    } else
		cv->ps_con->value = sym->value;
	} else {
	    cv->s_con->rectyp = S_CONSTANT;
	    cv->s_con->typind = cv_abs_type.uvalue;
	    if ( sym->value >= LF_NUMERIC ) {
		uint_8 *tmp = cv->s_con->name;
		cv->s_con->value = LF_ULONG;
		*(uint_32*)tmp = sym->value;
	    } else
		cv->s_con->value = sym->value;
	}
	cv->ps += len;
	cv->ps = SetPrefixName( cv->ps, sym->name, sym->name_size );
	return;
#endif
    } else {
	/* v2.10: set S_GDATA[16|32] if symbol is public */
	uint_32 typeref;

	if ( sym->isarray ) {
	    typeref = cv->currtype;
	    cv_write_array_type( cv, sym, 0, Ofssize );
	} else
	    typeref = GetTyperef( sym, Ofssize );

	if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
	    size = sizeof( DATASYM32_16t );
	    cv->ps_d32->off = 0;
	    cv->ps_d32->seg = 0;
	    cv->ps_d32->typind = typeref;
	    ofs = offsetof( DATASYM32_16t, off );
	} else {
	    size = sizeof( DATASYM32 );
	    cv->s_d32->off = 0;
	    cv->s_d32->seg = 0;
	    cv->s_d32->typind = typeref;
	    ofs = offsetof( DATASYM32, off );
	}
	cv->ps_d32->reclen = size - sizeof(uint_16) + sym->name_size;
	if ( Ofssize == USE16 ) {
	    leaf = (sym->ispublic ? S_GDATA16 : S_LDATA16 );
	    rlctype = FIX_PTR16;
	} else {
	    if ( ( ModuleInfo.cv_opt & CVO_STATICTLS ) && ((struct dsym *)sym->segment)->e.seginfo->clsym &&
		strcmp( ((struct dsym *)sym->segment)->e.seginfo->clsym->name, "TLS" ) == 0 )
		leaf = (sym->ispublic ? S_GTHREAD32_16t : S_LTHREAD32_16t );
	    else if ( Options.debug_symbols == CV_SIGNATURE_C7 )
		leaf = (sym->ispublic ? S_GDATA32_16t : S_LDATA32_16t );
	    else
		leaf = (sym->ispublic ? S_GDATA32 : S_LDATA32 );
	    rlctype = FIX_PTR32;
	}
	cv->ps_d32->rectyp = leaf;
    }
    cv->ps += ofs;
    cv->symbols->e.seginfo->current_loc = cv->symbols->e.seginfo->start_loc + ( cv->ps - cv->symbols->e.seginfo->CodeBuffer );
    if ( Options.output_format == OFORMAT_COFF ) {
	/* COFF has no "far" fixups. Instead Masm creates a
	 * section-relative fixup + a section fixup.
	 */
	fixup = CreateFixup( sym, FIX_OFF32_SECREL, OPTJ_NONE );
	fixup->locofs = cv->symbols->e.seginfo->current_loc;
	store_fixup( fixup, cv->symbols, (int_32 *)cv->ps );
	fixup = CreateFixup( sym, FIX_SEG, OPTJ_NONE );
	fixup->locofs = cv->symbols->e.seginfo->current_loc + ( rlctype == FIX_PTR32 ? sizeof( int_32 ) : sizeof ( int_16 ) );
	store_fixup( fixup, cv->symbols, (int_32 *)cv->ps );
    } else {
	fixup = CreateFixup( sym, rlctype, OPTJ_NONE );
	fixup->locofs = cv->symbols->e.seginfo->current_loc;
	/* todo: for OMF, delay fixup store until checkflush has been called! */
	store_fixup( fixup, cv->symbols, (int_32 *)cv->ps );
    }
    cv->ps += len - ofs;
    cv->ps = SetPrefixName( cv->ps, sym->name, sym->name_size );

    /* for PROCs, scan parameters and locals.
     * to mark the block's end, write an ENDBLK item.
     */
    if ( sym->isproc && Options.debug_ext >= CVEX_REDUCED ) { /* v2.10: no locals for -Zi0 */

	/* scan local symbols again */
	for ( i = 0; i < 2 ; i++ ) {
	    if ( ModuleInfo.defOfssize == USE64 && i == 0 ) {
		for ( j = 0, q = locals[0]; q; lcl = q, q = q->nextparam )
		    j++;
	    }
	    for ( q = locals[i]; q; q = q->nextparam ) {
		if ( !( ModuleInfo.defOfssize == USE64 && i == 0 ) )
		    lcl = q;
		/* FASTCALL register argument? */
		if ( lcl->sym.state == SYM_TMACRO ) {
		    len = sizeof( REGSYM ) - 1;
		    leaf = S_REGISTER;
		    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
			len = sizeof( REGSYM_16t ) - 1;
			leaf = S_REGISTER_16t;
		    }
		    cv->ps = checkflush( cv->symbols, cv->ps, 1 + lcl->sym.name_size + len, cv->param );
		    cv->ps_reg->reclen = len - sizeof(uint_16) + 1 + lcl->sym.name_size;
		    cv->ps_reg->rectyp = leaf;
		    if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
			cv->ps_reg->typind = lcl->sym.ext_idx1;
			cv->ps_reg->reg = cv_get_register( &lcl->sym );
		    } else {
			cv->s_reg->typind = lcl->sym.ext_idx1;
			cv->s_reg->reg = cv_get_register( &lcl->sym );
		    }
		} else if ( Ofssize == USE16 ) {
		    len = sizeof( BPRELSYM16 ) - 1;
		    cv->ps = checkflush( cv->symbols, cv->ps, 1 + lcl->sym.name_size + len, cv->param );
		    cv->ps_br16->reclen = sizeof( BPRELSYM16 ) - sizeof(uint_16) + lcl->sym.name_size;
		    cv->ps_br16->rectyp = S_BPREL16;
		    cv->ps_br16->off = lcl->sym.offset;
		    cv->ps_br16->typind = lcl->sym.ext_idx1;
		} else {
		    /* v2.11: use S_REGREL if 64-bit or frame reg != [E|BP */
		    if ( Ofssize == USE64 || ( GetRegNo( proc->e.procinfo->basereg ) != 5 )) {
			len = sizeof( REGREL32 ) - 1;
			leaf = S_REGREL32;
			if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
			    len = sizeof( REGREL32_16t ) - 1;
			    leaf = S_REGREL32_16t;
			}
			cv->ps = checkflush( cv->symbols, cv->ps, 1 + lcl->sym.name_size + len, cv->param );
			cv->ps_rr32->reclen = len - sizeof(uint_16) + 1 + lcl->sym.name_size;
			cv->ps_rr32->rectyp = leaf;
			/* x64 register numbers are different */
			if ( SpecialTable[ proc->e.procinfo->basereg ].cpu == P_64 )
			    size = cv_get_x64_regno( proc->e.procinfo->basereg );
			else
			    size = GetRegNo( proc->e.procinfo->basereg ) + CV_REG_EAX;
			if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
			    cv->ps_rr32->off = lcl->sym.offset;
			    cv->ps_rr32->typind = lcl->sym.ext_idx1;
			    cv->ps_rr32->reg = size;
			} else {
			    cv->s_rr32->off = lcl->sym.offset;
			    cv->s_rr32->typind = lcl->sym.ext_idx1;
			    cv->s_rr32->reg = size;
			}
		    } else {
			len = sizeof( BPRELSYM32 ) - 1;
			leaf = S_BPREL32;
			if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
			    len = sizeof( BPRELSYM32_16t ) - 1;
			    leaf = S_BPREL32_16t;
			}
			cv->ps = checkflush( cv->symbols, cv->ps, 1 + lcl->sym.name_size + len, cv->param );
			cv->ps_br32->reclen = len + 1 - sizeof(uint_16) + lcl->sym.name_size;
			cv->ps_br32->rectyp = leaf;
			if ( Options.debug_symbols == CV_SIGNATURE_C7 ) {
			    cv->ps_br32->off = lcl->sym.offset;
			    cv->ps_br32->typind = lcl->sym.ext_idx1;
			} else {
			    cv->s_br32->off = lcl->sym.offset;
			    cv->s_br32->typind = lcl->sym.ext_idx1;
			}
		    }
		}
		lcl->sym.ext_idx1 = 0; /* to be safe, clear the temp. used field */
		cv->ps += len;
		cv->ps = SetPrefixName( cv->ps, lcl->sym.name, lcl->sym.name_size );
		if ( ModuleInfo.defOfssize == USE64 && i == 0 )
		    for ( k = 1, j--, lcl = locals[0]; j > k; k++, lcl = lcl->nextparam );
	    }
	}

	cv->ps = checkflush( cv->symbols, cv->ps, sizeof( ENDARGSYM ), cv->param );
	cv->ps_eb->reclen = sizeof( ENDARGSYM ) - sizeof(uint_16);
	cv->ps_eb->rectyp = S_END;
	cv->ps += sizeof( ENDARGSYM );
    }
    return;
}

#define cv_align(p, base) for( ; ( p - base ) & 3; ) *p++ = NULLC

static void cv_write_F3( dbgcv *cv )
{
    char buffer[_MAX_PATH];
    char *name;
    int i,o,len;
    uint_8 *p;
    CV_SECTION *h = cv->sec;

    h->signature = 0x000000F3;
    p = cv->ps + sizeof(CV_SECTION);
    *p++ = '\0';

    for( o = 1, i = 0; i < ModuleInfo.g.cnt_fnames; i++ ) {

	cv->files[i].offset = o;
	name = cv->files[i].name;
	if ( name[0] != '\\' && name[0] != '.' && name[1] != ':' ) {

	    strcpy( buffer, cv->currdir );
	    strcat( buffer, "\\" );
	    name = strcat( buffer, name );
	}
	len = strlen( name ) + 1;
	memcpy( p, name, len );
	p += len;
	o += len;
    }
    h->length = o;
    cv_align(p, cv->symbols->e.seginfo->CodeBuffer);
    cv->ps = p;
}

#define USEMD5

#ifdef USEMD5

#define BUFSIZ 1024*4

static int calc_md5(const char *const filename, unsigned char *sum)
{
    FILE *fp;
    uint_8 *file_buf;
    MD5_CTX ctx;
    size_t i;

    fp = fopen(filename, "rb");
    if (!fp)
      return 0;

    file_buf = MemAlloc(BUFSIZ);
    MD5Init(&ctx);
    while (!feof(fp)) {
	i = fread(file_buf, 1, BUFSIZ, fp);
	if (ferror(fp)) {
	    fclose(fp);
	    return 0;
	} else if (i == 0)
	    break;
	MD5Update(&ctx, file_buf, i);
    }
    MD5Final(&ctx);
    memcpy(sum, ctx.digest, 16);
    MemFree(file_buf);
    fclose(fp);
    return 1;
}

#endif

static void cv_write_F4( dbgcv *cv )
{

    int i,o,len;
    uint_8 *p;

    CV_SECTION *h = cv->sec;

    h->signature = 0x000000F4;
    p = cv->ps + sizeof(CV_SECTION);

    for( o = 0, i = 0; i < ModuleInfo.g.cnt_fnames; i++ ) {

	*(uint_32 *)p = cv->files[i].offset;
	cv->files[i].offset = o;
	p += sizeof(uint_32);
#ifdef USEMD5
	*(uint_16*)p = 0x0110;
	p += sizeof(uint_16);
	calc_md5(cv->files[i].name, p);
	p += 16;
	*(uint_16*)p = 0x0000;
	p += sizeof(uint_16);
	o += 2 + 16 + 2 + sizeof(uint_32);
#else
	*(uint_32 *)p = 0x00000000;
	p += sizeof(uint_32);
	o += sizeof(uint_32)*2;
#endif
    }
    h->length = o;
    cv->ps = p;
}


static void cv_write_F2( dbgcv *cv )
{
    struct dsym *seg;
    struct line_num_info *o, *q;
    CV_SECTION *h = NULL;
    CV_DebugSLinesHeader_t *pLinesHeader;
    CV_DebugSLinesFileBlockHeader_t *s;
    CV_Line_t *pLineCur;
    CV_Line_t *pLinePrev;
    int offset;

    for( seg = SymTables[TAB_SEG].head; seg; seg = seg->next ) {
	if ( seg->e.seginfo->LinnumQueue ) {

	    h = cv->sec;
	    cv->ps += sizeof( CV_SECTION );
	    pLinesHeader = (CV_DebugSLinesHeader_t *)cv->ps;
	    cv->ps += sizeof( CV_DebugSLinesHeader_t );

	    h->signature = 0x000000F2;
	    h->length = sizeof( CV_DebugSLinesHeader_t );

	    pLinesHeader->offCon = 0;
	    pLinesHeader->segCon = 0;
	    pLinesHeader->flags	 = 0;
	    pLinesHeader->cbCon	 = seg->sym.max_offset;

	    o = (struct line_num_info *)((struct qdesc *)seg->e.seginfo->LinnumQueue)->head;
	    while ( o ) {

		int fileStart = o->srcfile;
		if ( o->number == 0 )
		    fileStart = o->file;

		s = (CV_DebugSLinesFileBlockHeader_t *)cv->ps;

		cv->ps += sizeof( CV_DebugSLinesFileBlockHeader_t );
		h->length += sizeof( CV_DebugSLinesFileBlockHeader_t );

		s->offFile = cv->files[fileStart].offset;
		s->nLines  = 0;
		s->cbBlock = 12;
		pLinePrev  = NULL;

		for ( ; o; o = o->next ) {

		    int fileCur, linenum;

		    if ( o->number == 0 ) {
			fileCur = o->file;
			linenum = o->line_number;
			offset	= o->sym->offset;
		    } else {
			fileCur = o->srcfile;
			linenum = o->number;
			offset	= o->line_number;
		    }
		    if ( fileStart != fileCur )
			break;

		    if ( pLinePrev )  {
			if ( offset < pLinePrev->offset )
			    continue;
			if ( offset == pLinePrev->offset &&
			    linenum == pLinePrev->linenumStart )
			    continue;
		    }

		    pLineCur = (CV_Line_t *)cv->ps;
		    cv->ps += sizeof( CV_Line_t );
		    h->length += sizeof( CV_Line_t );
		    s->nLines++;
		    s->cbBlock += 8;

		    pLineCur->offset = offset;
		    pLineCur->linenumStart = linenum;
		    pLineCur->deltaLineEnd = 0;
		    pLineCur->fStatement = 1;
		    pLinePrev = pLineCur;
		}
	    }
	}
    }
    if ( h != NULL ) {
	cv_align(cv->ps, cv->symbols->e.seginfo->CodeBuffer);
    }
}

static void cv_write_F1( dbgcv *cv )
{
    char buffer[_MAX_PATH];
    char *name = CurrFName[OBJ];
    char *p,*s;
    int q, len;
    COMPILESYM3 *cp;
    ENVBLOCKSYM *e;

    if ( name[0] != '\\' && name[1] != ':' ) {
	strcpy( buffer, cv->currdir );
	strcat( buffer, "\\" );
	name = strcat( buffer, name );
    }
    len = strlen( name );
    cv->s_on->reclen = sizeof( OBJNAMESYM ) - sizeof(unsigned short) + len;
    cv->s_on->rectyp = S_OBJNAME;
    cv->s_on->signature = 0;
    cv->ps += sizeof( OBJNAMESYM ) - 1;
    cv->ps = SetPrefixName( cv->ps, name, len );

    cp = cv->s_cp;
    cp->rectyp = S_COMPILE3;

    len = strlen( strcpy( cp->verSz, szCVCompiler ) );
    cp->reclen = sizeof( COMPILESYM3 ) - sizeof( unsigned short ) + len + 1;
    cv->ps += cp->reclen + sizeof( unsigned short ) - 1;
    *cv->ps++ = '\0';

    cp->flags = 0;
    cp->iLanguage = CV_CFL_MASM;
    if ( ModuleInfo.Ofssize == USE64 )
	cp->machine = CV_CFL_X64;
    else
	cp->machine = ( ( ModuleInfo.curr_cpu & P_CPU_MASK ) >> 4 );

    cp->verFEMajor  = 0;    /* front end */
    cp->verFEMinor  = 0;
    cp->verFEBuild  = 0;
    cp->verFEQFE    = 0;
    cp->verMajor    = 0;    /* back end */
    cp->verMinor    = 0;
    cp->verBuild    = 0;
    cp->verQFE	    = 0;

    e = cv->s_env;
    e->flags = 0;
    e->rectyp = S_ENVBLOCK;
    s = e->rgsz;

    /* pairs of 0-terminated strings - keys/values */

    /* cwd - current working directory */

    q = strlen( cv->currdir );
    s = strcpy( s, "cwd" ) + 4;
    s = strcpy( s, cv->currdir ) + q + 1;

    /* exe - full path of executable */

    s = strcpy( s, "exe" ) + 4;
    len = strlen( _pgmptr ) + 1;
    s = strcpy( s, _pgmptr ) + len;

    /* src - relative path to source (from cwd) */

    s = strcpy( s, "src" ) + 4;

    p = cv->files[0].name;
    if ( _memicmp( p, cv->currdir, q ) == 0 )
	p += q + 1;

    len = strlen( p ) + 1;
    s = strcpy( s, p ) + len;
    *s++ = '\0';
    e->reclen = (unsigned short)( s - cv->ps - 2 );
    cv->ps = s;
}

/* option -Zi: write debug symbols and types
 * - symbols: segment $$SYMBOLS (OMF) or .debug$S (COFF)
 * - types:   segment $$TYPES (OMF) or .debug$T (COFF)
 * field Options.debug_symbols contains the format version
 * which is to be generated (CV4_, CV5_ or CV8_SIGNATURE)
 */

extern struct asym *CV8Label;

static int compare_syms( const void *p1, const void *p2 )
{
    if ( strcmp( "mainCRTStartup", (*(struct asym * *)p2)->name ) == 0 )
	return 1;
    return( strcmp( (*(struct asym * *)p2)->name, (*(struct asym * *)p1)->name ) );
}

void cv_write_debug_tables( struct dsym *symbols, struct dsym *types, void *pv )
{
    struct asym *sym;
    struct asym **syms;
    int SymCount;
    int i;
    int len;
    char *objname;
    struct dbgcv cv;
    CV_SECTION *h;
    struct fixup *fixup;
    int_32 lineTable;

    cv.ps = symbols->e.seginfo->CodeBuffer;
    cv.pt = types->e.seginfo->CodeBuffer;
    cv.symbols = symbols;
    cv.types = types;
    cv.currtype = 0x1000; /* user-defined types start at 0x1000 */
    cv.level = 0;
    cv.param = pv;
#if GENPTRTYPE
    cv.ptrtype[0] = 0;
    cv.ptrtype[1] = 0;
    cv.ptrtype[2] = 0;
#endif
    /* init types */
    *(uint_32 *)cv.pt = Options.debug_symbols; /* "signature" */
    cv.pt += sizeof( uint_32 );

    /* init symbols */
    *(uint_32 *)cv.ps = Options.debug_symbols; /* "signature" */
    cv.ps += sizeof(uint_32);

    if ( Options.debug_symbols == CV_SIGNATURE_C13 ) {

	cv.files = MemAlloc( ModuleInfo.g.cnt_fnames * sizeof( cv_file ) );
	cv.currdir = MemAlloc( _MAX_PATH );
	_getcwd( cv.currdir, _MAX_PATH );

	for( i = 0; i < ModuleInfo.g.cnt_fnames; i++ ) {
	    cv.files[i].name = ModuleInfo.g.FNames[i];
	    cv.files[i].offset = 0;
	}
	h = cv.sec;
	cv_write_F3( &cv );
	cv_write_F4( &cv );
	lineTable = (uint_32)(cv.ps - (uint_8 *)h + sizeof(CV_SECTION) + 4);
	if ( CV8Label ) {
	    int_32 data = 0;
	    fixup = CreateFixup( CV8Label, FIX_OFF32_SECREL, OPTJ_NONE );
	    fixup->locofs = lineTable;
	    store_fixup( fixup, cv.symbols, &data );
	    fixup = CreateFixup( CV8Label, FIX_SEG, OPTJ_NONE );
	    fixup->locofs = lineTable + 4;
	    store_fixup( fixup, cv.symbols, &data );
	}
	cv_write_F2( &cv );

	h = cv.sec;
	h->signature = 0x000000F1;
	cv.ps += sizeof(CV_SECTION);
	cv_write_F1( &cv );

    } else {

	/* 1. symbol record: object name */
	objname = CurrFName[OBJ];
	for ( i = strlen( objname ); i; i-- )
	    if ( *(objname+i-1) == '/' || *(objname+i-1) == '\\' )
		break;
	objname += i;
	len = strlen( objname );
	cv.ps_on->reclen = sizeof( OBJNAMESYM ) - sizeof(uint_16) + len;
	cv.ps_on->rectyp = S_OBJNAME_ST;
	cv.ps_on->signature = 1;
	cv.ps += sizeof( OBJNAMESYM ) - 1;
	cv.ps = SetPrefixName( cv.ps, objname, len );

	/* 2. symbol record: compiler */
	len = strlen( szCVCompiler );
	cv.ps_cp->reclen = sizeof( CFLAGSYM ) - sizeof(uint_16) + len;
	cv.ps_cp->rectyp = S_COMPILE;
	/* v2.11: use a valid 64-bit value */
	cv.ps_cp->machine = ( ModuleInfo.defOfssize == USE64 ? CV_CFL_X64 : ( ModuleInfo.curr_cpu & P_CPU_MASK ) >> 4 );
	/* 0 isnt possible, 1 is 8086 and 80186 */
	if ( cv.ps_cp->machine == 0 )
	    cv.ps_cp->machine = CV_CFL_8086;

	cv.ps_cp->language	= CV_CFL_MASM;
	cv.ps_cp->pcode		= 0;
	cv.ps_cp->floatprec	= 0;
	cv.ps_cp->floatpkg	= 0;
	cv.ps_cp->ambdata	= 0;
	cv.ps_cp->ambcode	= 0;
	cv.ps_cp->mode32	= 0;
	cv.ps_cp->pad		= 0;

	if ( ModuleInfo.model ) {
	    if ( ModuleInfo.model == MODEL_HUGE )
		cv.ps_cp->ambdata = CV_AMB_HUGE;
	    else
		cv.ps_cp->ambdata = ( SIZE_DATAPTR & ( 1 << ModuleInfo.model ) ? CV_AMB_FAR : CV_AMB_NEAR );
	    cv.ps_cp->ambcode = ( SIZE_CODEPTR & ( 1 << ModuleInfo.model ) ? CV_AMB_FAR : CV_AMB_NEAR );
	}
	cv.ps += sizeof( CFLAGSYM ) - 1;
	cv.ps = SetPrefixName( cv.ps, szCVCompiler, len );
    }

    /* scan symbol table for types */

    if ( Options.debug_symbols == CV_SIGNATURE_C13 ) {

	sym = NULL;
	SymCount = 0;
	while ( sym = SymEnum( sym, &i ) ) {
	    if ( sym->state == SYM_TYPE && sym->typekind != TYPE_TYPEDEF && sym->cvtyperef == 0 )
		SymCount++;
	}
	if ( SymCount ) {
	    if ( ( syms = (struct asym **)MemAlloc( SymCount * sizeof( struct asym * ) ) ) != NULL ) {
		SymCount = 0;
		while ( sym = SymEnum( sym, &i ) ) {
		    if ( sym->state == SYM_TYPE && sym->typekind != TYPE_TYPEDEF && sym->cvtyperef == 0 )
			syms[SymCount++] = sym;
		}
		qsort( syms, SymCount, sizeof( struct asym * ), compare_syms );
		for ( i = 0; i < SymCount; i++ )
		    cv_write_type( &cv, syms[i] );
		MemFree(syms);
	    }
	}
	SymCount = 0;
	while ( sym = SymEnum( sym, &i ) ) {
	    switch ( sym->state ) {
	    case SYM_TYPE:
		if ( Options.debug_ext < CVEX_NORMAL )
		    break;
	    case SYM_INTERNAL:
		if (
#if EQUATESYMS
		    ( Options.debug_ext < CVEX_MAX ? sym->isequate : sym->variable )
#else
		    sym->isequate
#endif
		    || sym->predefined ) {
		    break;
		}
		if ( sym != CV8Label )
		    SymCount++;
		break;
	    }
	}
	if ( SymCount ) {
	    if ( ( syms = (struct asym **)MemAlloc( SymCount * sizeof( struct asym * ) ) ) != NULL ) {
		SymCount = 0;
		while ( sym = SymEnum( sym, &i ) ) {
		    switch ( sym->state ) {
		    case SYM_TYPE:
			if ( Options.debug_ext < CVEX_NORMAL )
			    break;
		    case SYM_INTERNAL:
			if (
#if EQUATESYMS
			    ( Options.debug_ext < CVEX_MAX ? sym->isequate : sym->variable )
#else
			    sym->isequate
#endif
			    || sym->predefined ) {
			    break;
			}
			if ( sym != CV8Label )
			    syms[SymCount++] = sym;
			break;
		    }
		}
		qsort( syms, SymCount, sizeof( struct asym * ), compare_syms );
		for ( i = 0; i < SymCount; i++ )
		    cv_write_symbol( &cv, syms[i] );
		MemFree(syms);
	    }
	}
	h->length = (uint_32)(cv.ps - (uint_8 *)h - sizeof(CV_SECTION));
	cv_align(cv.ps, cv.symbols->e.seginfo->CodeBuffer);

    } else {

	sym = NULL;
	while ( sym = SymEnum( sym, &i ) ) {
	    if ( sym->state == SYM_TYPE && sym->typekind != TYPE_TYPEDEF && sym->cvtyperef == 0 ) {
		cv_write_type( &cv, sym );
	    }
	}

    /* scan symbol table for SYM_TYPE, SYM_INTERNAL */

	sym = NULL;
	while ( sym = SymEnum( sym, &i ) ) {
	    switch ( sym->state ) {
	    case SYM_TYPE: /* may create an S_UDT entry in the symbols table */
		if ( Options.debug_ext < CVEX_NORMAL ) /* v2.10: no UDTs for -Zi0 and -Zi1 */
		    break;
	    case SYM_INTERNAL:
		if (
#if EQUATESYMS
		    /* emit constants if -Zi3 */
		    ( Options.debug_ext < CVEX_MAX ? sym->isequate : sym->variable )
#else
		    sym->isequate
#endif
		    || sym->predefined ) { /* EQUates? */
		    break;
		}
		cv_write_symbol( &cv, sym );
		break;
	    }
	}
    }

    /* final flush for both types and symbols.
     * use 'fictional' size of SIZE_CV_SEGBUF!
     */
    checkflush( cv.types, cv.pt, SIZE_CV_SEGBUF, cv.param );
    checkflush( cv.symbols, cv.ps, SIZE_CV_SEGBUF, cv.param );

    types->sym.max_offset = types->e.seginfo->current_loc;
    types->e.seginfo->start_loc = 0; /* required for COFF */
    symbols->sym.max_offset = symbols->e.seginfo->current_loc;
    symbols->e.seginfo->start_loc = 0; /* required for COFF */
    return;
}

