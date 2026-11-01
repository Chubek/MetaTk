/* No implementation switches here: link against tests/implementation.c. */
#include "mulib-json.h"
#include "mulib-yaml.h"
#include "mulib-toml.h"
#include "mulib-template.h"
#include "mulib-memtkx.h"
#include "mulib-mempool.h"
#include "mulib-strpool.h"
#include "mulib-vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct composition_vertex { const char *label; double norm; } composition_vertex;
static void composition_config(const mulib_json_t *document) {
    mulib_yaml_error_t yaml_error;
    mulib_toml_error_t toml_error;
    mulib_template_error_t template_error;
    mulib_json_t *yaml_value, *toml_value;
    const char source[] = "<p>{{label}}</p>";
    char *yaml, *toml, *rendered;
    size_t length;
    yaml = mulib_yaml_stringify(document, &yaml_error);
    assert(yaml);
    yaml_value = mulib_yaml_parse(yaml, strlen(yaml), &yaml_error);
    assert(yaml_value);
    toml = mulib_toml_stringify(yaml_value, &toml_error);
    assert(toml);
    toml_value = mulib_toml_parse(toml, strlen(toml), &toml_error);
    assert(toml_value);
    assert(mulib_json_numeric(mulib_json_at(mulib_json_get(toml_value, "coords"), 1)) == 4);
    rendered = mulib_template_render(source, sizeof(source) - 1, toml_value, &length, &template_error);
    assert(rendered && length == 13 && !strcmp(rendered, "<p>vertex</p>"));
    free(yaml); free(toml); free(rendered);
    mulib_json_destroy(yaml_value);
    mulib_json_destroy(toml_value);
}
int main(void) {
    const char source[] = "{\"label\":\"vertex\",\"coords\":[3,4,0]}";
    mulib_json_error_t error;
    mulib_json_t *document = mulib_json_parse(source, sizeof(source) - 1, &error);
    mulib_json_t *coordinates, *output, *roundtrip;
    mulib_memtkx_t *tracker = mulib_memtkx_create(NULL);
    mulib_allocator_t allocator = mulib_memtkx_allocator(tracker);
    mulib_strpool_t *strings = mulib_strpool_create(&allocator);
    mulib_mempool_t *vertices = mulib_mempool_create(sizeof(composition_vertex), 8, &allocator);
    mulib_vector_t vector;
    composition_vertex *vertex;
    const char *label;
    size_t length;
    char *rendered;
    assert(document && tracker && strings && vertices);
    composition_config(document);
    coordinates = mulib_json_get(document, "coords");
    assert(mulib_json_typeof(coordinates) == MULIB_JSON_ARRAY);
    assert(mulib_vector_init(&vector, mulib_json_size(coordinates)));
    for (size_t i = 0; i < mulib_json_size(coordinates); ++i)
        assert(mulib_vector_set(&vector, i, mulib_json_numeric(mulib_json_at(coordinates, i))));
    label = mulib_json_text(mulib_json_get(document, "label"), &length);
    assert(label);
    vertex = (composition_vertex *)mulib_mempool_acquire(vertices);
    assert(vertex);
    vertex->label = mulib_strpool_intern_n(strings, label, length);
    vertex->norm = mulib_vector_norm(&vector);
    assert(vertex->label && vertex->norm == 5);
    assert(vertex->label == mulib_strpool_intern(strings, "vertex"));
    output = mulib_json_new(MULIB_JSON_OBJECT);
    assert(output);
    assert(mulib_json_set(output, vertex->label, mulib_json_number(vertex->norm)));
    rendered = mulib_json_stringify(output, 0);
    assert(rendered);
    roundtrip = mulib_json_parse(rendered, strlen(rendered), &error);
    assert(roundtrip && mulib_json_numeric(mulib_json_get(roundtrip, "vertex")) == 5);
    free(rendered);
    mulib_json_destroy(roundtrip);
    mulib_json_destroy(output);
    mulib_json_destroy(document);
    mulib_vector_free(&vector);
    assert(mulib_memtkx_stats(tracker).live_blocks > 0);
    mulib_mempool_destroy(vertices);
    mulib_strpool_destroy(strings);
    assert(mulib_memtkx_stats(tracker).live_blocks == 0);
    mulib_memtkx_destroy(tracker);
    return 0;
}
