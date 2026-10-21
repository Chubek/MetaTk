# rubyconv.q — ruby C API primitives.
@module rubyconv
@version 1.0
@param PREFIX rubyconv
@include <ruby.h>
@guard

@fn from_int64 {
static inline VALUE ${PREFIX}_from_int64(long long v) {
    return LL2NUM(v);
}
}

@fn to_int64 {
static inline long long ${PREFIX}_to_int64(VALUE v) {
    return NUM2LL(v);
}
}

@fn from_double {
static inline VALUE ${PREFIX}_from_double(double v) {
    return DBL2NUM(v);
}
}

@fn to_double {
static inline double ${PREFIX}_to_double(VALUE v) {
    return NUM2DBL(v);
}
}

@fn from_boolean {
static inline VALUE ${PREFIX}_from_boolean(int v) {
    return v ? Qtrue : Qfalse;
}
}

@fn string {
static inline VALUE ${PREFIX}_string(const char *s, long n) {
    return rb_str_new(s, n);
}
}

@fn string_data {
static inline const char * ${PREFIX}_string_data(VALUE v) {
    Check_Type(v, T_STRING); return RSTRING_PTR(v);
}
}

@fn string_size {
static inline long ${PREFIX}_string_size(VALUE v) {
    Check_Type(v, T_STRING); return RSTRING_LEN(v);
}
}

