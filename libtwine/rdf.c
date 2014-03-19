/* Twine: RDF helpers
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

#include "p_libtwine.h"

#ifndef LIBRDF_MODEL_FEATURE_CONTEXTS
# error librdf library version is too old; please upgrade to a version which supports contexts
#endif

static librdf_world *twine_world;

static int twine_librdf_logger(void *data, librdf_log_message *message);

int
twine_rdf_init_(void)
{
	twine_world = librdf_new_world();
	if(!twine_world)
	{
		twine_logf(LOG_CRIT, "failed to create new RDF world\n");
		return -1;
	}
	librdf_world_open(twine_world);
	librdf_world_set_logger(twine_world, NULL, twine_librdf_logger);
	return 0;
}

librdf_world *
twine_rdf_world(void)
{
	return twine_world;
}

/* Create a new model */
librdf_model *
twine_rdf_model_create(void)
{
	librdf_model *model;
	librdf_storage *storage;
	
	storage = librdf_new_storage(twine_world, "hashes", NULL, "hash-type='memory',contexts='yes'");
	if(!storage)
	{
		twine_logf(LOG_CRIT, "failed to create new RDF storage\n");
		return NULL;
	}
	model = librdf_new_model(twine_world, storage, NULL);
	if(!model)
	{
		twine_logf(LOG_CRIT, "failed to create new RDF model\n");
		librdf_free_storage(storage);
		return NULL;
	}
	return model;
}

/* Parse a buffer of a particular MIME type into a model */
int
twine_rdf_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen)
{	
	static librdf_uri *base;
	const char *name;
	librdf_parser *parser;
	int r;

	if(!base)
	{
		base = librdf_new_uri(twine_world, (const unsigned char *) "/");
		if(!base)
		{
			twine_logf(LOG_CRIT, "failed to parse URI </>\n");
			return -1;
		}
	}
	name = NULL;
	/* Handle specific MIME types whether or not librdf already knows
	 * about them
	 */
	if(!strcmp(mime, "application/trig"))
	{
		name = "trig";
	}
	else if(!strcmp(mime, "application/nquads"))
	{
		name = "nquads";
	}
	/* If we have a specific parser name, don't use the MIME type */
	if(name)
	{
		mime = NULL;
	}
	parser = librdf_new_parser(twine_world, name, mime, NULL);
	if(!parser)
	{
		if(!name)
		{
			name = "auto";
		}
		twine_logf(LOG_ERR, "failed to create a new parser for %s (%s)\n", mime, name);
		return -1;
	}
	r = librdf_parser_parse_counted_string_into_model(parser, (const unsigned char *) buf, buflen, base, model);
	librdf_free_parser(parser);
	return r;
}

/* Log events from librdf */
static int
twine_librdf_logger(void *data, librdf_log_message *message)
{
	int level;

	(void) data;
	
	switch(librdf_log_message_level(message))
	{
	case LIBRDF_LOG_DEBUG:
		level = LOG_DEBUG;
		break;
	case LIBRDF_LOG_INFO:
		level = LOG_INFO;
		break;
	case LIBRDF_LOG_WARN:
		level = LOG_WARNING;
		break;
	case LIBRDF_LOG_ERROR:
		level = LOG_ERR;
		break;
	case LIBRDF_LOG_FATAL:
		level = LOG_CRIT;
		break;
	default:
		level = LOG_NOTICE;
		break;
	}
	twine_logf(level, "%s\n", librdf_log_message_message(message));
	return 0;
}
