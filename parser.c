/*
 * author: Nolan <sullen.goose@gmail.com>
 * Copy if you can.
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

#define ARRAY_INITIAL_CAPACITY 4
#define OBJECT_INITIAL_CAPACITY 8

enum LexemeKind {
    JLK_L_BRACE, JLK_R_BRACE, JLK_L_SQUARE_BRACKET, JLK_R_SQUARE_BRACKET,
    JLK_COMMA, JLK_COLON, JLK_TRUE, JLK_FALSE, JLK_NULL, JLK_STRING, JLK_NUMBER
};

struct LexemeData {
    enum LexemeKind kind;
    const char * begin;
    union {
        size_t unescaped_length;
        double number;
    } ex;
};

static int utf8_bytes_left[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5, -1, -1,
};

static int is_trailing_utf8_byte[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static uint32_t ctoh[256] = {
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
   0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/* Returns length of utf8 code point representing 'hex' unicode character. */
static int get_utf8_code_point_length(int32_t hex) {
    if (hex <= 0x007f)
        return 1;
    else if (0x0080 <= hex && hex <= 0x07ff)
        return 2;
    return 3;
}

/* Writes code point representing 'hex' unicode character into 'out' and returns
   pointer to first byte after written code point. */
static char * put_utf8_code_point(int32_t hex, char * out) {
    if (hex <= 0x007f) {
        *out++ = hex;
    } else if (0x0080 <= hex && hex <= 0x07ff) {
        *out++ = 0xc0 | hex >> 6;
        *out++ = 0x80 | (hex & 0x3f);
    } else {
        *out++ = 0xe0 | hex >> 12;
        *out++ = 0x80 | ((hex >> 6) & 0x3f);
        *out++ = 0x80 | (hex & 0x3f);
    }
    return out;
}

/* Copies code point from 'code_point' to 'out'. 'end' must point to first byte after
   memory block 'out' points to. If there's not enough memory to fit new code point in
   'out' function returns 0 and length of code point otherwise. */
static size_t copy_code_point(const char * code_point, char * out, char * end) {
    int bytes_left = utf8_bytes_left[*(unsigned char *)code_point];
    int i;
    if (-1 == bytes_left) return 0;
    if (end - out < bytes_left + 1) return 0;
    *out++ = *code_point++;
    for (i = 0; i < bytes_left; ++i) {
        if (!is_trailing_utf8_byte[*(unsigned char *)code_point]) return 0;
        *out++ = *code_point++;
    }
    return bytes_left + 1;
}

#define E(c) case (c): *out = (c); break;

#define F(c, d) case (c): *out = (d); break;

/* Unescapes json string's escape sequence pointed by 'escaped' and writes this
   code point to 'out'. */
static int unescape_code_point(const char * escaped, char * out, char * end) {
    const char * begin = escaped;
    char c;
    uint32_t hex;
    int i;
    switch (*escaped++) {
    E('\\') E('\"') E('/') F('b', '\b') F('f', '\f') F('n', '\n') F('r', '\r') F('t', '\t')
    case 'u':
        hex = 0;
        for (i = 0; i < 4; ++i) {
            c = *escaped++;
            if (!isxdigit(c)) return 0;
            hex = (hex << 4) | ctoh[*(unsigned char *)&c];
        }
        if (end - out < get_utf8_code_point_length(hex)) return 0;
        put_utf8_code_point(hex, out);
        break;
    default: return 0;
    }
    return escaped - begin;
}

/* Allocates 'data->measured_length' bytes, removes json escape sequences from
   string starting at 'data->begin' until unescaped quote(") character and writes it
   into allocated memory. If string was correct and memory was allocated returns
   pointer to allocated memory otherwise returns NULL. */
extern char * unescape_string(struct LexemeData * data) {
    const char * json = data->begin;
    char * unescaped = json_malloc(data->ex.unescaped_length);
    char * end = unescaped + data->ex.unescaped_length;
    char * out = unescaped;
    size_t bytes_read;
    if (!unescaped) return NULL;
    while (1) {
        if (out >= end) goto fail;
        switch (*json) {
        case '"':
            *out++ = '\0';
            assert(out == end);
            return unescaped;
        case '\\':
            ++json;
            if (!(bytes_read = unescape_code_point(json, out, end))) goto fail;
            json += bytes_read;
            out += utf8_bytes_left[*(unsigned char *)out] + 1;
            break;
        default:
            if (!(bytes_read = copy_code_point(json, out, end))) goto fail;
            out += bytes_read;
            json += bytes_read;
            break;
        }
    }
fail:
    json_free(unescaped);
    return NULL;
}

/* 'escape' must point to char after backslash '\' symbol. The function calculates
   length of code point produced by this escape sequence and puts it into
   location pointed by 'code_pont_length'. If escape sequence was valid returns
   number of bytes read otherwise returns 0. */
static size_t measure_escape_sequence(const char * escape, size_t * code_point_length) {
    uint32_t hex;
    char * end;
    switch (*escape) {
    case '\\': case '\"': case '/': case 'b':
    case 'f': case 'n': case 'r': case 't':
        *code_point_length = 1;
        return 1;
    case 'u':
        ++escape;
        hex = strtoul(escape, &end, 16);
        if (end - escape != 4) return 0;
        *code_point_length = get_utf8_code_point_length(hex);
        return 5;
    default: return 0;
    }
}

/* Checks that 'code_point' points to valid code point. If it is valid returns
   its lengths in bytes otherwise returns 0. */
static size_t check_code_point(const char * code_point) {
    const char * begin = code_point;
    int bytes_left = utf8_bytes_left[*(const unsigned char *)code_point];
    if (-1 == bytes_left) return 0;
    ++code_point;
    while (bytes_left--) {
        if (!is_trailing_utf8_byte[*(unsigned char *)code_point]) return 0;
        ++code_point;
    }
    return code_point - begin;
}

/* Validates and calculates length of json string after unescaping escape sequences
   including terminating '\0'. This length is written into location pointed by
   'unescaped_length'. 'json' must point to first char inside of quotes(").
   Returns number of bytes read if string was valid and 0 otherwise. */
static size_t measure_string_lexeme(const char * json, size_t * unescaped_length) {
    const char * begin = json;
    size_t code_point_length;
    size_t bytes_read;
    size_t ul = 0;
    while (1) {
        switch (*json) {
        case '"':
            ++json;
            *unescaped_length = ul + 1;
            return json - begin;
        case '\\':
            ++json;
            if (!(bytes_read = measure_escape_sequence(json, &code_point_length))) {
                return 0;
            }
            json += bytes_read;
            ul += code_point_length;
            break;
        default:
            if (!(bytes_read = check_code_point(json))) return 0;
            json += bytes_read;
            ul += bytes_read;
            break;
        }
    }
}

/* Returns non zero value if 'prefix' is prefix of 'str' and 0 otherwise. */
static int is_prefix(const char * prefix, const char * str) {
    while (*prefix && *str && *prefix++ == *str++);
    return *prefix;
}

#define SINGLE_CHAR_LEXEME(c, k) \
    case (c): \
        data->kind = (k); \
        return json + 1 - begin;

#define CONCRETE_WORD_LEXEME(c, w, k) \
    case (c):  \
        if (is_prefix((w), json)) return 0; \
        data->kind = (k); \
        return json - begin + sizeof(w) - 1;

/* Reads next lexeme pointed by 'json' and collects data about it. Lexeme can
   be preceded by whitespaces. Data about lexeme is written into location pointed
   by 'data'. Fields of the data that will be filled always: kind, begin. Ex field
   will be filled if kind is JLK_NUMBER or JLK_STRING it will be number and
   unescaped_length correspondingly. Returns number of bytes read if lexeme was
   successfully read and 0 otherwise. */
extern size_t next_lexeme(const char * json, struct LexemeData * data) {
    const char * begin = json;
    char * end;
    size_t t;
    while (isspace(*json)) ++json;
    data->begin = json;
    switch (*json) {
    case '\0': return 0;
    SINGLE_CHAR_LEXEME('{', JLK_L_BRACE)
    SINGLE_CHAR_LEXEME('}', JLK_R_BRACE)
    SINGLE_CHAR_LEXEME('[', JLK_L_SQUARE_BRACKET)
    SINGLE_CHAR_LEXEME(']', JLK_R_SQUARE_BRACKET)
    SINGLE_CHAR_LEXEME(',', JLK_COMMA)
    SINGLE_CHAR_LEXEME(':', JLK_COLON)
    CONCRETE_WORD_LEXEME('t', "true",  JLK_TRUE)
    CONCRETE_WORD_LEXEME('f', "false", JLK_FALSE)
    CONCRETE_WORD_LEXEME('n', "null",  JLK_NULL)
    case '"':
        ++json;
        t = measure_string_lexeme(json, &data->ex.unescaped_length);
        if (!t) return 0;
        data->begin = json;
        data->kind = JLK_STRING;
        json += t;
        return json - begin;
    default:
        data->ex.number = strtod(json, &end);
        if (end == json) return 0;
        json = end;
        data->kind = JLK_NUMBER;
        return json - begin;
    }
}

/* String hash function. */
static size_t hash(unsigned char * str) {
    size_t hash = 5381;
    size_t c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

static int json_object_grow(struct jsonObject * object);

static int json_object_add_pair(struct jsonObject * object, char * key,
        struct jsonValue value) {
    if (3 * object->size >= 2 * object->capacity) {
        if (!json_object_grow(object)) return 0;
    }
    size_t h = hash((unsigned char *) key) % object->capacity;
    while (object->keys[h]) {
        if (!strcmp(key, object->keys[h])) return 0;
        h = (h + 1) % object->capacity;
    }
    object->keys[h] = key;
    object->values[h] = value;
    ++object->size;
    return 0;
}

static int json_object_grow(struct jsonObject * object) {
    assert(object->capacity);
    size_t old_capacity = object->capacity;
    char ** old_keys = object->keys;
    struct jsonValue * old_values = object->values;
    size_t i;
    size_t keys_size = 2 * object->capacity * sizeof(char *);
    size_t values_size = 2 * object->capacity * sizeof(struct jsonValue);
    object->size = 0;
    object->capacity *= 2;
    object->keys = json_malloc(keys_size + values_size);
    if (!object->keys) return 0;
    object->values = (struct jsonValue *) ((char *) object->keys + keys_size);
    memset(object->keys, 0, keys_size);
    for (i = 0; i < old_capacity; ++i) {
        if (!old_keys[i]) continue;
        json_object_add_pair(object, old_keys[i], old_values[i]);
    }
    json_free(old_keys);
    return 1;
}

/* Allocates and initializes jsonObject and returns pointer to it. Never call
   json_object_add_pair and json_object_grow on object that was created with
   'initial_capacity' = 0. */
static struct jsonObject * json_object_create(size_t initial_capacity) {
    struct jsonObject * object = json_malloc(sizeof(struct jsonObject));
    if (!object) return NULL;
    object->capacity = initial_capacity;
    object->size = 0;
    if (!initial_capacity) {
        object->keys = NULL;
        object->values = NULL;
        return object;
    }
    object->keys = json_malloc(initial_capacity *
            (sizeof(char *) + sizeof(struct jsonValue)));
    if (!object->keys) {
        json_free(object);
        return NULL;
    }
    object->values = (struct jsonValue *)((char *) object->keys +
            initial_capacity * sizeof(char *));
    memset(object->keys, 0, initial_capacity *
            (sizeof(char *) + sizeof(struct jsonValue)));
    return object;
}

/* Returns value of attribute 'key' of object 'object'. */
extern struct jsonValue json_object_get_value(struct jsonObject * object,
        const char * key) {
    size_t h = hash((unsigned char *)key) % object->capacity;
    struct jsonValue value;
    while (object->keys[h] && strcmp(object->keys[h], key)) {
        h = (h + 1) % object->capacity;
    }
    if (object->keys[h]) return object->values[h];
    memset(&value, 0, sizeof(struct jsonValue));
    return value;
}

static void json_object_free(struct jsonObject * object) {
    size_t i;
    if (object->capacity) {
        for (i = 0; i < object->capacity; ++i) {
            if (!object->keys[i]) continue;
            json_free(object->keys[i]);
            json_value_free(object->values[i]);
        }
        json_free(object->keys);
    }
    json_free(object);
}

/* Calls 'action' for each attribute of 'object' with its key, value and 'user_data'. */
extern void json_object_for_each(struct jsonObject * object,
        void (*action)(const char * key, struct jsonValue, void *), void * user_data) {
    if (!object->size) return;
    size_t i;
    for (i = 0; i < object->capacity; ++i) {
        if (!object->keys[i]) continue;
        action(object->keys[i], object->values[i], user_data);
    }
}

/* Parses json object and stores pointer to it into '*object'. 'json' must
   point to next lexeme after '{'. Object will be created in heap so don't
   forget to free it using json_object_free function. */
static size_t json_parse_object(const char * json, struct jsonObject ** object) {
    const char * begin = json;
    char * name;
    struct jsonValue value;
    struct LexemeData data;
    size_t bytes_read;
    if (!(bytes_read = next_lexeme(json, &data))) return 0;
    json += bytes_read;
    if (data.kind == JLK_R_BRACE) {
        *object = json_object_create(0);
        return json - begin;
    }
    *object = json_object_create(10);
    while (1) {
        if (JLK_STRING != data.kind) goto fail2;
        name = unescape_string(&data);
        if (!name) goto fail2;
        if (!(bytes_read = next_lexeme(json, &data)) || JLK_COLON != data.kind) goto fail1;
        json += bytes_read;
        if (!(bytes_read = json_parse_value(json, &value))) goto fail1;
        json += bytes_read;
        json_object_add_pair(*object, name, value);
        if (!(bytes_read = next_lexeme(json, &data))) goto fail2;
        json += bytes_read;
        if (JLK_R_BRACE == data.kind) break;
        if (JLK_COMMA != data.kind) goto fail2;
        if (!(bytes_read = next_lexeme(json, &data))) goto fail2;
        json += bytes_read;
    }
    return json - begin;
fail1:
    json_free(name);
fail2:
    json_object_free(*object);
    *object = NULL;
    return 0;
}

/* Allocates and initializes json array with 'initial capacity' capacity and returns
   pointer to it. So don't forget to call json_array_free. If
   'initial_capacity' is 0 such array can't hold any values. */
static struct jsonArray * json_array_create(size_t initial_capacity) {
    struct jsonArray * array = json_malloc(sizeof(struct jsonArray));
    if (!array) return 0;
    array->size = 0;
    array->capacity = initial_capacity;
    if (initial_capacity) {
        array->values = json_malloc(initial_capacity * sizeof(struct jsonValue));
        return array->values ? array : 0;
    }
    array->values = NULL;
    return array;
}

/* Adds 'value' into 'array'. Returns 1 if value was successfully added and 0
   otherwise. */
static int json_array_add(struct jsonArray * array, struct jsonValue value) {
    assert(array->capacity);
    struct jsonValue * new_values;
    size_t new_capacity;
    if (array->size == array->capacity) {
        new_capacity = 2 * array->capacity;
        new_values = json_malloc(new_capacity * sizeof(struct jsonValue));
        if (!new_values) return 0;
        memcpy(new_values, array->values, array->capacity * sizeof(struct jsonValue));
        json_free(array->values);
        array->capacity = new_capacity;
        array->values = new_values;
    }
    array->values[array->size++] = value;
    return 1;
}

static void json_array_free(struct jsonArray * array) {
    size_t i;
    if (array->values) {
        for (i = 0; i < array->size; ++i) {
            json_value_free(array->values[i]);
        }
        json_free(array->values);
    }
    json_free(array);
}

/* Iterates over 'array' and for each value calls 'action' with index, value itself and
   'user_data'. */
extern void json_array_for_each(struct jsonArray * array,
        void (*action)(size_t, struct jsonValue, void *), void * user_data) {
    size_t i;
    for (i = 0; i < array->size; ++i) {
        action(i, array->values[i], user_data);
    }
}

/* Parses json array. 'json' must point to next lexeme after '['. Don't forget to free. */
static size_t json_parse_array(const char * json, struct jsonArray ** array) {
    const char * begin = json;
    struct jsonValue value;
    struct LexemeData data;
    size_t bytes_read;
    if (!(bytes_read = next_lexeme(json, &data))) return 0;
    if (JLK_R_SQUARE_BRACKET == data.kind) {
        json += bytes_read;
        *array = json_array_create(0);
        return json - begin;
    }
    *array = json_array_create(4);
    while (1) {
        if (!(bytes_read = json_parse_value(json, &value))) goto fail;
        json += bytes_read;
        if (!(bytes_read = next_lexeme(json, &data))) goto fail;
        json += bytes_read;
        if (!json_array_add(*array, value)) goto fail;
        if (JLK_R_SQUARE_BRACKET == data.kind) break;
        if (JLK_COMMA != data.kind) goto fail;
    }
    return json - begin;
fail:
    json_array_free(*array);
    *array = NULL;
    return 0;
}

/* Parses json value into 'value'. Don't forget to call json_value_free.  */
extern size_t json_parse_value(const char * json, struct jsonValue * value) {
    const char * begin = json;
    struct LexemeData data;
    struct jsonValue ret;
    size_t bytes_read;
    if (!(bytes_read = next_lexeme(json, &data))) return 0;
    json += bytes_read;
    switch (data.kind) {
    case JLK_L_BRACE:
        ret.kind = JVK_OBJ;
        bytes_read = json_parse_object(json, &ret.value.object);
        if (!bytes_read) return 0;
        json += bytes_read;
        break;
    case JLK_L_SQUARE_BRACKET:
        ret.kind = JVK_ARR;
        bytes_read = json_parse_array(json, &ret.value.array);
        if (!bytes_read) return 0;
        json += bytes_read;
        break;
    case JLK_TRUE:
        ret.kind = JVK_BOOL;
        ret.value.boolean = 1;
        break;
    case JLK_FALSE:
        ret.kind = JVK_BOOL;
        ret.value.boolean = 0;
        break;
    case JLK_NULL:
        ret.kind = JVK_NULL;
        break;
    case JLK_STRING:
        ret.kind = JVK_STR;
        ret.value.string = unescape_string(&data);
        if (!ret.value.string) return 0;
        break;
    case JLK_NUMBER:
        ret.kind = JVK_NUM;
        ret.value.number = data.ex.number;
        break;
    default: return 0;
    }
    *value = ret;
    return json - begin;
}

extern void json_value_free(struct jsonValue value) {
    switch (value.kind) {
    case JVK_OBJ: json_object_free(value.value.object); break;
    case JVK_ARR: json_array_free(value.value.array);   break;
    case JVK_STR: json_free(value.value.string);        break;
    default:;
    }
}
