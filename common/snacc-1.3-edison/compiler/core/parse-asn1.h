#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union
{
    int              intVal;
    unsigned int     uintVal;
    char            *charPtr;
    Type            *typePtr;
    NamedType       *namedTypePtr;
    NamedTypeList   *namedTypeListPtr;
    Value           *valuePtr;
    NamedValue      *namedValuePtr;
    SubtypeValue    *subtypeValuePtr;
    Subtype         *subtypePtr;
    ModuleId        *moduleId;
    OID             *oidPtr;
    OidList         *oidListPtr;
    TypeDef         *typeDefPtr;
    TypeDefList     *typeDefListPtr;
    ValueDef        *valueDefPtr;
    ValueDefList    *valueDefListPtr;
    ExportElmt      *exportList;
    ImportModule    *importModulePtr;
    ImportModuleList *importModuleListPtr;
    ImportElmt      *importElmtPtr;
    ImportElmtList  *importElmtListPtr;
    Tag             *tagPtr;
    TagList         *tagListPtr;
    Constraint      *constraintPtr;
    ConstraintList  *constraintListPtr;
    InnerSubtype    *innerSubtypePtr;
    ValueList       *valueListPtr;
    TypeOrValueList *typeOrValueListPtr;
    TypeOrValue     *typeOrValuePtr;
    AsnPort         *asnPortPtr;
    AsnPortList     *asnPortListPtr;
    AttributeList   *attrList;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	BSTRING_SYM	257
# define	HSTRING_SYM	258
# define	CSTRING_SYM	259
# define	UCASEFIRST_IDENT_SYM	260
# define	LCASEFIRST_IDENT_SYM	261
# define	NAMEDMACRO_SYM	262
# define	MACRODEFBODY_SYM	263
# define	BRACEBAL_SYM	264
# define	NUMBER_ERANGE	265
# define	NUMBER_SYM	266
# define	SNACC_ATTRIBUTES	267
# define	DOT_SYM	268
# define	COMMA_SYM	269
# define	LEFTBRACE_SYM	270
# define	RIGHTBRACE_SYM	271
# define	LEFTPAREN_SYM	272
# define	RIGHTPAREN_SYM	273
# define	LEFTBRACKET_SYM	274
# define	RIGHTBRACKET_SYM	275
# define	LESSTHAN_SYM	276
# define	MINUS_SYM	277
# define	GETS_SYM	278
# define	BAR_SYM	279
# define	TAGS_SYM	280
# define	BOOLEAN_SYM	281
# define	INTEGER_SYM	282
# define	BIT_SYM	283
# define	STRING_SYM	284
# define	OCTET_SYM	285
# define	NULL_SYM	286
# define	SEQUENCE_SYM	287
# define	OF_SYM	288
# define	SET_SYM	289
# define	IMPLICIT_SYM	290
# define	CHOICE_SYM	291
# define	ANY_SYM	292
# define	OBJECT_IDENTIFIER_SYM	293
# define	OPTIONAL_SYM	294
# define	DEFAULT_SYM	295
# define	COMPONENTS_SYM	296
# define	UNIVERSAL_SYM	297
# define	APPLICATION_SYM	298
# define	PRIVATE_SYM	299
# define	TRUE_SYM	300
# define	FALSE_SYM	301
# define	BEGIN_SYM	302
# define	END_SYM	303
# define	DEFINITIONS_SYM	304
# define	EXPLICIT_SYM	305
# define	ENUMERATED_SYM	306
# define	EXPORTS_SYM	307
# define	IMPORTS_SYM	308
# define	REAL_SYM	309
# define	INCLUDES_SYM	310
# define	MIN_SYM	311
# define	MAX_SYM	312
# define	SIZE_SYM	313
# define	FROM_SYM	314
# define	WITH_SYM	315
# define	COMPONENT_SYM	316
# define	PRESENT_SYM	317
# define	ABSENT_SYM	318
# define	DEFINED_SYM	319
# define	BY_SYM	320
# define	PLUS_INFINITY_SYM	321
# define	MINUS_INFINITY_SYM	322
# define	SEMI_COLON_SYM	323
# define	IA5STRING_SYM	324
# define	PRINTABLESTRING_SYM	325
# define	NUMERICSTRING_SYM	326
# define	TELETEXSTRING_SYM	327
# define	T61STRING_SYM	328
# define	VIDEOTEXSTRING_SYM	329
# define	VISIBLESTRING_SYM	330
# define	ISO646STRING_SYM	331
# define	GRAPHICSTRING_SYM	332
# define	GENERALSTRING_SYM	333
# define	GENERALIZEDTIME_SYM	334
# define	UTCTIME_SYM	335
# define	EXTERNAL_SYM	336
# define	OBJECTDESCRIPTOR_SYM	337
# define	OPERATION_SYM	338
# define	ARGUMENT_SYM	339
# define	RESULT_SYM	340
# define	ERRORS_SYM	341
# define	LINKED_SYM	342
# define	ERROR_SYM	343
# define	PARAMETER_SYM	344
# define	BIND_SYM	345
# define	BINDERROR_SYM	346
# define	UNBIND_SYM	347
# define	UNBINDERROR_SYM	348
# define	ASE_SYM	349
# define	OPERATIONS_SYM	350
# define	CONSUMERINVOKES_SYM	351
# define	SUPPLIERINVOKES_SYM	352
# define	AC_SYM	353
# define	ASES_SYM	354
# define	REMOTE_SYM	355
# define	INITIATOR_SYM	356
# define	RESPONDER_SYM	357
# define	ABSTRACTSYNTAXES_SYM	358
# define	CONSUMER_SYM	359
# define	EXTENSIONS_SYM	360
# define	CHOSEN_SYM	361
# define	EXTENSION_SYM	362
# define	CRITICAL_SYM	363
# define	FOR_SYM	364
# define	DELIVERY_SYM	365
# define	SUBMISSION_SYM	366
# define	TRANSFER_SYM	367
# define	EXTENSIONATTRIBUTE_SYM	368
# define	TOKEN_SYM	369
# define	TOKENDATA_SYM	370
# define	SECURITYCATEGORY_SYM	371
# define	OBJECT_SYM	372
# define	PORTS_SYM	373
# define	BOXC_SYM	374
# define	BOXS_SYM	375
# define	PORT_SYM	376
# define	ABSTRACTOPS_SYM	377
# define	REFINE_SYM	378
# define	AS_SYM	379
# define	RECURRING_SYM	380
# define	VISIBLE_SYM	381
# define	PAIRED_SYM	382
# define	ABSTRACTBIND_SYM	383
# define	ABSTRACTUNBIND_SYM	384
# define	TO_SYM	385
# define	ABSTRACTERROR_SYM	386
# define	ABSTRACTOPERATION_SYM	387
# define	ALGORITHM_SYM	388
# define	ENCRYPTED_SYM	389
# define	SIGNED_SYM	390
# define	SIGNATURE_SYM	391
# define	PROTECTED_SYM	392
# define	OBJECTTYPE_SYM	393
# define	SYNTAX_SYM	394
# define	ACCESS_SYM	395
# define	STATUS_SYM	396
# define	DESCRIPTION_SYM	397
# define	REFERENCE_SYM	398
# define	INDEX_SYM	399
# define	DEFVAL_SYM	400


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
