/* Build with Clang -fsanitize=fuzzer,address,undefined, or include with
 * MULIB_FUZZ_STANDALONE for a deterministic hostile-input smoke test.
 * No filesystem, subprocess, display, terminal or device access is performed. */
#include "implementation.c"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int mulib_fuzz_csv_(void *user, const char *const *fields, size_t count) {
    (void)user; for (size_t i = 0; i < count; ++i) assert(fields[i]); return 1;
}
static int mulib_fuzz_ini_(void *user, const char *section, const char *key, const char *value) {
    (void)user; assert(section && key && value); return 1;
}
static int mulib_fuzz_send_(void *user, const void *bytes, size_t length) {
    (void)user; assert(bytes || !length); return 1;
}
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    char *text; unsigned char decoded[1024]; size_t used; uint64_t integer;
    mulib_json_t *json; mulib_xml_node_t *xml; mulib_sexpr_t *sexpr;
    if (size > sizeof(decoded)) return 0;
    text = (char *)malloc(size+1); assert(text);
    if (size) memcpy(text,data,size);
    text[size] = 0;
    json = mulib_json_parse(text,size,NULL); mulib_json_destroy(json);
    json = mulib_yaml_parse(text,size,NULL); mulib_json_destroy(json);
    json = mulib_toml_parse(text,size,NULL); mulib_json_destroy(json);
    xml = mulib_xml_parse(text,size,NULL); mulib_xml_destroy(xml);
    sexpr = mulib_sexpr_parse(text,NULL);
    if (sexpr) { char *rendered = mulib_sexpr_render(sexpr); free(rendered); }
    mulib_sexpr_destroy(sexpr);
    (void)mulib_csv_parse(text,mulib_fuzz_csv_,NULL);
    (void)mulib_ini_parse(text,mulib_fuzz_ini_,NULL);
    (void)mulib_base64_decode(text,size,decoded,sizeof(decoded),&used);
    (void)mulib_hex_decode(text,size,decoded,sizeof(decoded));
    (void)mulib_url_decode(text,size,decoded,sizeof(decoded),&used);
    (void)mulib_rle_decode(data,size,decoded,sizeof(decoded),&used);
    (void)mulib_varint_decode(data,size,&integer,&used);
    (void)mulib_utf8_validate(data,size);
    { mulib_semver_t version; mulib_uuid_t id; mulib_uri_t uri; mulib_inet4_t v4; mulib_inet6_t v6;
      (void)mulib_semver_parse(text,&version); (void)mulib_uuid_parse(text,&id);
      (void)mulib_uri_parse(text,size,&uri); (void)mulib_inet4_parse(text,size,&v4); (void)mulib_inet6_parse(text,size,&v6); }
    { mulib_http_options_t options = {1024,16,4096,MULIB_HTTP_EOF}; mulib_http_message_t message = {0};
      for (int response = 0; response < 2; ++response) {
          if (mulib_http_parse(data,size,response,&options,&message,&used) == MULIB_HTTP_OK) mulib_http_message_free(&message);
      } }
    { mulib_dbus_limits_t limits = {4096,128,8}; mulib_dbus_message_t message = {0};
      if (mulib_dbus_decode(data,size,&limits,&message,&used) == MULIB_DBUS_OK) mulib_dbus_message_free(&message);
      (void)mulib_dbus_signature_valid(text,size); }
    { mulib_udev_event_t event; mulib_wayland_frame_t frame; mulib_elf_t elf;
      (void)mulib_udev_parse(data,size,&event); (void)mulib_wayland_frame_parse(data,size,&frame,&used); (void)mulib_elf_open(&elf,data,size); }
    { mulib_terminfo_t info = {0}; mulib_strbuf_t output;
      (void)mulib_terminfo_load(&info,data,size); mulib_terminfo_free(&info);
      mulib_strbuf_init(&output); (void)mulib_terminfo_expand(text,NULL,&output); mulib_strbuf_free(&output); }
    { mulib_termkey_t decoder; mulib_termkey_event_t event; size_t offset = 0;
      mulib_termkey_init(&decoder);
      while (offset < size) {
          size_t fed = mulib_termkey_feed(&decoder,data+offset,size-offset); offset += fed;
          while (mulib_termkey_next(&decoder,&event,0) == 1) {}
          if (!fed) break;
      }
      while (mulib_termkey_next(&decoder,&event,1) == 1) {} }
    { mulib_vterm_t screen; assert(mulib_vterm_init(&screen,8,4));
      assert(mulib_vterm_feed(&screen,data,size)); mulib_vterm_finish(&screen); mulib_vterm_free(&screen); }
    { mulib_css_style_t *style = mulib_css_parse_declarations(text,size,NULL); mulib_css_style_destroy(style);
      mulib_css_sheet_t *sheet = mulib_css_parse(text,size,NULL); mulib_css_destroy(sheet);
      (void)mulib_css_matches(NULL,text); }
    { mulib_xml_node_t *root = mulib_xml_new(MULIB_XML_ELEMENT,"root"); mulib_xpath_result_t result = {0};
      assert(root); (void)mulib_xpath_query(root,text,&result,NULL); mulib_xpath_result_free(&result); mulib_xml_destroy(root); }
    { char *result = mulib_markdown_to_html(text,size,NULL,NULL); free(result);
      result = mulib_template_render(text,size,NULL,NULL,NULL); free(result); }
    { mulib_regex_t *regex = mulib_regex_compile(text,size,NULL);
      if (regex) { (void)mulib_regex_full_match(regex,text,size); mulib_regex_destroy(regex); } }
    { mulib_peg_t *grammar = mulib_peg_compile(text,size,NULL);
      if (grammar) { mulib_parsec_result_t result; mulib_parsec_options_t limits = {64,10000,1000};
          (void)mulib_peg_run(grammar,NULL,text,size,1,&limits,&result); mulib_parsec_result_free(&result); mulib_peg_destroy(grammar); } }
    { char name[256]; mulib_dns_message_t message;
      (void)mulib_dns_name_normalize(text,size,name);
      (void)mulib_dns_name_decode(data,size,0,name,&used);
      (void)mulib_dns_message_parse(data,size,16,&message); }
    { mulib_microjs_limits_t limits = {1024,4096,32,16,1024};
      mulib_microjs_value_t value = {0}; mulib_microjs_error_t error;
      mulib_microjs_t *interpreter = mulib_microjs_create(&limits); assert(interpreter);
      (void)mulib_microjs_eval(interpreter,text,size,&value,&error);
      mulib_microjs_value_free(&value); mulib_microjs_destroy(interpreter); }
    { mulib_microaudio_limits_t limits = {1024,4096}; mulib_microaudio_pcm_t pcm = {0};
      if (mulib_microaudio_wav_decode(data,size,&limits,&pcm) == MULIB_MICROAUDIO_OK && pcm.frames > 1) {
          mulib_microaudio_pcm_t source = pcm, destination = pcm;
          source.frames--; destination.frames--;
          destination.samples += pcm.channels;
          assert(mulib_microaudio_mix(&destination,0,&source,0,source.frames,0.5) == MULIB_MICROAUDIO_OK);
          assert(mulib_microaudio_mix(&source,0,&destination,0,source.frames,-0.5) == MULIB_MICROAUDIO_OK);
      }
      mulib_microaudio_free(&pcm); }
    { mulib_perlpod_limits_t limits = {1024,4096,16};
      char *rendered = mulib_perlpod_render(text,size,MULIB_PERLPOD_HTML,&limits,&used,NULL); free(rendered); }
    { mulib_cron_t schedule; char normalized[MULIB_VFS_PATH_CAP], *path = NULL;
      (void)mulib_cron_parse(&schedule,text,&used);
      (void)mulib_vfs_normalize(text,normalized,sizeof(normalized));
      (void)mulib_fileserver_path(text,NULL,&path); free(path); }
    { mulib_dsymspec_t *spec = mulib_dsymspec_load_json(text,size,NULL); mulib_dsymspec_destroy(spec); }
    { char line[128]; mulib_ftpserver session;
      const mulib_ftpserver_io io = {mulib_fuzz_send_,NULL,NULL,NULL,NULL,NULL};
      assert(mulib_ftpserver_init(&session,line,sizeof(line),&io,NULL));
      (void)mulib_ftpserver_feed(&session,data,size); }
    { char line[128]; unsigned char message[256]; mulib_smtpserver session;
      const mulib_smtpserver_io io = {mulib_fuzz_send_,NULL,NULL};
      assert(mulib_smtpserver_init(&session,line,sizeof(line),message,sizeof(message),&io,NULL));
      (void)mulib_smtpserver_feed(&session,data,size); }
    { char line[128]; mulib_imapserver session;
      const mulib_imapserver_io io = {mulib_fuzz_send_,NULL,NULL};
      assert(mulib_imapserver_init(&session,line,sizeof(line),&io,NULL));
      (void)mulib_imapserver_feed(&session,data,size); }
    free(text); return 0;
}
#ifdef MULIB_FUZZ_STANDALONE
int main(void) {
    static const char *const seeds[] = {
        "", "\"", "\"a\\\"", "(a (b) \"c\")", "[1,true,null,\"\\u0000\"]", "{\"a\":[1,2,3]}",
        "key: [1, 2]", "[section]\na=42", "<a x=\"b\">text&amp;</a>", "a,b\n\"x\"\"y\",z",
        "a { color: red; }", "//a[@id='x']", "{{#if a}}x{{else}}y{{/if}}", "[x](javascript:bad)",
        "root <- ('a' / 'b')* !.", "(a?)*", "GET / HTTP/1.1\r\nHost: a\r\n\r\n", "::ffff:192.0.2.1",
        "\033[99999999999999999999999999999999999A", "%?%p1%{0}%/%d%;",
        "let answer = (6 * 7); answer", "let s = 'a\\'b'; s + 'c'", "false && (1/0)",
        "=pod\n=head1 Title\n\nB<bold I<nested>>\n\n=cut\n", "*/15 1-5 * * 0,7",
        "USER guest\r\nPASS password\r\nTYPE I\r\nQUIT\r\n",
        "EHLO example\r\nMAIL FROM:<>\r\nRCPT TO:<a@b>\r\nDATA\r\n..x\r\n.\r\n",
        "a1 CAPABILITY\r\na2 LOGIN \"guest\" \"password\"\r\na3 LOGOUT\r\n"
    };
    static const unsigned char dns[] = {0x12,0x34,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};
    static const unsigned char wav[] = {
        'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',
        16,0,0,0,1,0,1,0,0x44,0xac,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a',4,0,0,0,
        0x10,0,0x20,0
    };
    unsigned char bytes[256]; uint32_t random = UINT32_C(0x63e5a912);
    for (size_t i = 0; i < sizeof(seeds)/sizeof(*seeds); ++i) {
        size_t n = strlen(seeds[i]);
        for (size_t cut = 0; cut <= n; ++cut) LLVMFuzzerTestOneInput((const unsigned char *)seeds[i],cut);
    }
    for (size_t cut = 0; cut <= sizeof(dns); ++cut) LLVMFuzzerTestOneInput(dns,cut);
    for (size_t cut = 0; cut <= sizeof(wav); ++cut) LLVMFuzzerTestOneInput(wav,cut);
    for (unsigned i = 0; i < 2000; ++i) {
        size_t n = i % sizeof(bytes);
        for (size_t j = 0; j < n; ++j) { random = random*UINT32_C(1664525)+UINT32_C(1013904223); bytes[j] = (unsigned char)(random>>24); }
        LLVMFuzzerTestOneInput(bytes,n);
    }
    return 0;
}
#endif
