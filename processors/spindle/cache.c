/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle.h"

static int spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname);
static int spindle_cache_cleanup_(SPINDLECACHE *data);

/* Re-build the cached data for a set of proxies */
int
spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set)
{
	size_t c;

	for(c = 0; c < set->count; c++)
	{
		spindle_cache_update(spindle, set->strings[c]);
	}
	return 0;
}

/* Re-build the cached data for the proxy entity identified by localname;
 * if no references exist any more, the cached data will be removed.
 */
int
spindle_cache_update(SPINDLE *spindle, const char *localname)
{
	SPINDLECACHE data;
	int r;

	if(spindle_cache_init_(&data, spindle, localname))
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Find all of the triples related to all of the subjects linked to the
	 * proxy.
	 */
	r = sparql_queryf_model(spindle->sparql, data.sourcedata,
							"SELECT ?s ?p ?o ?g\n"
							" WHERE {\n"
							"  GRAPH %V {\n"
							"   ?s %V %V .\n"
							"  }\n"
							"  GRAPH ?g {\n"
							"   ?s ?p ?o .\n"
							"  }\n"
							"}",
							data.graph, data.sameas, data.self);

	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain cached data from SPARQL store\n");
		spindle_cache_cleanup_(&data);
		return -1;
	}   

	/* Remove our own derived proxy data from the source model */
	librdf_model_context_remove_statements(data.sourcedata, data.graph);
	
	/* Add the cache triples to the new proxy model */	
	if(spindle_class_update(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	if(spindle_prop_update(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}	
	/* Delete the old cache triples.
	 * Note that our owl:sameAs statements take the form
	 * <external> owl:sameAs <proxy>, so we can delete <proxy> ?p ?o with
	 * impunity.
	 */	
	r = sparql_updatef(spindle->sparql,
					   "WITH %V\n"
					   " DELETE { %V ?p ?o }\n"
					   " WHERE { %V ?p ?o }",
					   data.graph, data.self, data.self);   
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Insert the new proxy triples, if any */
	r = sparql_insert_model(spindle->sparql, data.proxydata);

	spindle_cache_cleanup_(&data);
	return r;
}

static int
spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname)
{	
	memset(data, 0, sizeof(SPINDLECACHE));
	data->spindle = spindle;
	data->sparql = spindle->sparql;
	data->localname = localname;
	data->self = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) localname);
	if(!data->self)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", localname);
		return -1;
	}
	data->graph = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) spindle->root);
	if(!data->graph)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", spindle->root);
		return -1;
	}
	data->sameas = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) "http://www.w3.org/2002/07/owl#sameAs");
	if(!data->sameas)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for owl:sameAs\n");
		return -1;
	}
	if(!(data->sourcedata = twine_rdf_model_create()))
	{
		return -1;
	}
	if(!(data->proxydata = twine_rdf_model_create()))
	{
		return -1;
	}
	return 0;
}

static int
spindle_cache_cleanup_(SPINDLECACHE *data)
{
	if(data->proxydata)
	{
		librdf_free_model(data->proxydata);
	}
	if(data->sourcedata)
	{
		librdf_free_model(data->sourcedata);
	}
	if(data->graph)
	{
		librdf_free_node(data->graph);
	}
	if(data->self)
	{
		librdf_free_node(data->self);
	}
	if(data->sameas)
	{
		librdf_free_node(data->sameas);
	}
	return 0;
}
