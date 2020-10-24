#include <string.h>
#include <globals.h>
#include <hllext.h>
#include <atofloat.h>

int CreateFloat(int, struct expr *, char *);

int RetLineQueue( void )
{
    if ( ModuleInfo.list )
	LstWrite( LSTTYPE_DIRECTIVE, GetCurrOffset(), 0 );
    RunLineQueue();
    return NOT_ERROR;
}

int SizeFromExpression( struct expr *opnd )
{
    if ( opnd->mbr && opnd->mbr->state == SYM_STRUCT_FIELD ) {
	if ( opnd->mbr->isarray )
	    return ( opnd->mbr->total_size / opnd->mbr->total_length );
	return ( opnd->mbr->total_size );
    }
    if ( opnd->mem_type != MT_EMPTY )
	return SizeFromMemtype( opnd->mem_type, opnd->Ofssize, opnd->type );
    if ( opnd->type ) {
	if ( opnd->type->isarray )
	    return ( opnd->type->total_size / opnd->type->total_length );
	return ( opnd->type->total_size );
    }
    return 0;
}

int mem2mem( unsigned op1, unsigned op2, struct asm_tok tokenarray[], struct expr *opnd )
{
    int o1 = op1 & OP_MS;
    int o2 = op2 & OP_MS;
    int r1, r2, i, x;
    int op;
    char c;
    int reg, regz, size;
    char *dst;
    char *src;

    if ( !(op1 & OP_M_ANY ) || !(op2 & OP_M_ANY ) || ModuleInfo.strict_masm_compat )
	return asmerr( 2070 );

    reg = T_EAX;
    regz = 4;
    if ( ModuleInfo.Ofssize == USE64 ) {
	reg = T_RAX;
	regz = 8;
    }

    size = SizeFromExpression( opnd );
    i = SizeFromExpression( &opnd[1] );
    if ( i && i < size )
	size = i;
    else if ( size == 0 && opnd->mem_type == MT_EMPTY )
	size = i;

    r1 = reg;
    switch ( o1 ) {
      case OP_MS:
      case OP_M08: r1 = T_AL;  break;
      case OP_M16: r1 = T_AX;  break;
      case OP_M32: r1 = T_EAX; break;
    }
    r2 = reg;
    switch ( o2 ) {
      case OP_MS:
      case OP_M08: r2 = T_AL;  break;
      case OP_M16: r2 = T_AX;  break;
      case OP_M32: r2 = T_EAX; break;
    }
    if ( r2 > r1 && o1 == OP_MS )
	r1 = r2;
    if ( r1 > r2 && o2 == OP_MS )
	r2 = r1;

    op	= tokenarray[0].tokval;
    dst = tokenarray[1].tokpos;

    for ( i = 1; tokenarray[i].token != T_FINAL; i++ ) {
	if ( tokenarray[i].tokval == reg )
	    return asmerr( 2070 );

	if ( tokenarray[i].token == T_COMMA )
	    break;
    }
    if ( tokenarray[i].token != T_COMMA )
	return asmerr( 2070 );

    src = tokenarray[i].tokpos;
    c = *src;
    *src++ = '\0';

    if ( tokenarray[i+1].token == '&' ) {
	r1 = reg;
	AddLineQueueX( " lea %r,%s", reg, tokenarray[i+2].tokpos );
    } else if ( r1 > r2 && r2 < T_EAX ) {
	AddLineQueueX( " movzx eax,%s", src );
    } else {
	i = 8;
	switch ( r1 ) {
	case T_AL:  i = 1; break;
	case T_AX:  i = 2; break;
	case T_EAX: i = 4; break;
	}

	if ( size <= i ) {
	    AddLineQueueX( " mov %r,%s", r2, src );
	} else {
	    if ( op != T_MOV )
		return asmerr( 2070 );
	    i = size;
	    if ( regz == 8 ) {
		if ( i > 32 ) {
		    AddLineQueue ( " push rsi" );
		    AddLineQueue ( " push rdi" );
		    AddLineQueue ( " push rcx" );
		    AddLineQueueX( " lea rsi,%s", src );
		    AddLineQueueX( " lea rdi,%s", dst );
		    AddLineQueueX( " mov ecx,%d", i );
		    AddLineQueue ( " rep movsb" );
		    AddLineQueue ( " pop rcx" );
		    AddLineQueue ( " pop rdi" );
		    AddLineQueue ( " pop rsi" );
		} else {
		    for ( x = 0; i >= 8; i -= 8, x += 8 ) {
			AddLineQueueX( " mov rax,qword ptr %s[%d]", src, x );
			AddLineQueueX( " mov qword ptr %s[%d],rax", dst, x );
		    }
		    for ( ; i >= 4; i -= 4, x += 4 ) {
			AddLineQueueX( " mov eax,dword ptr %s[%d]", src, x );
			AddLineQueueX( " mov dword ptr %s[%d],eax", dst, x );
		    }
		    for ( ; i; i--, x++ ) {
			AddLineQueueX( " mov al,byte ptr %s[%d]", src, x );
			AddLineQueueX( " mov byte ptr %s[%d],al", dst, x );
		    }
		}
	    } else {
		if ( i > 16 ) {
		    AddLineQueue ( " push esi" );
		    AddLineQueue ( " push edi" );
		    AddLineQueue ( " push ecx" );
		    AddLineQueueX( " lea esi,%s", src );
		    AddLineQueueX( " lea edi,%s", dst );
		    AddLineQueueX( " mov ecx,%d", i );
		    AddLineQueue ( " rep movsb" );
		    AddLineQueue ( " pop ecx" );
		    AddLineQueue ( " pop edi" );
		    AddLineQueue ( " pop esi" );
		} else {
		    for ( x = 0; i >= 4; i -= 4, x += 4 ) {
			AddLineQueueX( " mov eax,dword ptr %s[%d]", src, x );
			AddLineQueueX( " mov dword ptr %s[%d],eax", dst, x );
		    }
		    for ( ; i; i--, x++ ) {
			AddLineQueueX( " mov al,byte ptr %s[%d]", src, x );
			AddLineQueueX( " mov byte ptr %s[%d],al", dst, x );
		    }
		}
	    }
	    i = 0;
	}
    }
    if ( i )
	AddLineQueueX( " %r %s,%r", op, dst, r1 );

    src--;
    *src = c;

    return RetLineQueue();
}

int immarray16( struct asm_tok tokenarray[], struct expr *result )
{
    int i;
    int count;
    int size;
    struct expr opnd;
    char oldtok[1024];
    char *p;

    strcpy( oldtok, tokenarray[0].tokpos );
    p = strcpy( CurrSource, tokenarray[3].string_ptr );

    for ( count = 1; *p; p++ ) {
	if ( *p == ',' )
	    count++;
    }
    size = 16 / count;


    if ( count * size != 16 ) {
	asmerr( 2036, CurrSource );
	count = 1;
	size = 4;
    }
    p = (char *)result;
    Token_Count = Tokenize( CurrSource, 0, tokenarray, TOK_DEFAULT );
    for ( i = 0; count; count--, i++, p += size ) {
	if ( EvalOperand( &i, tokenarray, Token_Count, &opnd, 0 ) == ERROR )
	    break;
	if ( opnd.mem_type & MT_FLOAT )
	    quad_resize(&opnd, size);
	memcpy(p, &opnd, size);
    }
    strcpy( CurrSource, oldtok );
    Token_Count = Tokenize( CurrSource, 0, tokenarray, TOK_DEFAULT );
    return 16;
}

int imm2xmm( struct asm_tok tokenarray[], struct expr *opnd )
{
    char flabel[16];
    int reg = tokenarray[1].tokval;
    int cmd = tokenarray[0].tokval;
    int size = 4;

    if ( opnd->mem_type == MT_REAL8 )
	size = 8;
    else if ( opnd->mem_type == MT_EMPTY )
	size = immarray16(tokenarray, opnd);

    CreateFloat(size, opnd, flabel);
    AddLineQueueX( " %r %r,%s", cmd, reg, flabel );
    return RetLineQueue();
}
