#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#define MULIB_IMPL_INPUT
#define MULIB_IMPL_KEYREMAP
#define MULIB_IMPL_VEC
#define MULIB_IMPL_MARIONETTE
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UDEV
#define MULIB_IMPL_TERMUTILS
#define MULIB_IMPL_VDEV
#define MULIB_IMPL_KVM
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_WAYLAND
#define MULIB_IMPL_WLROOTS
#define MULIB_IMPL_XLIB
#include "mulib-keyremap.h"
#include "mulib-marionette.h"
#include "mulib-udev.h"
#include "mulib-termutils.h"
#include "mulib-vdev.h"
#include "mulib-kvm.h"
#include "mulib-wayland.h"
#include "mulib-wlroots.h"
#include "mulib-xlib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void remap(void) {
    mulib_keyremap_t r; mulib_input_event_t e = { 100, MULIB_INPUT_KEY, 30, 1 }, out = {0};
    mulib_keyremap_init(&r);
    assert(mulib_keyremap_set(&r,30,40) && mulib_keyremap_set(&r,31,40));
    assert(mulib_keyremap_apply(&r,&e,&out) == 1 && out.code == 40);
    assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    e.code = 31; assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    assert(mulib_keyremap_set(&r,30,50));
    e.code = 30; e.value = 2; assert(mulib_keyremap_apply(&r,&e,&out) == 1 && out.code == 40);
    e.value = 0; assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    e.code = 31; assert(mulib_keyremap_apply(&r,&e,&out) == 1 && out.code == 40 && !out.value);
    assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    e.code = 30; e.value = 1; assert(mulib_keyremap_apply(&r,&e,&e) == 1 && e.code == 50);
    assert(mulib_keyremap_set(&r,31,MULIB_KEYREMAP_DROP));
    e.code = 31; assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    e.value = 2; assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    e.value = 0; assert(mulib_keyremap_apply(&r,&e,&out) == 0);
    e.value = 3; assert(mulib_keyremap_apply(&r,&e,&out) == -1);
    e.type = MULIB_INPUT_SYN; e.value = 0; assert(mulib_keyremap_apply(&r,&e,&out) == 1);
    assert(!mulib_keyremap_set(&r,768,1) && !mulib_keyremap_set(&r,1,768));
}
typedef struct sink_state { size_t count; int fail; } sink_state;
static int sink(const mulib_input_event_t *e, void *user) {
    sink_state *s = (sink_state *)user;
    assert(mulib_input_event_valid(e)); if (s->fail) return -1; ++s->count; return 0;
}
static void playback(void) {
    mulib_marionette_t r; mulib_input_event_t e = {10,MULIB_INPUT_KEY,30,1}; size_t sent; sink_state s = {0,0};
    mulib_marionette_init(&r,2);
    assert(mulib_marionette_append(&r,&e)); e.time_us = 9; assert(!mulib_marionette_append(&r,&e));
    e.time_us = 20; e.value = 0; assert(mulib_marionette_append(&r,&e) && !mulib_marionette_append(&r,&e));
    assert(!mulib_marionette_start(&r,UINT64_MAX-19)); assert(mulib_marionette_start(&r,100));
    assert(!mulib_marionette_append(&r,&e));
    assert(!mulib_marionette_tick(&r,99,0,sink,&s,&sent) && !sent);
    assert(!mulib_marionette_tick(&r,109,0,sink,&s,&sent) && !sent);
    s.fail = 1; assert(mulib_marionette_tick(&r,120,0,sink,&s,&sent) == -2 && !sent && !r.cursor);
    s.fail = 0; assert(!mulib_marionette_tick(&r,120,1,sink,&s,&sent) && sent == 1 && r.playing);
    assert(!mulib_marionette_tick(&r,120,0,sink,&s,&sent) && sent == 1 && !r.playing && s.count == 2);
    assert(mulib_marionette_start(&r,0)); mulib_marionette_stop(&r);
    mulib_marionette_free(&r); mulib_marionette_free(&r);
    assert(mulib_marionette_start(&r,UINT64_MAX) && !r.playing);
}
static void uevents(void) {
    const char packet[] = "add@/devices/test\0ACTION=add\0DEVPATH=/devices/test\0SUBSYSTEM=input\0NAME=a=b\0";
    const char duplicate[] = "add@/x\0A=1\0A=2\0";
    const char mismatch[] = "add@/x\0ACTION=remove\0";
    const char badkey[] = "add@/x\0bad=1\0";
    const char padding[] = "add@/x\0\0A=1\0";
    mulib_udev_event_t e, before;
    assert(mulib_udev_parse(packet,sizeof(packet),&e) == MULIB_UDEV_OK);
    assert(e.action_length == 3 && !memcmp(e.action,"add",3) && !strcmp(e.path,"/devices/test"));
    assert(!strcmp(mulib_udev_get(&e,"SUBSYSTEM"),"input") && !strcmp(mulib_udev_get(&e,"NAME"),"a=b"));
    assert(!mulib_udev_get(&e,"ABSENT")); before = e;
    assert(mulib_udev_parse(duplicate,sizeof(duplicate)-1,&e) == MULIB_UDEV_INVALID && !memcmp(&e,&before,sizeof(e)));
    assert(mulib_udev_parse(mismatch,sizeof(mismatch),&e) == MULIB_UDEV_INVALID);
    assert(mulib_udev_parse(badkey,sizeof(badkey),&e) == MULIB_UDEV_INVALID);
    assert(mulib_udev_parse(padding,sizeof(padding),&e) == MULIB_UDEV_INVALID);
    assert(mulib_udev_parse("add@/x",6,&e) == MULIB_UDEV_INVALID);
    assert(mulib_udev_parse(NULL,0,&e) == MULIB_UDEV_INVALID);
    assert(mulib_udev_parse(packet,65537,&e) == MULIB_UDEV_LIMIT);
    {
        char many[2048]; size_t n = 7; memcpy(many,"add@/x",7);
        for (unsigned i = 0; i < 65; ++i) n += (size_t)snprintf(many+n,sizeof(many)-n,"K%u=v",i)+1;
        assert(mulib_udev_parse(many,n,&e) == MULIB_UDEV_LIMIT);
    }
}
static void frames(void) {
    unsigned char payload[4] = {1,2,3,4}, *wire = NULL; size_t n, used;
    mulib_wayland_frame_t f = {7,42,payload,4}, parsed = {0};
    assert(mulib_wayland_frame_encode(&f,&wire,&n) == MULIB_WAYLAND_OK && n == 12);
    for (size_t i = 0; i < n; ++i) {
        assert(mulib_wayland_frame_parse(wire,i,&parsed,&used) == MULIB_WAYLAND_MORE && !used && !parsed.object);
    }
    assert(mulib_wayland_frame_parse(wire,n,&parsed,&used) == MULIB_WAYLAND_OK && used == n && parsed.object == 7 && parsed.opcode == 42);
    assert(parsed.length == 4 && !memcmp(parsed.payload,payload,4));
    { unsigned char unaligned[13]; memcpy(unaligned+1,wire,n); assert(mulib_wayland_frame_parse(unaligned+1,n,&parsed,&used) == MULIB_WAYLAND_OK); }
    memset(wire,0,4); assert(mulib_wayland_frame_parse(wire,n,&parsed,&used) == MULIB_WAYLAND_INVALID && !used);
    free(wire); f.length = 3; assert(mulib_wayland_frame_encode(&f,&wire,&n) == MULIB_WAYLAND_INVALID);
    assert(mulib_wayland_fd(NULL) == -1 && mulib_wayland_roundtrip(NULL) == -1 && !mulib_wayland_display(NULL));
    mulib_wayland_disconnect(NULL);
    assert(!mulib_xlib_window_create(NULL,0,0,1,1) && !mulib_xlib_display(NULL)); mulib_xlib_close(NULL);
}
static void geometry(void) {
    mulib_wlroots_layout_t l; mulib_wlroots_box_t a = {10,20,30,40}, b = {20,30,50,60}, out;
    static const mulib_wlroots_box_t expected[8] = {
        {10,20,30,40},{140,10,40,30},{60,140,30,40},{20,60,40,30},
        {60,20,30,40},{140,60,40,30},{10,140,30,40},{20,10,40,30}
    };
    for (unsigned t = 0; t < 8; ++t) { assert(mulib_wlroots_box_transform(&a,t,100,200,&out)); assert(!memcmp(&out,&expected[t],sizeof(out))); }
    assert(mulib_wlroots_box_contains(&a,10,20) && !mulib_wlroots_box_contains(&a,40,20));
    assert(mulib_wlroots_box_intersection(&a,&b,&out) && out.x == 20 && out.y == 30 && out.width == 20 && out.height == 30);
    assert(!mulib_wlroots_box_transform(&a,8,100,200,&out) && !mulib_wlroots_box_transform(&a,0,10,10,&out));
    b.x = INT32_MAX; assert(!mulib_wlroots_box_valid(&b));
    mulib_wlroots_layout_init(&l);
    assert(mulib_wlroots_layout_set(&l,1,&a) && mulib_wlroots_layout_at(&l,11,21) == 1);
    a.x = 50; assert(mulib_wlroots_layout_set(&l,1,&a) && !mulib_wlroots_layout_at(&l,11,21));
    assert(mulib_wlroots_layout_set(&l,2,&a)); assert(mulib_wlroots_layout_at(&l,51,21) == 1);
    assert(mulib_wlroots_layout_remove(&l,1) && mulib_wlroots_layout_at(&l,51,21) == 2);
    assert(!mulib_wlroots_layout_remove(&l,1)); mulib_wlroots_layout_free(&l); mulib_wlroots_layout_free(&l);
}
static void native_errors(void) {
    mulib_input_t input = MULIB_INPUT_INIT; mulib_input_event_t event = {0};
    mulib_vdev_t vdev = MULIB_VDEV_INIT; mulib_kvm_t kvm = MULIB_KVM_INIT;
    mulib_kvm_vm_t vm = MULIB_KVM_VM_INIT; mulib_kvm_vcpu_t cpu = MULIB_KVM_VCPU_INIT;
    mulib_termutils_raw_t raw = MULIB_TERMUTILS_RAW_INIT; mulib_udev_monitor_t monitor = MULIB_UDEV_MONITOR_INIT;
    assert(mulib_input_close(&input) == 0 && mulib_input_read(&input,&event) == MULIB_INPUT_EINVAL);
    assert(mulib_vdev_close(&vdev) == 0 && mulib_vdev_sync(&vdev) == MULIB_INPUT_EINVAL);
    assert(mulib_vdev_create(&vdev,NULL,"",NULL,0,1) == MULIB_INPUT_EINVAL);
    assert(mulib_kvm_close(&kvm) == 0 && mulib_kvm_vm_close(&vm) == 0 && mulib_kvm_vcpu_close(&cpu) == 0);
    assert(mulib_kvm_run(&cpu,NULL) == MULIB_KVM_EINVAL);
    assert(mulib_termutils_raw_end(&raw) == 0 && mulib_termutils_raw_begin(&raw,-1) == MULIB_TERMUTILS_EINVAL);
    assert(mulib_udev_monitor_close(&monitor) == MULIB_UDEV_OK);
#ifdef __linux__
    {
        int fds[2]; struct input_event native; size_t written; char bytes[4];
        assert(pipe(fds) == 0); memset(&native,0,sizeof(native)); native.type = EV_KEY; native.code = 30; native.value = 1;
        native.input_event_sec = 2; native.input_event_usec = 3;
        assert(write(fds[1],&native,sizeof(native)) == (ssize_t)sizeof(native)); input.fd = fds[0];
        assert(mulib_input_read(&input,&event) == 0 && event.time_us == 2000003 && event.code == 30);
        assert(fcntl(fds[0],F_SETFL,O_NONBLOCK) == 0 && mulib_input_read(&input,&event) == MULIB_INPUT_AGAIN);
        assert(mulib_termutils_write(fds[1],"abc",3,&written) == 0 && written == 3 && read(fds[0],bytes,3) == 3 && !memcmp(bytes,"abc",3));
        assert(mulib_input_close(&input) == 0); assert(close(fds[1]) == 0);
        assert(mulib_input_open(&input,"/dev/null") == 0 && mulib_input_read(&input,&event) == MULIB_INPUT_EIO);
        assert(mulib_input_grab(&input,1) == MULIB_INPUT_EIO && mulib_input_close(&input) == 0);
        assert(mulib_kvm_open(&kvm,"/dev/null") == MULIB_KVM_EIO && kvm.fd == -1);
        assert(mulib_vdev_create(&vdev,"/dev/null","test",NULL,0,1) == MULIB_INPUT_EIO && vdev.fd == -1);
    }
    {
        int master = posix_openpt(O_RDWR | O_NOCTTY), slave; struct termios before, after; struct winsize size = {24,80,0,0}; unsigned cols, rows;
        assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
        slave = open(ptsname(master),O_RDWR | O_NOCTTY); assert(slave >= 0 && tcgetattr(slave,&before) == 0);
        assert(mulib_termutils_raw_begin(&raw,slave) == 0 && raw.saved);
        assert(mulib_termutils_raw_begin(&raw,slave) == MULIB_TERMUTILS_ESTATE);
        assert(tcgetattr(slave,&after) == 0 && !(after.c_lflag & (ICANON | ECHO | ISIG)));
        assert(ioctl(slave,TIOCSWINSZ,&size) == 0 && mulib_termutils_size(slave,&cols,&rows) == 0 && cols == 80 && rows == 24);
        assert(mulib_termutils_raw_end(&raw) == 0 && !raw.saved && tcgetattr(slave,&after) == 0);
        assert(before.c_iflag == after.c_iflag && before.c_oflag == after.c_oflag && before.c_lflag == after.c_lflag && before.c_cflag == after.c_cflag);
        assert(close(slave) == 0 && close(master) == 0);
    }
#endif
}
int main(void) { remap(); playback(); uevents(); frames(); geometry(); native_errors(); return 0; }
