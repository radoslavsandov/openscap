/*
 * Copyright 2008 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * OpenScap CPE Lang Module Test Suite Helper
 *
 * Authors:
 *      Ondrej Moris <omoris@redhat.com>
 */

#include <stdio.h>
#include <string.h>
#include <cpelang.h>
#include <cpeuri.h>

/* ###### FIX THIS ##### */
/*
#include "../../src/CPE/cpelang_priv.h"


struct cpe_testexpr {
  struct xml_metadata xml;
  cpe_lang_oper_t oper;
  union {
    struct cpe_testexpr *expr;
    struct cpe_name *cpe;	  
  } meta;			          
};
*/

// Strings representing operators.
const char *CPE_OPER_STRS[] = { "", "AND", "OR", "" };

// Print usage.
void print_usage(const char *program_name, FILE *out) 
{
  fprintf(out, 
	  "Usage: \n\n"
	  "  %s --help\n"
	  "  %s --get-all CPE_LANG_XML ENCODING\n"
	  "  %s --get-key CPE_LANG_XML ENCODING KEY\n"
	  "  %s --set-all CPE_LANG_XML ENCODING NS_PREFIX NS_HREF (PLATFORM_ID)*\n"
	  "  %s --set-key CPE_LANG_XML ENCODING KEY ID (TITLE)*\n"
	  "  %s --set-new CPE_LANG_XML ENCODING NS_PREFIX NS_HREF (PLATFORM_ID)*\n"
	  "  %s --sanity-check\n",
	  program_name, program_name, program_name, program_name, program_name,
	  program_name, program_name);
}

// Print expression in prefix form.
int print_expr_prefix_form(const struct cpe_testexpr *expr) 
{

  const struct cpe_testexpr *sub;

  putchar('(');
  
  if (cpe_testexpr_get_oper(expr) & CPE_LANG_OPER_NOT)
    printf("!");

  switch (cpe_testexpr_get_oper(expr) & CPE_LANG_OPER_MASK) {
  case CPE_LANG_OPER_AND:
  case CPE_LANG_OPER_OR:
    printf("%s", CPE_OPER_STRS[cpe_testexpr_get_oper(expr) & CPE_LANG_OPER_MASK]);
/*    for (sub = cpe_testexpr_get_meta_expr(expr); cpe_testexpr_get_oper(sub); ++sub)
      print_expr_prefix_form(sub);   */
    break;
  case CPE_LANG_OPER_MATCH:
    printf("%s", cpe_name_get_uri(cpe_testexpr_get_meta_cpe(expr)));
    break;
  default:  
    return 1;
  }

  putchar(')');

  return 0;
}

// Print platform.
void print_platform(struct cpe_platform *platform) 
{
  struct oscap_title *title = NULL;
  struct oscap_title_iterator *title_it = NULL;
  const char *remark, *id, *content, *language;

  id = cpe_platform_get_id(platform);
  printf("%s:", id == NULL ? "" : id);
  
  remark = cpe_platform_get_remark(platform);
  printf("%s:", remark == NULL ? "" : remark);
  
  title_it = cpe_platform_get_titles(platform);
  while (oscap_title_iterator_has_more(title_it)) {
    title = oscap_title_iterator_next(title_it);

    content = oscap_title_get_content(title);
    printf("%s.", content == NULL ? "" : content);
    
    language = oscap_title_get_language(title);
    printf("%s,", language == NULL ? "" : language);
  }
  putchar(':');
  oscap_title_iterator_free(title_it);
  print_expr_prefix_form(cpe_platform_get_expr(platform));
  putchar('\n');
} 

int main (int argc, char *argv[]) 
{  
  struct cpe_lang_model *lang_model = NULL;
  struct cpe_platform *platform = NULL, *new_platform = NULL;
  struct cpe_testexpr *testexpr = NULL;
  struct cpe_platform_iterator *platform_it = NULL;
  struct oscap_import_source *import_source = NULL;
  struct oscap_export_target *export_target = NULL;
  struct oscap_title_iterator *title_it = NULL;
  struct oscap_title *title = NULL;
  int ret_val = 0, i;
  
  if (argc == 2 && !strcmp(argv[1], "--help")) {
    print_usage(argv[0], stdout);
    ret_val = 0;
  }

  // Print complete content.
  else if (argc == 4 && !strcmp(argv[1], "--get-all")) {        
    import_source = oscap_import_source_new(argv[2], argv[3]);    
    lang_model = cpe_lang_model_import(import_source);

    printf("%s:", cpe_lang_model_get_ns_href(lang_model));
    printf("%s\n", cpe_lang_model_get_ns_prefix(lang_model));
    platform_it = cpe_lang_model_get_platforms(lang_model);
    while (cpe_platform_iterator_has_more(platform_it)) {
      print_platform(cpe_platform_iterator_next(platform_it));
    }
    cpe_platform_iterator_free(platform_it);
    oscap_import_source_free(import_source);
    cpe_lang_model_free(lang_model);      
  }
  
  // Print platform of given key only.
  else if (argc == 5 && !strcmp(argv[1], "--get-key")) {        
    import_source = oscap_import_source_new(argv[2], argv[3]);
    
    if ((lang_model = cpe_lang_model_import(import_source)) == NULL)
      return 1;

    if ((platform = cpe_lang_model_get_item(lang_model, argv[4])) == NULL)
      return 2;
    
    print_platform(platform);

    oscap_import_source_free(import_source);
    cpe_lang_model_free(lang_model);      
  }

  // Set ns_prefix, ns_href, add new platforms.
  else if (argc >= 6 && !strcmp(argv[1], "--set-all")) {        
    import_source = oscap_import_source_new(argv[2], argv[3]);    
    if ((lang_model = cpe_lang_model_import(import_source)) == NULL)
      return 1;
    oscap_import_source_free(import_source);
    
    if (strcmp(argv[4], "-")) 
      cpe_lang_model_set_ns_prefix(lang_model, argv[4]);
    if (strcmp(argv[5], "-")) 
      cpe_lang_model_set_ns_href(lang_model, argv[5]);

    for (i = 6; i < argc; i++) {
      if ((new_platform =  cpe_platform_new()) == NULL)
	return 1;
      cpe_platform_set_id(new_platform, argv[i]);      
      if (!cpe_lang_model_add_item(lang_model, new_platform))
	return 2;
    }      

    export_target = oscap_export_target_new(argv[2], argv[3]);
    cpe_lang_model_export(lang_model, export_target);
    oscap_export_target_free(export_target);
    cpe_lang_model_free(lang_model);      
  }

  // Set id, change titles of platform of given key.
  else if (argc >= 6 && !strcmp(argv[1], "--set-key")) {        
    import_source = oscap_import_source_new(argv[2], argv[3]);
    if ((lang_model = cpe_lang_model_import(import_source)) == NULL)
      return 1;
    oscap_import_source_free(import_source);
    
    if ((platform = cpe_lang_model_get_item(lang_model, argv[4])) == NULL)
      return 2;
    
    if (strcmp(argv[5], "-"))
      cpe_platform_set_id(platform, argv[5]);        

    i = 6;
    title_it = cpe_platform_get_titles(platform);
    while (i < argc && oscap_title_iterator_has_more(title_it)) {
      title = oscap_title_iterator_next(title_it);
      if (strcmp(argv[i], "-"))
	oscap_title_set_content(title, argv[i]);
      i++;
    } 

    export_target = oscap_export_target_new(argv[2], argv[3]);
    cpe_lang_model_export(lang_model, export_target);
    oscap_export_target_free(export_target);
    cpe_lang_model_free(lang_model);      
  }

  // Create new content with new platforms.
  else if (argc >= 6 && !strcmp(argv[1], "--set-new")) {        
    if ((lang_model = cpe_lang_model_new()) == NULL)
      return 1;
    
    if (strcmp(argv[4], "-")) 
      cpe_lang_model_set_ns_prefix(lang_model, argv[4]);
    if (strcmp(argv[5], "-")) 
      cpe_lang_model_set_ns_href(lang_model, argv[5]);
    
    for (i = 6; i < argc; i++) {
      if ((new_platform =  cpe_platform_new()) == NULL)
	return 1;
      cpe_platform_set_id(new_platform, argv[i]);      
      if (!cpe_lang_model_add_item(lang_model, new_platform))
	return 2;
    }      

    export_target = oscap_export_target_new(argv[2], argv[3]);
    cpe_lang_model_export(lang_model, export_target);
    oscap_export_target_free(export_target);    
    cpe_lang_model_free(lang_model);      
  }

  // Sanity checks.
  else if (argc == 2 && !strcmp(argv[1], "--sanity-check")) {        

    if ((lang_model = cpe_lang_model_new()) == NULL) 
      return 1;
    else
      cpe_lang_model_free(lang_model);
    
    if ((new_platform =  cpe_platform_new()) == NULL) 
      return 1;
    else
      cpe_platform_free(new_platform);
    
    if ((testexpr = cpe_testexpr_new()) == NULL) 
      return 1;
    else
      cpe_testexpr_free(testexpr);
  }
  
  else {
    print_usage(argv[0], stderr);
    ret_val = 1;
  }

  return ret_val;
}
