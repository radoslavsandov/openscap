/**
 * @file oval_criteriaNode.c
 * \brief Open Vulnerability and Assessment Language
 *
 * See more details at http://oval.mitre.org/
 */

/*
 * Copyright 2009-2010 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "David Niemoller" <David.Niemoller@g2-inc.com>
 *      "Peter Vrabec" <pvrabec@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "oval_definitions_impl.h"
#include "oval_collection_impl.h"
#include "oval_agent_api_impl.h"
#include "common/util.h"
#include "common/debug_priv.h"
#include "common/_error.h"

/***************************************************************************/
/* Variable definitions
 * */

typedef struct oval_criteria_node {
	struct oval_definition_model *model;
	oval_criteria_node_type_t type;
	int negate;
	char *comment;
} oval_criteria_node_t;

typedef struct oval_criteria_node_CRITERIA {
	struct oval_definition_model *model;
	oval_criteria_node_type_t type;
	int negate;
	char *comment;
	oval_operator_t operator;	/*type==NODETYPE_CRITERIA */
	struct oval_collection *subnodes;	/*type==NODETYPE_CRITERIA */
} oval_criteria_node_CRITERIA_t;

typedef struct oval_criteria_node_CRITERION {
	struct oval_definition_model *model;
	oval_criteria_node_type_t type;
	int negate;
	char *comment;
	struct oval_test *test;	/*type==NODETYPE_CRITERION */
} oval_criteria_node_CRITERION_t;

typedef struct oval_criteria_node_EXTENDDEF {
	struct oval_definition_model *model;
	oval_criteria_node_type_t type;
	int negate;
	char *comment;
	struct oval_definition *definition;	/*type==NODETYPE_EXTENDDEF */
} oval_criteria_node_EXTENDDEF_t;

/* End of variable definitions
 * */
/***************************************************************************/

bool oval_criteria_node_iterator_has_more(struct oval_criteria_node_iterator
					  *oc_criteria_node)
{
	return oval_collection_iterator_has_more((struct oval_iterator *)
						 oc_criteria_node);
}

struct oval_criteria_node *oval_criteria_node_iterator_next(struct
							    oval_criteria_node_iterator
							    *oc_criteria_node)
{
	return (struct oval_criteria_node *)
	    oval_collection_iterator_next((struct oval_iterator *)
					  oc_criteria_node);
}

void oval_criteria_node_iterator_free(struct
				      oval_criteria_node_iterator
				      *oc_criteria_node)
{
	oval_collection_iterator_free((struct oval_iterator *)
				      oc_criteria_node);
}

oval_criteria_node_type_t oval_criteria_node_get_type(struct oval_criteria_node
						      *node)
{
	__attribute__nonnull__(node);

	return ((struct oval_criteria_node *)node)->type;
}

bool oval_criteria_node_get_negate(struct oval_criteria_node *node)
{
	__attribute__nonnull__(node);

	return ((struct oval_criteria_node *)node)->negate;
}

char *oval_criteria_node_get_comment(struct oval_criteria_node *node)
{
	__attribute__nonnull__(node);

	return ((struct oval_criteria_node *)node)->comment;
}

oval_operator_t oval_criteria_node_get_operator(struct oval_criteria_node *node)
{
	__attribute__nonnull__(node);

	return (node->type == OVAL_NODETYPE_CRITERIA)
	    ? ((struct oval_criteria_node_CRITERIA *)node)->operator : OVAL_OPERATOR_UNKNOWN;
}

struct oval_criteria_node_iterator *oval_criteria_node_get_subnodes(struct
								    oval_criteria_node
								    *node)
{
	__attribute__nonnull__(node);

	struct oval_criteria_node_iterator *subnodes = NULL;
	if (node->type == OVAL_NODETYPE_CRITERIA) {
		struct oval_criteria_node_CRITERIA *criteria = (struct oval_criteria_node_CRITERIA *)node;
		subnodes = (struct oval_criteria_node_iterator *)
		    oval_collection_iterator(criteria->subnodes);
	}
	return subnodes;
}

struct oval_test *oval_criteria_node_get_test(struct oval_criteria_node *node)
{
	__attribute__nonnull__(node);

	/* type == NODETYPE_CRITERION */
	return (node->type == OVAL_NODETYPE_CRITERION)
	    ? ((struct oval_criteria_node_CRITERION *)node)->test : NULL;
}

struct oval_definition *oval_criteria_node_get_definition(struct oval_criteria_node *node) {
	__attribute__nonnull__(node);

	return (node->type == OVAL_NODETYPE_EXTENDDEF)
	    ? ((struct oval_criteria_node_EXTENDDEF *)node)->definition : NULL;
}

struct oval_criteria_node *oval_criteria_node_new(struct oval_definition_model *model, oval_criteria_node_type_t type)
{
	struct oval_criteria_node *node;
	switch (type) {
	case OVAL_NODETYPE_CRITERIA:{
			node = (struct oval_criteria_node *)
			    oscap_calloc(1, sizeof(oval_criteria_node_CRITERIA_t));
			if (node == NULL)
				return NULL;

			((struct oval_criteria_node_CRITERIA *)node)->operator = OVAL_OPERATOR_UNKNOWN;
			((struct oval_criteria_node_CRITERIA *)node)->subnodes = oval_collection_new();
		} break;
	case OVAL_NODETYPE_CRITERION:{
			node = (struct oval_criteria_node *)
			    oscap_calloc(1, sizeof(oval_criteria_node_CRITERION_t));
			if (node == NULL)
				return NULL;

			((struct oval_criteria_node_CRITERION *)node)->test = NULL;
		} break;
	case OVAL_NODETYPE_EXTENDDEF:{
			node = (struct oval_criteria_node *)
			    oscap_calloc(1, sizeof(oval_criteria_node_EXTENDDEF_t));
			if (node == NULL)
				return NULL;

			((struct oval_criteria_node_EXTENDDEF *)node)->definition = NULL;
		} break;
	case OVAL_NODETYPE_UNKNOWN:
	default:		/* TODO: Is constructor of criteria_node with unknown type valid ? */
		return NULL;
	}
	node->type = type;
	node->negate = 0;
	node->comment = NULL;
	node->model = model;
	return node;
}

bool oval_criteria_node_is_valid(struct oval_criteria_node * criteria_node)
{
	bool is_valid = true;
	oval_criteria_node_type_t type;

	if (criteria_node == NULL) {
                oscap_dlprintf(DBG_W, "Argument is not valid: NULL.\n");
		return false;
        }

	type = oval_criteria_node_get_type(criteria_node);
	switch (type) {
	case OVAL_NODETYPE_CRITERIA:
		{
			struct oval_criteria_node_iterator *subnodes_itr;

                        if (oval_criteria_node_get_operator(criteria_node) == OVAL_OPERATOR_UNKNOWN) {
                                oscap_dlprintf(DBG_W, "Argument is not valid: operator == OVAL_OPERATOR_UNKNOWN.\n");
                                return false;
                        }

			subnodes_itr = oval_criteria_node_get_subnodes(criteria_node);
                        if (oval_criteria_node_iterator_has_more(subnodes_itr) == false) {
                                oscap_dlprintf(DBG_W, "Argument is not valid: there are no subnodes.\n");
                                return false;
                        }
			while (oval_criteria_node_iterator_has_more(subnodes_itr)) {
				struct oval_criteria_node *subnode;

				subnode = oval_criteria_node_iterator_next(subnodes_itr);
				if (oval_criteria_node_is_valid(subnode) != true) {
					is_valid = false;
					break;
				}
			}
			oval_criteria_node_iterator_free(subnodes_itr);
			if (is_valid != true)
				return false;
		}
		break;
	case OVAL_NODETYPE_CRITERION:
		{
			struct oval_test *test;

			test = oval_criteria_node_get_test(criteria_node);
			if (oval_test_is_valid(test) != true)
				return false;
		}
		break;
	case OVAL_NODETYPE_EXTENDDEF:
		{
			struct oval_definition *def;

			def = oval_criteria_node_get_definition(criteria_node);
			if (oval_definition_is_valid(def) != true)
				return false;
		}
		break;
	default:
                oscap_dlprintf(DBG_W, "Argument is not valid: wrong node type: %d.\n", type);
		return false;
	}

	return true;
}

bool oval_criteria_node_is_locked(struct oval_criteria_node * criteria_node)
{
	__attribute__nonnull__(criteria_node);

	return oval_definition_model_is_locked(criteria_node->model);
}

struct oval_criteria_node *oval_criteria_node_clone
    (struct oval_definition_model *new_model, struct oval_criteria_node *old_node) {
	__attribute__nonnull__(old_node);

	struct oval_criteria_node *new_node = oval_criteria_node_new(new_model, old_node->type);
	if (new_node == NULL)
		return NULL;

	char *cn_comment = oval_criteria_node_get_comment(old_node);
	oval_criteria_node_set_comment(new_node, cn_comment);
	int negate = oval_criteria_node_get_negate(old_node);
	oval_criteria_node_set_negate(new_node, negate);
	switch (new_node->type) {
	case OVAL_NODETYPE_CRITERIA:{
			oval_operator_t operator = oval_criteria_node_get_operator(old_node);
			oval_criteria_node_set_operator(new_node, operator);
			struct oval_criteria_node_iterator *subnodes = oval_criteria_node_get_subnodes(old_node);
			while (oval_criteria_node_iterator_has_more(subnodes)) {
				struct oval_criteria_node *subnode = oval_criteria_node_iterator_next(subnodes);
				oval_criteria_node_add_subnode(new_node, oval_criteria_node_clone(new_model, subnode));
			}
			oval_criteria_node_iterator_free(subnodes);
		} break;
	case OVAL_NODETYPE_EXTENDDEF:{
			struct oval_definition *new_definition = NULL;
			struct oval_definition *old_definition = oval_criteria_node_get_definition(old_node);
			if (old_definition)
				new_definition = oval_definition_clone(new_model, old_definition);
			oval_criteria_node_set_definition(new_node, new_definition);
		}
		break;
	case OVAL_NODETYPE_CRITERION:{
			struct oval_test *new_test = NULL;
			struct oval_test *old_test = oval_criteria_node_get_test(old_node);
			if (old_test)
				new_test = oval_test_clone(new_model, old_test);
			oval_criteria_node_set_test(new_node, new_test);
		}
		break;
	default:		/*NOOP -> this will never happen, because oval_criteria_node_new will return NULL 
				   in case of unknown nodetype */ ;
	}

	return new_node;
}

void oval_criteria_node_free(struct oval_criteria_node *node)
{
	__attribute__nonnull__(node);

	oval_criteria_node_type_t type = node->type;
	switch (type) {
	case OVAL_NODETYPE_CRITERIA:{
			struct oval_criteria_node_CRITERIA *criteria = (struct oval_criteria_node_CRITERIA *)node;
			oval_collection_free_items(criteria->subnodes, (oscap_destruct_func) oval_criteria_node_free);
			criteria->subnodes = NULL;
		} break;
	case OVAL_NODETYPE_CRITERION:{
			//NOOP
		}
		break;
	case OVAL_NODETYPE_EXTENDDEF:{
			//NOOP
		}
	case OVAL_NODETYPE_UNKNOWN:{
			//NOOP
		}
	}
	if (node->comment != NULL) {
		oscap_free(node->comment);
	}
	node->comment = NULL;
	oscap_free(node);
}

void oval_criteria_set_node_type(struct oval_criteria_node *node, oval_criteria_node_type_t type)
{
	__attribute__nonnull__(node);

	if (node && !oval_criteria_node_is_locked(node)) {
		node->type = type;
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

void oval_criteria_node_set_negate(struct oval_criteria_node *node, bool negate)
{
	if (node && !oval_criteria_node_is_locked(node)) {
		node->negate = negate;
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

void oval_criteria_node_set_comment(struct oval_criteria_node *node, char *comm)
{
	if (node && !oval_criteria_node_is_locked(node)) {
		if (node->comment != NULL)
			oscap_free(node->comment);
		node->comment = (comm == NULL) ? NULL : oscap_strdup(comm);
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

void oval_criteria_node_set_operator(struct oval_criteria_node *node, oval_operator_t op)
{
	if (node && !oval_criteria_node_is_locked(node)) {
		if (node->type == OVAL_NODETYPE_CRITERIA) {
			struct oval_criteria_node_CRITERIA *criteria = (struct oval_criteria_node_CRITERIA *)node;
			criteria->operator = op;
		}
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

void oval_criteria_node_add_subnode(struct oval_criteria_node *node, struct oval_criteria_node *subnode)
{
	if (node && !oval_criteria_node_is_locked(node)) {
		if (node->type == OVAL_NODETYPE_CRITERIA) {
			struct oval_criteria_node_CRITERIA *criteria = (struct oval_criteria_node_CRITERIA *)node;
			oval_collection_add(criteria->subnodes, (void *)subnode);
		}
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

void oval_criteria_node_set_test(struct oval_criteria_node *node, struct oval_test *test)
{
	if (node && !oval_criteria_node_is_locked(node)) {
		if (node->type == OVAL_NODETYPE_CRITERION) {
			struct oval_criteria_node_CRITERION *criterion = (struct oval_criteria_node_CRITERION *)node;
			criterion->test = test;
		}
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

void oval_criteria_node_set_definition(struct oval_criteria_node *node, struct oval_definition *definition)
{
	if (node && !oval_criteria_node_is_locked(node)) {
		if (node->type == OVAL_NODETYPE_EXTENDDEF) {
			struct oval_criteria_node_EXTENDDEF *extenddef = (struct oval_criteria_node_EXTENDDEF *)node;
			extenddef->definition = definition;
		}
	} else
		oscap_dlprintf(DBG_W, "Attempt to update locked content.\n");
}

static void _oval_criteria_subnode_consume(struct oval_criteria_node *subnode, void *criteria)
{
	oval_criteria_node_add_subnode((struct oval_criteria_node *)
				       criteria, subnode);
}

static int _oval_criteria_subnode_consumer(xmlTextReaderPtr reader, struct oval_parser_context *context, void *user)
{
	struct oval_criteria_node_CRITERIA *criteria = (struct oval_criteria_node_CRITERIA *)user;
	int return_code = oval_criteria_parse_tag(reader, context, &_oval_criteria_subnode_consume, criteria);
	return return_code;
}

//typedef void (*oval_criteria_consumer)(struct oval_criteria_node *node, void*);
int oval_criteria_parse_tag(xmlTextReaderPtr reader,
			    struct oval_parser_context *context, oval_criteria_consumer consumer, void *user)
{
	char *tagname = (char *)xmlTextReaderLocalName(reader);
	xmlChar *namespace = xmlTextReaderNamespaceUri(reader);
	oval_criteria_node_type_t type = OVAL_NODETYPE_UNKNOWN;
	if (strcmp(tagname, "criteria") == 0)
		type = OVAL_NODETYPE_CRITERIA;
	else if (strcmp(tagname, "criterion") == 0)
		type = OVAL_NODETYPE_CRITERION;
	else if (strcmp(tagname, "extend_definition") == 0)
		type = OVAL_NODETYPE_EXTENDDEF;
	int return_code;
	if (type != OVAL_NODETYPE_UNKNOWN) {
		assert(context != NULL);	/* This is not asserted as attribute, because we 
						   can pass NULL pointer in case of OVAL_NODETYPE_UNKNOWN */
		struct oval_criteria_node *node = oval_criteria_node_new(context->definition_model, type);
		node->type = type;
		char *comm = (char *)xmlTextReaderGetAttribute(reader, BAD_CAST "comment");
		if (comm != NULL) {
			oval_criteria_node_set_comment(node, comm);
			oscap_free(comm);
			comm = NULL;
		}
		oval_criteria_node_set_negate(node, oval_parser_boolean_attribute(reader, "negate", 0));
		return_code = 1;
		switch (oval_criteria_node_get_type(node)) {
		case OVAL_NODETYPE_CRITERIA:{
				struct oval_criteria_node_CRITERIA *criteria =
				    (struct oval_criteria_node_CRITERIA *)node;
				oval_operator_t operator = oval_operator_parse(reader, "operator", OVAL_OPERATOR_AND);
				oval_criteria_node_set_operator((struct oval_criteria_node *)criteria, operator);
				return_code =
				    oval_parser_parse_tag(reader, context, &_oval_criteria_subnode_consumer, criteria);
			} break;
		case OVAL_NODETYPE_CRITERION:{
				char *test_ref = (char *)xmlTextReaderGetAttribute(reader,
										   BAD_CAST "test_ref");
				struct oval_definition_model *model = oval_parser_context_model(context);
				struct oval_test *test = oval_test_get_new(model, test_ref);
				oscap_free(test_ref);
				test_ref = NULL;
				oval_criteria_node_set_test(node, test);
			} break;
		case OVAL_NODETYPE_EXTENDDEF:{
				char *definition_ref = (char *)xmlTextReaderGetAttribute(reader,
											 BAD_CAST "definition_ref");
				struct oval_definition_model *model = oval_parser_context_model(context);
				struct oval_definition *definition = oval_definition_get_new(model, definition_ref);
				oval_criteria_node_set_definition(node, definition);
				oscap_free(definition_ref);
				definition_ref = NULL;
			}
		case OVAL_NODETYPE_UNKNOWN:
			break;
		}
		//oval_parser_parse_tag(reader, context,&_oval_criteria_parse_tag,node);
		(*consumer) (node, user);
	} else {
		return_code = 0;
		oscap_dlprintf(DBG_W, "Invalid node type: OVAL_NODETYPE_UNKNOWN.\n");
		oval_parser_skip_tag(reader, context);
	}
	oscap_free(tagname);
	oscap_free(namespace);
	return return_code;
}

void oval_criteria_node_to_print(struct oval_criteria_node *node, char *indent, int idx)
{
	__attribute__nonnull__(node);

	char *nodetype = NULL;
	char nxtindent[100];

	switch (node->type) {
	case OVAL_NODETYPE_CRITERIA:
		nodetype = "CRITERIA";
		break;
	case OVAL_NODETYPE_CRITERION:
		nodetype = "CRITERION";
		break;
	case OVAL_NODETYPE_EXTENDDEF:
		nodetype = "EXTEND_DEFINITION";
		break;
	case OVAL_NODETYPE_UNKNOWN:
		nodetype = "UNKNOWN_CRITNODE";
		break;
	}

	if (strlen(indent) > 80)
		indent = "....";

	if (idx == 0)
		snprintf(nxtindent, sizeof(nxtindent), "%s%s.", indent, nodetype);
	else
		snprintf(nxtindent, sizeof(nxtindent), "%s%s[%d].", indent, nodetype, idx);

	oscap_dprintf("%sCOMMENT = %s\n", nxtindent, node->comment);
	oscap_dprintf("%sNEGATE  = %d\n", nxtindent, node->negate);
	switch (node->type) {
	case OVAL_NODETYPE_CRITERIA:{
			struct oval_criteria_node_CRITERIA *criteria = (struct oval_criteria_node_CRITERIA *)node;
			oscap_dprintf("%sOPERATOR = %d\n", nxtindent, criteria->operator);
			struct oval_iterator *subnodes = oval_collection_iterator(criteria->subnodes);
			for (idx = 1; oval_collection_iterator_has_more(subnodes); idx++) {
				void *subnode = oval_collection_iterator_next(subnodes);
				oval_criteria_node_to_print((struct oval_criteria_node *)subnode, nxtindent, idx);
			}
			oval_collection_iterator_free(subnodes);
		} break;
	case OVAL_NODETYPE_CRITERION:{
			struct oval_criteria_node_CRITERION *criterion = (struct oval_criteria_node_CRITERION *)node;
			if (criterion->test != NULL)
				oval_test_to_print(criterion->test, nxtindent, 0);
			else
				oscap_dprintf("%sTEST = <<NONE>>\n", nxtindent);
		}
		break;
	case OVAL_NODETYPE_EXTENDDEF:{
			struct oval_criteria_node_EXTENDDEF *extenddef = (struct oval_criteria_node_EXTENDDEF *)node;
			if (extenddef->definition != NULL)
				oval_definition_to_print(extenddef->definition, nxtindent, 0);
			else
				oscap_dprintf("%sDEFINITION = <<NONE>>\n", nxtindent);
		}
		break;
	case OVAL_NODETYPE_UNKNOWN:
		break;
	}
}

static xmlNode *_oval_CRITERIA_to_dom(struct oval_criteria_node *cnode, xmlDoc * doc, xmlNode * parent) {
	xmlNs *ns_definitions = xmlSearchNsByHref(doc, parent, OVAL_DEFINITIONS_NAMESPACE);
	xmlNode *criteria_node = xmlNewTextChild(parent, ns_definitions, BAD_CAST "criteria", NULL);

	oval_operator_t operator = oval_criteria_node_get_operator(cnode);
	if (operator!= OVAL_OPERATOR_AND)
		xmlNewProp(criteria_node, BAD_CAST "operator", BAD_CAST oval_operator_get_text(operator));

	struct oval_criteria_node_iterator *subnodes = oval_criteria_node_get_subnodes(cnode);
	while (oval_criteria_node_iterator_has_more(subnodes)) {
		struct oval_criteria_node *subnode = oval_criteria_node_iterator_next(subnodes);
		oval_criteria_node_to_dom(subnode, doc, criteria_node);
	}
	oval_criteria_node_iterator_free(subnodes);

	return criteria_node;
}

static xmlNode *_oval_CRITERION_to_dom(struct oval_criteria_node *cnode, xmlDoc * doc, xmlNode * parent) {
	xmlNs *ns_definitions = xmlSearchNsByHref(doc, parent, OVAL_DEFINITIONS_NAMESPACE);
	xmlNode *criterion_node = xmlNewTextChild(parent, ns_definitions, BAD_CAST "criterion", NULL);

	struct oval_test *test = oval_criteria_node_get_test(cnode);
	char *test_ref = oval_test_get_id(test);
	xmlNewProp(criterion_node, BAD_CAST "test_ref", BAD_CAST test_ref);

	return criterion_node;
}

static xmlNode *_oval_EXTENDDEF_to_dom(struct oval_criteria_node *cnode, xmlDoc * doc, xmlNode * parent) {
	xmlNs *ns_definitions = xmlSearchNsByHref(doc, parent, OVAL_DEFINITIONS_NAMESPACE);
	xmlNode *extenddef_node = xmlNewTextChild(parent, ns_definitions, BAD_CAST "extend_definition", NULL);

	struct oval_definition *definition = oval_criteria_node_get_definition(cnode);
	char *definition_ref = oval_definition_get_id(definition);
	xmlNewProp(extenddef_node, BAD_CAST "definition_ref", BAD_CAST definition_ref);

	return extenddef_node;
}

xmlNode *oval_criteria_node_to_dom(struct oval_criteria_node * cnode, xmlDoc * doc, xmlNode * parent) {
	xmlNode *criteria_node;
	switch (oval_criteria_node_get_type(cnode)) {
	case OVAL_NODETYPE_CRITERIA:
		criteria_node = _oval_CRITERIA_to_dom(cnode, doc, parent);
		break;
	case OVAL_NODETYPE_CRITERION:
		criteria_node = _oval_CRITERION_to_dom(cnode, doc, parent);
		break;
	case OVAL_NODETYPE_EXTENDDEF:
		criteria_node = _oval_EXTENDDEF_to_dom(cnode, doc, parent);
		break;
	default:
		criteria_node = NULL;
		break;
	}

	bool negate = oval_criteria_node_get_negate(cnode);
	if (negate)
		xmlNewProp(criteria_node, BAD_CAST "negate", BAD_CAST "true");

	char *comm = oval_criteria_node_get_comment(cnode);
	if (comm)
		xmlNewProp(criteria_node, BAD_CAST "comment", BAD_CAST comm);

	return criteria_node;
}
