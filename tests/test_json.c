/* sce::Json: building, serializing, parsing and copying values the way libSceNpCppWebApi does. */
#undef NDEBUG /* the checks are asserts; release builds define NDEBUG */
#include "../src/runtime_json.c"
#include <assert.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }

static const char *serialized(const JValue *v) {
    static JString out;
    assert(value_serialize(v,&out)==0);
    return string_c_str(&out);
}

int main(void) {
    /* Build {"name":"Alain","level":5,"tags":["a",true],"ratio":0.5} as the library does. */
    JValue root; value_init(&root);
    assert(value_type(&root)==J_NULL && value_refer_object(&root)==NULL);
    value_set(&root,J_OBJECT);
    JObject *object=value_refer_object(&root);
    assert(object);
    JString key; string_init_text(&key,"name");
    JValue text; value_init_text(&text,"Alain");
    value_assign(object_member(object,&key),&text);
    string_destroy(&key);
    string_init_text(&key,"level");
    JValue *level=object_member(object,&key); /* stays valid while more members are added */
    JString k2; string_init_text(&k2,"tags");
    JValue *tags=object_member(object,&k2);
    value_set(tags,J_ARRAY);
    JArray *array=value_refer_array(tags);
    assert(array && value_refer_array(level)==NULL);
    JValue element; value_init_text(&element,"a");
    array_push_back(array,&element);
    value_destroy(&element);
    value_init_bool(&element,true);
    array_push_back(array,&element);
    assert(array_size(array)==2 && value_boolean(array_back(array)));
    JValue five; value_init_integer(&five,5);
    value_assign(level,&five);
    JString k3; string_init_text(&k3,"ratio");
    JValue half; value_init_real(&half,0.5);
    value_assign(object_member(object,&k3),&half);
    const char *json=serialized(&root);
    assert(!strcmp(json,"{\"name\":\"Alain\",\"level\":5,\"tags\":[\"a\",true],\"ratio\":0.5}"));

    /* Parse it back; read through const accessors. */
    JValue parsed; value_init(&parsed);
    assert(parser_parse(&parsed,json,strlen(json))==0 && value_type(&parsed)==J_OBJECT);
    assert(!strcmp(string_c_str(value_string(value_member(&parsed,"name"))),"Alain"));
    assert(value_integer(value_member(&parsed,"level"))==5 && value_real(value_member(&parsed,"ratio"))==0.5);
    const JValue *t=value_member(&parsed,"tags");
    assert(value_type(t)==J_ARRAY && array_size(value_array(t))==2);
    assert(!strcmp(string_c_str(value_string(value_element(t,0))),"a") && value_boolean(value_element(t,1)));
    assert(value_type(value_element(t,5))==J_NULL && value_type(value_member(&parsed,"missing"))==J_NULL);
    assert(string_length(value_string(value_member(&parsed,"missing")))==0);

    /* Escapes, unicode, numbers, nesting, whitespace and a trailing NUL inside the size. */
    const char *tricky=" {\"s\":\"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\",\"n\":-12,\"big\":18446744073709551615,"
                       "\"e\":1.5e3,\"z\":null,\"o\":{\"x\":[[],{}]}} ";
    JValue v; value_init(&v);
    assert(parser_parse(&v,tricky,strlen(tricky)+1)==0);
    assert(!strcmp(string_c_str(value_string(value_member(&v,"s"))),"a\"b\\c\n\xc3\xa9\xf0\x9f\x98\x80"));
    assert(value_integer(value_member(&v,"n"))==-12 && value_type(value_member(&v,"big"))==J_UINTEGER);
    assert(value_real(value_member(&v,"e"))==1500.0 && value_type(value_member(&v,"z"))==J_NULL);
    assert(!strcmp(serialized(&v),"{\"s\":\"a\\\"b\\\\c\\n\xc3\xa9\xf0\x9f\x98\x80\",\"n\":-12,\"big\":18446744073709551615,"
                                  "\"e\":1500,\"z\":null,\"o\":{\"x\":[[],{}]}}"));

    /* Errors leave the value alone. */
    const char *bad[]={"","{","{\"a\":}","[1,]","tru","\"open","{\"a\" 1}","[1] x","\"\\x\""};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) assert(parser_parse(&v,bad[i],strlen(bad[i]))<0);
    assert(value_type(&v)==J_OBJECT);

    /* Copies are deep. */
    JObject copy; object_init_copy(&copy,value_object(&parsed));
    JString nk; string_init_text(&nk,"name");
    value_assign(object_member(&copy,&nk),&five);
    assert(!strcmp(string_c_str(value_string(value_member(&parsed,"name"))),"Alain"));
    JValue from_object; value_init_object(&from_object,&copy);
    assert(value_integer(value_member(&from_object,"name"))==5);
    JObject assigned; object_init(&assigned);
    object_assign(&assigned,&copy);
    object_assign(&assigned,&assigned);
    object_clear(&copy);
    assert(!strcmp(serialized(&(JValue){NULL}),"null"));
    JString s1, s2; string_init_text(&s1,"x"); string_init(&s2);
    string_assign(&s2,&s1); string_assign(&s2,&s2);
    assert(!strcmp(string_c_str(&s2),"x") && string_length(&s2)==1);

    value_assign(&root,&root);
    value_set(&root,J_NULL);
    value_destroy(&root); value_destroy(&parsed); value_destroy(&v); value_destroy(&from_object);
    value_destroy(&text); value_destroy(&element); value_destroy(&five); value_destroy(&half);
    object_destroy(&copy); object_destroy(&assigned);
    string_destroy(&key); string_destroy(&k2); string_destroy(&k3); string_destroy(&nk); string_destroy(&s1); string_destroy(&s2);
    puts("json test: ok");
    return 0;
}
