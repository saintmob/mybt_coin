#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "kyk_config.h"
#include "kyk_file.h"
#include "kyk_utils.h"
#include "dbg.h"

enum ConfigKVType {
   CONFIG_KV_UNKNOWN,
   CONFIG_KV_STRING,
   CONFIG_KV_INT64,
   CONFIG_KV_BOOL,
};


struct KeyValuePair {
   char                *key;
   bool              save;
   struct KeyValuePair *next;
   enum ConfigKVType    type;
   union {
      int64_t  val;
      bool  trueOrFalse;
      char  *str;
   } u;
};

static void config_freekvlist(struct KeyValuePair *list);

static bool kyk_config_parseline(char *line,
				 char **key,
				 char **val);

static void kyk_config_chomp(char *str);

static void kyk_config_setunknownkv(struct config *config,
				    const char    *key,
				    const char    *val)
    ;
static void kyk_config_insert(struct config *config,
			      struct KeyValuePair *ev);




struct config* kyk_config_create(void)
{
    struct config* cfg = NULL;
    cfg = calloc(1, sizeof(*cfg));
    check(cfg != NULL, "calloc config failed");

    return cfg;
error:
    return NULL;
}


int kyk_config_load(const char* fileName, struct config** conf)
{
    struct file_descriptor* fd;
    struct config* cfg = NULL;
    struct KeyValuePair* list;
    int res;

    *conf = NULL;
    list = NULL;

    res = kyk_file_open(fileName, TRUE, &fd);
    check(res == 0, "Failed to open config '%s'", fileName);

    cfg = kyk_config_create();
    check(cfg != NULL, "Failed to create config");

    cfg -> fileName = kyk_strdup(fileName);
    cfg -> list = list;

    while (TRUE) {
	char *line = NULL;
	char *key;
	char *val;
	bool s;

	res = kyk_file_getline(fd, &line);
	check(res == 0, "Failed to getline");
	
	if (line == NULL) {
	    break;
	}

	s = kyk_config_parseline(line, &key, &val);
	free(line);
	check(s == TRUE, "Failed to parseline: '%s'", line);
	
	if (key == NULL) {
	    /* comment in the config file */
	    continue;
	}

	kyk_config_setunknownkv(cfg, key, val);
	free(key);
	free(val);
    }

    kyk_free_file_desc(fd);

    return 0;
    
error:
    kyk_free_file_desc(fd);
    if(cfg) kyk_config_free(cfg);
    return -1;
}


void kyk_config_free(struct config *conf)
{
    if (conf == NULL) {
	return;
    }
    config_freekvlist(conf->list);
    free(conf -> fileName);
    free(conf);
}


static void config_freekvlist(struct KeyValuePair *list)
{
    struct KeyValuePair *e;

    e = list;
    while (e) {
	struct KeyValuePair *next;

	next = e->next;
	if (e->type == CONFIG_KV_UNKNOWN || e->type == CONFIG_KV_STRING) {
	    free(e->u.str);
	}
	free(e->key);
	free(e);
	e = next;
    }
}


static bool kyk_config_parseline(char *line,
				 char **key,
				 char **val)
{
    size_t len;
    char *ptr;
    char *k;
    char *v;
    char *v0;
    int res;

    *key = NULL;
    *val = NULL;
    *v0 = NULL;

    len = strlen(line);
    if (line[len - 1] == '\n' || line[len - 1] == '\r') {
	line[len - 1] = '\0';
    }

    ptr = line;
    while (*ptr != '\0' && *ptr == ' ') {
	ptr++;
    }
    if (*ptr == '\n' || *ptr == '\0') {
	return TRUE;
    }
    if (*ptr == '#') {
	return TRUE;
    }

    k = malloc(len + 1);
    check(k != NULL, "Failed to malloc");
    
    v = malloc(len + 1);
    check(v != NULL, "Failed to malloc");

    res = sscanf(ptr, "%[^=]=%[^\n]", k, v);
    check(res == 2, "Failed to parse '%s'", ptr);

    kyk_config_chomp(k);
    kyk_config_chomp(v);

    v0 = v;
    while (*v != '\0' && *v == ' ') {
	v++;
    }
    if (v[0] == '\"') {
	char *l;
	v++;
	l = strrchr(v, '\"');
	check(l != v, "Failed to parse string: '%s'", v);
	*l = '\0';
    }
    v = kyk_strdup(v);
    free(v0);

    *key = k;
    *val = v;
    return TRUE;

error:
    
    if(k) free(k);
    if(v) free(v);
    if(v0) free(v0);
    
    return FALSE;
}


static void kyk_config_chomp(char *str)
{
   ssize_t i = strlen(str) - 1;

   check(i >= 0, "Failed to chomp %s", str);

   while (i > 0 && str[i] == ' ') {
      str[i] = '\0';
      i--;
   }

error:
   return;
}


static void kyk_config_setunknownkv(struct config *config,
				    const char    *key,
				    const char    *val)
{
    struct KeyValuePair *ev;

    ev = malloc(sizeof *ev);
    check(ev != NULL, "Failed to malloc");
    ev -> key   = kyk_strdup(key);
    ev -> u.str = kyk_strdup(val);
    ev -> type  = CONFIG_KV_UNKNOWN;
    ev -> save  = 1;

    kyk_config_insert(config, ev);

error:
    return;
}


static void kyk_config_insert(struct config *config,
			      struct KeyValuePair *ev)
{
    struct KeyValuePair *prev = NULL;
    struct KeyValuePair *item;

    item = config->list;

    while (item && strcmp(item -> key, ev -> key) < 0) {
	prev = item;
	item = item->next;
    }
    if (prev) {
	ev -> next = prev -> next;
	prev -> next = ev;
    } else {
	ev -> next = config -> list;
	config -> list = ev;
    }
}
