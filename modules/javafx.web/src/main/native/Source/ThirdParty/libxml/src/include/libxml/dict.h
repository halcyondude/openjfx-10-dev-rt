/*
 * Summary: string dictionnary
 * Description: dictionary of reusable strings, just used to avoid allocation
 *         and freeing operations.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Daniel Veillard
 */

#ifndef __XML_DICT_H__
#define __XML_DICT_H__

#include <libxml/xmlversion.h>
#include <libxml/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The dictionnary.
 */
typedef struct _xmlDict xmlDict;
typedef xmlDict *xmlDictPtr;

/*
 * Constructor and destructor.
 */
XMLPUBFUN xmlDictPtr XMLCALL
            xmlDictCreate   (void);
XMLPUBFUN xmlDictPtr XMLCALL
            xmlDictCreateSub(xmlDictPtr sub);
XMLPUBFUN int XMLCALL
            xmlDictReference(xmlDictPtr dict);
XMLPUBFUN void XMLCALL
            xmlDictFree (xmlDictPtr dict);

/*
 * Lookup of entry in the dictionnary.
 */
XMLPUBFUN const xmlChar * XMLCALL
            xmlDictLookup   (xmlDictPtr dict,
                                 const xmlChar *name,
                                 int len);
XMLPUBFUN const xmlChar * XMLCALL
            xmlDictExists   (xmlDictPtr dict,
                                 const xmlChar *name,
                                 int len);
XMLPUBFUN const xmlChar * XMLCALL
            xmlDictQLookup  (xmlDictPtr dict,
                                 const xmlChar *prefix,
                                 const xmlChar *name);
XMLPUBFUN int XMLCALL
            xmlDictOwns (xmlDictPtr dict,
                     const xmlChar *str);
XMLPUBFUN int XMLCALL
            xmlDictSize (xmlDictPtr dict);

/*
 * Cleanup function
 */
XMLPUBFUN void XMLCALL
                        xmlDictCleanup  (void);

#ifdef __cplusplus
}
#endif
#endif /* ! __XML_DICT_H__ */
