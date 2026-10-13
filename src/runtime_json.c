/* sce::Json (libSceJson2, a firmware library) for libSceNpCppWebApi: values, objects, arrays,
 * strings, a parser and a serializer. The caller allocates every object (on its stack or inside
 * its own classes) and only touches it through these imports, so each guest object here holds a
 * single pointer to host memory: String a NUL-terminated copy, Value a node, Array and Object a
 * list of separately allocated items (references stay valid while items are added, as with the
 * SDK's containers). Value types (from Value::set calls before referArray/referObject):
 * null 0, boolean 1, integer 2, unsigned integer 3, real 4, string 5, array 6, object 7. */
#define _GNU_SOURCE
#include "runtime.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { J_NULL, J_BOOLEAN, J_INTEGER, J_UINTEGER, J_REAL, J_STRING, J_ARRAY, J_OBJECT };
#define JSON_ERROR_PARSE ((int32_t)0x80920101) /* callers only check for a negative result */
#define JSON_MAX_DEPTH 256

typedef struct JNode JNode;
typedef struct { char *text; } JString;          /* sce::Json::String */
typedef struct { JNode *node; } JValue;          /* sce::Json::Value */
typedef struct { size_t count, capacity; JValue **items; } JList;
typedef struct { JList *list; } JArray;          /* sce::Json::Array */
typedef struct { JString key; JValue value; } JEntry;
typedef struct { size_t count, capacity; JEntry **items; } JMap;
typedef struct { JMap *map; } JObject;           /* sce::Json::Object */
struct JNode {
    uint32_t type;
    union { bool boolean; int64_t integer; uint64_t uinteger; double real; };
    JString string;
    JArray array;
    JObject object;
};

static const JValue null_value;
static const JString empty_string;
static const JArray empty_array;
static const JObject empty_object;

static void *allocate(size_t size) {
    void *p=calloc(1,size ? size : 1);
    if (!p) { fprintf(stderr,"Runtime: sce::Json out of memory\n"); abort(); }
    return p;
}
static char *copy_text(const char *text, size_t length) {
    char *p=allocate(length+1);
    memcpy(p,text,length);
    return p;
}

static void node_free(JNode *node);
static JNode *node_copy(const JNode *node);
static void list_clear(JList *list) {
    if (!list) return;
    for (size_t i=0;i<list->count;++i) { node_free(list->items[i]->node); free(list->items[i]); }
    list->count=0;
}
static void map_clear(JMap *map) {
    if (!map) return;
    for (size_t i=0;i<map->count;++i) {
        free(map->items[i]->key.text); node_free(map->items[i]->value.node); free(map->items[i]);
    }
    map->count=0;
}
static void list_append(JArray *array, JNode *node) {
    if (!array->list) array->list=allocate(sizeof(JList));
    JList *list=array->list;
    if (list->count==list->capacity) {
        size_t capacity=list->capacity ? list->capacity*2 : 8;
        JValue **items=realloc(list->items,capacity*sizeof(*items));
        if (!items) { fprintf(stderr,"Runtime: sce::Json out of memory\n"); abort(); }
        list->items=items; list->capacity=capacity;
    }
    JValue *value=allocate(sizeof(JValue));
    value->node=node;
    list->items[list->count++]=value;
}
static JEntry *map_find(const JObject *object, const char *key) {
    const JMap *map=object->map;
    if (!map) return NULL;
    for (size_t i=0;i<map->count;++i) if (!strcmp(map->items[i]->key.text,key)) return map->items[i];
    return NULL;
}
/* Insertion order is kept, so serializing gives the members back in the order they were set. */
static JEntry *map_insert(JObject *object, const char *key, size_t length) {
    if (!object->map) object->map=allocate(sizeof(JMap));
    JMap *map=object->map;
    if (map->count==map->capacity) {
        size_t capacity=map->capacity ? map->capacity*2 : 8;
        JEntry **items=realloc(map->items,capacity*sizeof(*items));
        if (!items) { fprintf(stderr,"Runtime: sce::Json out of memory\n"); abort(); }
        map->items=items; map->capacity=capacity;
    }
    JEntry *entry=allocate(sizeof(JEntry));
    entry->key.text=copy_text(key,length);
    map->items[map->count++]=entry;
    return entry;
}
static void array_free(JArray *array) {
    if (!array->list) return;
    list_clear(array->list); free(array->list->items); free(array->list); array->list=NULL;
}
static void object_free(JObject *object) {
    if (!object->map) return;
    map_clear(object->map); free(object->map->items); free(object->map); object->map=NULL;
}
static void array_copy(JArray *to, const JArray *from) {
    if (!from->list) return;
    for (size_t i=0;i<from->list->count;++i) list_append(to,node_copy(from->list->items[i]->node));
}
static void object_copy(JObject *to, const JObject *from) {
    if (!from->map) return;
    for (size_t i=0;i<from->map->count;++i) {
        const JEntry *e=from->map->items[i];
        map_insert(to,e->key.text,strlen(e->key.text))->value.node=node_copy(e->value.node);
    }
}
static void node_free(JNode *node) {
    if (!node) return;
    free(node->string.text);
    array_free(&node->array);
    object_free(&node->object);
    free(node);
}
static JNode *node_new(uint32_t type) {
    JNode *node=allocate(sizeof(JNode));
    node->type=type;
    if (type==J_STRING) node->string.text=copy_text("",0);
    return node;
}
static JNode *node_copy(const JNode *node) {
    if (!node) return NULL;
    JNode *copy=allocate(sizeof(JNode));
    copy->type=node->type; copy->uinteger=node->uinteger;
    if (node->string.text) copy->string.text=copy_text(node->string.text,strlen(node->string.text));
    array_copy(&copy->array,&node->array);
    object_copy(&copy->object,&node->object);
    return copy;
}
static uint32_t type_of(const JValue *v) { return v && v->node ? v->node->type : J_NULL; }

/* ---- String ---- */
static ABI void string_init(JString *s) { s->text=NULL; }
static ABI void string_init_text(JString *s, const char *text) { s->text=text ? copy_text(text,strlen(text)) : NULL; }
static ABI void string_destroy(JString *s) { free(s->text); s->text=NULL; }
static ABI JString *string_assign(JString *s, const JString *from) {
    if (s==from) return s;
    char *text=from->text ? copy_text(from->text,strlen(from->text)) : NULL;
    free(s->text); s->text=text;
    return s;
}
static ABI const char *string_c_str(const JString *s) { return s->text ? s->text : ""; }
static ABI uint64_t string_length(const JString *s) { return s->text ? strlen(s->text) : 0; }

/* ---- Value ---- */
static ABI void value_init(JValue *v) { v->node=NULL; }
static ABI void value_init_bool(JValue *v, bool b) { v->node=node_new(J_BOOLEAN); v->node->boolean=b; }
static ABI void value_init_integer(JValue *v, int64_t i) { v->node=node_new(J_INTEGER); v->node->integer=i; }
static ABI void value_init_real(JValue *v, double r) { v->node=node_new(J_REAL); v->node->real=r; }
static ABI void value_init_text(JValue *v, const char *text) {
    v->node=node_new(J_STRING);
    if (text) { free(v->node->string.text); v->node->string.text=copy_text(text,strlen(text)); }
}
static ABI void value_init_object(JValue *v, const JObject *object) {
    v->node=node_new(J_OBJECT);
    object_copy(&v->node->object,object);
}
static ABI void value_destroy(JValue *v) { node_free(v->node); v->node=NULL; }
static ABI JValue *value_assign(JValue *v, const JValue *from) {
    if (v==from) return v;
    JNode *copy=node_copy(from->node);
    node_free(v->node); v->node=copy;
    return v;
}
static ABI void value_set(JValue *v, uint32_t type) {
    node_free(v->node);
    v->node=type==J_NULL || type>J_OBJECT ? NULL : node_new(type);
}
static ABI uint32_t value_type(const JValue *v) { return type_of(v); }
static ABI bool value_boolean(const JValue *v) { return type_of(v)==J_BOOLEAN && v->node->boolean; }
static ABI int64_t value_integer(const JValue *v) {
    switch (type_of(v)) {
    case J_INTEGER: return v->node->integer;
    case J_UINTEGER: return (int64_t)v->node->uinteger;
    case J_REAL: return (int64_t)v->node->real;
    default: return 0;
    }
}
static ABI double value_real(const JValue *v) {
    switch (type_of(v)) {
    case J_INTEGER: return (double)v->node->integer;
    case J_UINTEGER: return (double)v->node->uinteger;
    case J_REAL: return v->node->real;
    default: return 0.0;
    }
}
static ABI const JString *value_string(const JValue *v) { return type_of(v)==J_STRING ? &v->node->string : &empty_string; }
static ABI const JArray *value_array(const JValue *v) { return type_of(v)==J_ARRAY ? &v->node->array : &empty_array; }
static ABI const JObject *value_object(const JValue *v) { return type_of(v)==J_OBJECT ? &v->node->object : &empty_object; }
static ABI JArray *value_refer_array(JValue *v) { return type_of(v)==J_ARRAY ? &v->node->array : NULL; }
static ABI JObject *value_refer_object(JValue *v) { return type_of(v)==J_OBJECT ? &v->node->object : NULL; }
static ABI const JValue *value_member(const JValue *v, const char *key) {
    if (type_of(v)!=J_OBJECT || !key) return &null_value;
    const JEntry *e=map_find(&v->node->object,key);
    return e ? &e->value : &null_value;
}
static ABI const JValue *value_element(const JValue *v, uint64_t index) {
    if (type_of(v)!=J_ARRAY || !v->node->array.list || index>=v->node->array.list->count) return &null_value;
    return v->node->array.list->items[index];
}

/* ---- Array ---- */
static ABI uint64_t array_size(const JArray *a) { return a->list ? a->list->count : 0; }
static ABI void array_push_back(JArray *a, const JValue *v) { list_append(a,node_copy(v->node)); }
static ABI const JValue *array_back(const JArray *a) {
    return a->list && a->list->count ? a->list->items[a->list->count-1] : &null_value;
}

/* ---- Object ---- */
static ABI void object_init(JObject *o) { o->map=NULL; }
static ABI void object_init_copy(JObject *o, const JObject *from) { o->map=NULL; object_copy(o,from); }
static ABI void object_destroy(JObject *o) { object_free(o); }
static ABI JObject *object_assign(JObject *o, const JObject *from) {
    if (o==from) return o;
    JObject copy={NULL};
    object_copy(&copy,from);
    object_free(o); *o=copy;
    return o;
}
static ABI void object_clear(JObject *o) { map_clear(o->map); }
static ABI JValue *object_member(JObject *o, const JString *key) {
    const char *k=string_c_str(key);
    JEntry *e=map_find(o,k);
    return e ? &e->value : &map_insert(o,k,strlen(k))->value;
}

/* ---- Parser ---- */
typedef struct { const char *p, *end; int depth; } Reader;
static void skip_space(Reader *r) { while (r->p<r->end && (*r->p==' ' || *r->p=='\t' || *r->p=='\n' || *r->p=='\r')) ++r->p; }
static int hex4(Reader *r, uint32_t *out) {
    if (r->end-r->p<4) return 0;
    *out=0;
    for (int i=0;i<4;++i) {
        char c=*r->p++;
        *out<<=4;
        if (c>='0' && c<='9') *out|=(uint32_t)(c-'0');
        else if (c>='a' && c<='f') *out|=(uint32_t)(c-'a'+10);
        else if (c>='A' && c<='F') *out|=(uint32_t)(c-'A'+10);
        else return 0;
    }
    return 1;
}
static void put_utf8(char *out, size_t *n, uint32_t c) {
    if (c<0x80) out[(*n)++]=(char)c;
    else if (c<0x800) { out[(*n)++]=(char)(0xC0|c>>6); out[(*n)++]=(char)(0x80|(c&63)); }
    else if (c<0x10000) { out[(*n)++]=(char)(0xE0|c>>12); out[(*n)++]=(char)(0x80|((c>>6)&63)); out[(*n)++]=(char)(0x80|(c&63)); }
    else { out[(*n)++]=(char)(0xF0|c>>18); out[(*n)++]=(char)(0x80|((c>>12)&63)); out[(*n)++]=(char)(0x80|((c>>6)&63)); out[(*n)++]=(char)(0x80|(c&63)); }
}
/* A quoted string (r->p at the opening quote) into a new NUL-terminated buffer. */
static char *read_string(Reader *r, size_t *length) {
    ++r->p;
    char *out=allocate((size_t)(r->end-r->p)+1); /* escapes never make it longer */
    size_t n=0;
    while (r->p<r->end && *r->p!='"') {
        unsigned char c=(unsigned char)*r->p++;
        if (c<0x20) { free(out); return NULL; }
        if (c!='\\') { out[n++]=(char)c; continue; }
        if (r->p>=r->end) break;
        char e=*r->p++;
        uint32_t u;
        switch (e) {
        case '"': case '\\': case '/': out[n++]=e; break;
        case 'b': out[n++]='\b'; break;
        case 'f': out[n++]='\f'; break;
        case 'n': out[n++]='\n'; break;
        case 'r': out[n++]='\r'; break;
        case 't': out[n++]='\t'; break;
        case 'u':
            if (!hex4(r,&u)) { free(out); return NULL; }
            if (u>=0xD800 && u<0xDC00) { /* a surrogate pair */
                uint32_t low;
                if (r->end-r->p<6 || r->p[0]!='\\' || r->p[1]!='u') { free(out); return NULL; }
                r->p+=2;
                if (!hex4(r,&low) || low<0xDC00 || low>=0xE000) { free(out); return NULL; }
                u=0x10000+((u-0xD800)<<10)+(low-0xDC00);
            }
            put_utf8(out,&n,u);
            break;
        default: free(out); return NULL;
        }
    }
    if (r->p>=r->end) { free(out); return NULL; }
    ++r->p;
    out[n]=0; *length=n;
    return out;
}
static JNode *read_value(Reader *r);
static JNode *read_number(Reader *r) {
    const char *start=r->p;
    int real=0;
    if (r->p<r->end && *r->p=='-') ++r->p;
    while (r->p<r->end && ((*r->p>='0' && *r->p<='9') || *r->p=='.' || *r->p=='e' || *r->p=='E' || *r->p=='+' || *r->p=='-')) {
        if (*r->p=='.' || *r->p=='e' || *r->p=='E') real=1;
        ++r->p;
    }
    size_t length=(size_t)(r->p-start);
    if (!length || length>=64) return NULL;
    char text[64], *end;
    memcpy(text,start,length); text[length]=0;
    JNode *node;
    if (!real) {
        if (text[0]=='-') {
            long long i=strtoll(text,&end,10);
            if (!*end) { node=node_new(J_INTEGER); node->integer=i; return node; }
        } else {
            unsigned long long u=strtoull(text,&end,10);
            if (!*end) {
                if (u<=INT64_MAX) { node=node_new(J_INTEGER); node->integer=(int64_t)u; }
                else { node=node_new(J_UINTEGER); node->uinteger=u; }
                return node;
            }
        }
    }
    double d=strtod(text,&end);
    if (*end || end==text) return NULL;
    node=node_new(J_REAL); node->real=d;
    return node;
}
static int literal(Reader *r, const char *word) {
    size_t n=strlen(word);
    if ((size_t)(r->end-r->p)<n || memcmp(r->p,word,n)) return 0;
    r->p+=n; return 1;
}
static JNode *read_value(Reader *r) {
    skip_space(r);
    if (r->p>=r->end || r->depth>JSON_MAX_DEPTH) return NULL;
    JNode *node;
    char c=*r->p;
    if (c=='{') {
        ++r->p; ++r->depth;
        node=node_new(J_OBJECT);
        skip_space(r);
        if (r->p<r->end && *r->p=='}') { ++r->p; --r->depth; return node; }
        for (;;) {
            skip_space(r);
            size_t length;
            char *key=r->p<r->end && *r->p=='"' ? read_string(r,&length) : NULL;
            if (!key) { node_free(node); return NULL; }
            skip_space(r);
            JNode *member=NULL;
            if (r->p<r->end && *r->p==':') { ++r->p; member=read_value(r); }
            if (!member) { free(key); node_free(node); return NULL; }
            JEntry *e=map_find(&node->object,key);
            if (e) node_free(e->value.node); /* a repeated key: the last one wins */
            else e=map_insert(&node->object,key,length);
            e->value.node=member;
            free(key);
            skip_space(r);
            if (r->p<r->end && *r->p==',') { ++r->p; continue; }
            if (r->p<r->end && *r->p=='}') { ++r->p; --r->depth; return node; }
            node_free(node); return NULL;
        }
    }
    if (c=='[') {
        ++r->p; ++r->depth;
        node=node_new(J_ARRAY);
        skip_space(r);
        if (r->p<r->end && *r->p==']') { ++r->p; --r->depth; return node; }
        for (;;) {
            JNode *element=read_value(r);
            if (!element) { node_free(node); return NULL; }
            list_append(&node->array,element);
            skip_space(r);
            if (r->p<r->end && *r->p==',') { ++r->p; continue; }
            if (r->p<r->end && *r->p==']') { ++r->p; --r->depth; return node; }
            node_free(node); return NULL;
        }
    }
    if (c=='"') {
        size_t length;
        char *text=read_string(r,&length);
        if (!text) return NULL;
        node=allocate(sizeof(JNode)); node->type=J_STRING; node->string.text=text;
        return node;
    }
    if (literal(r,"true")) { node=node_new(J_BOOLEAN); node->boolean=true; return node; }
    if (literal(r,"false")) { node=node_new(J_BOOLEAN); return node; }
    if (literal(r,"null")) return node_new(J_NULL);
    return read_number(r);
}
static ABI int32_t parser_parse(JValue *v, const char *text, uint64_t size) {
    if (!v || !text) return JSON_ERROR_PARSE;
    Reader r={text,text+size,0};
    JNode *node=read_value(&r);
    skip_space(&r);
    if (!node || (r.p<r.end && *r.p)) { node_free(node); return JSON_ERROR_PARSE; } /* a trailing NUL is fine */
    node_free(v->node); v->node=node;
    return 0;
}

/* ---- Serializer ---- */
typedef struct { char *data; size_t length, capacity; } Buffer;
static void put(Buffer *b, const char *s, size_t n) {
    if (b->length+n+1>b->capacity) {
        size_t capacity=b->capacity ? b->capacity : 64;
        while (b->length+n+1>capacity) capacity*=2;
        char *data=realloc(b->data,capacity);
        if (!data) { fprintf(stderr,"Runtime: sce::Json out of memory\n"); abort(); }
        b->data=data; b->capacity=capacity;
    }
    memcpy(b->data+b->length,s,n); b->length+=n; b->data[b->length]=0;
}
static void put_string(Buffer *b, const char *s) {
    put(b,"\"",1);
    for (; *s; ++s) {
        unsigned char c=(unsigned char)*s;
        char escape[8];
        if (c=='"') put(b,"\\\"",2);
        else if (c=='\\') put(b,"\\\\",2);
        else if (c=='\n') put(b,"\\n",2);
        else if (c=='\r') put(b,"\\r",2);
        else if (c=='\t') put(b,"\\t",2);
        else if (c<0x20) { snprintf(escape,sizeof(escape),"\\u%04x",c); put(b,escape,6); }
        else put(b,s,1);
    }
    put(b,"\"",1);
}
static void put_node(Buffer *b, const JNode *node) {
    char text[40];
    switch (node ? node->type : J_NULL) {
    case J_BOOLEAN: if (node->boolean) put(b,"true",4); else put(b,"false",5); break;
    case J_INTEGER: put(b,text,(size_t)snprintf(text,sizeof(text),"%lld",(long long)node->integer)); break;
    case J_UINTEGER: put(b,text,(size_t)snprintf(text,sizeof(text),"%llu",(unsigned long long)node->uinteger)); break;
    case J_REAL:
        if (isfinite(node->real)) put(b,text,(size_t)snprintf(text,sizeof(text),"%.17g",node->real));
        else put(b,"null",4);
        break;
    case J_STRING: put_string(b,node->string.text ? node->string.text : ""); break;
    case J_ARRAY:
        put(b,"[",1);
        for (size_t i=0;node->array.list && i<node->array.list->count;++i) {
            if (i) put(b,",",1);
            put_node(b,node->array.list->items[i]->node);
        }
        put(b,"]",1);
        break;
    case J_OBJECT:
        put(b,"{",1);
        for (size_t i=0;node->object.map && i<node->object.map->count;++i) {
            if (i) put(b,",",1);
            put_string(b,node->object.map->items[i]->key.text);
            put(b,":",1);
            put_node(b,node->object.map->items[i]->value.node);
        }
        put(b,"}",1);
        break;
    default: put(b,"null",4);
    }
}
static ABI int32_t value_serialize(const JValue *v, JString *out) {
    Buffer b={0};
    put_node(&b,v->node);
    free(out->text); out->text=b.data;
    return 0;
}

/* Set-up: MemAllocator's base constructor leaves the vtable to the game's derived class (the
 * allocations here use the host heap); Initializer has nothing to set up. */
static ABI int32_t json_noop(void *self) { (void)self; return 0; }

static const RuntimeExport exports[]={
    {"_ZN3sce4Json12MemAllocatorC2Ev",json_noop}, {"_ZN3sce4Json12MemAllocatorD2Ev",json_noop},
    {"_ZN3sce4Json11InitializerC1Ev",json_noop}, {"_ZN3sce4Json11InitializerD1Ev",json_noop},
    {"_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE",json_noop},
    {"_ZN3sce4Json6StringC1Ev",string_init}, {"_ZN3sce4Json6StringC1EPKc",string_init_text},
    {"_ZN3sce4Json6StringD1Ev",string_destroy}, {"_ZN3sce4Json6StringaSERKS1_",string_assign},
    {"_ZNK3sce4Json6String5c_strEv",string_c_str}, {"_ZNK3sce4Json6String6lengthEv",string_length},
    {"_ZN3sce4Json5ValueC1Ev",value_init}, {"_ZN3sce4Json5ValueC1Eb",value_init_bool},
    {"_ZN3sce4Json5ValueC1El",value_init_integer}, {"_ZN3sce4Json5ValueC1Ed",value_init_real},
    {"_ZN3sce4Json5ValueC1EPKc",value_init_text}, {"_ZN3sce4Json5ValueC1ERKNS0_6ObjectE",value_init_object},
    {"_ZN3sce4Json5ValueD1Ev",value_destroy}, {"_ZN3sce4Json5ValueaSERKS1_",value_assign},
    {"_ZN3sce4Json5Value3setENS0_9ValueTypeE",value_set}, {"_ZNK3sce4Json5Value7getTypeEv",value_type},
    {"_ZNK3sce4Json5Value10getBooleanEv",value_boolean}, {"_ZNK3sce4Json5Value10getIntegerEv",value_integer},
    {"_ZNK3sce4Json5Value7getRealEv",value_real}, {"_ZNK3sce4Json5Value9getStringEv",value_string},
    {"_ZNK3sce4Json5Value8getArrayEv",value_array}, {"_ZNK3sce4Json5Value9getObjectEv",value_object},
    {"_ZN3sce4Json5Value10referArrayEv",value_refer_array}, {"_ZN3sce4Json5Value11referObjectEv",value_refer_object},
    {"_ZNK3sce4Json5ValueixEPKc",value_member}, {"_ZNK3sce4Json5ValueixEm",value_element},
    {"_ZN3sce4Json5Value9serializeERNS0_6StringE",value_serialize},
    {"_ZNK3sce4Json5Array4sizeEv",array_size}, {"_ZN3sce4Json5Array9push_backERKNS0_5ValueE",array_push_back},
    {"_ZNK3sce4Json5Array4backEv",array_back},
    {"_ZN3sce4Json6ObjectC1Ev",object_init}, {"_ZN3sce4Json6ObjectC1ERKS1_",object_init_copy},
    {"_ZN3sce4Json6ObjectD1Ev",object_destroy}, {"_ZN3sce4Json6ObjectaSERKS1_",object_assign},
    {"_ZN3sce4Json6Object5clearEv",object_clear}, {"_ZN3sce4Json6ObjectixERKNS0_6StringE",object_member},
    {"_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm",parser_parse},
};
uintptr_t runtime_json_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
