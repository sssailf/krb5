/*
 * lib/krb5/krb/init_ctx.c
 *
 * Copyright 1994 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * krb5_init_contex()
 */

#include "k5-int.h"

krb5_error_code INTERFACE
krb5_init_context(context)
	krb5_context *context;
{
	krb5_context ctx;
	krb5_error_code retval;
	int tmp;

	*context = 0;

	ctx = malloc(sizeof(struct _krb5_context));
	if (!ctx)
		return ENOMEM;
	memset(ctx, 0, sizeof(struct _krb5_context));
	ctx->magic = KV5M_CONTEXT;

	/* Set the default encryption types, possible defined in krb5/conf */
	if ((retval = krb5_set_default_in_tkt_ktypes(ctx, NULL)))
		goto cleanup;

	if ((retval = krb5_set_default_tgs_ktypes(ctx, NULL)))
		goto cleanup;

	if ((retval = krb5_os_init_context(ctx)))
		goto cleanup;

	ctx->default_realm = 0;
	profile_get_integer(ctx->profile, "libdefaults",
			    "clockskew", 0, 5 * 60,
			    &tmp);
	ctx->clockskew = tmp;
	ctx->kdc_req_sumtype = CKSUMTYPE_RSA_MD5;
	ctx->kdc_default_options = KDC_OPT_RENEWABLE_OK;
	profile_get_integer(ctx->profile, "libdefaults",
			    "kdc_timesync", 0, 0,
			    &tmp);
	ctx->library_options = tmp ? KRB5_LIBOPT_SYNC_KDCTIME : 0;

	*context = ctx;
	return 0;

cleanup:
	krb5_free_context(ctx);
	return retval;
}

void
krb5_free_context(ctx)
	krb5_context	ctx;
{
     krb5_os_free_context(ctx);

     if (ctx->in_tkt_ktypes)
          free(ctx->in_tkt_ktypes);

     if (ctx->tgs_ktypes)
          free(ctx->tgs_ktypes);

     if (ctx->default_realm)
	  free(ctx->default_realm);

     if (ctx->ser_ctx_count && ctx->ser_ctx)
	 free(ctx->ser_ctx);

     ctx->magic = 0;
     free(ctx);
}

/*
 * Set the desired default ktypes, making sure they are valid.
 */
krb5_error_code
krb5_set_default_in_tkt_ktypes(context, ktypes)
	krb5_context context;
	const krb5_enctype *ktypes;
{
    krb5_enctype * new_ktypes;
    int i;

    if (ktypes) {
	for (i = 0; ktypes[i]; i++) {
	    if (!valid_enctype(ktypes[i])) 
		return KRB5_PROG_ETYPE_NOSUPP;
	}

	/* Now copy the default ktypes into the context pointer */
	if ((new_ktypes = (krb5_enctype *)malloc(sizeof(krb5_enctype) * i)))
	    memcpy(new_ktypes, ktypes, sizeof(krb5_enctype) * i);
	else
	    return ENOMEM;

    } else {
	i = 3;

	/* Should reset the list to the runtime defaults */
	if ((new_ktypes = (krb5_enctype *)malloc(sizeof(krb5_enctype) * i))) {
	    new_ktypes[0] = ENCTYPE_DES3_CBC_MD5;
	    new_ktypes[1] = ENCTYPE_DES_CBC_MD5;
	    new_ktypes[2] = ENCTYPE_DES_CBC_CRC;
	} else {
	    return ENOMEM;
	}
    }

    if (context->in_tkt_ktypes) 
        free(context->in_tkt_ktypes);
    context->in_tkt_ktypes = new_ktypes;
    context->in_tkt_ktype_count = i;
    return 0;
}

krb5_error_code
krb5_get_default_in_tkt_ktypes(context, ktypes)
    krb5_context context;
    krb5_enctype **ktypes;
{
    krb5_enctype * old_ktypes;

    if ((old_ktypes = (krb5_enctype *)malloc(sizeof(krb5_enctype) *
					     (context->in_tkt_ktype_count + 1)))) {
	memcpy(old_ktypes, context->in_tkt_ktypes, sizeof(krb5_enctype) * 
		  		context->in_tkt_ktype_count);
	old_ktypes[context->in_tkt_ktype_count] = 0;
    } else {
	return ENOMEM;
    }

    *ktypes = old_ktypes;
    return 0;
}

krb5_error_code
krb5_set_default_tgs_ktypes(context, ktypes)
	krb5_context context;
	const krb5_enctype *ktypes;
{
    krb5_enctype * new_ktypes;
    int i;

    if (ktypes) {
	for (i = 0; ktypes[i]; i++) {
	    if (!valid_enctype(ktypes[i])) 
		return KRB5_PROG_ETYPE_NOSUPP;
	}

	/* Now copy the default ktypes into the context pointer */
	if ((new_ktypes = (krb5_enctype *)malloc(sizeof(krb5_enctype) * i)))
	    memcpy(new_ktypes, ktypes, sizeof(krb5_enctype) * i);
	else
	    return ENOMEM;

    } else {
	i = 0;
	new_ktypes = (krb5_enctype *)NULL;
    }

    if (context->tgs_ktypes) 
        free(context->tgs_ktypes);
    context->tgs_ktypes = new_ktypes;
    context->tgs_ktype_count = i;
    return 0;
}

krb5_error_code
krb5_get_tgs_ktypes(context, princ, ktypes)
    krb5_context context;
    krb5_const_principal princ;
    krb5_enctype **ktypes;
{
    krb5_enctype * old_ktypes;

    if (context->tgs_ktype_count) {

	/* Application-set defaults */

	if ((old_ktypes =
	     (krb5_enctype *)malloc(sizeof(krb5_enctype) *
				    (context->tgs_ktype_count + 1)))) {
	    memcpy(old_ktypes, context->tgs_ktypes, sizeof(krb5_enctype) * 
		   context->tgs_ktype_count);
	    old_ktypes[context->tgs_ktype_count] = 0;
	} else {
	    return ENOMEM;
	}
    } else {
	/*
	   XXX - For now, we only support libdefaults
	   Perhaps this should be extended to allow for per-host / per-realm
	   session key types.
	 */

	char *retval;
	char *sp, *ep;
	int i, j, count;
	krb5_error_code code;

	code = profile_get_string(context->profile,
				  "libdefaults", "default_tgs_enctypes", NULL,
				  "des3-cbc-md5 des-cbc-md5 des-cbc-crc",
				  &retval);
	if (code)
	    return code;

	count = 0;
	sp = retval;
	while (sp) {
	    for (ep = sp; *ep && (*ep != ',') && !isspace(*ep); ep++)
		;
	    if (*ep) {
		*ep++ = '\0';
		while (isspace(*ep))
		    ep++;
	    } else
		ep = (char *) NULL;

	    count++;
	    sp = ep;
	}
	
	if ((old_ktypes =
	     (krb5_enctype *)malloc(sizeof(krb5_enctype) * (count + 1))) ==
	    (krb5_enctype *) NULL)
	    return ENOMEM;
	
	sp = retval;
	j = 0;
	for (i = 0; i < count; i++) {
	    if (! krb5_string_to_enctype(sp, &old_ktypes[j]))
		j++;

	    /* skip to next token */
	    while (*sp) sp++;
	    while (! *sp) sp++;
	}

	old_ktypes[j] = (krb5_enctype) 0;
	free(retval);
    }

    *ktypes = old_ktypes;
    return 0;
}
